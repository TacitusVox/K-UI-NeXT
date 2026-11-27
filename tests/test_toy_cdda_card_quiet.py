#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Test 85 (CARD) asked for nothing is Test 84: the integration test's whole
course ahead of the audio's own scenarios, built with the audio's card work
and without it, sends the card the same and answers every visit the same.

usage: test_toy_cdda_card_quiet.py <log of the test without CARD> <log of the test with CARD>

The test folds into one number every programmed byte's place on the card, the
count of card commands, every admitted visit with its event, argument and
answer, and after each of the game's calls the service's answer fields, the
low stream's state and place, and the runs begun (tests/test_toy_wire_integration.c,
traffic). Nothing in that course makes the worker's heartbeat for the audio:
the auxiliary's part is then one test of a byte in a delivery visit, and
nothing in any other. The two builds must print the same number
(Makefile.vmu-native-toy-suite, toy-cdda-card-quiet-check).
"""
import re
import sys
from pathlib import Path

LINE = re.compile(r"without the audio's heartbeat: traffic ([0-9a-f]{8})")


def digest(path):
    text = Path(path).read_text(errors="replace")
    found = LINE.findall(text)
    if len(found) != 1:
        sys.exit("FAIL %s has %d traffic lines" % (path, len(found)))
    if not re.search(r"^PASS Test 8[45] reader integration: \d+ checks$", text, re.M):
        sys.exit("FAIL %s is not a passed integration test" % path)
    return found[0], text


def main(without, with_card):
    a, _ = digest(without)
    b, text = digest(with_card)
    if "Test 85, the audio from the card:" not in text:
        sys.exit("FAIL %s is not a build with the audio's card work" % with_card)
    if a != b:
        sys.exit("FAIL the card's traffic differs with the audio's card work built in and never asked: %s against %s" % (b, a))
    print("PASS Test 85 asked for nothing: the integration test ahead of the audio's scenarios folds to %s with the "
          "audio's card work built in and without it" % a)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
