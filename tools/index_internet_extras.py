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
# Archive lists these two US gameplay clips under their box subtitles.
MOVIE_RELEASES = {
    "Elemental Gimmick Gear (USA)": "EGG - Elemental Gimmick Gear (USA)",
    "Railroad Tycoon II (USA)": "Railroad Tycoon II - Gold Edition (USA)",
}
# These scans use alternative titles or distinct limited-edition product numbers.
# Each record pairs the exact release with the search index imagecount and its
# verified available leaves from the item's *_scandata.xml. Some items repeat
# the cover for an out-of-range page. A changed publication uses its new count.
MANUAL_RELEASES = {
    "4_x_4_Evolution_2000_U_color":
        ("4x4 Evo (USA)", 15, 14),
    "Akihabara_Dennou_Kumi_Peta_Pies_1999_J_color":
        ("Akihabara Dennou-gumi Pata Pies! (Japan)", 29, 28),
    "Dead_Or_Alive_2_Limited_Edition_2000_J_color_w_pages_1819":
        ("Dead or Alive 2 (Japan) (Shokai Genteiban)", 71, 70),
    "Death_Crimson_2_Meranito_No_Saidan_19999_J_color_w_back_cover":
        ("Death Crimson 2 (Japan)", 17, 16),
    "Disney-Pixars_Toy_Story_2_Buzz_Lightyear_To_The_Rescue_2000_U":
        ("Disney-Pixar Toy Story 2 - Buzz Lightyear to the Rescue! (USA)", 15, 14),
    "Eldorado_Gate_Vol._3_2001_J_color":
        ("Eldorado Gate Dai-3-kan (Japan)", 44, 44),
    "Elemental_Gimmick_Gear_-E.G.G.-_1999_J_color":
        ("Elemental Gimmick Gear (Japan)", 29, 28),
    "Get_Bass_Sega_Bass_Fishing_1999_J_color":
        ("Get Bass (Japan)", 21, 20),
    "King_Of_Fighters_Evolution_The_2000_J_color":
        ("King of Fighters '99, The - Evolution (Japan)", 33, 32),
    "King_Of_Fighters_Dream_Match_1999_The_1999_J_color":
        ("King of Fighters, The - Dream Match 1999 (Japan) (En,Ja,Es,Pt)", 33, 32),
    "King_Of_Fighters_Dream_Match_99_The_1999_U_color":
        ("King of Fighters, The - Dream Match 1999 (USA) (En,Ja,Es,Pt)", 17, 16),
    "King_Of_Fighters_Evolution_The_2000_U_color":
        ("King of Fighters, The - Evolution (USA) (En,Ja,Es,Pt)", 17, 16),
    "Last_Blade_2_Heart_Of_The_Samurai_The_2001_U":
        ("Last Blade 2, The - Heart of the Samurai (USA)", 17, 16),
    "Metropolis_Street_Racer_2000_U":
        ("MSR - Metropolis Street Racer (USA) (Rev A)", 16, 16),
    "MTV_Sports_Skateboarding_Ft._Andy_MacDonald_2000_U":
        ("MTV Sports - Skateboarding featuring Andy Macdonald (USA)", 16, 15),
    "Mars_Matrix_Hyper_Solid_Shooting_2000_J_color":
        ("Mars Matrix (Japan) (En,Ja)", 17, 16),
    "Matt_Hoffmans_Pro_BMX_2001_U_w_pages_67":
        ("Mat Hoffman's Pro BMX (USA)", 17, 16),
    "Midways_Greatest_Arcade_Hits_Vol1_2000_U_color":
        ("Midway's Greatest Arcade Hits - Volume 1 (USA)", 13, 12),
    "Midways_Greatest_Arcade_Hits_Vol2_2000_U":
        ("Midway's Greatest Arcade Hits - Volume 2 (USA)", 14, 13),
    "NCAA_College_Football_2K2_2001_U_color":
        ("NCAA College Football 2K2 - Road to the Rose Bowl (USA)", 19, 18),
    "Next_Tetris_On-Line_Edition_The_2000_U":
        ("Next Tetris, The - On-Line Edition (USA)", 11, 10),
    "PenPen_Trilcelon_1998_J_color":
        ("PenPen TriIcelon (Japan) (Rev A)", 33, 32),
    "Pen_Pen_Trilcelon_1999_U_color":
        ("PenPen TriIcelon (USA)", 17, 16),
    "Puzzle_Bubble_4_2000_J_color":
        ("Puzzle Bobble 4 (Japan)", 33, 32),
    "Railroad_Tycoon_II_Gold_Edition_2000_U":
        ("Railroad Tycoon II (USA)", 17, 16),
    "Ring_Terrors_Realm_The_2000_U":
        ("Ring, The - Terror's Realm (USA)", 15, 14),
    "SGGG_2001_J_color":
        ("SGGG - Segagaga (Japan)", 45, 44),
    "Sega_GT_Homologation_Special_Manual_Book_2000_J_color":
        ("Sega GT - Homologation Special (Japan)", 81, 80),
    "Sega_Rally_2_Championship_1999_U":
        ("Sega Rally 2 (USA)", 15, 14),
    "Shenmue_Passport_2000_U":
        ("Shenmue (USA) (Shenmue Passport)", 11, 10),
    "Shutoko_Battle_1999_J_color":
        ("Shutokou Battle (Japan)", 17, 16),
    "Shutoko_Battle_2_2000_J_color":
        ("Shutokou Battle 2 (Japan)", 20, 20),
    "Street_Fighter_III_3rd_Strike_2000_J_color":
        ("Street Fighter III - 3rd Strike - Fight for the Future (Japan)", 53, 52),
}
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


# These Archive listing counts include a non-page image. Each leaf count
# was verified against the item's ordered, accessible scandata.xml leaves.
# Apply only to the observed listing version if a provider updates its book.
MANIFEST_COUNTS = {
    "4_x_4_Evolution_2000_U_color": (15, 14),
    "Akihabara_Dennou_Kumi_Peta_Pies_1999_J_color": (29, 28),
    "Dead_Or_Alive_2_Limited_Edition_2000_J_color_w_pages_1819": (71, 70),
    "Death_Crimson_2_Meranito_No_Saidan_19999_J_color_w_back_cover": (17, 16),
    "Disney-Pixars_Toy_Story_2_Buzz_Lightyear_To_The_Rescue_2000_U": (15, 14),
    "Elemental_Gimmick_Gear_-E.G.G.-_1999_J_color": (29, 28),
    "Get_Bass_Sega_Bass_Fishing_1999_J_color": (21, 20),
    "King_Of_Fighters_Dream_Match_1999_The_1999_J_color": (33, 32),
    "King_Of_Fighters_Dream_Match_99_The_1999_U_color": (17, 16),
    "King_Of_Fighters_Evolution_The_2000_J_color": (33, 32),
    "King_Of_Fighters_Evolution_The_2000_U_color": (17, 16),
    "Last_Blade_2_Heart_Of_The_Samurai_The_2001_U": (17, 16),
    "MTV_Sports_Skateboarding_Ft._Andy_MacDonald_2000_U": (16, 15),
    "Mars_Matrix_Hyper_Solid_Shooting_2000_J_color": (17, 16),
    "Matt_Hoffmans_Pro_BMX_2001_U_w_pages_67": (17, 16),
    "Midways_Greatest_Arcade_Hits_Vol1_2000_U_color": (13, 12),
    "Midways_Greatest_Arcade_Hits_Vol2_2000_U": (14, 13),
    "NCAA_College_Football_2K2_2001_U_color": (19, 18),
    "Next_Tetris_On-Line_Edition_The_2000_U": (11, 10),
    "PenPen_Trilcelon_1998_J_color": (33, 32),
    "Pen_Pen_Trilcelon_1999_U_color": (17, 16),
    "Puzzle_Bubble_4_2000_J_color": (33, 32),
    "Ring_Terrors_Realm_The_2000_U": (15, 14),
    "SGGG_2001_J_color": (45, 44),
    "Sega_GT_Homologation_Special_Manual_Book_2000_J_color": (81, 80),
    "Sega_Rally_2_Championship_1999_U": (15, 14),
    "Shutoko_Battle_1999_J_color": (17, 16),
    "Street_Fighter_III_3rd_Strike_2000_J_color": (53, 52),
}


def select_manuals(docs):
    result = defaultdict(list)
    by_release = {}
    for doc in docs:
        title = doc["title"].removeprefix("Sega Dreamcast Manual: ")
        item = doc["identifier"]
        pages = int(doc.get("imagecount") or 0)
        verified = MANIFEST_COUNTS.get(item)
        if verified and pages == verified[0]:
            pages = verified[1]
        tags = re.findall(r"\(([^)]*)\)", title)
        region = next((REGIONS[t] for t in tags if t in REGIONS), None)
        if (not region or not 1 <= pages <= 256 or doc.get("access-restricted-item")
                or not re.fullmatch(r"[A-Za-z0-9_.-]+", item)):
            continue
        # Prefer color scans, then the fewest network requests for the same game.
        if item in MANUAL_RELEASES:
            release, advertised, leaves = MANUAL_RELEASES[item]
            market = REGION.search(release)
            if market and market[1] == region:
                by_release[release] = (leaves if pages == advertised else pages, item)
        title = title.partition("(")[0].strip()
        key = textkey(title)
        result[key, region].append(("color" not in tags, pages, item))
    return {k: min(values)[1:] for k, values in result.items()}, by_release


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


# These are media markets, independent of the release used to select a manual.
PREVIEW_MARKETS = {"USA": 2, "Japan": 1, "Europe": 4, "UK": 4, "France": 4,
                   "Germany": 4, "Italy": 4, "Spain": 4, "Portugal": 4, "Australia": 4}


def preview_family(stem):
    # Only ignore language, retail revision and disc-number tags. Unknown
    # editions, demos, bonus discs and compilations keep their own identities.
    match = re.fullmatch(r"([^()]+?)\s*\(([^()]+)\)((?:\s*\([^()]+\))*)", stem)
    if not match or match[2] not in PREVIEW_MARKETS:
        return None
    languages = {"En", "Ja", "Fr", "De", "Es", "It", "Pt", "Nl", "Sv", "No", "Da"}
    for tag in re.findall(r"\(([^()]+)\)", match[3]):
        if not (set(tag.split(",")) <= languages or re.fullmatch(r"Disc [1-9][0-9]*|Rev [A-Z0-9]+", tag)):
            return None
    return textkey(match[1]), PREVIEW_MARKETS[match[2]]


# Original Dreamcast gameplay independently verified in K-UI's Theora decoder.
# These sources do not identify their release region. Try them last.
SHARED_GAMEPLAY = {"segarally2": "/011564/011564.ogv"}


def region_previews(covers, movie_names):
    # Archive's two qualified US release names use box subtitles.
    aliases = {name:stem for stem, name in MOVIE_RELEASES.items()}
    donors = defaultdict(set)
    for movie in movie_names:
        # A later numbered disc can be a bonus program rather than the game.
        disc = re.search(r"\(Disc ([0-9]+)\)", movie)
        if disc and disc[1] != "1":
            continue
        family = preview_family(aliases.get(movie, movie))
        if family:
            donors[family].add(movie)
    records = []
    seen = set()
    for stem in sorted(set(covers)):
        release = hash_name(stem)
        if release in seen:
            raise ValueError("Ambiguous title fingerprint: " + stem)
        seen.add(release)
        family = preview_family(stem)
        if not family:
            continue
        key, market = family
        sources = []
        for wanted in (2, 4):
            options = donors[key, wanted] if market != wanted else set()
            # Ambiguous captures in one market are skipped.
            sources.append(next(iter(options)) if len(options) == 1 else "")
        sources.append(SHARED_GAMEPLAY.get(key, ""))
        if any(sources):
            records.append((release, *sources))
    return sorted(records)


def write_region_previews(covers, movie_names, output):
    with output.open("w", encoding="utf-8", newline="\n") as f:
        f.write("/* Generated by tools/index_internet_extras.py from public media metadata. */\n")
        for release, usa, pal, shared in region_previews(covers, movie_names):
            f.write(f"{{0x{release:08x}u, {{{literal(usa)}, {literal(pal)}, {literal(shared)}}}}},\n")


# Encode UTF-8 as octal escapes so the C bytes do not depend on locale.
def literal(s):
    if not s:
        return "NULL"
    return '"' + ''.join(chr(c) if 32 <= c < 127 and c not in (34, 92)
                         else '\\%03o' % c for c in s.encode()) + '"'


def generate(covers, manuals, movie_metadata, dp_html, output, regions=None):
    manuals, by_release = select_manuals(manuals)
    movie_names = sorted({f["name"][:-4] for f in movie_metadata["files"]
                          if f["name"].endswith(".ogv") and "/" not in f["name"]
                          and not f.get("private") and 0 < int(f["size"]) <= 32 * 1024**2})
    if regions is not None:
        write_region_previews(covers, movie_names, regions)
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
            # A bonus disc can have a separately published booklet. Never
            # borrow a retail manual, PDF or movie for an excluded edition.
            if stem in by_release:
                pages, item = by_release[stem]
                records[h] = (pages, item, "", "")
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
        if not movie and MOVIE_RELEASES.get(stem) in movie_names:
            movie = MOVIE_RELEASES[stem]
        n, item = by_release.get(stem, manuals.get(key, (0, "")))
        pdf = pdfs.items.get(key, "")
        if item or pdf or movie:
            records[h] = (n, item, pdf, movie)
    if not records:
        raise ValueError("Empty catalogue")
    with output.open("w", encoding="utf-8", newline="\n") as f:
        f.write("/* Generated by tools/index_internet_extras.py from public media metadata. */\n")
        for h, (pages, item, pdf, movie) in sorted(records.items()):
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
    p.add_argument("--regions", type=Path, default=Path("src/apps/internet_preview_regions.inc"))
    args = p.parse_args()
    generate(cover_names(args.covers), ia_manuals(), json.load(urlopen(ARCHIVE + "/metadata/" + MOVIES, timeout=90)),
             urlopen(DP, timeout=60).read().decode("latin-1"), args.output, args.regions)


if __name__ == "__main__":
    main()
