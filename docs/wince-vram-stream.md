# WinCE stream transfers into video memory (generic `ce-boot.kui`, 20 Nov 2026)

**Status: host-tested and linked, not console-tested.** One behaviour change
to the generic Windows CE module, `KUI/apps/games/ce-boot.kui`, against the
installed `7b216f2e1136` (payload CRC `04D83892`). The CD-music and Armada
modules are unchanged.

Test build: `a660354d804c` (commits `deb8319d` and its correction
`a660354d`), 85,236 bytes, payload CRC `9CF2A176`, SHA-256
`72dc006d86650b24daaec15e6a8ecb1575aa1cf7de8be409cc027658511dc8d5`.

## What failed

In the first data session (19 Nov 2026, probe build `88ff969d505c`, the same
reader as `7b216f2e1136`, CD music Off, Virtual VMU Off), two games stopped on
the reader fault `GD FUNCTION UNSUPPORTED` in `DMA_TRANSFER`
([session 1](wince-data-session-1.md)):

| Game | GD calls before | Refused transfer | Request |
|---|---|---|---|
| KISS Psycho Circus: The Nightmare Child (US) | 416 (2.6 s of CE time) | `0x044C0360`, `0x4800` bytes | `DMAREAD_STREAM_EX`, 10 sectors (`0x5000` bytes) |
| Virtua Cop 2 (JPN) | 4,241 (after the memory-card check) | `0x04575600`, `0x80000` bytes | `DMAREAD_STREAM_EX`, 257 sectors (`0x80800` bytes) |

Both RAM dumps hold the GD driver's scatter list for the request: 32 bytes of
RAM, then the body in PowerVR video memory (the 64-bit window at
`0x04000000`), then 2,016 bytes of RAM. The first piece had been delivered;
the reader refused the second. Virtua Cop 2's destination, piece and request
sizes are the ones photographed on the native route
([vc2-vram-dma.md](vc2-vram-dma.md)).

The CE reader admitted only main RAM as a DMA destination (`dma_guest`,
`src/core/retail_gd.c`). The native fix (`KUI_NATIVE_GD_VRAM_DMA`) was never
part of the CE readers. The other 15 games sent no such transfer in the part
that was played; any refused one stops the game.

## The change

`KUI_RETAIL_CE_VRAM_DMA` (Makefile.dc `KUI_CE_VRAM_DMA`, default 1 for the
generic module, 0 for the data and timing probes):

- `transfer()` and the synchronous `stream_step()` admit a stream piece in
  either PVR window (`0x04000000` or `0x05000000`, physical, P1 or P2), whole
  inside one 8 MiB window and 32-byte aligned (`stream_guest`, through
  `kui_retail_vram_dma_map`). Every other destination keeps `dma_guest`.
- The resident's mapper returns the uncached P2 address for a P1/P2 PVR
  address (`map_guest`). No cache maintenance: the G1 DMA does none either.
- Only a stream piece reaches that branch: `guest()` refuses any CPU pointer
  in P1/P2 outside RAM (a TOC buffer, a status block, a PIO destination), as
  the installed module does (correction `a660354d`; the first commit let such
  a pointer through to the mapper).
- The background reader's interrupt fills the piece through its P1 alias, as
  for RAM, and `kui_retail_ce_payload_copy` writes non-RAM destinations with
  aligned halfword stores (no byte stores to video memory).

Unchanged: an ordinary `DMAREAD` into video memory is still refused (none was
seen), and a CPU pointer in `0x04...` or `0x05...` (U0) stays CE's virtual
address.

## Build and gates

```
make -f Makefile.dc build/retail-ce/entry.elf BUILD_ID=<12 hex> \
    KUI_RETAIL_CE_ASYNC_SLOTS=96 KUI_RETAIL_CE_MAP_RECORD=1
```

- `KUI_CE_VRAM_DMA=0` rebuilds `7b216f2e1136` byte for byte. The four CD-music
  modules and `ce-armada-all-on.kui` rebuild their installed `7b216f2e1136`
  bytes from the same tree, and the data probe (`KUI_WCE_PROBE=1`) rebuilds
  `88ff969d505c` byte for byte.
- Linked: the background resident ends at `0x8c00d700` (limit `0x8c00d800`),
  the standard CE resident at `0x8c00c4e8` (limit `0x8c00c800`); call-graph
  stack worst case 348 of 2,000 bytes; relaxed-call, instruction and layout
  gates pass; `tools/check_ce_virtual_vram_width.py` passes on both residents.
- `tests/test_wce_probe_admission.py` admits the exact module bytes in the
  GUI's seven production cases on FAT32 and exFAT.

## Host tests

`build/test-retail-gd-ce-vram`, `build/test-retail-async-ce-vram` (`make
test`) and `tests/test_retail_ce_map_guest.py`:

- the exact KISS and Virtua Cop 2 piece shapes, through the synchronous
  service and through the background reader with its interrupt: every byte in
  the modelled video memory and RAM, the neighbours untouched, the DMA-end
  and drive interrupts, completion;
- both windows through physical, P1 and P2 addresses, to the window's end,
  and through the background reader KISS's shape in each view: the body lands
  in that view only (`0b0cdf94`; dropping the view bit from the piece's P1
  alias fails it);
- refusals: crossing a window's end, the holes above each window, the TA and
  other areas, P3, an unaligned piece;
- an ordinary `DMAREAD` to video memory stays refused; a PIO pointer in
  `0x04...` reaches the mapper as CE's virtual address; a `GETTOC2` buffer or
  `PIOREAD` destination in P1/P2 video memory is refused before the mapper
  (CE's virtual window still passes), and that check fails without the guard;
- the resident's own `map_guest` and `ram_alias`, built for the host with and
  without the switch: PVR spans map to P2 with no cache maintenance, the rest
  of P1/P2 outside RAM never maps, CE's U0/P3 addresses keep their meaning,
  RAM maps through P2 after a purge.

The video memory is a host array. The G1 bus, PVR timing and what the games
draw are not modelled. The synchronous resident's own copy path (`read_part`
to `kui_retail_ce_payload_copy`) is not run on the host with a video-memory
destination: its halfword sink is executed in both linked residents by
`tools/check_ce_virtual_vram_width.py`, and its choice of that sink for P2
video-memory addresses was confirmed by disassembly in review.

## Console test

`KUI-WinCE-VRAM-Stream-Test-20NOV2026-0035-CST-a660354.zip` (SHA-256
`744e817bae8eb2f6f471de731a9f7511d3d076321aeb9880f976397eb12cd13b`): the
module as `KUI/apps/games/ce-boot.kui` and the installed `7b216f2e1136` in
`IF-NEEDED/`. KISS Psycho Circus (US) and Virtua Cop 2 (JPN), CD music Off,
Virtual VMU Off, cold boot from the K-UI CD:

- Do they get past the point where they stopped (KISS at boot, Virtua Cop 2
  after the memory-card check)?
- Do the screens look right (textures, movies)?
- Record the GUI build and `last-launch-a.txt` (module path, build, payload CRC).

## Not answered

- CD music On: those launches use the `ce-cdda-*` modules, which keep the
  refusal. Their resident space has to be audited before they take it.
- Later scenes of the other titles, and any unrelated later stop in KISS or
  Virtua Cop 2.
