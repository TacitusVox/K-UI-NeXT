# The card stream reader beyond Toy Commander: a proposal

Status: proposal of 15 November 2026. Step 2 was begun at the owner's
word of the same day (decision 1), is delivered as Test 77
(`docs/toy-wire-stream-test-77.md`) and has its console result: two
runs that evening, the second movie at 1,959 and 1,965 of 1,968 frames
against the baseline's 1,953, the game played on into a level, no error
in either report. Step 3 has its baseline. Nothing else here is built
or approved. It answers the owner's remark after Test 76's console run,
"then we can probably package this for every game's FMVs", with what
that takes and in what order. The decisions at the end are the owner's.

It rests on an independent map of the code and documents
(`docs/evidence/toy-wire-stream-76/generalization-map.md`, as returned).
The author checked three of its facts against the source (the lease's
size, the roadmap's order, the delivered candidate named in two places)
and takes the rest from it. Where this document says "the map", that is
the source.

## The baseline

Test 76 (`docs/toy-wire-stream-test-76.md`): module build
`7fe7e83c2082`, payload CRC32 `8D0D12DA`, run on the owner's console,
report 66-10. Toy Commander's second opening movie shows 1,942 of 1,968
frames from the SCI card, the first all 331. One hiccup of 24 to 26
frames near the 39th second is left; its cause is open between the
player's processor time and its own buffer, and one measurement would
decide it. The variant with GAP switched off ran on the console as well
(report 66-11): 1,953 frames, 15 lost in the same stretch of the movie,
the reader's cost and its restarts unchanged. GAP is left out from here
on, and that variant (module `7ce3c839...1c67`, payload CRC32
`C50A20B9`) is the baseline of step 2.

The module is a test module. It is installed where Card Tools looks for
Test 66 and replaces no installed reader, which is why none of this has
touched the delivered native candidate or its measurement checkpoint
(`AGENTS.md`).

## What the reader is made of

General already, by the map:

- The card stream (`src/loader/toy_wire_stream.c`): the open multi-block
  read received by DMA into a 32 KiB ring, every lost byte logged,
  blocks handed on as clean, rebuilt or with lost bytes. Its interface
  names no game.
- The verified window and its sector checks, for raw 2,352-byte Mode 1
  images only: 62 sectors ahead of the game, each accepted on its
  blocks' CRC16 and its address, on its P parity for a rebuilt byte, or
  through its EDC; lost bytes filled from the sector's own parity.
- The low resident's block feed and its table of operations, which are
  the production ones.
- The memory: a lease of 284 KiB from the game's own allocator, the same
  size as the accepted compact Background reservation, at no fixed
  address. Inside it the stream build has a 24 KiB auxiliary and a
  180 KiB arena where the installed read-ahead has 12 KiB and 192 KiB.

What it needs of a game: that lease; the SCI receive-error interrupt
reaching the low resident through the SDK's own table; DMA channel 1
free, which the installed SCI reader needs as well; timer channel 0 in
the SDK's ordinary setting for the clock; a spare timer channel if it is
to wait without a foreground call; the operand cache on.

Specific to Toy Commander, by the map:

- The movie logic is admitted for one exact executable (size and
  SHA-256), and the movies are four file names looked up on the disc.
- Five hooks patched into the game's code at fixed addresses. One of
  them, the player's update call, starts and ends the window and is one
  of the reader's pump sources, about 2,500 calls a second.
- A request uses the window only if it lies inside one of those files
  while that movie plays. Nothing in the reader detects sequential
  reading by itself.
- The game's disc calls are told apart as polling or waiting by return
  addresses inside this executable.
- The low residents under the module are Test 66's Toy-suite builds, not
  the production ones, and the stream's glue uses fields only they have.
- The report, the player measurements, and the Card Tools test entry
  (whose source is on another branch).

## Where it can go first

One launch family already has what the reader needs: compact Background
"New" on a raw SCI GDI image, the family the Toy module is built from. It
has the lease, the interrupt route and the auxiliary. So "every game's
FMVs" can at first only mean: every title admitted to that family, on a
raw image, with virtual VMU and CD music off.

Not without steps of their own, by the map:

| Route | What is missing |
|---|---|
| Standard SCI reader | No read-ahead lease |
| Standalone New | No lease; no interrupt route in its foreground variant |
| Cooked 2,048-byte and compressed images | No raw sectors on the card, so no parity and no EDC: the window's repair does not apply, and the project does not accept a block rebuilt from its CRC16 without a second check |
| WinCE | Its own ABI, memory ownership and interrupt table; the auxiliary does not build for it |
| CD music | One open card stream cannot serve audio sectors and a movie window at once |
| Virtual VMU on | The stream has to stop for the worker's use of the card and start again |
| Physical disc, SCIF, IDE | No SCI card stream |

The installed read-ahead of that family reads two card blocks a
controller heartbeat by programmed transfer. The map reckons that at
about 60 KiB a second, by its own arithmetic and an assumed heartbeat a
frame, where Toy's second movie needs about 0.8 MB a second. That is the
difference a stream-fed window would make there, and it would take the
place of today's 192 KiB block cache in the same lease.

## Steps

Each is one change against a named baseline with its own console test,
as `AGENTS.md` asks. Steps 1 to 3 stay inside the Toy test module and
replace nothing that is installed.

| Step | Baseline | The one change | Rebuilt | Console test |
|---|---|---|---|---|
| 0 | Test 76 | None: the owner's decisions below | Nothing | None |
| 1 (optional) | Test 76 | Measurement only: by piece, the time in which the player's next chunk was missing from its buffer. Room has to be made first, the auxiliary's image is full | Auxiliary and worker | The Toy run. Says whether the hiccup is the player's processor time or its data |
| 2 | Test 76 or step 1 | The window opens when the reader itself sees consecutive requests on a raw data track, each beginning where the last ended, and closes on a request elsewhere. The movie names and the playing-movie witness decide nothing any more; the player's hook still pumps | Auxiliary, worker if rows change | The Toy run: both movies as Test 76, and play into the first level, because ordinary sequential loads open windows too |
| 3 | Step 2 | The player's hook no longer pumps the reader; it only measures. Pumps come from the SCI interrupt, the stream's timer and the game's own disc calls | Auxiliary | The Toy run. This is the step that says whether a reader with no knowledge of the title keeps Test 76's result |
| 4 | Step 3, and the installed compact Background module as read from the card | A reader module of its own for that family, without Toy's admission and hooks, with general counters in place of the Toy report | The new module only; installed modules byte for byte | Toy first, which must equal step 3; then two or three other raw GDI games of the owner's: the same FMV scene before and after, first and repeat launch, a loading-heavy area |
| 5 | Step 4 | The stream window becomes that family's behaviour for every admitted raw SCI GDI title, with no per-title switch | The family's modules as one cohort; all other families exact prior bytes | Cold ROOT boot, module path, build and payload CRC confirmed; the games of step 4, an FMV, a load and a save |
| 6 | Step 5 | Virtual VMU on beside the stream | That family | An FMV with VMU on, a save, power off, reload |
| 7 | Step 5 | CD music beside the stream, each adapter by itself | That family | An FMV and a music track |
| 8 | | The other routes of the table above, each under its own contract | | |

If step 3 loses frames against step 2, the next single change is
another general source of foreground calls: the extra timer call Test 69
had, or the controller heartbeat the installed read-ahead already
receives.

Step 2 as built (Test 77) adds one thing to this order. With the window
no longer ended by a movie, a filled window's stream stands until the
game reads elsewhere, and with it the card inside its read command, DMA
channel 1's addresses, a timer channel's level and the SCI level
(`docs/toy-wire-stream-test-77.md`, Known limits 1, from that test's
review). Before step 4 puts this reader under a game other than Toy
Commander, a standing stream has to be ended after a time, its sectors
kept: one change of its own, with its own console comparison. Where it
goes between steps 2 and 4 is the owner's to say with decision 2.

Per-title hooks are the alternative to step 2 and 3: for every game the
exact executable, hook addresses and context offsets, found by analysis.
They give an exact start and end and the player's own calls as a pump.
As a way to deliver they are a title-specific opt-in, which `AGENTS.md`
forbids; they fit only as measurement inside a test module.

## The owner's decisions

1. **The hiccup.** Accept Test 76 as the reference with the hiccup's
   cause left open, or have it explained first (step 1). Answered by
   the owner after the comparison run of 15 November: "Otherwise it's
   probably safe to adapt this loader to all other games". Step 1 is
   not taken; step 2 is begun on the variant without GAP.
2. **Order.** FMV streaming is fourth of the roadmap's five sections
   and fifth in its current sequence
   (`docs/universal-loader-roadmap.md`), and the measurement checkpoint
   in `AGENTS.md` says not to stack a performance change on the
   delivered native candidate while its console baseline is pending.
   Steps 1 to 3 touch nothing installed. Step 4 installs a new file
   beside the installed ones; step 5 replaces installed modules. Whether
   this moves ahead of the roadmap's earlier items, and whether steps 4
   and 5 wait for that checkpoint, is the owner's to say.
3. **What "every game" is to mean first.** Compact Background New on
   raw SCI GDI images with VMU and CD music off, as above, or something
   else.
4. **Which games to qualify on** in step 4: two or three with FMVs that
   the owner has as raw GDI and that are admitted to that family today.
5. **Memory.** Keep the 284 KiB lease and give the 192 KiB block cache
   over to the window and the ring, which changes ordinary loads for
   every game on that reader; or enlarge the lease to keep both, which
   needs allocator and lifetime proofs for each family; or shrink one of
   the two.
6. **The low residents.** A general module on Test 66's residents with
   their Toy parts inert, or on the production residents extended. The
   second rebuilds residents and so touches every game on that reader.
7. **How a second game reaches a test build.** A new module file that a
   changed GUI routes to (then both GUI aliases), or a package that
   replaces the installed Background module for the test. The second
   touches the installed reader.
8. **Which bytes are the installed baseline.** `AGENTS.md` names
   `f8807cb16561` as the native test source to keep unchanged; the
   roadmap and the coverage record name `9e3295bcfaec` as the delivered
   candidate. The module path, build and payload CRC on the owner's card
   have to be read before anything is compared against "the installed
   reader".

## What is not known

By the map, and not established by anyone: other games' movie players
(their request sizes, how often they make disc calls while a movie
plays, their data rates); whether Test 66's low residents behave like
the production ones for ordinary loads in other games; and what a game
that runs with the operand cache off or in another mode does to the
reader's cached reads. Requests of more than 60 sectors bypass the
window today. The roadmap's own request stands for step 4: record
request sizes, contiguity and gaps for each candidate game before
anything is built for it.
