# Review of Toy Test 79

An independent reader's review of commit `fed458d6ef97`, 16 November 2026. The
reader worked in a private copy outside the tree and was told what the change
was meant to do, not what to conclude. This is its result as it returned it,
with scratch paths shortened. `ran`: it built and ran something that shows the
finding (always the host fixture or a host test; nothing here ran on a
console). `read`: derived from the code. What was done with each finding is in
the test's document, under Review.

## Finding 1: Every other title runs the stream without a timer: a cut-short run is resumed with no pause inside the same interrupt visit, or is left standing until the game's next disc call

- Kind: hang-or-stall; established: ran
- Where: src/loader/toy_wire_stream.c timer_select(), wire_stopped(), kui_wire_service(); src/loader/toy_wire_ahead.inc toy_wire_drain(), toy_wire_seek(); reached for non-Toy titles through src/loader/native_readahead_aux.c suite_demand() since fed458d

Scenario:

Another title (the plan's console titles are Evolution 1 and 2).
timer_select() takes a TMU channel only if the SDK table word equals Toy
Commander's own default callback 0x8c0be408, so mode is 0. A run is cut after
fewer than 128 bytes (the regime docs/toy-wire-probe-test-67.md measured for
27-44% of Toy's playback: 21,218 of 27,783 overruns, 'runs last a few dozen
bytes and the wait ladder sits at its 1.3 ms limit'). The SCI error interrupt
visit: toy_wire_drain -> kui_wire_service -> wire_stopped sets WAIT and arms
nothing (mode 0). (a) The drain's next look (next loop turn, or the trailing
toy_wire_serve) takes 'if(!w->mode) wire_resume(w)': the run is re-armed
microseconds later, before the handler returns; if the bus is still held it
overruns again and the next ERI is pending at RTE. (b) If the cut is found by
the visit's last look, or the visit halts the stream (sector past its parity
budget, frame lost, extent jump), nothing restarts it: toy_wire_seek refuses
in an interrupt, there is no timer, no update hook (kui_toy_movie_ahead_update
is Toy's), TICK only counts, and reader_cooperative_service needs
toy_suite_active. It stands until the game's next GD call.

Consequence:

The 82 us to 1.3 ms backoff ladder, which exists because an immediate resume
was expected to overrun again while the bus is held, does not exist for any
title but Toy Commander. Expect bursts of back-to-back SCI visits each costing
a lost byte (so more sectors refused and re-read by programmed transfers with
CMD12+CMD18), and read-ahead that stops between the game's disc calls. It is
not a permanent hang and not an unbounded spin: a polled request pumps on
every EXEC/CHECK, and a burst ends when the block or sector exceeds its
lost-byte budget (24/32) or the log fills (255). How bad it is on a console
cannot be said from here: the host model has no bus timing. The report will
show it as TMR 0 with WAIT counted and almost no WTICK.

Evidence:

Host fixture only (no console). Private copy <scratch>/t79/review/revB,
tests/test_toy_wire_integration.c with added scenarios (the SDK table words
set to another title's default 0x8c1add0c); built with
../revB-tools/build_integ.sh (the fixture's own compile line), run REVB=1
build/revx/test-toy-wire-integration; output in
<scratch>/t79/review/revB-x4.log: 'other title, slot its own default, stalls
150/80: TMR 0 ... visits that began a wait 3719: left RUN 3573, left WAIT 108,
off 38' against 'slot Toy's default ... TMR 2 ... left RUN 0, left WAIT 247'
(timer visits 14229). Held bus: 'hold of 600 runs, slot its own default: TMR
0; with no call of the game's: 191 SCI visits and 0 timer visits one behind
the other, then nothing due; stream off' and 'bus free again, no call of the
game's for 2000 moments: 0 events; stream off'. All requests completed with
exact data. Code: toy_wire_stream.c:53 '#define SDK_DEFAULT
UINT32_C(0x8c0be408)', :456 'word_read(slot_of(event)) != SDK_DEFAULT)
continue;', :386 'if(w->mode) timer_arm(w, w->backoff);', :432 'if(!w->mode)
wire_resume(w);'; toy_wire_ahead.inc:513 'if(r->engine.in_irq ||
ops->busy())return;'; vmu_native_reader_capture.inc:259 'if(!r ||
!r->engine.active || !r->shared.stream.toy_suite_active) return;'.
docs/fmv-stream-reader-plan.md lists 'a spare timer channel if it is to wait
without a foreground call' and generalization-map item 43 'the timer's SDK
table values per family' as needs of a title-independent reader.

Suggested:

Select the timer by the title's own admitted values, which the resident
already holds (region->vector600[KUI_NATIVE_SDK_SCI_VBR_INDEX] and
[KUI_NATIVE_SDK_SCI_DEFAULT_INDEX]), instead of Toy's two constants; as its
own change with its own console comparison. Until then, at least do not end a
mode-0 wait before its backoff has passed by the clock, and give a standing or
halted stream a foreground source that is not the game's disc calls (the
controller heartbeat). Add a fixture for another title with its own table word
(mode 0) and stalls.

## Finding 2: A raw image whose sectors carry no valid EDC: every consecutive read opens a window that fails nine times before the low reader may serve it, and nothing remembers it

- Kind: hang-or-stall; established: ran
- Where: src/loader/toy_wire_ahead.inc toy_wire_sector(), toy_wire_rewind(), toy_wire_seek(); src/loader/toy_movie_ahead.inc toy_movie_ahead() (window opened per request); made reachable by src/loader/toy_movie_iso_stage.inc toy_movie_scan() first branch

Scenario:

Another title on an image whose 2,352-byte Mode 1 sectors have correct sync,
mode and address but zero or stale EDC (a converted or patched track). The
launch accepts it: the stage checks headers and addresses only (retail_boot.h,
retail_image.c data_header), the low reader's cursor checks sync and mode only
(retail_cursor.c header_byte), no Games admission path calls
kui_sector_edc_valid. The stage calls the disc a source by its track flags.
Each read that begins where the last ended opens a window; its first sector
comes from the low reader and must pass the EDC (toy_wire_ahead.inc:438); it
fails, toy_wire_rewind counts a stall, and after TOY_WIRE_STALL_LIMIT (8) more
rounds, each a CMD12, a CMD18 and five blocks by programmed transfers in the
game's own calls, the window is marked in error and the low reader reads the
request. The next consecutive read does the same; A.failed_token covers one
request only.

Consequence:

Data stays exact, but an image that the plain reader (and Test 78 for another
title) reads in one card command becomes several times slower, with interrupts
masked for the failed rounds. Up to Test 78 only Toy Commander's verified
image could open a window. A single sector with stale EDC (a patched file)
costs one such episode each time a window reaches it; the lighter check also
switches itself off for the session when its 1-in-16 sample meets one. How
many images in use are like this is not known to me.

Evidence:

Host fixture, same private copy and log as above (revB-x4.log), twelve
consecutive 13-sector reads, another title: 'image without EDC, no source
(plain reader): windows 0 ... low reader blocks 717 (PIO 0), CMD18 1 CMD12 0,
EXEC+CHECK calls 24' against 'image without EDC, a source: windows 11, served
from windows 0, abandoned 11 ... sector checks failed 99, ... blocks by the
low reader for the window 493, low reader blocks 1210 (PIO 488), CMD18 100
CMD12 99, EXEC+CHECK calls 112'. Code: toy_wire_ahead.inc:438 'if(raw[15]!=1u
|| !kui_sector_edc_valid_at(raw,lba+150u)) { ++k->report.check_bad; return
false; }', :346 'if(++k->stalls>TOY_WIRE_STALL_LIMIT || ...)A.error=true;'.

Suggested:

Remember it: when windows are abandoned without one sector ever having passed
the check since the bind (or K times in a row), stop opening windows for the
session and say so in the report. Or have the stage call a disc a source only
after one sector of the raw track has passed the EDC.

## Finding 3: Toy Commander's executable with another IP.BIN is now given all of Toy's hooks and the owner flag, with no movie ranges: the stage's two identity tests no longer nest

- Kind: misleading-report-or-message; established: ran
- Where: src/loader/toy_movie_iso_stage.inc toy_movie_scan() (new first branch) against src/loader/vmu_native_readahead_stage.inc native_readahead_bind() lines 194-195 and 247-277

Scenario:

A disc whose executable is Toy Commander's to the byte (size 748444, CRC32,
SHA-256) but whose first sixteen sectors differ from the verified ones (a
region- or VGA-patched IP.BIN). The scan's new branch is taken because
'manifest.ip_crc32!=KUI_VMU_LAUNCH_TOY_IP_CRC': toy_suite_source=1 by the raw
track, no file looked for, toy_movie_ranges all zero. The bind then computes
'cfg.toy_poll_owner=cfg.suite_source==1u && exec_bytes==748444u &&
!memcmp(native_driver.image_sha256,toy_poll_digest,...)', which holds, and
installs the exit, chunk, refill, present and render hooks. Up to Test 78
source==1 could only come from the path that had checked the IP and found the
four files, so this disc had toy_poll_owner 0, no hook and no window.

Consequence:

A state no earlier build had: hooks in the player, owner_verified 1 and the
timer, but movie_mask 0, so no request is ever a movie's, the low reader's
movie policy is never on, and every window is an 'other' one. The stage prints
SCI MOVIE READ AHEAD READY and the report says TOY 1, which
toy_suite_report_save.inc explains as 'the verified Toy Commander, with its
hooks and its movie policy'. It contradicts the stated rule that the hooks
stay Toy's 'by the same verification as before'. I found no wrong index or
memory access in that state; the executable is the verified one, so the hooks'
addresses are right.

Evidence:

Scan on the host (stage text with stubs): in <scratch>/t79/review/revB, cc ...
-DKUI_TOY_WIRE_TITLES=1 ../revB-tools/revb_titles_mismatch.c -o
build/revs/mismatch1 && build/revs/mismatch1 prints 'same executable, other
IP.BIN (TITLES=1): source 1, card acquired 0, movie ranges known 0; the bind's
owner test (source==1 && exec_bytes==748444, digest equal) gives 1'; without
the define: 'source 0 ... gives 0'. The bind itself is read, not run
(console-only code).

Suggested:

Decide Toy-ness once. Either send any disc whose executable matches (boot_crc
and exec_bytes) down the old path whatever its IP, where it fails closed as
before, or make toy_poll_owner also require what the old path established (the
ranges found). tests/test_toy_titles_stage.c has no case with Toy's executable
and another IP; the integration fixture has none with owner 1 and no ranges.

## Finding 4: A report taken while a request is still pending leaves the stream running, and Start can then never restart the console although the page says to try again

- Kind: misleading-report-or-message; established: read
- Where: src/loader/vmu_native_passive.c toy_suite_finish_gesture(), report(), toy_report_pause(); src/loader/retail_async.c kui_retail_native_async_loan_begin(); src/loader/native_return_hardware.c kui_native_return_quiesce()

Scenario:

The gesture is held and for sixty more controller frames a request is pending
or active at each look (a long load in one request, or a title that has a
request out at every frame's controller completion). toy_suite_finish_gesture
then fails with 0x4f anyway. toy_suite_save_report calls the lease;
kui_retail_native_async_loan_begin returns BUSY at 's->pending || s->executing
|| e.active' before it reaches the QUIESCE visit, and quiesce() refuses a
pending request in any case. Nothing ends the reception. The pages are shown;
Start calls kui_native_return_quiesce, which returns false while CHCR1 bit 0
is set. A stream in a run, or the low reader's own block reception, leaves
that bit set for good: interrupts are masked for ever and no visit comes.

Consequence:

The footer shows RESTART BUSY / TRY START AGAIN on every page from then on,
and trying again cannot work; the console has to be switched off. The new
comment in toy_suite_save_report ('The resident's loan is what ends a card
stream still running') and the header's 'a card stream still running is ended
first' hold only when the loan is not BUSY. The same hole exists with a report
file since earlier tests; Test 79 makes it reachable from any title's loading
screen.

Evidence:

retail_async.c:1113-1119 'if(!s->initialized || s->pending || s->executing ||
e.active || e.in_irq || e.release.depth ||
!native_readahead_visit(KUI_NATIVE_RA_QUIESCE,0u) || kui_sci_stream_busy()) {
kui_retail_hook_active=0u;return KUI_VMU_NATIVE_ASYNC_LOAN_BUSY; }';
native_readahead_aux.c:819 'if(r->engine.active || r->engine.in_irq ||
r->shared.service.pending || r->shared.service.executing)return 0u;';
native_return_hardware.c:93-94 'if(!irq_masked() || controller_fault ||
!wait_clear(MAPLE_STATE,1u) || (read_hw(UINT32_C(0xffa0001c))&1u) ...)return
false;'; vmu_native_passive.c:1017
'++held_report>=HELD_REPORT_FRAMES+60u)fail(0x4fu);'.

Suggested:

In the terminal report nothing of the game runs again, so a pending request
need not be protected: when the loan answers BUSY, end the reception directly
(a terminal quiesce that drops the request, or stop channel 1 in the Start
path when its source is the SCI's receive register). Failing that, show a
message that says to switch the console off.

## Finding 5: NO RAW SCI IMAGE / PLAIN READER is shown for Toy Commander on a raw SCI image whenever its own file scan fails

- Kind: minor; established: read
- Where: src/loader/vmu_native_readahead_stage.inc native_readahead_bind() lines 306-307; src/loader/toy_movie_iso_stage.inc toy_movie_scan() old path

Scenario:

The verified Toy Commander disc on a raw SCI image, where the old path ends
with toy_suite_source 0: the card could not be acquired for the scan, a movie
file was not found, or the stop after the scan failed. toy_poll_owner is 0 and
suite_source is not 1, so the banner's last text is chosen.

Consequence:

The screen says the image is not a raw SCI image when it is; the real reason
(the title's files were not found) is not shown. And in that case the verified
title gets no window at all while any unknown title on the same kind of image
gets one.

Evidence:

vmu_native_readahead_stage.inc:306
'retail_display_line(cfg.toy_poll_owner?"SCI MOVIE READ AHEAD READY":
cfg.suite_source==1u?"SCI STREAM READ AHEAD READY":"NO RAW SCI IMAGE / PLAIN
READER");'; toy_movie_iso_stage.inc:149 '...
kui_retail_storage_acquire(&card)!=KUI_LOADER_SD_OK)return;' and :169-174
'if(!ok || stopped_stream!=KUI_LOADER_SD_OK) { ... toy_suite_source=0u; }'.
tests/test_toy_titles_stage.c itself has this case ('assert(!toy_suite_source
&& acquired == 1u)').

Suggested:

Give that case its own text, or let the verified title fall back to the
raw-track rule when its scan fails.

## Finding 6: The tests do not cover what Test 79 adds outside the auxiliary's one line: the other title in the fixture has Toy's timer, the screen splitter's rules are not pinned, and the worker and stage changes are under no test or mutation

- Kind: test-gap; established: ran
- Where: tests/test_toy_wire_integration.c titles_other(); tests/test_toy_wire_report.c screen(); tests/test_toy_wire_stream_mutations.py TITLES_MUTATIONS

Scenario:

(1) titles_other() binds another title but leaves open_card()'s SDK table
words at Toy's 0x8c0be408 and sets no stalls, so its stream selects TMU2 and
never waits: the mode every real other title runs in is not exercised. (2)
screen() compares the shown characters with spaces removed and only counts
cuts; it never checks that a cut falls on a space, has no line of 51 places
and none without a space, and the page loop toy_report_pages is not compiled
on the host. (3) The mutation list with the Test 79 switches has 244 entries
in five files (stream, two window files, the auxiliary, the stage scan); none
in toy_suite_report_save.inc, vmu_native_passive.c,
vmu_native_heap_probe_stage.c or vmu_native_readahead_stage.inc, so the VMU-on
stop, the wait and loan without a report file, the pages and the banner can be
broken unnoticed.

Consequence:

The host evidence for 'another title' says less than its printed line
suggests, and a defect in the console-only code would first show on the
console.

Evidence:

(1) revB-x4.log: 'REVB the shipped titles_other(true) as it stands: TMR 2 (0
none, 1 TMU1, 2 TMU2), timer visits 0, cuts 0, waits 0'. (2) In a scratch tree
<scratch>/t79/review/revB-mut, tests/test_toy_wire_report.c built with the
Test 79 defines still prints its PASS line and exits 0 with each of these
planted in toy_report_screen_line: the line 'if(at+n<end && at[n]!=\'\\n\' &&
space)n=space;' removed (rows cut in the middle of a value);
'n<TOY_PAGE_PLACES' made 'n<=TOY_PAGE_PLACES' (51 places, row[] written one
past its end for such a line). It does catch the space behind a cut not being
skipped and padding taken for text. With 'if(!config.toy_report.test || ...'
put back in toy_suite_save_report it also passes. (3) Importing the mutation
script with the switches set: '244 mutations; {toy_wire_stream.c: 77,
toy_wire_ahead.inc: 76, toy_movie_ahead.inc: 68, native_readahead_aux.c: 14,
toy_movie_iso_stage.inc: 9}', none stale.

Suggested:

Give titles_other() another title's table word and a stalled bus as well;
assert in screen() that a cut line is followed by a space and add a 51-place
and a spaceless line; run the page loop on the host (it builds against three
stubs: <scratch>/t79/review/revB-tools/revb_pages.c does it with the worker's
own function text); list the worker and stage changes as console-only in the
mutation notes or add mutants for them.

## Examined and found sound

- Whole-reader host fixture with the switch on, unmodified tree copy: 'PASS Test
  79 reader integration: 284235801 checks' (host run, not console);
  toy-wire-host-tests with TITLES=1 all pass, including 'PASS Test 79 report'
  and the stage scan test in both modes.
- suite_demand, toy_movie_ahead.inc and toy_wire_ahead.inc with variant 0 and
  movie_mask 0 (read on the console build's effective source): every [variant-1]
  index is behind 'variant &&' or 'if(variant)'; AHEAD_OTHER reaches only
  other_windows/other_sectors; movie_variant, playback_delivery and
  movie_complete return at once; toy_suite_active and toy_suite_poll stay 0, so
  DEMAND_GD_ENTER/LEAVE and the CHECK shortcut never fire.
- Dispatcher events for a title without hooks: MOVIE_AHEAD, the update, present,
  exit, chunk and refill handlers are entered only from Toy's hooks, which the
  stage installs only under cfg.toy_poll_owner (vmu_native_readahead_stage.inc
  247-277); TICK only counts and is not sent in this build; QUIESCE, RESET,
  RESUME, CAPTURE_START and MOVIE_COMPLETE have no title rule.
- No fixed address of the game is written for another title: word_publish, the
  IPRA and TMU writes are all behind w->mode. The only fixed-address reads are
  timer_select's two table words, and only when VBR is 0x8c00f400. toy_poll_word
  and toy_chunk_word are behind toy_poll_owner or the hooks.
- An SCI error interrupt is always answered or the stream halted in the same or
  the next entry (plain path, whole path, fallback, the publish-failed case): no
  path leaves ORER asserted with the stream on, and WAIT, FULL and halted states
  have the receiver's interrupt off.
- A polled request cannot wait for ever in mode 0: each EXEC/CHECK pumps with
  demand; STAY gives the window up after 195,312 ticks without growth (at once
  if the clock is not the expected one); nine failed rounds mark the window in
  error and the low reader reads the request. My mode-0 host runs with four
  stall densities all completed with exact data.
- Data path for another title is Test 78's: sectors are published only after the
  EDC or the lighter check on CRC16-clean blocks with a consecutive address;
  slices, restart on give-up and kui_retail_gd_progress are unchanged; a request
  served in an interrupt visit is reported by the game's next call.
- No-file report path: toy_suite_report_write returns -1 at '!seed->test' before
  any bus byte; kui_sci_sd_acquire clocks nothing; the only card traffic is the
  CMD12 that ends an open CMD18. The loan then ends as a fault (QUARANTINED,
  sd.ready false, hook_fault 1), which the terminal page does not need;
  kui_wire_stop leaves CHCR1 with bit 0 clear, and kui_native_return_quiesce
  looks at hardware only, so Start reboots (except the BUSY case reported).
- c->sd_transaction is published whether or not a report file exists
  (vmu_native_passive_stage.inc 592), so the lease admits the worker's callback
  in a normal launch.
- toy_report_pages and toy_report_screen_line: every read is bounded by end; a
  page has at most 12 text lines plus its own two (the display has 16 rows from
  108 to 438); no line over 50 places; no page without text; no spin for any
  text the formatter writes (it always begins with the heading); row[51] is
  never overrun; the text at issued_chain[0..1023] is clear of the controller
  workspace at +2048. Ran the worker's own function text on the host against
  formatted and hand-made texts: 4 pages a pass, most 14 lines, 0 wrapped rows,
  0 over 50, 0 empty pages.
- native_runtime_mode's new stop: first reached in native_persist_storage_init
  before kui_retail_storage_init, so the card is not opened and nothing of the
  game is read; later calls cannot stop anew; in toy_movie_scan the mode is
  evaluated before the card is acquired, so a stop there could not strand the
  card.
- toy_movie_scan for the verified Toy Commander disc: the new branch is not
  taken and the old path runs unchanged. toy_suite_raw_track refuses audio,
  2,448-byte, Mode 2, 2,336-byte and cooked tracks; each of the nine stage
  mutants is met by a case of tests/test_toy_titles_stage.c (checked by reading
  the cases).
- TOY_WIRE_TITLES=0: preprocessed sources (sh-elf-gcc -E -P with each object's
  own flags, minus the switch) are identical to 1d0e5ba for the auxiliary,
  toy_wire_stream.c, the stage and the four resident files; the worker differs
  only in two constant expressions ('| 0 << 10' in PT and '0?79u:' in the test
  number) that fold to the same values. The resident objects get no TOY_WIRE
  flag at all. Makefile: with TITLES=0 the test number stays 78 and no flag or
  config line changes.
- The new report rows: TOY is ra->demand.owner_verified, SD's values are the low
  reader's own stats and w->declined; the host report test asserts both rows'
  text and the PT bit.
- Mutation list with the Test 79 switches: 244 entries, none stale (count check
  only; I did not run the list).

## Not settled

- How the timerless stream behaves on a console: the host model lands a run
  whole and has no bus timing, so the length of the interrupt bursts, the CPU
  they take from a game and the read-ahead lost while a stream stands cannot be
  measured here.
- Whether another title's TMU0 is in the setting the reader's clock expects
  (FRQCR 0x0e0a, reload 0xffffffff, Pphi/64, running). If not, toy_wire_stay
  never holds and every request the game waits for gives its window up at its
  second call unless the window already has it; seeks take one block and drains
  four blocks a visit.
- Titles that send GD commands other than reads regularly (GETSCD, REQ_STAT,
  PLAY, SEEK): each gives an open window up with a CMD12, and the next
  consecutive read opens another. The report's C column will show it; I do not
  know which titles of the family do this.
- A title that rewrites the SDK's interrupt table or VBR while a stream is in a
  run between requests: the resident's callback would not be entered and the
  overrun would stay asserted at level 1. The low reader has the same exposure
  only while a request is active; the window's stream runs on after the last
  read.
- How common raw images without valid EDC (or with patched sectors) are among
  the images people launch.
- A bus held with no transfer at all for 255 runs inside one unfinished block
  fills the lost-position log and leaves the stream in FULL with nothing to
  release it; by reading, a request the game waits for ends that after STAY's
  quarter second. I did not reproduce this exact case (my held-bus run ended in
  a lost frame after 191 visits instead).
- Test 77's known limit (a filled window's stream stands, with the card inside
  its command, the SCI level and channel 1's addresses, until the game reads
  elsewhere) is still open and now applies to other titles; the plan records it
  as owed.
- How the GUI routes and admits this module at the Background reader's path for
  a normal launch, and what it does with the VMU setting before the stage: not
  in the diff and not examined.
- Binary identity of a Test 78 build before and after the change: I compared
  preprocessed sources only; the running verification job is the one that
  compares binaries.
