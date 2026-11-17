#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a media-link index from public catalogue metadata; never download media.

Indexes the release names already present in K-UI. This maintainer tool is
unnecessary on a console.
"""
import argparse
from collections import defaultdict
from html.parser import HTMLParser
import json
from pathlib import Path
import re
from urllib.parse import urlencode
from urllib.request import urlopen

ARCHIVE = "https://archive.org"
MOVIES = "SegaDreamcastVideoSnaps"
DP = "https://www.digitpress.com/library/manuals/dreamcast/index.html"
REGIONS = {"U": "USA", "US": "USA", "J": "Japan", "JP": "Japan", "E": "Europe",
           "GB": "Europe", "UK": "Europe", "E Fr": "France", "Fr": "France"}
# Names with different wording between the publishers' catalogues.
ALIASES = {"segarallychampionship2": "segarally2", "smashpackv1": "segasmashpackvol1",
           "quake3arena": "quakeiiiarena", "lodossrecordoflodosswar": "recordoflodosswar"}
REGION = re.compile(r"\s*\((USA|Europe|Japan|UK|France|Germany|Australia)\)")


def textkey(title):
    title = title.casefold().replace("&", "and")
    title = re.sub(r",\s*the$", "", title)
    title = re.sub(r"^the\s+", "", title)
    key = re.sub(r"[^a-z0-9]", "", title)
    return ALIASES.get(key, key)


def release_key(stem):
    match = REGION.search(stem)
    if not match or re.search(r"\((?:Demo|Beta|Proto|Taikenban|Sample|Shenmue Passport|"
                             r"Dream Passport|Bonus Disc)\b", stem, re.I):
        return None
    region = match[1]
    return textkey(stem[:match.start()]), ("Europe" if region == "UK" else region)


def ia_manuals():
    args = [("q", "collection:segadreamcastmanuals"), ("output", "json"), ("rows", "2000")]
    fields = ["identifier", "title", "imagecount", "access-restricted-item"]
    url = ARCHIVE + "/advancedsearch.php?" + urlencode(args + [("fl[]", f) for f in fields])
    return json.load(urlopen(url, timeout=90))["response"]["docs"]


def select_manuals(docs):
    result = defaultdict(list)
    for doc in docs:
        title = doc["title"].removeprefix("Sega Dreamcast Manual: ")
        item = doc["identifier"]
        pages = int(doc.get("imagecount") or 0)
        tags = re.findall(r"\(([^)]*)\)", title)
        region = next((REGIONS[t] for t in tags if t in REGIONS), None)
        if (not region or not 1 <= pages <= 256 or doc.get("access-restricted-item")
                or not re.fullmatch(r"[A-Za-z0-9_.-]+", item)):
            continue
        # Prefer color scans, then the fewest network requests for the same game.
        title = title.partition("(")[0].strip()
        key = textkey(title)
        result[key, region].append(("color" not in tags, pages, item))
    return {k: min(values)[1:] for k, values in result.items()}


class DpLinks(HTMLParser):
    def __init__(self):
        super().__init__()
        self.link = ""
        self.title = ""
        self.items = {}

    def handle_starttag(self, tag, attrs):
        if tag == "a":
            self.link = dict(attrs).get("href", "")
            self.title = ""

    def handle_endtag(self, tag):
        if tag == "a" and re.fullmatch(r"[a-z0-9_-]+\.pdf", self.link):
            key = textkey(self.title.removesuffix(" (UK)"))
            region = "Europe" if "-uk" in self.link or self.link == "shenmue_2.pdf" else "USA"
            self.items[key, region] = self.link
        if tag == "a":
            self.link = ""

    def handle_data(self, data):
        if self.link:
            self.title += data


def hash_name(s):
    h = 2166136261
    for octet in s.encode():
        h = ((h ^ octet) * 16777619) & 0xffffffff
    return h


def generate(covers, manuals, movie_metadata, dp_html, output):
    manuals = select_manuals(manuals)
    movie_names = sorted({f["name"][:-4] for f in movie_metadata["files"]
                          if f["name"].endswith(".ogv") and "/" not in f["name"]
                          and not f.get("private") and 0 < int(f["size"]) <= 32 * 1024**2})
    by_key = defaultdict(list)
    for name in movie_names:
        by_key[release_key(name)].append(name)
    pdfs = DpLinks()
    pdfs.feed(dp_html)
    records = {}
    seen = {}
    for stem in sorted(set(covers)):
        h = hash_name(stem)
        if h in seen and seen[h] != stem:
            raise ValueError("Ambiguous title fingerprint: " + stem)
        seen[h] = stem
        key = release_key(stem)
        if key is None:
            continue
        # A sole same-game/region movie can also serve discs or revisions.
        movies = by_key.get(key, [])
        movie = stem if stem in movies else ""
        if not movie:
            first = [name for name in movies if "(Disc 1)" in name]
            if len(movies) == 1:
                movie = movies[0]
            elif len(first) == 1 and all(re.search(r"\(Disc [0-9]+\)", name) for name in movies):
                movie = first[0]
        n, item = manuals.get(key, (0, ""))
        pdf = pdfs.items.get(key, "")
        if item or pdf or movie:
            records[h] = (n, item, pdf, movie)
    if not records:
        raise ValueError("Empty catalogue")
    with output.open("w", encoding="utf-8", newline="\n") as f:
        f.write("/* Generated by tools/index_internet_extras.py from public media metadata. */\n")
        for h, (pages, item, pdf, movie) in sorted(records.items()):
            # Encode UTF-8 as octal escapes so the C bytes do not depend on locale.
            def literal(s):
                if not s:
                    return "NULL"
                return '"' + ''.join(chr(c) if 32 <= c < 127 and c not in (34, 92)
                                     else '\\%03o' % c for c in s.encode()) + '"'
            f.write(f'{{0x{h:08x}u,{pages},{literal(item)},{literal(pdf)},{literal(movie)}}},\n')
    print(len(records), "release filenames;", len({r[1] for r in records.values()} - {""}),
          "page sets;", len({r[3] for r in records.values()} - {""}), "movies")


def cover_names(path):
    # The checked-in cover table contains escaped strings for C. In this
    # ASCII catalogue the quoted string escapes are also valid JSON.
    pattern = r'^\s*\{("(?:[^"\\]|\\.)*"), \d+, \d+, \d+\}'
    return [json.loads(s) for s in re.findall(pattern, path.read_text(), re.MULTILINE)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--covers", type=Path, default=Path("src/apps/internet_cover_catalog.inc"))
    p.add_argument("--output", type=Path, default=Path("src/apps/internet_extra_data.inc"))
    args = p.parse_args()
    generate(cover_names(args.covers), ia_manuals(), json.load(urlopen(ARCHIVE + "/metadata/" + MOVIES, timeout=90)),
             urlopen(DP, timeout=60).read().decode("latin-1"), args.output)


if __name__ == "__main__":
    main()
