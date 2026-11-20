#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Serve Dreamcast covers, manuals and previews to K-UI on a trusted LAN.

Example: python3 tools/media_server.py --bind 192.168.1.10
Add --gamelist /roms/dreamcast/gamelist.xml to use a ROCKNIX library first.
ScreenScraper needs KUI_SS_DEVID and KUI_SS_DEVPASSWORD. Optional member
credentials use KUI_SS_USER and KUI_SS_PASSWORD. The public Libretro cover
source works without credentials. Use --offline for local media only.
"""
from __future__ import annotations

import argparse
import collections
import datetime
import functools
import http.client
import http.server
import io
import os
import re
import shutil
import sys
import tempfile
import time
import unicodedata
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

from PIL import Image, ImageOps

MIB = 1024 * 1024
SOURCE_LIMIT = {"cover": 16 * MIB, "manual": 96 * MIB, "preview": 96 * MIB}
OUTPUT = {"cover": ("image/png", 256 * 1024),
          "manual": ("application/x-kui-manual", 32 * MIB),
          "preview": ("application/x-kui-preview", 3840016)}
REGIONS = {"U": ("us", "USA"), "E": ("eu", "Europe"), "J": ("jp", "Japan")}
DAT_URL = "https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/redump/Sega%20-%20Dreamcast.dat"
COVER_BASE = "https://raw.githubusercontent.com/libretro-thumbnails/Sega_-_Dreamcast/master/Named_Boxarts/"
SS_URL = "https://api.screenscraper.fr/api2/jeuRecherche.php"


class MediaError(Exception):
    def __init__(self, status, message):
        super().__init__(message)
        self.status = status


def normalized(value):
    value = unicodedata.normalize("NFKC", value).casefold().strip()
    article = re.fullmatch(r"(.+),\s*(the|an|a)", value)
    if article:
        value = article[2] + " " + article[1]
    return " ".join(re.findall(r"[^\W_]+", value, re.UNICODE))


def serial(value):
    return re.sub(r"[ -]", "", value.strip().upper())


@dataclass(frozen=True)
class Game:
    product: str
    region: str
    title: str

    def regions(self):
        return [pair for key, pair in REGIONS.items() if key in self.region.upper()]


def parse_request(target):
    url = urllib.parse.urlsplit(target)
    kind = url.path.rsplit("/", 1)[-1]
    if url.scheme or url.netloc or kind not in OUTPUT or url.path != "/v1/dreamcast/" + kind:
        raise MediaError(404, "Unknown media path")
    try:
        query = urllib.parse.parse_qs(url.query, keep_blank_values=True, strict_parsing=True,
                                      max_num_fields=3, errors="strict")
    except (UnicodeError, ValueError):
        raise MediaError(400, "Invalid media query") from None
    fields = []
    for key, limit in (("product", 10), ("region", 8), ("title", 128)):
        values = query.get(key, [""])
        if (len(values) != 1 or len(values[0]) > limit or
                any(ord(c) < 32 or ord(c) == 127 for c in values[0])):
            raise MediaError(400, "Invalid game identity")
        fields.append(values[0].strip())
    if set(query) - {"product", "region", "title"} or not (fields[0] or normalized(fields[2])):
        raise MediaError(400, "Missing game identity")
    return kind, Game(*fields)


def checked_url(url, domains):
    parts = urllib.parse.urlsplit(url)
    host = parts.hostname or ""
    if (parts.scheme != "https" or parts.username is not None or parts.password is not None or
            parts.port not in (None, 443) or
            not any(host == domain or host.endswith("." + domain) for domain in domains)):
        raise MediaError(502, "Provider returned an unsupported media URL")
    return url


class ProviderRedirect(urllib.request.HTTPRedirectHandler):
    def __init__(self, domains):
        self.domains = domains

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return super().redirect_request(req, fp, code, msg, headers, checked_url(newurl, self.domains))


def fetch(url, domains, limit, output):
    """Download one bounded HTTPS object. Never expose provider URLs or credentials in errors."""
    req = urllib.request.Request(checked_url(url, domains), headers={"User-Agent": "K-UI-Media/1"})
    opener = urllib.request.build_opener(ProviderRedirect(domains))
    with opener.open(req, timeout=20) as response:
        length = response.headers.get("Content-Length")
        expected = int(length) if length and length.isdecimal() else None
        if expected is not None and expected > limit:
            raise MediaError(413, "Provider file exceeds the source limit")
        used = 0
        deadline = time.monotonic() + 120
        while True:
            if time.monotonic() >= deadline:
                raise MediaError(504, "Provider download timed out")
            block = response.read(min(65536, limit + 1 - used))
            if not block:
                break
            used += len(block)
            if used > limit:
                raise MediaError(413, "Provider file exceeds the source limit")
            output.write(block)
        if not used or (expected is not None and expected != used):
            raise MediaError(502, "Incomplete provider download")


def local_regions(item, stem):
    # ROCKNIX writes short region codes; Redump filenames carry a parenthesized region.
    # An absent region is usable only if the remaining identity is unique.
    declared = re.findall(r"[a-z]+", (item.findtext("region") or "").casefold())
    tags = re.findall(r" \(([^()]+)\)", stem)
    aliases = {"j": "jp", "jp": "jp", "japan": "jp",
               "u": "us", "us": "us", "usa": "us",
               "e": "eu", "eu": "eu", "europe": "eu"}
    labels = declared or [word for tag in tags for word in re.findall(r"[a-z]+", tag.casefold())]
    return {aliases[word] for word in labels if word in aliases}


class LocalLibrary:
    """Only regular files beneath this Dreamcast gamelist's directory are eligible."""
    def __init__(self, path, media_root=None):
        self.root = (media_root or path.resolve().parent).resolve()
        tree = ET.parse(path).getroot()
        if tree.tag != "gameList" or not self.root.is_dir():
            raise ValueError("Expected a Dreamcast gamelist.xml and its ROM folder")
        self.games = tree.findall("game")

    def media_path(self, value):
        path = Path(value.strip())
        # A copied ROCKNIX SD card can contain absolute paths from the handheld.
        # Rebase only the system's own path, then enforce the chosen host root.
        for prefix in ("/storage/roms/dreamcast", "/roms/dreamcast"):
            if path.is_relative_to(prefix):
                path = path.relative_to(prefix)
                break
        found = (self.root / path).resolve()
        return found if found.is_relative_to(self.root) and found.is_file() else None

    def find(self, game, kind):
        matches = []
        allowed_regions = {code for code, _ in game.regions()}
        for item in self.games:
            stem = Path(item.findtext("path") or "").stem
            regions = local_regions(item, stem)
            if allowed_regions and regions and not regions.intersection(allowed_regions):
                continue
            item_serial = serial(item.findtext("serial") or "")
            if item_serial and game.product:
                if item_serial != serial(game.product):
                    continue
                rank = 0
            else:
                names = [item.findtext("name") or ""]
                names.append(stem)
                # Common Redump filenames include the release region even if
                # the scraped display name is absent. Keep edition qualifiers.
                plain = re.sub(r" \((?:USA|Europe|Japan|World|[A-Z][a-z](?:,[A-Z][a-z])*)\)", "", stem)
                names.append(plain)
                if not game.title or normalized(game.title) not in map(normalized, names):
                    continue
                rank = 1
            matches.append((rank, item))
        if not matches:
            return None
        rank = min(r for r, _ in matches)
        best = [item for r, item in matches if r == rank]
        if len(best) != 1:
            return None
        for tag in {"cover": ("boxart", "thumbnail", "image"),
                    "manual": ("manual",), "preview": ("video",)}[kind]:
            value = best[0].findtext(tag)
            if value:
                found = self.media_path(value)
                if found:
                    return found
        return None


@dataclass(frozen=True)
class Release:
    name: str
    region: str
    product: str


def parse_dat(data):
    records = []
    for block in re.findall(r'^game \(\s*\n(.*?)^\)', data, re.M | re.S):
        values = dict(re.findall(r'^\s*(name|region|serial) "([^"\n]+)"', block, re.M))
        if "name" in values and "region" in values:
            records.append(Release(values["name"], values["region"], serial(values.get("serial", ""))))
    if not records:
        raise MediaError(502, "Unrecognized Libretro Dreamcast catalogue")
    return records


# Libretro often files one box under the retail title/region for every disc,
# revision and language combination. Keep demo and special edition tags.
BOX_VARIANTS = re.compile(r" \((?:Disc [0-9]+|Rev [A-Z0-9]+|[A-Z][a-z](?:,[A-Z][a-z])*)\)")


def shared_box(names):
    if len(names) == 1:
        return next(iter(names))
    generic = {BOX_VARIANTS.sub("", name) for name in names}
    return next(iter(generic)) if len(generic) == 1 else None


def libretro_release(records, game):
    regions = {name for _, name in game.regions()}
    compatible = [r for r in records if not regions or r.region in regions or r.region == "World"]
    product = serial(game.product)
    serial_matches = set()
    for r in compatible:
        # USA first-party boot headers use MK-51035; the Redump catalogue
        # lists 51035. Other regions and third-party serials retain their prefix.
        if product and (r.product == product or
                        (r.region == "USA" and r.product.isdigit() and product == "MK" + r.product)):
            serial_matches.add(r.name)
    if serial_matches:
        return shared_box(serial_matches)
    # Fall back to an exact retail title in one compatible region.
    matches = set()
    for r in compatible:
        box = BOX_VARIANTS.sub("", r.name)
        title = box.removesuffix(f" ({r.region})")
        if title != box and normalized(title) == normalized(game.title):
            matches.add(r.name)
    return shared_box(matches)

class Libretro:
    @functools.cached_property
    def releases(self):
        data = io.BytesIO()
        fetch(DAT_URL, ["raw.githubusercontent.com"], 8 * MIB, data)
        return parse_dat(data.getvalue().decode("utf-8-sig"))

    def download(self, game, dest):
        name = libretro_release(self.releases, game)
        if not name:
            return False
        for box in dict.fromkeys([name, BOX_VARIANTS.sub("", name)]):
            filename = re.sub(r'[&*/:`"<>?\\|]', "_", box) + ".png"
            try:
                with dest.open("wb") as fp:
                    fetch(COVER_BASE + urllib.parse.quote(filename, safe=""),
                          ["raw.githubusercontent.com"], SOURCE_LIMIT["cover"], fp)
                return True
            except urllib.error.HTTPError as error:
                if error.code != 404:
                    raise
        return False


def screenscraper_xml(data):
    # ElementTree does not need DTDs for any of the published API records.
    if b"<!DOCTYPE" in data.upper() or b"<!ENTITY" in data.upper():
        raise MediaError(502, "Unexpected ScreenScraper document")
    try:
        root = ET.fromstring(data)
    except ET.ParseError:
        raise MediaError(502, "Invalid ScreenScraper document") from None
    if root.tag != "Data":
        raise MediaError(502, "Unexpected ScreenScraper response")
    return root


def exact_ss_game(root, title):
    hits = {}
    key = normalized(title)
    for item in root.findall("./jeux/jeu"):
        system = item.find("systeme")
        identity = item.get("id")
        if (identity and system is not None and system.get("id") == "23" and
                any(normalized(n.text or "") == key for n in item.findall("./noms/nom"))):
            hits[identity] = item
    return next(iter(hits.values())) if len(hits) == 1 else None


def select_media(item, game, kind):
    if item is None:
        return None
    types = {"cover": ["box-2D", "box-3D", "ss"], "manual": ["manuel"],
             "preview": ["video-normalized", "video"]}[kind]
    regions = [abbrev for abbrev, _ in game.regions()] + ["wor", ""]
    choices = []
    for medium in item.findall("./medias/media"):
        typ, region = medium.get("type"), medium.get("region", "")
        if typ in types and region in regions and medium.text:
            choices.append((types.index(typ), regions.index(region), medium.text.strip()))
    if not choices:
        return None
    url = min(choices)[2].replace(" ", "%20")
    # ScreenScraper sometimes supplies a host token in its media links.
    token = "#screenscraperserveur#"
    if url.startswith(token):
        url = "https://api.screenscraper.fr/" + url[len(token):].lstrip("/")
    elif url.startswith("http://"):
        url = "https://" + url[7:]
    return checked_url(url, ["screenscraper.fr"])


class ScreenScraper:
    """The single-threaded relay also serializes all upstream requests."""
    def __init__(self, auth):
        self.auth = auth.copy()
        self.next_requests = collections.deque()
        self.minute_limit = 30
        self.blocked_until = 0
        self.daily_remaining = None
        self.day = None
        try:
            self.provider_zone = ZoneInfo("Europe/Paris")
        except ZoneInfoNotFoundError:
            # UTC rolls over after Paris when OS timezone data is absent.
            self.provider_zone = datetime.timezone.utc

    def request(self, url, limit, fp):
        today = datetime.datetime.now(self.provider_zone).date()
        if today != self.day:
            self.daily_remaining = None
            self.day = today
        now = time.monotonic()
        if self.daily_remaining is not None and self.daily_remaining <= 0:
            raise MediaError(429, "ScreenScraper daily quota reached")
        if now < self.blocked_until:
            raise MediaError(429, "ScreenScraper is rate limited")
        # Pace a healthy bulk job through the known minute quota. An actual
        # upstream rate-limit response still stops scraping and honors cooldown.
        while True:
            while self.next_requests and self.next_requests[0] + 60 <= now:
                self.next_requests.popleft()
            if len(self.next_requests) < self.minute_limit:
                break
            time.sleep(self.next_requests[0] + 60 - now)
            now = time.monotonic()
        self.next_requests.append(now)
        if self.daily_remaining is not None:
            self.daily_remaining -= 1
        try:
            fetch(url, ["screenscraper.fr"], limit, fp)
        except urllib.error.HTTPError as error:
            if error.code == 404:
                return False
            if error.code in (430, 431):
                self.daily_remaining = 0
                raise MediaError(429, "ScreenScraper daily quota reached") from None
            if error.code == 429:
                retry = error.headers.get("Retry-After", "")
                self.blocked_until = now + max(60, int(retry) if retry.isdecimal() else 60)
                raise MediaError(429, "ScreenScraper is rate limited") from None
            if error.code in (401, 403, 423, 426):
                raise MediaError(503, "Check ScreenScraper credentials and service status") from None
            raise
        return True

    @functools.lru_cache(maxsize=192)
    def lookup(self, title):
        params = dict(self.auth, systemeid="23", recherche=title, softname="K-UI Media", output="xml")
        fp = io.BytesIO()
        if not self.request(SS_URL + "?" + urllib.parse.urlencode(params), 6 * MIB, fp):
            return None
        root = screenscraper_xml(fp.getvalue())
        user = root.find("ssuser")
        if user is not None:
            minute = user.findtext("maxrequestsperdmin") or user.findtext("maxrequestspermin") or ""
            if minute.isdecimal() and int(minute):
                self.minute_limit = int(minute)
            for used, maximum in [("requeststoday", "maxrequestsperday"),
                                  ("requestskotoday", "maxrequestskoperday")]:
                count, quota = user.findtext(used) or "", user.findtext(maximum) or ""
                if count.isdecimal() and quota.isdecimal() and int(quota):
                    left = max(0, int(quota) - int(count))
                    if used == "requeststoday" or left == 0:
                        self.daily_remaining = left
        return exact_ss_game(root, title)

    def download(self, game, kind, dest):
        if not normalized(game.title):
            return False
        url = select_media(self.lookup(game.title), game, kind)
        if not url:
            return False
        with dest.open("wb") as fp:
            return self.request(url, SOURCE_LIMIT[kind], fp)


def convert_cover(source, dest):
    with Image.open(source, formats=["JPEG", "PNG", "BMP", "WEBP"]) as img:
        if img.width * img.height > 24_000_000:
            raise ValueError("Source cover exceeds pixel limit")
        img = ImageOps.exif_transpose(img)
        img.thumbnail((512, 512), Image.Resampling.LANCZOS)
        img = img.convert("RGBA" if "A" in img.getbands() or "transparency" in img.info else "RGB")
        while True:
            img.save(dest, format="PNG")
            if dest.stat().st_size <= OUTPUT["cover"][1]:
                break
            img = img.resize((max(1, img.width * 3 // 4), max(1, img.height * 3 // 4)),
                             Image.Resampling.LANCZOS)


class Catalogue:
    def __init__(self, local=None, screenscraper=None, libretro=None):
        self.local = local
        self.screenscraper = screenscraper
        self.libretro = libretro

    def prepare(self, kind, game, directory):
        from media_formats import convert_manual, convert_preview
        source = self.local.find(game, kind) if self.local else None
        if source is None:
            source = directory / "source"
            obtained = self.screenscraper and self.screenscraper.download(game, kind, source)
            if not obtained and kind == "cover" and self.libretro:
                obtained = self.libretro.download(game, source)
            if not obtained:
                raise MediaError(404, "No matching media")
        if source.stat().st_size > SOURCE_LIMIT[kind]:
            raise MediaError(413, "Source media too large")
        target = directory / "prepared"
        programs = {"cover": (), "manual": ("pdfinfo", "pdftoppm"), "preview": ("ffmpeg",)}
        for program in programs[kind]:
            if shutil.which(program) is None:
                raise MediaError(503, f"Install {program} on the media companion")
        try:
            if kind == "preview":
                # Clients of the v1 endpoint only understand silent KPV1 clips.
                convert_preview(source, target, legacy=True)
            else:
                {"cover": convert_cover, "manual": convert_manual}[kind](source, target)
        except (OSError, ValueError, Image.DecompressionBombError) as error:
            # Decoder errors can include a host path, not useful to the console.
            print(f"Could not convert {kind}: {type(error).__name__}: {error}", file=sys.stderr)
            raise MediaError(422, "Could not convert media; see companion terminal") from None
        if not 0 < target.stat().st_size <= OUTPUT[kind][1]:
            raise MediaError(413, "Converted media too large")
        return target


class MediaHandler(http.server.BaseHTTPRequestHandler):
    timeout = 15
    server_version = "K-UI-Media/1"

    def do_GET(self):
        started = False
        try:
            kind, game = parse_request(self.path)
            with tempfile.TemporaryDirectory(prefix="kui-media-") as tmp:
                result = self.server.catalogue.prepare(kind, game, Path(tmp))
                with result.open("rb") as fp:
                    self.send_response(200)
                    self.send_header("Content-Type", OUTPUT[kind][0])
                    self.send_header("Content-Length", str(os.fstat(fp.fileno()).st_size))
                    self.end_headers()
                    started = True
                    shutil.copyfileobj(fp, self.wfile, length=65536)
        except MediaError as error:
            self.send_error(error.status, str(error))
        except (OSError, ValueError, http.client.HTTPException):
            if not started:
                self.send_error(502, "Media service unavailable")

    def log_message(self, fmt, *args):
        # Older Python versions do not sanitize terminal control sequences.
        print(self.log_date_time_string(), self.client_address[0], ascii(fmt % args), file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--bind", default="127.0.0.1", help="computer's LAN IPv4 address (default localhost)")
    parser.add_argument("--port", type=int, default=8732)
    parser.add_argument("--gamelist", type=Path, help="Dreamcast EmulationStation gamelist.xml")
    parser.add_argument("--media-root", type=Path, help="Dreamcast ROM folder if gamelist.xml is stored elsewhere")
    parser.add_argument("--offline", action="store_true", help="serve only the local gamelist")
    args = parser.parse_args()
    auth = {key: os.environ[var] for key, var in (
        ("devid", "KUI_SS_DEVID"), ("devpassword", "KUI_SS_DEVPASSWORD"),
        ("ssid", "KUI_SS_USER"), ("sspassword", "KUI_SS_PASSWORD")) if os.environ.get(var)}
    provider = None
    if not args.gamelist and (args.offline or args.media_root):
        parser.error("--offline and --media-root require --gamelist")
    if auth and not args.offline:
        if not {"devid", "devpassword"} <= auth.keys() or ("ssid" in auth) != ("sspassword" in auth):
            parser.error("ScreenScraper needs the developer credential pair and optionally both member credentials")
        provider = ScreenScraper(auth)
    try:
        local = LocalLibrary(args.gamelist, args.media_root) if args.gamelist else None
    except (OSError, ValueError, ET.ParseError) as error:
        parser.error(f"Could not open Dreamcast gamelist: {error}")
    with http.server.HTTPServer((args.bind, args.port), MediaHandler) as server:
        server.catalogue = Catalogue(local, provider, None if args.offline else Libretro())
        print(f"K-UI media companion at {args.bind}:{server.server_port}", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
