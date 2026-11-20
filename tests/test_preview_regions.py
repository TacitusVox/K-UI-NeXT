#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check cross-region preview identity using a synthetic source catalogue."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from index_internet_extras import hash_name, region_previews


class RegionalPreviews(unittest.TestCase):
    def test_priority_and_identity(self):
        jp = "Example 2 (Japan) (En,Ja) (Disc 2)"
        us = "Example 2 (USA)"
        eu = "Example 2 (Europe)"
        excluded = ["Example 2 (Japan) (" + tag + ")" for tag in
                    ("Demo", "Bonus Disc", "Shokai Genteiban", "Shenmue Passport")]
        sources = [us, eu, "Example 2 (Japan) (Disc 1)", "Example 2 (Japan) (Disc 2)",
                   "Example 3 (USA) (Demo)", "Example 3 (Europe) (Disc 4)"]
        result = {h: (usa, pal) for h, usa, pal, shared in
                  region_previews([jp, us, eu, "Example 3 (Japan)", *excluded], sources)}
        self.assertEqual(result, {hash_name(jp): (us, eu),
                                  hash_name(us): ("", eu),
                                  hash_name(eu): (us, "")})

    def test_alternate_market_ambiguity(self):
        rows = region_previews(["Example (Japan)"], ["Example (USA)", "Example (USA) (Rev A)",
                               "Example (Europe) (Disc 1)", "Example (Europe) (Disc 2)"])
        self.assertEqual(rows, [(hash_name("Example (Japan)"), "", "Example (Europe) (Disc 1)", "")])

    def test_shared_source_is_specific_to_game(self):
        rows = region_previews(["Sega Rally 2 (Japan)", "Sega Rally 2 (Japan) (Demo)",
                                "Sega Rally 3 (Japan)"], [])
        self.assertEqual(rows, [(hash_name("Sega Rally 2 (Japan)"), "", "", "/011564/011564.ogv")])


if __name__ == "__main__":
    unittest.main()
