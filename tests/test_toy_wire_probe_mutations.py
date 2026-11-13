#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check that the wire probe's host model notices deliberate defects.

Each mutation changes one behaviour of src/loader/toy_wire_probe.c: the
resume offset, the byte accounting, a register that must be given back, a
refusal, the wait policy or the block check. The mutant is built against
tests/test_toy_wire_probe.c and every scenario is run; a mutation that no
scenario fails is a gap in the model's assertions. This checks the test,
not the Dreamcast.
"""
import os
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
SCENARIOS = 16
MUTATIONS = [
 ("resume one byte early","    *next = lost + 1u;\n","    *next = lost;\n"),
 ("lost byte not counted","    if(held) wire_buffer[at] = tail;\n    w->now.bytes += run + held + 1u;","    if(held) wire_buffer[at] = tail;\n    w->now.bytes += run + held;"),
 ("held byte not stored","    if(held) wire_buffer[at] = tail;\n","\n"),
 ("priority not restored","        wr16(IPRB, (levels & ~SCI_FIELD) | (w->saved_iprb & SCI_FIELD));","        (void)levels;"),
 ("count ignores offset","    wr32(TCR1, SEGMENT - offset);","    wr32(TCR1, SEGMENT);"),
 ("no module reset","        wr8(STBCR, before | 1u);","        (void)before;"),
 ("channel not restored","        for(unsigned i = 0; i < 4u; ++i) wr32(SAR1 + 4u * i, w->saved_dma[i]);\n    wire_unhook(w);","    wire_unhook(w);"),
 ("slots not restored","        if(word_read(SDK_ERI_SLOT + 4u * i) == CALLBACK) word_publish(SDK_ERI_SLOT + 4u * i, SDK_DEFAULT);","        (void)i;"),
 ("no CMD12","        if(!wire_stop_card(w)) wire_refuse(w, KUI_WIRE_E_CLOSE, 1u);","        (void)0;"),
 ("timer left running","    wr8(TSTR, rd8(TSTR) & ~w->timer_bit);\n    wr16(w->timer_reg + 8u, 0x0002u); /* no interrupt, underflow cleared */","    wr16(w->timer_reg + 8u, 0x0002u);"),
 ("always resume at once","    if(!cut || run >= KUI_WIRE_GOOD_RUN) {\n        /* Only","    if(true) {\n        /* Only"),
 ("backoff never grows","    if(w->backoff < KUI_WIRE_BACKOFF_MAX) w->backoff <<= 1;","    (void)0;"),
 ("holes ignored in tail","        const bool hole = holed(w, p, p + 514u);","        const bool hole = false;"),
 ("vector not checked","    if(vector_base() != SDK_VBR || word_read(SDK_ERI_SLOT) != SDK_DEFAULT ||\n       word_read(SDK_ERI_SLOT + 4u) != SDK_DEFAULT) {","    if(false) {"),
 ("channel not checked","    } else if((rd32(CHCR1) & 7u) || (rd32(DMAOR) & 7u) != 1u) {","    } else if(false) {"),
 ("foreign channel stopped","        ours = rd32(DAR1) == start + SEGMENT - count;\n        if(ours) wr32(CHCR1, 0);","        ours = rd32(DAR1) == start + SEGMENT - count;\n        wr32(CHCR1, 0);"),
 ("segment end fault on late last byte","        w->now.bytes += run + held + 1u;\n        w->tail_filled = SEGMENT;","        if(!held) {wire_fault(w, KUI_WIRE_E_END, status); return false;}\n        w->now.bytes += run + held + 1u;\n        w->tail_filled = SEGMENT;"),
 ("window ignores run in flight","        if(stored >= w->run_off && stored <= SEGMENT) c.bytes += stored - w->run_off;","        (void)stored;"),
 ("sample not scaled","        win->top_count[rank] = best_count * (win->pc_mask + 1u);","        win->top_count[rank] = best_count;"),
 ("first run has no start time","            w->run_tick = rd32(TCNT0);\n            w->s.state = KUI_WIRE_RUN;","            w->s.state = KUI_WIRE_RUN;"),
 ("late time not counted","    if(elapsed > wired) w->now.late_ticks += elapsed - wired;","    (void)elapsed; (void)wired;"),
 ("disc cuts not counted","        if(w->gd_busy && w->sample) ++w->sample->gd_cuts;","        (void)0;"),
 ("disc interval never closed","        w->sample->gd_bytes += bytes - w->gd_bytes;","        (void)bytes;"),
 ("disc read before window counted","    w->gd_tick = win->begin_tick; w->gd_bytes = win->begin.bytes;","    (void)0;"),
 ("poll resumes beside the timer","    if(w->s.state == KUI_WIRE_PAUSED && !w->s.mode) wire_resume(w, rd32(TCNT0));","    if(w->s.state == KUI_WIRE_PAUSED) wire_resume(w, rd32(TCNT0));"),
]


def main():
    source = (ROOT / "src/loader/toy_wire_probe.c").read_text()
    cc = os.environ.get("CC", "cc")
    survived = 0
    with TemporaryDirectory(prefix="kui-wire-mutants-") as tmp:
        mutant, binary = Path(tmp) / "mutant.c", Path(tmp) / "mutant"
        for name, old, new in MUTATIONS:
            assert source.count(old) == 1, name
            mutant.write_text(source.replace(old, new))
            subprocess.run([cc, "-std=c11", "-O1", "-g", "-w", "-fsanitize=address,undefined",
                            "-DKUI_WIRE_TEST=1", "-DKUI_VMU_NATIVE_PERSISTENT_PROBE=1",
                            "-I", str(ROOT / "include"), "-I", str(ROOT / "src/loader"),
                            str(ROOT / "tests/test_toy_wire_probe.c"), str(mutant), "-o", str(binary)],
                           check=True)
            failed = [i for i in range(SCENARIOS)
                      if subprocess.run([str(binary), str(i)], capture_output=True,
                                        env={"ASAN_OPTIONS": "detect_leaks=0"}).returncode]
            print(("caught   " if failed else "SURVIVED ") + name, failed)
            survived += not failed
    print(f"{len(MUTATIONS) - survived} of {len(MUTATIONS)} mutations caught")
    return 1 if survived else 0


if __name__ == "__main__":
    sys.exit(main())
