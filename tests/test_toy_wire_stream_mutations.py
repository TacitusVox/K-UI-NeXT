#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check that the Test 68 host tests notice deliberate defects.

Each mutation changes one behaviour of the card stream, its ring reader, or
the glue that feeds the verified movie window: a position, a lost-byte
rule, a register that must be given back, the CRC, the flow control, the
re-read of a failed sector. Stream mutants are built against
tests/test_toy_wire_stream.c; glue mutants against the reader integration
test (tests/test_toy_wire_integration.c) through its make target. A
mutation that no test fails is a gap in the tests. This checks the tests,
not the Dreamcast.
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
STREAM = "src/loader/toy_wire_stream.c"
GLUE = "src/loader/toy_wire_ahead.inc"
AHEAD = "src/loader/toy_movie_ahead.inc"
MUTATIONS = [
 (STREAM, "resume one byte early", "    w->head = lost + 1u;\n", "    w->head = lost;\n"),
 (STREAM, "resume one byte late", "    w->head = lost + 1u;\n", "    w->head = lost + 2u;\n"),
 (STREAM, "held byte not stored", "    if(held) w->ring[at & MASK] = tail;\n", "    (void)tail;\n"),
 (STREAM, "held byte taken for the lost one", "    const uint32_t lost = at + held;\n", "    const uint32_t lost = at;\n"),
 (STREAM, "lost position not logged", "    w->hole[w->hole_put++ & HOLE_MASK] = lost;\n", "    (void)lost;\n"),
 (STREAM, "no module reset", "        wr8(STBCR, before | 1u);", "        (void)before;"),
 (STREAM, "run started without room for it", "    if(room < RUN_ROOM || w->hole_put", "    if(room < 2u || w->hole_put"),
 (STREAM, "short run for a waiting caller overshoots the ring", "        if(most < length) length = most; /* never beyond the ring's end */", "        length = most;"),
 (STREAM, "ring index ignores the wrap", "    wr32(DAR1, w->ring_physical + index);", "    wr32(DAR1, w->ring_physical + w->head);"),
 (STREAM, "channel not restored at stop", "    if(usable && state != KUI_WIRE_FAULT)\n        for(unsigned i = 0; i < 4u; ++i) wr32(SAR1 + 4u * i, w->saved_dma[i]);", "    (void)usable;"),
 (STREAM, "stop does not wait for a whole byte", "        for(uint32_t n = 0; n < 200000u && !(rd8(SSR) & ORER); ++n) {}\n        uint32_t count, status;", "        uint32_t count, status;"),
 (STREAM, "wrong CRC polynomial", "c >> 1 ^ 0x8408u", "c >> 1 ^ 0x8404u"),
 (STREAM, "clean block not checked", "        else if(!lost_data && !lost_crc) accept = !crc;", "        else if(!lost_data && !lost_crc) accept = true;"),
 (STREAM, "rebuild counts one byte too few behind", "wire_solve(w, crc, FRAME - 1u - w->lost[0])", "wire_solve(w, crc, FRAME - 2u - w->lost[0])"),
 (STREAM, "rebuilt byte not reversed", "                w->block[w->lost[0]] = w->reverse[value];", "                w->block[w->lost[0]] = (uint8_t)value;"),
 (STREAM, "lost data byte not listed", "                w->lost[(*lost_data)++] = (uint16_t)(at - first);\n", "                ++*lost_data;\n"),
 (STREAM, "lost index off by one", "                w->lost[(*lost_data)++] = (uint16_t)(at - first);", "                w->lost[(*lost_data)++] = (uint16_t)(at - first + 1u);"),
 (STREAM, "lost gap byte never the token", "                w->data_at = w->gap_hole;\n                w->alternative = 0;", "                return wire_refuse(w, KUI_WIRE_LOST_FRAME);"),
 (STREAM, "second reading of the gap dropped", "                w->alternative = w->gap_hole;", "                w->alternative = 0;"),
 (STREAM, "reader runs past what has landed", "        if(landed < need) return KUI_WIRE_NEED;", "        if(landed + 64u < need) return KUI_WIRE_NEED;"),
 (STREAM, "data token taken as received", "#define TOKEN_WIRE 0x7fu", "#define TOKEN_WIRE 0xfeu"),
 (STREAM, "block one byte short", "#define FRAME 514u", "#define FRAME 513u"),
 (GLUE, "lost bytes not mapped into the sector", "        if(k->lost_count<=KUI_MODE1_ERASURE_MAX)k->lost[k->lost_count]=(uint16_t)(part+at-from);", "        if(k->lost_count<=KUI_MODE1_ERASURE_MAX)k->lost[k->lost_count]=(uint16_t)(at-from);"),
 (GLUE, "failed sector not read again", "            toy_wire_rewind(r,KUI_TOY_WIRE_STOP_SECTOR);\n            return;", "            k->bad=0u;++A.verified;\n            return;"),
 (GLUE, "extent jump ignored", "        if(A.cursor.block!=next) {\n            toy_wire_halt(r,KUI_TOY_WIRE_STOP_JUMP);\n            return;\n        }", ""),
 (GLUE, "stream state not handed back", "    stream->state=RA_LOST;\n    stream->ready[0]", "    stream->state=RA_PAUSED;\n    stream->ready[0]"),
 (GLUE, "resident not told the stream receives", "    stream->state=RA_DMA;\n    k->on=1u;", "    k->on=1u;"),
 (GLUE, "rewind keeps the old cursor", "       kui_retail_cursor_begin(&A.cursor,A.cursor.manifest,A.verified,end-A.verified,\n           KUI_GAME_SECTOR_RAW,toy_ahead_write,NULL)!=KUI_GAME_OK)A.error=true;", "       false)A.error=true;"),
 (AHEAD, "sector served without its check", "    if(toy_wire_sector(k,raw,lba))++A.verified;\n    else k->bad=1u;", "    (void)toy_wire_sector;++A.verified;"),
 (AHEAD, "stream left running when the window closes", "    toy_wire_halt(r,KUI_TOY_WIRE_STOP_CLOSE);\n#endif\n    if(A.receiving) {", "#endif\n    if(A.receiving) {"),
]


def run(cmd, cwd, timeout):
    try:
        return subprocess.run(cmd, cwd=cwd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                              timeout=timeout).returncode
    except subprocess.TimeoutExpired:
        return 124  # a mutant that hangs is caught too


def main():
    cc = os.environ.get("CC", "cc")
    survived = []
    with TemporaryDirectory(prefix="kui-wire68-mutants-") as tmp:
        tree = Path(tmp) / "tree"
        # A private copy of what the tests build from; the working tree is never touched.
        for part in ("src", "include", "tests", "fatfs"):
            shutil.copytree(ROOT / part, tree / part, symlinks=True)
        for makefile in ROOT.glob("Makefile*"):
            shutil.copy(makefile, tree / makefile.name)
        for path, name, old, new in MUTATIONS:
            original = (ROOT / path).read_text()
            if original.count(old) != 1:
                print(f"STALE   {name}: its source text occurs {original.count(old)} times", flush=True)
                survived.append(name)
                continue
            (tree / path).write_text(original.replace(old, new))
            if path == STREAM:
                binary = Path(tmp) / "stream"
                code = run([cc, "-std=c11", "-O1", "-g", "-w", "-fsanitize=address,undefined",
                            "-DKUI_WIRE_STREAM_TEST=1", "-Iinclude", "-Isrc/loader",
                            "tests/test_toy_wire_stream.c", path, "-o", str(binary)], tree, 120)
                if code == 0:
                    code = run([str(binary), "8"], tree, 300)
                if code == 0:  # the stream mutant may still be caught end to end
                    code = run(["make", "-f", "Makefile.vmu-native-toy-suite", "TOY_SUITE_MODE=66",
                                "TOY_WIRE_STREAM=1", f"BUILD={tmp}/build", "toy-wire-integration-host-test"], tree, 600)
            else:
                code = run(["make", "-f", "Makefile.vmu-native-toy-suite", "TOY_SUITE_MODE=66",
                            "TOY_WIRE_STREAM=1", f"BUILD={tmp}/build", "toy-wire-integration-host-test"], tree, 600)
            (tree / path).write_text(original)
            print(("caught  " if code else "MISSED  ") + name, flush=True)
            if not code:
                survived.append(name)
    print(f"{len(MUTATIONS) - len(survived)} of {len(MUTATIONS)} mutations caught")
    return 1 if survived else 0


if __name__ == "__main__":
    sys.exit(main())
