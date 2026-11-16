# Test 75: independent review of the three parts

Three reviewers, one for each part, read the source at commit `98f6a74`
(before the last code commit) under Test 75's switches. Each was asked
for concrete failure cases only: a sequence of states and events and the
wrong outcome it leads to. They read; they did not build or run anything,
and none of this is a console result. What follows is their answers as
returned, unedited but for line wrapping. The author's dispositions are
in `docs/toy-wire-stream-test-75.md`, Review.

## STAY

Findings: 0.

Examined and found handled:

- Scope read: toy_movie_ahead() selected expression and below,
  toy_wire_stay(),
  toy_ahead_pump()/toy_wire_drain()/toy_wire_seek()/toy_wire_take_over()/toy_wire_halt()/toy_wire_rewind(),
  toy_ahead_clear()/toy_ahead_fallback()/toy_ahead_publish(), suite_demand()
  and the dispatcher in native_readahead_aux.c,
  kui_retail_async_call()/deliver()/deliver_work()/hook()/after-work in
  retail_async.c, kui_sci_stream_fetch()/busy() in sci_stream.c,
  kui_wire_service()/kui_wire_stop()/kui_wire_start()/wire_arm() in
  toy_wire_stream.c, toy_resume_deliver(). Nothing was built or run.
- What the resident does with 'wait' when the window is kept: the dispatcher
  returns 1 for every DELIVER, so deliver() never calls deliver_work()
  itself; the auxiliary decides. A kept request returns true from
  toy_movie_ahead, suite_demand returns, the resident only runs report().
  e.since is reset to 0 after every EXEC/CHECK and the window never
  increments it, so every further unclassified call again carries wait!=0
  and re-evaluates the watchdog; publish sets e.since=e.target and
  e.active=0.
- A waited visit is always foreground: wait!=0 is produced only in
  kui_retail_async_call (in_irq==0). The SCI interrupt, the demand wake
  (in_irq==2) and the REQUEST after-work all call deliver(0). So
  toy_wire_seek's in_irq refusal and toy_ahead_clear's in_irq refusal cannot
  occur on a waited visit or on the watchdog's drop.
- Stream off and low reader busy: not reachable inside a window.
  toy_ahead_begin is refused while ops->busy(); toy_wire_seek only fetches
  with polled=true, which receives by programmed transfers and never leaves
  state DMA; with the wire stream on, state is RA_DMA but k->on routes to
  drain, not seek. After any halt the state is RA_LOST, which the next
  foreground seek ends with CMD12 and reopens.
- Seek cannot stand still on a parked RESUME block: toy_wire_seek clears
  RESUME_ACTIVE, so resume_admitted() is false and kui_sci_stream_fetch
  falls through to stop_card and CMD18 instead of returning PENDING for
  good. A token search left PENDING (TOKEN_WAIT) is continued by the next
  visit's fetch.
- Every waited visit is bounded: drain stops after 1172 ticks (or 4 blocks
  without a clock), seek after 64 steps or TOY_REFILL_TICKS, kui_wire_stop
  waits at most one segment. No loop spins on PENDING.
- Window full: a selected request has count<=AHEAD_REQUEST_MAX (60) and
  toy_wire_room() holds until 61 sectors are verified, so a request at
  A.first that is not yet servable always leaves room; a cursor at its end
  means A.verified is the range end, which covers the request (it is then
  'warm').
- A.error: the STAY term requires !A.error, so a window in error is dropped
  at once with the reason ERROR; an error raised by the pump in the same
  visit sets A.failed_token and goes through toy_ahead_fallback to the low
  path in that same call.
- Request not at A.first, other variant, not in play, executing, partly
  completed, or cursor not pristine: the earlier terms or the STAY term are
  false, selected is false and the window is dropped exactly as before
  (toy_wire_stay is not even called because of short-circuit).
- Clock invalid: toy_wire_stay returns false before touching stay_token, the
  window is dropped (reason WAITING) on the first such call, as before the
  change.
- Watchdog arithmetic: clock_read().valid requires TCOR0==0xffffffff,
  prescaler Pphi/64 and the timer running, so stay_tick-now is correct
  modulo 2^32 across the 91 minute wrap; 195312 ticks at 781250 Hz is 0.25
  s.
- Token reuse: retail_gd.c increments s->token for every accepted request
  and wraps only at 0x7fffffff; the service state is zeroed only at init.
  stay_token starts 0 (toy_wire_reset(true) at auxiliary init) and selected
  requires s->token!=0. A stale equal token could only make the watchdog
  expire at once, which is the old behaviour.
- stay_token/stay_tick never reset: harmless. One request is pending at a
  time, so the token compared cannot alternate and restart the watch; once
  expired for a token it stays expired; a later waited call for the same
  token long after its first (for example after classified polls, a loan or
  a pause) only drops at once, as before.
- Watchdog never firing: the STAY term is evaluated on every waited visit
  unless kick (cleared by the resident after one EXEC) or warm (then the
  request is published in that visit). kick cannot repeat within a request
  because only REQUEST sets toy_suite_kick.
- After expiry no window is reopened for the request: the drop sets
  A.failed_token=s->token, selected is false, and the !A.variant branch
  returns false for that token on every later visit including interrupt
  visits; the token is not reused.
- Consistency of the drop after expiry: toy_ahead_clear halts the stream
  (state RA_LOST, k->on=0), then because A.receiving is set (k->on or
  state>RA_PAUSED at the last pump) runs ops->stop() (CMD12) in the
  foreground, then A.variant=0. The game's cursor was never touched (publish
  writes only when the whole request is verified). suite_demand then goes on
  to toy_resume_deliver(wait) and ops->deliver(wait). This is the same path,
  in the same possible stream states (seeking, token wait, running, waiting,
  full), that the old drop at the first waited call already took.
- CMD12 failing on that drop: toy_ahead_fallback marks the request failed
  with an I/O error instead of leaving it pending. Unchanged from the old
  drop with a stream running; STAY adds no new way into it.
- Resume staging against the stream's memory: toy_resume_deliver stages into
  the arena from its start only when toy_movie_ahead returned false, which
  always follows toy_ahead_clear (stream halted) or no window at all.
  stay_token/stay_tick live in the kept area at TOY_WIRE_KEEP_OFFSET, above
  the 64 sector staging and below the ring (static asserts hold with the two
  added words).
- Publishing wrong sectors: for a demand the drop test still requires
  variant==A.variant and s->lba==A.first after the STAY term,
  toy_ahead_publish requires s->count<=A.verified-A.first, and A.first
  changes only in publish. toy_wire_rewind restarts the cursor at A.verified
  without moving A.first. Sectors between A.first and A.verified have passed
  the address check. The kept path is the same publish path classified polls
  already use.
- Stream progress with interrupts held off for the whole wait: each waited
  visit calls toy_wire_drain, which calls kui_wire_service before every
  block and once after; that polls ORER, the channel end and the wait
  timer's underflow, and restarts a stream standing still for room. w->want
  is applied only when the next run is armed, so repeated waited visits do
  not cut runs.
- Take-over declined (SDK route not armed at 0x10, channel 1 busy, bus not
  in command framing): the seek loop keeps reading by programmed transfers
  for its time budget on each waited call, so the request still advances and
  the watchdog still bounds it.
- QUICK and IDLE: the quick path requires arg==0 and no active request, so a
  waited visit always takes the whole path; the idle rule is bypassed by
  demand.
- LATER: only an interrupt visit with no request waiting leaves the reading;
  foreground waited visits always drain.
- Worst case the change adds, by design and bounded: the watch runs from the
  request's first waited call, not from the last progress. A waited request
  the stream cannot finish in 0.25 s (a large request on a window just
  opened under a busy bus, or take-over declined so that only programmed
  transfers fill it) is dropped with the sectors verified so far discarded
  and then read from its first sector by the low path, so it completes about
  0.25 s later than before STAY. It does not hang and no data is wrong; each
  request token can pay this once.

## LATER

Findings: 2.

### LATER finding 1: LATER leaves a WAIT that has no timer (mode 0) standing until the game's next foreground call

Where: `src/loader/toy_wire_ahead.inc`, line 818 (of commit `98f6a74`). Confidence: certain.

Scenario: Stream started with no free TMU (timer_select leaves w->mode==0,
toy_wire_stream.c:449-463). A run is cut after fewer than 128 bytes. The SCI
error interrupt visit (in_irq==1, no request waiting) reaches toy_ahead_pump
-> toy_wire_later (toy_wire_ahead.inc:816-820). Its single toy_wire_serve ->
kui_wire_service(RUN) -> wire_stopped sets state=KUI_WIRE_WAIT and arms no
timer because mode is 0 (toy_wire_stream.c:382-386). toy_wire_later then
returns true because it accepts state==KUI_WIRE_WAIT without looking at
w->mode, and the visit leaves. In WAIT the SCI is back in command framing
with the card deselected, so no further SCI interrupt comes, and there is no
timer. The only thing that ends a mode-0 wait is another kui_wire_service
call (toy_wire_stream.c:432, `if(!w->mode) wire_resume(w)`). Before LATER
the same visit always made that call: toy_wire_drain serves at the top of
every loop pass and once more at its end (toy_wire_ahead.inc:513 and 572),
so the wait ended inside the visit that began it, whether or not the window
had room.

Consequence: Lost progress, not a hang. After every short-run cut in mode 0
the stream stands still with interrupts idle until a foreground call
arrives: an update or idle GD call that toy_wire_foreground_due passes on
because kui_wire_overdue is true for WAIT with !mode, or a request's demand
pump. If the game makes no call for a while, nothing is received and the
window is not filled, where Test 74 kept receiving on interrupts alone. A
request that arrives later restarts the stream through its own foreground
visit, so data stays correct. This is the fallback path only: every console
report so far shows TMR 1 (reports 66-2 through 66-8).

### LATER finding 2: LATER drops the same-visit timer poll and IPRA repair, so a WAIT whose timer interrupt cannot be delivered stands until a foreground call

Where: `src/loader/toy_wire_ahead.inc`, line 816 (of commit `98f6a74`). Confidence: possible.

Scenario: Mode 1 or 2, and the SDK has rewritten IPRA so the stream timer's
level nibble is 0 (the case kui_wire_service guards against at
toy_wire_stream.c:434-436). An SCI interrupt visit with no request waiting
calls toy_wire_later. Its one serve enters WAIT and timer_arm starts the
one-shot (toy_wire_stream.c:382-386); the visit returns because
state==KUI_WIRE_WAIT. The timer underflows but its interrupt is masked at
level 0, so toy_wire_timer_visit never runs. Before LATER the same visit
served again after the first block and at the end of toy_wire_drain
(toy_wire_ahead.inc:513, 572): that second call either saw the underflow
flag and resumed (toy_wire_stream.c:433) or restored the IPRA level
(434-436). With LATER the IPRA repair branch is reached only when a draining
visit (foreground, waiting request, or ring/log past the marks) happens to
call service during a WAIT before its timer has run out. A foreground call
that comes after the underflow takes the kui_wire_timer branch and does not
repair IPRA, so the next wait is stuck the same way.

Consequence: Same outcome as the mode-0 case while the level stays cleared:
each wait lasts until the next foreground call that toy_wire_foreground_due
passes on (kui_wire_overdue sees the underflow flag) instead of its 82 us to
1.3 ms backoff, and with no foreground call the stream stands still. Lost
receive progress only; no wrong data and no permanent hang as long as the
game calls again. Whether the SDK really clears that IPRA nibble during a
movie is not established from the code or the console reports; the code's
own comments assume it may.

Examined and found handled:

- FULL at a leaving visit: not possible. FULL is only set by wire_arm, which
  runs inside the visit's own serve, and toy_wire_later tests the state
  after that serve, so the visit that produces FULL drains at once with 1172
  ticks.
- floor far behind tail: floor trails tail by at most about FRAME+2 (515
  bytes) in both SEEK (gap_hole) and DATA (alternative), so head-floor <=
  unread+516. A visit can leave at just under 16384 unread and the next full
  run (8194) can push used past wire_arm's 24574 limit, but that only yields
  a FULL seen and drained by the very next interrupt visit (one or two
  blocks' parse time, no extra lost byte).
- Timer visit turning WAIT into FULL after a leaving visit: a left WAIT has
  head-floor < 16900 and fewer than 128 log entries, so the timer's wire_arm
  always admits the run (barring E_ARM, which is unchanged).
- tail ahead of head: during a run the reader may have parsed past w->head
  (head moves only in wire_account). head-tail then wraps to a huge value
  and the visit drains, so it errs towards reading, never towards leaving.
- Hole count: hole_put-hole_get only over-counts (hole_get advances lazily
  at the start of kui_wire_next), which makes a visit drain sooner. Each
  interrupt visit adds at most one entry and timer visits add none, so the
  count cannot jump from below 128 to wire_arm's 255 limit without a
  draining visit in between.
- Hole log full inside a block that has not fully landed (255 cuts within
  one frame gives FULL with kui_wire_next returning NEED forever): reachable
  in principle but identical before LATER, since draining every visit also
  returned NEED. Not caused by this change.
- FAULT: toy_wire_later returns false for FAULT, and toy_wire_drain then
  rewinds in the same visit as before.
- w->want: toy_ahead_pump writes w->want (0 when no request waits) before
  toy_wire_later serves, so a leaving visit never arms a run with a stale
  want and never clears one a waiting request needs, because a waiting
  request never takes the LATER branch. Want left set after a late publish
  is as before LATER and costs at most one early cut.
- demand implies waiting at the pump: toy_movie_ahead only reaches
  toy_ahead_pump(r,demand) after toy_ahead_publish failed, so an interrupt
  visit with an active selected request always drains for 1172 ticks and
  then publishes, exactly as in Test 74.
- A request that starts waiting just after a visit left: its foreground
  DELIVER visit pumps with demand (sets want, drains, serves the stream).
  After that every game GD call with the request active pumps, and every SCI
  interrupt visit sees waiting and drains. want at or behind head does not
  shorten a run. Bounded by the game's next GD call or the run in flight (at
  most 5 ms), with no stuck state.
- Accounting after a leaving visit: toy_wire_later sits inside the if(k->on)
  block only. producer_blocks, A.receiving, receive_token and
  toy_wire_visit_time run as before; update_tick and clock_good are
  foreground-only as before; budget_ends is a report counter only.
- Window without room: before LATER the drain served, broke at
  toy_wire_room() and served again; a leaving visit does the same single
  service. The resulting standstill (ring FULL until a publish frees slots
  and a foreground pump restarts it) is unchanged.
- Game makes no foreground call, timer present and deliverable: runs end in
  an overrun at most 8192 bytes apart, so SCI visits keep coming; waits are
  ended by the timer visit; once 16384 bytes are unread or 128 entries are
  logged the interrupt visits read with the 1172-tick budget, so the window
  still fills from interrupts alone.
- Deferred parse decisions (first-token bit order, CLOSE/JUMP/FRAME/SECTOR
  stops): they only change how ring bytes are interpreted or end the stream.
  Deferring them wastes at most a ring's worth of reception, as a slow
  reader already could, and hands no different data to the game.
- Ring overwrite: wire_arm's admission still uses head-floor, so bytes left
  unread by leaving visits are never written over.

## RING

Findings: 0.

Examined and found handled:

- Scope of evidence: read the source at 98f6a74 under the Test 75 switches
  and disassembled the already-linked build/dev75/native-ra-aux.elf
  (kui_wire_fresh at 0x8c425bd0, its call site inside kui_wire_next at
  0x8c425f54..0x8c425ffc). Nothing was built or run; I did not confirm that
  this ELF is the delivered package's module.
- Central claim holds: the only cached reads of the ring are the word loops
  fed by kui_wire_fresh (toy_wire_stream.c:752-758), and every such call
  drops all lines of [view, view+4*words) immediately before the read. Every
  byte of a run is below `landed`, which kui_wire_next takes once (line 912)
  from DAR1 followed by the uncached fence, so each byte was in memory
  before its line was dropped.
- Later-landing bytes in a line fetched earlier: a later run reads them
  either after its own drop (>= 8 words) or through the uncached pointer (<
  8 words). No path reads the view without a drop in the same call.
- Both w->msb branches receive the same `fresh` pointer. Linked code
  confirms it: r4 = kui_wire_fresh's return value for kui_wire_words_true
  (0x8c425f6c..f88) and for kui_wire_words_lsb (0x8c425fec).
- Pieces split at holes (wire_block:827-830): each sub-run calls
  kui_wire_fresh separately, the hole byte is never read, and the line
  shared by the two sub-runs is dropped again or read uncached.
- Data/CRC split: the two CRC bytes are run with out==0, so `if(out &&
  piece>=4)` is false and they are read through uncached p (line 789).
- Leading unaligned bytes (line 744) and the trailing 0-3 bytes (line 789)
  use uncached p; p is advanced from the uncached pointer, never from the
  view.
- Ring wrap: wire_run cuts a piece at KUI_WIRE_RING_BYTES - index and the
  next piece starts at index 0 with its own kui_wire_fresh call; end never
  exceeds the ring's last line.
- Second lap over the same memory: stale clean lines from the previous lap
  are dropped before any cached read, and uncached reads bypass them.
- Token search, wire_gap_like, fence() and the alternative-reading re-parse
  read through w->ring (P2) only; the re-parsed block is again below
  `landed` and dropped again.
- Held byte: wire_account writes w->ring[at & MASK] through P2 before head
  advances, and only from kui_wire_service, which is never called inside
  kui_wire_next. Its position is >= any `landed` an earlier parse used, so
  it is consumed only after a later drop or uncached.
- No dirty line can be discarded by OCBI: nothing writes the ring through
  P1. The only CPU write is the P2 held byte; kui_wire_start's purge uses
  OCBP on lines that are now at most clean.
- Stale clean ring lines can never be written back over DMA or held bytes:
  lines read through the view never get the U bit, so eviction drops them
  silently.
- Assembly loops (toy_wire_words.inc): exactly one `mov.l @r4+,r8` per `dt
  r5` iteration, no prefetch and no look-ahead; they read exactly `words`
  words, all inside the dropped range.
- kui_wire_fresh arithmetic, source and linked code: line0 = view & ~31,
  count = (end - line0 + 31) >> 5, one OCBI per line, returns view; words <
  8 returns the uncached p unchanged. A 32-byte run straddling two lines
  drops both.
- Alignment at the ring's start and end: C.config.data is 32-byte aligned
  (config_valid), TOY_WIRE_RING_OFFSET % 32 == 0 (static assert), and the
  ring is 1024 whole lines ending exactly at the lease end (0x47000). The
  first and last drops never touch a line shared with toy_wire_keep, the
  tables or memory beyond the lease.
- View address: w->ring is P2 (config_valid requires data ==
  (p|0xa0000000)+DATA_OFFSET, passed uncast at toy_wire_ahead.inc:415);
  wire_view maps it to the same physical line in P1, and p - ring <= 32767
  cannot overflow.
- Other users of the arena through the cached alias toy_resume_data():
  window slots (< 62*2352 = 145824), stream state and tables
  (145824..150528), toy_wire_keep (150528, static-asserted to end before the
  ring), request staging (<= 64*2352 = 150528). None reaches the ring at
  0x25000..0x2d000.
- Arena users that would overlap the ring are not compiled in mode 66:
  toy_edc_table (WIDE_EDC is mode 52 only), the kui_native_ra_cache producer
  and consumer (!ASYNC_DEMAND or modes 26-28), seed windows (modes 2, 6,
  35).
- Other cache-control instructions in the linked aux: only OCBP in
  purge/word_publish and an OCBI loop in kui_cd_edc over its own EDC table.
  The other objdump hits (0x8c422d28, 0x8c422e5a, 0x8c42304e) are
  literal-pool words, not instructions.
- SH-4 OCBI on a P1 address with MMU off and CCR=0x105 (OCE=1, CB=1, OIX=0,
  ORA=0), from the SH7750 manual as I recall it: invalidates on a tag hit
  without write-back, no-op on a miss, no TLB exception possible. If the
  operand cache were ever off, the P1 read would go to memory and still be
  correct.
- 16 KiB direct-mapped operand cache versus the 32 KiB ring: ring addresses
  16 KiB apart share an entry. OCBI on X only hits X's own tag, and a read
  of X then misses and refetches, so aliasing cannot return the other line's
  bytes.
- Index conflicts between a ring line and the tables, w->block or the stack
  inside the word loop cause refetches and write-backs of the other (dirty)
  line. The ring refetch is fresher memory, so this costs speed only, not
  correctness; the cost is for the console's P RING timer to show.
- A line fill overlapping an active DMA transfer: bus cycles are serialized
  and the bytes concerned are beyond `landed`, so they are not consumed from
  that fetch.
- Stream stop and restart: clean ring lines left in the cache are
  invalidated by kui_wire_start's OCBP purge, and would be dropped before
  any cached read regardless.
- Interrupt between drop and read (if a caller ever ran unmasked): harmless,
  because all of the run's bytes were in memory before the drop and a nested
  account writes only positions >= `landed`.
