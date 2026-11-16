# The card stream reader beyond Toy Commander: a proposal

Status: proposal of 15 November 2026. Step 2 was begun at the owner's
word of the same day (decision 1), is delivered as Test 77
(`docs/toy-wire-stream-test-77.md`) and has its console result: two
runs that evening, the second movie at 1,959 and 1,965 of 1,968 frames
against the baseline's 1,953, the game played on into a level, no error
in either report. Step 3 has its baseline. After the owner's direction
of 16 November (below) two more tests were built, delivered and run by
the owner the same day: Test 78 (`docs/toy-wire-stream-test-78.md`,
`1d0e5bab78ba`) in Toy Commander and Test 79
(`docs/toy-wire-stream-test-79.md`, `9f183a7f72f5`) in the two
Evolution games. Their results, and the two tests that follow from
Test 79's, are under The order from here: Test 80
(`docs/toy-wire-stream-test-80.md`, `e58dd2e6b641`) and Test 81
(`docs/toy-wire-stream-test-81.md`, `684f485644e5`), built and delivered
together on 16 November and not yet run on a console. Nothing else
here is built or approved. It answers the
owner's remark after Test 76's console run,
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

## The owner's direction of 16 November 2026

After Test 77's console result the owner wrote:

> So the eventual goal is
> - Universal VMU
> - Universal reader (or at least a few for the different classes of
>   games - CDDA, WinCE + CDDA, WinCE, and regular DC)
> - Run-ahead or prefetch where possible/needed. Goal is smooth FMVs and
>   very smooth gameplay.
>
> Since the FMV is basically a non-issue at this point, I want those
> other three goals ironed out. I don't know that chasing down those 9
> frames is value-added. People can deal with an almost imperceivable
> fmv issue.
>
> Evolution/Evolution 2 had no CD music. They just were my first proving
> grounds for the prefetch. They got rid of the lag. Wondering if this
> new loader would be better.

These are the goals of `docs/universal-loader-roadmap.md`. For this
plan they mean: the movie work on Toy Commander is at rest where Test 77
left it; the stream reader is from here on judged as read-ahead for the
third goal, against the installed prefetch and first in the two
Evolution games; and it counts for the first two goals only where it
runs beside the virtual VMU and, for a CD music game, beside the music.

## What the reading of 16 November found

Five independent readers were each given one part of the owner's
question: what a launch with the virtual VMU on does, how the VMU uses
the card, memory, the written record, and native CD music in the code.
Their answers are in
`docs/evidence/universal-reader-reading-2026-11-16/`, as returned. A
sceptical check of their claims was to follow and was cut off by the
session's usage limit, as were four more readers (the reader's own side
of a card loan, the map of classes, the CD music record and the record
on the Evolution games). The author checked these himself: the two lines
in Card Tools named below, the Games app's checks of the Background
module, the sizes of Test 77's worker and auxiliary, and every figure
taken from the Evolution 2 capture. The rest is the readers'.

### The virtual VMU

- **Switching it on changes nothing in a Toy test.** Card Tools' test
  entry sets the virtual VMU off for every Toy test launch whatever the
  saved setting says (`if(toy_test)emulation_enabled=false;`, Card Tools
  source `09c9a4b686d4`, `src/apps/games_retail.c` line 950), and
  refuses a title that is not Toy Commander (line 1621). The test
  module's worker is built without the virtual card's code
  (`reader_only()` is the constant true under `KUI_TOY_SUITE_MODE`) and
  does not arm in any other mode. The stream reader has never run
  beside the virtual VMU.
- **The installed reader is one file for both settings**:
  `KUI/apps/games/retail-native-dual.kui`. VMU On gives a virtual card
  in A2; VMU Off passes the physical cards through. Both keep the whole
  284 KiB lease and the 192 KiB block cache.
- **How the VMU uses the card.** Nothing of the card's 128 KiB is in
  RAM, only a checksum a block. Every block the game reads is fetched
  from the SD card and every finished block write is committed at once,
  one card transaction each, inside the game's Maple completion with
  interrupts masked. The worker borrows the card from the disc reader
  for it. While the game has a read outstanding the loan is refused and
  the game is told to ask again; otherwise the reader's open command is
  ended with CMD12, the port changes hands, and the reader stays closed
  until the game's next read.
- **In the stream reader that loan halts the stream and drops the
  window**, and sets a flag (`quiesced`) that nothing in this build
  clears. The path has its host scenario (`sequence_lent()`) and no
  console run.
- **How often.** Toy Commander was seen to read 15 to 25 blocks once,
  when it finds the card, and to write nothing outside a save; The House
  of the Dead 2 read 23 blocks in a session. A movie with no save in
  progress sees no loan at all.
- **How long a loan lasts with interrupts masked has never been
  measured on the console.**
- **Memory.** The lease is the same in every mode. The virtual VMU lives
  wholly in its first 0x12000 bytes, the worker; the stream from 0x13000
  up, and the room the stream's code needed came out of the block cache,
  not out of the worker. Nothing is left over on either side: the
  installed worker with the VMU has 384 bytes free, Test 77's auxiliary
  32. Test 77's worker made room for its report and screen pages by
  compiling the VMU out; with the VMU put back it is 4,832 bytes too
  large. So the virtual VMU beside the stream is the installed worker,
  as it is, with the stream's auxiliary in place of the block cache's,
  and without the Toy report. It is not a switch on the test module.
- **The record.** Background New with the virtual VMU on has console
  results on earlier builds (Evolution 2, Rayman 2, Lodoss, Code
  Veronica, The House of the Dead 2 and others). No console record
  exists of a card loan arriving while read-ahead is in flight. The
  House of the Dead 2 has lag and one freeze recorded with the card on
  and none with it off, cause open.

### CD music

- **One launch loads one loader file, and CD music, the virtual VMU and
  prefetch are in different files.** With CD Music On the Games app
  forces the Standard reader and loads a CD music file of its own
  (`cdda-toy-raw.kui` for the raw Toy Commander image). That file has no
  virtual VMU and no prefetch, and reads the movies the old way.
- **Two programs are admitted**, Toy Commander and MDK2, each by the
  SHA-256 of the program and of its sound driver and about twenty fixed
  addresses: the adapter plays through the game's own sound driver.
- **It reads one audio sector a card command**, 75 a second, each a
  whole CMD18 and CMD12 with interrupts masked, gives way to the game's
  data reads, and keeps about 0.74 s of music buffered.
- **The Background family has no CD music code**: the game's music
  commands are answered and nothing plays, as in every Toy test.
- **What stands between the three for a game like Toy Commander**: no CD
  music adapter on the Background reader, and no prefetch or VMU in the
  CD music files; each service takes the game's first heap block for
  itself; no rule lets an audio reader with a deadline share the card
  with a stream that stays inside one open read and with a VMU writer;
  the launcher's one-file routing; and, for "universal", sound adapters
  for more than two programs.

### The two Evolution games

The record has how Evolution 2 reads in play: the console capture of 25
October on the prefetch build `d0aac421584c`
(`docs/evidence/native-read-ahead-pio-pair-observed-2026-10-25.json`),
of which the owner said "I think we fixed it! I noticed a single
hiccup." Evolution 1 has the owner's word of 27 October on `b116d540b382`:
"That lag seems to be gone now."

| | In that capture |
|---|---|
| The game's last eight requests | 73, 50, 73, 50, 73, 50, 73, 50 sectors, each beginning where the one before ended (sectors 321,500 to 321,991) |
| The image | Raw: the eight requests' 492 sectors took 2,264 card blocks, 4.6 a sector |
| Prefetch | 2,230 blocks in 1,200 controller frames, two a frame by programmed transfer. The cache was full 55 times and fed all eight requests |
| Its cost | One block at most 1.44 ms, one visit at most 2.87 ms, inside the Maple completion with interrupts masked |
| A request, fed from the cache | 83 ms over 17 of the game's calls for 73 sectors, 52 ms over 12 for 50 |
| With one block a frame, the build before | 251 visits found nothing cached; a request waited up to 114 ms |

So prefetch there is a producer that just keeps pace with the game (it
made 2,230 blocks where the game took 2,264), for up to 2.9 ms of every
frame with interrupts off. The stream reader gets a card block for 66
microseconds of pump time (Test 77, console) and has about twenty times
the rate. That is the case for it in the owner's question, "Wondering if
this new loader would be better."

As built it cannot serve this game. A window takes a request of at most
60 sectors and holds 62. Every 73-sector request would go back to the
low reader and the window with its read-ahead would be given up, twice
in every pair of requests. The lag prefetch removed would be back. Three
things come first:

1. A window serves a request of any size, as the stream delivers it.
2. The window is offered to every title of the family, not to Toy
   Commander alone.
3. The virtual VMU on beside it, which is how these games are played.

Not known: how Evolution 1 reads, how either game reads a movie, and
how often either makes disc calls while one plays.

### The classes

From `docs/universal-library-coverage.md` (6 November) and the readers.
Where no reader returned, the cell says so.

| Class | Loader | Virtual VMU | Prefetch | The stream reader |
|---|---|---|---|---|
| Native, compact Background New and Legacy | `retail-native-dual.kui` and its A1 variant | Yes, by structure: 16 of the 21 checked originals resolve | The 192 KiB block cache, two blocks a controller frame, paused while the game streams | Lives here, in a test module for one title |
| Native, Standard | The Standard reader with its 72 KiB VMU service | Yes | None | No lease and no interrupt route for it |
| Native, standalone New | `retail-native-standalone.kui` | No: DOA2, Crazy Taxi and Zombie Revenge wait for a proved arena | None: it has neither the lease nor the heartbeat | Not without a lease |
| Native with CD music | `cdda-toy-raw.kui`, `cdda-toy-cooked.kui`, `retail-cdda-mdk2.kui` on the Standard reader | None | None | Needs the adapter beside it and a rule for the card |
| Windows CE | The generic module (`ce-boot.kui`; older installations have `ce-probe.kui`) | Armada only, by its own adapter | Not read | Another reader core; not read |
| Windows CE with CD music | Four adapters; Armada's VMU-on module accepted 9 November | Armada | Not read | Not read |
| Sega Smash Pack | Its accepted set, kept as it is | Its own | | Not touched |

## The order from here

The owner, 16 November, on the three things named above: "Go ahead and
work on those changes." Each is one behavior change against a named
baseline. The first two stay in the test module and are delivered
together as two modules, so that one console session answers both:

| Test | Baseline | The one change | Console |
|---|---|---|---|
| 78 | Test 77 | A window serves a request of any size, in slices, and a consecutive read of any size opens one. Room is made first: the auxiliary's image has 32 bytes free | Toy Commander as Test 77: the movies, then a level |
| 79 | Test 78 | The window is offered to every title of the family on a raw SCI image. Toy's hooks and its movie policy stay Toy's. The module can be put at the Background reader's path for a normal launch with the virtual VMU off; with it on it stops at once, before any card step | Evolution 1 and 2 with the virtual VMU off, against the installed module with it off: play where the lag was, and the intro movies |
| 80 | Test 79 as run on the console | A window stands through a command that only asks the drive something (Evolution 2 gave up 969 of 972 windows for such a command). The report names the commands a window meets | Evolution 2: `C` near zero, requests to a window far above 1.3 |
| 81 | Test 80 | The stream's timer in every title, by the SDK values the reader already admits for it (both games: no timer, 10 ms a wait) | Both games: `TMR` 1 or 2, a wait under 1.3 ms |
| Behind them | Test 81 and the installed module | The virtual VMU on: the installed worker with the stream's auxiliary and low residents that carry the stream's glue; the card loan with a window open; `quiesced` cleared | Evolution with the virtual VMU on, against the installed module: play, a movie, a save, power off, reload |

Tests 78 and 79 were built as the table says, on 16 November, delivered
as two archives and run by the owner the same day. Test 78: Toy
Commander's large loads came through windows, six requests of 2,135
sectors, none back; the movies as before, and no frame lost in that
run. Test 79: both Evolution games ran on it from the Games menu, their
movies without fault, with a slight stutter at times while the card was
read that the owner is not sure the installed reader is free of. Its
reports show the window doing the prefetch's work in Evolution (445
sectors to a window) and never getting to in Evolution 2, where a
command that is not a read gave up 969 of 972 windows; and in both a
stream without a timer, standing 10 ms at each wait. Both were named as
limits of the test before it ran. They are Tests 80 and 81 in the
table, each one change with its own run; the virtual VMU comes behind
them. No run compared the games with the installed module on the same
day: whether the window is smoother than the installed prefetch there
is still open.

Tests 80 and 81 were built as the table says, on 16 November, and
delivered as two archives: Test 80 for Evolution 2, Test 81 for both
games, each with three pages of report to photograph, and a last look
at the same places under the installed module. Neither has run on a
console. Each had a reader of its own before it went out. Test 80's
found no fault in the change and two holes in its tests, which are
closed. Test 81's found the hand-back of a title's timer wanting, as
the author's own reading had begun to: the timer now goes back as it
was found, and only what is still the stream's is written. What that
review left owed is in the list below.

Test 79 is
the change with three corrections its review asked for before any
module of it went out: Toy Commander's executable on another boot
sector no longer passes for another title that then gets Toy
Commander's hooks; a stream that has no timer, which is the stream of
every title but Toy Commander, waits its time before it begins a cut
run again; and the report's page says what to do when Start cannot
restart the console. The two tests' documents have their reviews in
full.

What the reviews add to the work behind them, none of it built:

- **A timer or a heartbeat for the stream in every title.** In another
  title only the game's own disc calls move a stream that waits. The
  resident already holds the title's admitted SDK values (its vector
  base and its default callback), by which the stream could take a
  spare timer as it does in Toy Commander; or the controller's
  heartbeat could move it. Which, and whether either is needed, is for
  Test 79's console result to say (`WAIT`, `WTICK`, `TMR`). It has said
  it: a wait lasted 10 ms in both games, the spacing of the games' own
  disc calls, which a heartbeat once a frame would not shorten. The
  timer is Test 81.
- **Images without a valid EDC.** The window checks each sector's EDC
  and the installed reader does not. On a raw image without it every
  read behind another opens a window that fails first. A rule that
  stops opening windows after a few in a row have failed is owed with
  the standing-stream rule, before the reader is anyone's default.
- **A large request in one visit.** A call of the game's hands over 32
  sectors of a large request at most. The bound is for interrupt
  visits; a call of the game's could hand over all the window holds, as
  it does for a request of up to sixty. Worth it if Test 79's console
  result shows the calls to matter.

- **Whose the timer is, by its registers.** Test 81 takes a title's
  spare timer by the SDK's table and the timer's registers at a
  stream's start and gives it back as found. It cannot see a title that
  drives the same timer by its registers alone and leaves it stopped
  between uses, or takes it under a wait that stands armed; and a wait
  with a timer has no clock behind it. No title is known to do either.
  The check (the registers compared with what the stream last left, or
  with what the title had at its first disc call) and the clock are
  owed before the reader is anyone's default
  (`docs/toy-wire-stream-test-81.md`, Known limits 2 to 4).
- **Room in the auxiliary.** Test 81's image is full: 32 bytes are
  left. The virtual VMU beside the stream needs code there (the loan
  with a window open), and gets its room from the Toy report's
  counting, which a module without the report's pages does not need.
- **The report with a command pending.** A window that stands through
  a command that only asks also stands when the report is asked for
  with one pending; Start then cannot restart the console
  (`docs/toy-wire-stream-test-80.md`, Known limit 6). The report's path
  could end the stream where the card's loan is refused.

Behind those, in the order the roadmap gives them: the rule that ends a
standing stream after a time (this plan's Known limits of Test 77, owed
before the reader is anyone's default); the reader as the family's
default; cooked images, which have no parity for the window's repair;
and CD music beside it, which is the roadmap's item 5 and needs the
adapter and the card rule named above. Steps 3 to 8 of the table under
Steps are superseded by this order where they differ: the hook's pump
(step 3) is answered by a title that has no hook.

## The owner's decisions

1. **The hiccup.** Accept Test 76 as the reference with the hiccup's
   cause left open, or have it explained first (step 1). Answered by
   the owner after the comparison run of 15 November: "Otherwise it's
   probably safe to adapt this loader to all other games". Step 1 is
   not taken; step 2 is begun on the variant without GAP. And after
   Test 77's console result, on 16 November: "Since the FMV is
   basically a non-issue at this point, I want those other three goals
   ironed out. I don't know that chasing down those 9 frames is
   value-added. People can deal with an almost imperceivable fmv
   issue." The frames still lost in Toy Commander's second movie are
   accepted as they are; no test is made for them.
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
   else. The owner, 16 November: "remember this is a CDDA game so I
   need that to be universal as well", and the goals quoted under The
   owner's direction below: the virtual VMU and CD music belong to what
   "every game" is to mean in the end. They were off in every run of
   this reader so far.
4. **Which games to qualify on** in step 4: two or three with FMVs that
   the owner has as raw GDI and that are admitted to that family today.
   The owner, 16 November: "Chat thought the first/second Evolution
   games would be good testing grounds, since we had to do the prefetch
   on those games to eliminate lag", and: "Evolution/Evolution 2 had no
   CD music. They just were my first proving grounds for the prefetch.
   They got rid of the lag. Wondering if this new loader would be
   better." So the two Evolution games, and the question put to them is
   the stream window against the installed prefetch, in play as well as
   in movies.
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
