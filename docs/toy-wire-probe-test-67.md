# Toy Commander Test 67: SCI wire probe on the physical GD-ROM

Prepared 2026-11-13 (America/Chicago). Source commit `e5fa67e75f73`. This build
has passed host and linked checks only. No console result exists yet.

## The question

Tests 1 to 66 read the Toy Commander movies from the SCI microSD card and none
played both movies smoothly. In the best of them (Test 57) the reader held the
CPU for 17.1 s of a 28.7 s intro window. Four out of five SD blocks arrived
while the game stood inside a GD call, at 346 us a block, which is the wire
time of 512 bytes at 12.5 MHz. The original disc costs the game almost nothing
for the same data.

A reader can only work if the wire runs while the decoder runs. Whether the
SCI can do that beside this game was never measured in isolation. Test 67
measures it and nothing else:

> While the game decodes both movies at full speed, how many bytes can the SCI
> clock out of the card in the background, how often does its one-byte
> receiver overrun, and what does servicing that cost?

## Baseline and change

Baseline is the physical-disc trace of Test 63, build `5034eb26898d`, which
ran on the console: 331 and 1,968 frames, none skipped. Commit `f216fac`
imports that source unchanged. Built with the pinned toolchain (GCC 15.2.0,
binutils 2.45.1) and `BUILD_ID=5034eb26898d` it reproduces the delivered
module byte for byte (SHA-256 `545c6c2c...31ff`), and still does with the
probe compiled out (`TOY_WIRE_PROBE=0`).

Test 67 is that module with `TOY_WIRE_PROBE=1`. Every game read still comes
from the GD-ROM firmware. The module additionally:

1. At the bumper's start opens the card and one CMD18 stream at card block
   `0x20000`, by the existing programmed bus code.
2. Receives the stream by DMA channel 1 in 16 KiB segments that cross block
   boundaries, into a private buffer. The data is never used.
3. On each receiver overrun stops and counts the run, resets the SCI module
   and receives on from the byte after the lost one. These are the
   `finish()`, `handoff()`/`module_reset()` and `start_dma()` register
   sequences of `src/loader/sci_stream.c`, including the in-place resume that
   Tests 48 to 51 ran thousands of times.
4. Answers a run shorter than 128 bytes with a wait instead of an immediate
   resume: 82 us, doubling to 1.3 ms while runs stay short. The wait uses TMU2
   or TMU1 if the game has left one stopped, unprioritised and without a
   callback; otherwise hook visits resume the stream and the report says so.
5. Stops when the intro's player exits, sends CMD12 and returns channel 1,
   the two priority fields, the callback slots, the timer and the SCI port.

New relative to earlier tests is only the policy: no per-block stop, resume
from the error interrupt, and the wait. The earlier burst tests (26 to 28)
treated every overrun as a failure; the resume tests (48 to 51) kept one DMA
per block and re-armed from the game's own calls.

### How the interrupt reaches the probe

The exact executable (SHA-256 `ee68a326...9bbd`, checked by the stage before
the module is installed) keeps an SDK interrupt table at `VBR + 0x1c0`, one
RTS callback per event. The SD reader used the same table in Tests 32 to 66.
The probe checks the live VBR (`0x8c00f400`) and that the SCI slots still hold
the SDK default (`0x8c0be408`) before it writes them, and uses level 1, the SD
reader's level. The values are those recorded in the handoff evidence
`toy-graphics-priority-final-original-sdk-callback.json`.

The module is linked in the uncached mirror, as Test 63 was. The callback
alone is entered through its cached alias so that its cost is not inflated by
uncached instruction fetches. The stage invalidates both caches before the
game starts, nothing else executes in the lease from the cached side, the
code is never modified and all its data stays at the uncached addresses.

## Reading the report

The file is 1,024 bytes of fixed-width hexadecimal rows; its length does not
depend on the run and is asserted at compile time. One second is 781,250
ticks. `B` is the bumper and `I` the intro, each from its first submitted
frame to the player's exit.

| Row | Values |
| --- | --- |
| `ST ERR AUX MODE SEGS` | Stream state (3 closed cleanly, 0 never started, 4 fault), first refusal or fault and its detail, timer channel (2, 1, or 0 for none), completed segments |
| `SUB CAD MAXGAP SKIP EXIT MAXREQ` | Test 63's frame counters: submitted, on time, longest gap, skipped, exit code (`7DC` normal), longest GD request |
| `SPAN BYTES CUTS IRQ ISR` | Window ticks, wire bytes clocked, overruns inside a segment, handler calls, ticks inside the handler |
| `PAUSE TICKS MAX LATE` | Overruns answered with a wait, total and longest wait, ticks the stopped receiver waited to be served |
| `GD TICKS BYTES CUTS` | The share of the window, bytes and overruns inside GD-ROM movie transfers |
| `RUN 16 64 256 1K 4K MORE` | Overruns by bytes received before them |
| `PC N PC N` | The two 64-byte code regions most often interrupted by an overrun, with estimated counts |
| `TAIL BYTES OK BAD HOLE` | The last segment, checked afterwards: bytes held, SD blocks whose CRC16 agrees, blocks failing without a logged lost byte, blocks containing one |

Derived figures:

- Wire rate is `BYTES / SPAN * 781250` bytes per second. The receiver's limit
  is 1,562,500 (two bytes a tick). The raw 2352-byte image needs about 939,000
  on average for the intro and 809,000 for the bumper, so `BYTES / SPAN` must
  exceed about 1.20 and 1.04. A usable reader needs margin for the intro's
  peak second and for repairs: about 1.35 or better.
- The same ratio outside disc transfers is
  `(BYTES - GD BYTES) / (SPAN - GD TICKS)`. The drive's own transfers load the
  bus in this test and would not exist with an SD reader, so this is the
  figure to judge by if the two differ.
- Handler share is `ISR / SPAN`. The SDK's own save and restore around each
  call is not inside `ISR`; `IRQ` gives the number of calls it applies to.
- `LATE` near zero means level 1 is served promptly. A large value means the
  wire often stood still waiting, which a higher level could address.

A result supports building a reader on this transport if the frame counters
match Test 63 (`SUB 14B` and `7B0`, `SKIP 0`, exit `7DC`), the wire ratio
outside disc transfers clears about 1.35, the handler share is a few percent
and `TAIL BAD` is zero. If the frames suffer or the ratio stays well under
1.2 even outside disc transfers, the SCI cannot feed this movie beside the
decoder and no reader design will change that.

`ERR` values, decimal here and hexadecimal in the report: 1 game timer profile, 2 SCI port busy, 3 card initialisation,
4 card slow or too small, 5 SCI bus state, 6 card never idle, 7 CMD18
response, 8 first data token, 9 SDK table not the admitted one, 10 DMAC
channel 1 or DMAOR not free, 11 first receive. Running faults: 16 channel 1
taken by someone else, 17 SCI handoff, 18 segment end, 19 stop without
overrun, 20 resume, 21 CMD12 or port restoration (`10` to `15` in the report).

## What was checked

| Check | Result |
| --- | --- |
| Test 63 reproduction, probe compiled out | Byte-identical to the delivered module |
| Reproducible build of Test 67 | Two clean builds, identical SHA-256 |
| Existing linked audits (`Makefile.disc-native` `audit`) | Handoff, 22 handoff mutations, launch policy and instruction audit pass |
| Card Tools admission on the packaged bytes (`toy-trace-package-test`) | Layout for the Toy executable, one report seed, one range seed, encode and validate pass; same seed offsets as Test 63 |
| Host model (`toy-wire-probe-host-test`), ASan and UBSan | 16 scenarios pass |
| Mutation check (`tests/test_toy_wire_probe_mutations.py`) | 25 of 25 deliberate defects caught |
| Linked callback | No calls, no multiply, 36 bytes of stack |
| Worker size and layout | 20,092 bytes (limit 32,768); BSS ends at `0xacfda380`, stack top `0xacfdb380`, inside the 64 KiB the stage admits |

The host model drives the real probe source against a register-level model
of the SCI receiver, DMAC channel 1, a TMU channel, the SDK table and a card
in an open CMD18. It covers clean segments, 1,500 random cuts with waits,
every stall position at a segment's end, the wait ladder, the poll fallback,
eight refusals, a foreign channel, an abandoned stream, damaged data and
complete restoration of every borrowed register. While building it the model
exposed two defects that were fixed: a stall landing on a segment's last byte
was treated as a fault, and the first run had no start time for `LATE`.

## Limits

- The model encodes the receiver behaviour documented from the console in
  `sci_stream.c` and the October SCI notes. It checks logic and accounting. It
  says nothing about bus timing, which is the thing under test.
- Resuming from the error interrupt is new on hardware for this game. A source
  comment from Test 48 says "SCI IRQ re-arming failed on the original
  hardware" without a record of how. The wait after a short run exists for the
  likely cause, an immediate second overrun while the bus is still held, and
  the `RUN` histogram will show whether resumes fail at once.
- The interrupted address is read from SPC inside the SDK's callback. If the
  SDK's entry code reuses SPC, the `PC` row will show SDK addresses instead of
  game code and should be ignored.
- GD-ROM transfers share the bus in this test. The `GD` row separates them by
  request and completion, which bounds the transfers but includes seek time.
- The stream starts at a fixed card block and is not the movie file. Card
  timing for unwritten areas can differ slightly from file data.
- The installed Card Tools names the report after Test 63 (`63-N.txt`).

## Package

`KUI-Toy-Wire-Probe-Test-67-13NOV2026-0940-CST-e5fa67e.zip`, SHA-256
`5e8044d3a6821f8f0b8a80258faff8f20e9912259298ea1b8bed5d25d3b0b9ac`.

| File | Build | Payload CRC32 | SHA-256 | Status |
| --- | --- | --- | --- | --- |
| `KUI/apps/disc/native-toy-trace.kui` | `e5fa67e75f73` | `2BEF9B88` | `7533fb8b...f4e5` | Changed: replaces Test 63's `5034eb26898d` |
| `KUI/tools.kui` | `09c9a4b686d4` | `8111E794` | `04b8657a...c800` | Unchanged, identical to the Test 66 package |

The module, the packaged README and checksum list, and the check logs are in
`docs/evidence/toy-wire-probe-67/`. The Card Tools binary is not duplicated
there; its hash above identifies it.

Validate by cold CD boot, Card Tools, GD Play, Right. Rebuild with:

```sh
make -f Makefile.disc-native TOY_TRACE=1 TOY_WIRE_PROBE=1 BUILD=build/toy-wire-67
make -f Makefile.disc-native TOY_TRACE=1 TOY_WIRE_PROBE=1 BUILD=build/toy-wire-67 toy-trace-package-test
make -f Makefile.disc-native BUILD=build/toy-wire-67 toy-wire-probe-host-test
python3 tests/test_toy_wire_probe_mutations.py
```
