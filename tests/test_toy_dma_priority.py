#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the real Toy DMA priority installer and replay five original C2 starts.

Pass the private ToyCommander-1GUTH.BIN as the only argument. The host test
models P1/P2 cache publication. The SH replay observes register writes, with
no hardware arbitration or movie playback emulation.
"""

import argparse
from hashlib import sha256
import os
from pathlib import Path
from struct import iter_unpack
import subprocess
from tempfile import TemporaryDirectory

from test_toy_original_gd_poll import SOURCE_BYTES, SOURCE_SHA256
from test_vmu_native_heap_execution import ROOT, SH, signed


BASE = 0x8C010000
PRIORITY_LITERALS = (0x8C094F98, 0x8C095A6C)
ORIGINAL_DMAOR, RX_FIRST_DMAOR = 0x8201, 0x8001
SAR2, DMATCR2, CHCR2, DMAOR = 0xFFA00020, 0xFFA00028, 0xFFA0002C, 0xFFA00040
C2DSTAT, C2DLEN, C2DST = 0xA05F6800, 0xA05F6804, 0xA05F6808
SRC, DST, LENGTH = 0x0C200000, 0x10800000, 0x6000


HOST = r"""
#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define BASE UINT32_C(0x8c010000)
#define P2 UINT32_C(0x20000000)
#define BYTES 748444u

static const uint32_t literals[] = {0x8c094f98u, 0x8c095a6cu};
static uint8_t *p1, *p2;
static unsigned publishes;
static uint32_t drop_publication;

static uint32_t native_word(uint32_t address) {
    assert(address == (literals[0] | P2) || address == (literals[1] | P2));
    uint32_t value;
    memcpy(&value, p2 + (address - (BASE | P2)), sizeof(value));
    return value;
}

static void native_publish(uint32_t address) {
    assert(address == literals[0] || address == literals[1]);
    ++publishes;
    size_t offset = (address & ~31u) - BASE;
    if(address != drop_publication) memcpy(p2 + offset, p1 + offset, 32u);
}

#include "toy_dma_priority_stage.inc"

int main(int argc, char **argv) {
    assert(argc == 2);
    p1 = mmap((void *)(uintptr_t)BASE, BYTES, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    p2 = malloc(BYTES);
    uint8_t *original = malloc(BYTES);
    assert(p1 == (void *)(uintptr_t)BASE && p2 && original);
    FILE *file = fopen(argv[1], "rb");
    assert(file && fread(original, 1u, BYTES, file) == BYTES && fgetc(file) == EOF);
    assert(fclose(file) == 0);

    for(unsigned i = 0; i < 2u; ++i) {
        memcpy(p1, original, BYTES);
        memcpy(p2, original, BYTES);
        size_t offset = literals[i] - BASE;
        p1[offset] ^= 1u;
        p2[offset] ^= 1u;
        assert(!toy_dma_priority_install() && publishes == 0u);
        assert(p1[offset] == (original[offset] ^ 1u));
        assert(p2[offset] == (original[offset] ^ 1u));
        p1[offset] ^= 1u;
        p2[offset] ^= 1u;
        assert(memcmp(p1, original, BYTES) == 0 && memcmp(p2, original, BYTES) == 0);
    }

    for(unsigned i = 0; i < 2u; ++i) {
        memcpy(p1, original, BYTES);
        memcpy(p2, original, BYTES);
        drop_publication = literals[i];
        assert(!toy_dma_priority_install());
    }

    memcpy(p1, original, BYTES);
    memcpy(p2, original, BYTES);
    drop_publication = 0u;
    publishes = 0u;
    assert(toy_dma_priority_install() && publishes == 2u);
    assert(memcmp(p1, p2, BYTES) == 0);
    assert(fwrite(p2, 1u, BYTES, stdout) == BYTES);
}
"""


class C2Start(SH):
    """Replay the original integer setup instructions up to C2DST."""

    def __init__(self, image):
        super().__init__(image)
        self.mmio = {address: 0 for address in (SAR2, DMATCR2, CHCR2, DMAOR, C2DSTAT, C2DLEN, C2DST)}
        self.r[14] = 0x8C00E000
        for offset, value in ((12, LENGTH), (16, SRC), (20, DST)):
            self.write(self.r[14] + offset, value)

    def read(self, address, count=4):
        if address in self.mmio:
            assert count == 4
            return self.mmio[address]
        return super().read(address, count)

    def write(self, address, value, count=4):
        if address in self.mmio:
            assert count == 4
            self.mmio[address] = value
            self.writes.append((self.executing, address, value, count))
        else:
            super().write(address, value, count)

    def step_plain(self, pc):
        self.plain(self.read(pc, 2), pc)

    def run_setup(self, entry, dmaor_pc, start_pc, priority):
        if entry == 0x8C094EE0:
            self.r[6], self.r[5] = CHCR2, 1
        else:
            # These four setup slices run after the common prologue has
            # loaded the priority literal and initialized the other inputs.
            self.step_plain(0x8C095986)
            self.r[9], self.r[10], self.r[11], self.r[12], self.r[13] = SAR2, C2DSTAT, 0, 1, CHCR2
        self.writes.clear()
        pc = entry
        for _ in range(50):
            op = self.read(pc, 2)
            if op >> 12 == 0xA:
                self.step_plain(pc + 2)  # The branches have real store delay slots.
                pc += 4 + 2 * signed(op & 0xFFF, 12)
            else:
                self.step_plain(pc)
                pc += 2
            if self.mmio[C2DST]:
                break
        assert self.writes[-1] == (start_pc, C2DST, 1, 4)
        assert self.mmio == {
            SAR2: SRC, DMATCR2: LENGTH // 32, CHCR2: 0x12C1,
            DMAOR: priority, C2DSTAT: DST, C2DLEN: LENGTH, C2DST: 1,
        }
        assert [write for write in self.writes if write[1] == DMAOR] == [(dmaor_pc, DMAOR, priority, 4)]
        return self.writes


def install_with_host(path):
    with TemporaryDirectory(prefix="kui-toy-dma-priority-") as tmp:
        src, tool = Path(tmp) / "install.c", Path(tmp) / "install"
        src.write_text(HOST)
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                        "-I", str(ROOT / "src/loader"), str(src), "-o", str(tool)], check=True)
        return subprocess.run([str(tool), str(path)], check=True, stdout=subprocess.PIPE).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    args = parser.parse_args()
    original = args.original.read_bytes()
    assert len(original) == SOURCE_BYTES and sha256(original).hexdigest() == SOURCE_SHA256
    assert tuple(BASE + 4 * i for i, (word,) in enumerate(iter_unpack("<I", original))
                 if word == ORIGINAL_DMAOR) == PRIORITY_LITERALS
    patched = install_with_host(args.original)
    expected = bytearray(original)
    assert ORIGINAL_DMAOR ^ RX_FIRST_DMAOR == 0x0200
    for address in PRIORITY_LITERALS:
        offset = address - BASE
        expected[offset:offset + 4] = RX_FIRST_DMAOR.to_bytes(4, "little")
    assert patched == expected

    for route in ((0x8C094EE0, 0x8C094EFA, 0x8C094F52), (0x8C0959C0, 0x8C0959DA, 0x8C095AD0),
                  (0x8C0959E6, 0x8C095A00, 0x8C095AD0), (0x8C095AA4, 0x8C095AC0, 0x8C095AD0),
                  (0x8C095AEC, 0x8C095B08, 0x8C095B1A)):
        old_writes = C2Start(original).run_setup(*route, ORIGINAL_DMAOR)
        new_writes = C2Start(patched).run_setup(*route, RX_FIRST_DMAOR)
        assert new_writes == [(pc, address, RX_FIRST_DMAOR if address == DMAOR else value, count)
                              for pc, address, value, count in old_writes]
    print("Original Toy DMA priority: exact SHA and two edits, five C2 starts, P1/P2 publication and refusals verified")


if __name__ == "__main__":
    main()
