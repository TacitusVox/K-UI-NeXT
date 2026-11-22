#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the Windows CE resident's own guest mapper on the host.

`map_guest` and `ram_alias` are taken verbatim from
src/loader/retail_resident.c (MMUCR becomes a host variable and the cache
purge a counter) and built twice: with `KUI_RETAIL_CE_VRAM_DMA`, as the
generic `ce-boot.kui` is, and without it, as the installed `7b216f2e1136`
and every other CE module are. The GD service's own tests model this mapper;
this one checks that the model matches the resident's code.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
RESIDENT = "src/loader/retail_resident.c"
MMUCR = "*(volatile const uint32_t *)(uintptr_t)0xff000010u"


def function(path, signature):
    source = (ROOT / path).read_text()
    start = source.index(signature)
    at = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[at] == "{") - (source[at] == "}")
        at += 1
    return source[start:at]


HARNESS = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "kui/retail_gd.h"
#include "kui/retail_loader_layout.h"
#include "kui/retail_vram_dma.h"
#if KUI_ARMADA_CE_EXCERPT
#include "kui/armada_ce_excerpt_pool.h"
#endif

static unsigned checks, purges;
static uint32_t host_mmucr;
#define CHECK(x) do { ++checks; if(!(x)) { \
    fprintf(stderr, "CE guest mapper assertion at line %u: %s\n", __LINE__, #x); abort(); \
} } while(0)
static void purge(uint32_t address, uint32_t bytes) { (void)address; (void)bytes; ++purges; }

/* RAM_ALIAS */
/* MAP_GUEST */

static uint32_t map(uint32_t address, uint32_t bytes, int writing) {
    return (uint32_t)(uintptr_t)map_guest(NULL, address, bytes, writing);
}

int main(void) {
    static const int modes[] = {0, 1, KUI_RETAIL_MAP_VALIDATE};
    /* PVR memory through P1 or P2: the KISS and Virtua Cop 2 bodies, each
     * view to its end, and an interrupt's partial piece (the resident does
     * not re-check alignment; stream_guest checked the whole piece). */
    static const struct { uint32_t address, bytes, p2; } pvr[] = {
        {0x844c0360u, 0x4800u, 0xa44c0360u}, {0xa4575600u, 0x80000u, 0xa4575600u},
        {0x847fffc0u, 0x40u, 0xa47fffc0u}, {0xa5000000u, 0x800000u, 0xa5000000u},
        {0x857fffc0u, 0x40u, 0xa57fffc0u}, {0x84000002u, 0x7feu, 0xa4000002u}};
    /* Never: crossing a window's end, the holes above each window, the rest
     * of P1/P2 outside RAM, an empty or oversized span. */
    static const struct { uint32_t address, bytes; } never[] = {
        {0x847fffe0u, 0x40u}, {0x84800000u, 0x20u}, {0xa5800000u, 0x20u}, {0xa0800000u, 0x20u},
        {0x80000000u, 0x20u}, {0xa6000000u, 0x20u}, {0x94000000u, 0x20u}, {0xbfffffe0u, 0x20u},
        {0x8b000000u, 0x20u}, {0x844c0360u, 0u}, {0xa5000000u, 0x800001u}};
    for(unsigned m = 0; m < 3; ++m)
        for(uint32_t mmu = 0; mmu < 2; ++mmu) {
            const int w = modes[m];
            host_mmucr = mmu;
            unsigned before = purges;
            for(unsigned k = 0; k < sizeof(pvr) / sizeof(pvr[0]); ++k)
#if KUI_RETAIL_CE_VRAM_DMA
                CHECK(map(pvr[k].address, pvr[k].bytes, w) == pvr[k].p2);
#else
                CHECK(!map(pvr[k].address, pvr[k].bytes, w));
#endif
            for(unsigned k = 0; k < sizeof(never) / sizeof(never[0]); ++k)
                CHECK(!map(never[k].address, never[k].bytes, w));
            /* No cache maintenance outside RAM: the G1 DMA does none. */
            CHECK(purges == before);
            /* CE's virtual addresses keep their meaning, PVR-like or not:
             * U0 and P3 with the MMU on, nothing with it off, never P4. */
            CHECK(map(0x044c0360u, 0x4800u, w) == (mmu ? 0x044c0360u : 0u));
            CHECK(map(0x05000000u, 0x20u, w) == (mmu ? 0x05000000u : 0u));
            CHECK(map(0x0c3b0000u, 0x800u, w) == (mmu ? 0x0c3b0000u : 0u));
            CHECK(map(0xc0001000u, 0x800u, w) == (mmu ? 0xc0001000u : 0u));
            CHECK(!map(0xe0000000u, 0x20u, w) && !map(0x7fffffe0u, 0x40u, w));
            /* RAM is written through P2, purged first unless validating;
             * the reader's own reservation is refused. */
            CHECK(map(0x8c010000u, 0x800u, w) == 0xac010000u);
            CHECK(purges == before + (w != KUI_RETAIL_MAP_VALIDATE));
            CHECK(!map(KUI_RETAIL_RESIDENT_ADDRESS, 0x20u, w));
#if KUI_ARMADA_CE_EXCERPT
            /* The CD music modules' resident: the music pool stays refused
             * through either RAM alias, and RAM next to it still maps. */
            before = purges;
            CHECK(!map(KUI_ARMADA_CE_EXCERPT_POOL_BEGIN, 0x20u, w));
            CHECK(!map((KUI_ARMADA_CE_EXCERPT_POOL_END - 0x20u) | 0x20000000u, 0x20u, w));
            CHECK(!map(KUI_ARMADA_CE_EXCERPT_POOL_BEGIN - 0x20u, 0x40u, w));
            CHECK(purges == before);
            CHECK(map(KUI_ARMADA_CE_EXCERPT_POOL_BEGIN - 0x20u, 0x20u, w) ==
                  ((KUI_ARMADA_CE_EXCERPT_POOL_BEGIN - 0x20u) | 0x20000000u));
            CHECK(map(KUI_ARMADA_CE_EXCERPT_POOL_END, 0x20u, w) == (KUI_ARMADA_CE_EXCERPT_POOL_END | 0x20000000u));
#endif
        }
    printf("CE guest mapper: %u checks passed\n", checks);
    return 0;
}
'''


class CeGuestMapper(unittest.TestCase):
    def source(self):
        ram_alias = function(RESIDENT, "static int ram_alias(uint32_t address) {")
        map_guest = function(RESIDENT, "static uint8_t *map_guest(void *unused, uint32_t address, uint32_t bytes,")
        self.assertIn(MMUCR, map_guest)
        self.assertIn("kui_retail_vram_dma_span(address, bytes)", map_guest)
        map_guest = map_guest.replace(MMUCR, "host_mmucr")
        return HARNESS.replace("/* RAM_ALIAS */", ram_alias).replace("/* MAP_GUEST */", map_guest)

    def run_variant(self, vram, extra=()):
        with tempfile.TemporaryDirectory(prefix="kui-ce-map-guest-") as temp:
            generated = Path(temp) / "harness.c"
            binary = Path(temp) / "harness"
            generated.write_text(self.source())
            command = shlex.split(os.environ.get("CC", "cc")) + [
                "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-Wpedantic",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "-I", str(ROOT / "include"), "-DKUI_RETAIL_CE=1", "-DKUI_RETAIL_ASYNC=1"]
            if vram:
                command.append("-DKUI_RETAIL_CE_VRAM_DMA=1")
            command += list(extra)
            subprocess.run(command + [str(generated), "-o", str(binary)], check=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True, check=True)
            self.assertRegex(result.stdout, r"CE guest mapper: \d+ checks passed")

    def test_pvr_stream_pieces(self):
        self.run_variant(True)

    def test_installed_mapper_refuses_pvr(self):
        self.run_variant(False)

    # The CD music modules' resident mapper (Makefile.armada-audio scia: the
    # music pool reservation of KUI_ARMADA_CE_EXCERPT), with and without PVR.
    CD_MUSIC = ("-DKUI_ARMADA_CE_AUDIO_OBSERVE=1", "-DKUI_ARMADA_CE_EXCERPT=1", "-DKUI_ARMADA_CE_STREAM=1",
                "-DKUI_WINCE_CDDA_BAM4=1", "-DKUI_WINCE_CDDA_BAM4_KERNEL=KUI_WINCE_CDDA_BAM4_KERNEL_VIRTUA_COP_2")

    def test_cd_music_mapper_pvr_stream_pieces(self):
        self.run_variant(True, self.CD_MUSIC)

    def test_cd_music_mapper_refuses_pvr(self):
        self.run_variant(False, self.CD_MUSIC)


if __name__ == "__main__":
    unittest.main()
