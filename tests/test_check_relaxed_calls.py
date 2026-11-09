#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The relaxed-call gate refuses a branch four bytes into another function,
accepts branches to function starts and within a function, and never reads
a literal word that merely decodes as bsr. Needs sh-elf-as/ld."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / 'tools/check_relaxed_calls.py'

SOURCE = """
    .text
    .align 1
    .global _caller
    .type _caller, @function
_caller:
    sts.l pr,@-r15
    bsr {target}
    nop
    bra 1f
    nop
1:  lds.l @r15+,pr
    rts
    nop
    .align 2
    .long 0xb0fe0000   /* data that decodes as bsr: never a branch site */
    .size _caller, .-_caller
    .align 1
    .global _callee
    .type _callee, @function
_callee:
    mov.l r8,@-r15
    mov.l r9,@-r15
.Linside:
    mov.l @r15+,r9
    rts
    mov.l @r15+,r8
    .size _callee, .-_callee
"""


def gate(work, target):
    asm = work / (target.strip('._') + '.s')
    obj, elf = asm.with_suffix('.o'), asm.with_suffix('.elf')
    asm.write_text(SOURCE.format(target=target))
    subprocess.run(['sh-elf-as', '-little', '-relax', str(asm), '-o', str(obj)], check=True)
    subprocess.run(['sh-elf-ld', '-q', '-EL', '-Ttext=0x8c010000', '-e', '_caller',
                    str(obj), '-o', str(elf)], check=True)
    return subprocess.run([sys.executable, str(TOOL), str(elf)],
                          capture_output=True, text=True)


def main():
    if not shutil.which('sh-elf-as'):
        print('relaxed-call gate: sh-elf binutils not on PATH', file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        good = gate(work, '_callee')
        assert good.returncode == 0, good.stdout + good.stderr
        assert '"entering_mid_function": []' in good.stdout, good.stdout
        bad = gate(work, '.Linside')
        assert bad.returncode == 1, bad.stdout + bad.stderr
        assert '_callee+4' in bad.stdout and '"caller": "_caller"' in bad.stdout, bad.stdout
        none = work / 'empty.elf'
        shutil.copy(work / 'callee.elf', none)
        subprocess.run(['sh-elf-strip', '--remove-section=.rela.text', str(none)], check=True)
        refused = subprocess.run([sys.executable, str(TOOL), str(none)],
                                 capture_output=True, text=True)
        assert refused.returncode == 1 and 'link with -Wl,-q' in refused.stderr, refused.stderr
    print('relaxed-call gate: ok')
    return 0


if __name__ == '__main__':
    sys.exit(main())
