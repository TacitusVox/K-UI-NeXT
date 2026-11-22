# Test 83: the review's record

Four readers beside the author on 20 November 2026, each with one question, on the
change as it stood while they read (`55ed2f8`, then `3059447`; the fourth read the
delivered build, `3bd4bed`, and its result came in after the delivery). Their findings
and their checks are given as they wrote them. None of it is a console result.

## The card's safety

### A slot sector the card can no longer read turns the survey off at every later launch: a cut-off write is not always 'passed over' (minor)

Where: `src/core/toy_survey.c:144`

Scenario:
  State reviewed: HEAD 3059447 (the change was committed and revised while I
  reviewed; the sources I tested hash-match that commit). Card: a valid
  KUI/SURVEY/STREAM.TXT with slots 0..k-1 used. Launch k: toy_survey_look
  claims slot k and toy_survey_write (src/loader/toy_survey_stage.inc:163)
  sends CMD24 for the slot's part 0. Power is cut while the card programs that
  block, and the card afterwards answers a read of the sector with a data
  error token instead of old data, new data or garbage. Every later launch:
  kui_toy_survey_claim_find walks slots 0..k; claim_read of slot k part 0
  fails (KUI_LOADER_SD_TOKEN from the one-block CMD18; the stream is stopped
  normally, the card stays ready); line 145 returns KUI_TOY_SURVEY_CLAIM_IO;
  the stage shows "SURVEY OFF / FILE CHECK 00000001". Reproduced with the
  production sources against a byte-level SD card model:
  <scratch>/card-safety-review/e2e2.c and e2e2.log (power cut at each of 7,064
  wire bytes; in the 41 positions where the card was programming, the next
  launch is "SURVEY OFF / FILE CHECK 00000001 (card kept)"). The same holds
  for any sector of an earlier slot that becomes unreadable.

Consequence:
  No report can be saved to that file again although every later slot is
  blank, and the line names no remedy (FILE FULL does). Nothing is written and
  the launch goes on, so the card is not harmed. But
  include/kui/toy_survey.h:76-77 ("a write that was cut off" is passed over)
  and toy_survey_stage.inc:160-161 ("A write cut off leaves a slot that is no
  longer blank, which later launches pass over") hold only while the sector
  still reads back; I confirmed they do hold for a sector that reads back
  blank, as the record, or as garbage.

Suggested:
  Treat a failed read of a slot's part as 'not blank: pass this slot over'
  while the card is still ready (keep the stop for a card left not ready), or
  keep the refusal and say what to do on the screen (e.g. FILE CHECK ... COPY
  A BLANK ONE); correct the two comments either way.

### With a damaged FAT or directory entry that leads the file's chain into another copy of the same blank file, the launch record and the report are written into that copy (minor)

Where: `src/loader/toy_survey_stage.inc:163`

Scenario:
  This is the one way I found for the stage (and then the worker) to write a
  sector that is not the survey file's own. Card: KUI/SURVEY/STREAM.TXT plus a
  second file with the same bytes and so the same ID: a backup copy
  (0:/BACKUP/STREAM.TXT in my run), or the deleted earlier copy of the same
  master file left in free space after the 'FILE FULL - COPY A BLANK ONE'
  step. Damage: the FAT32 entry of the survey file's first cluster points at
  the copy's second cluster (on exFAT: the entry's first cluster points into
  the copy). kui_wce_probe_fat_find follows the chain (its overlap check,
  src/core/wce_probe_fat.c:211-216, sees only overlaps inside the one file);
  kui_toy_survey_claim_find reads the copy's slot 0 parts 0-2, which are byte
  for byte this ID's blank text (src/core/toy_survey.c:149); toy_survey_place
  passes; toy_survey_write puts the launch record into the copy's sector; the
  seed names the copy's parts 1 and 2, and the worker's CID and CRC-32 checks
  pass. Reproduced: <scratch>/card-safety-review/e2e3.c and e2e3.log ("wrote
  sector 3250 = slot 0 part 0 of THE COPY", seed sectors 3251 and 3252; the
  same with the copy deleted). Random damage to 600,000 cards produced this
  class only where a second copy existed (9 launch records and 21 seed sectors
  in the copy) and no other foreign write.

Consequence:
  Sectors outside the file the owner sees at KUI/SURVEY/STREAM.TXT are
  overwritten: a live duplicate gets a slot filled, or free space the damaged
  chain claims is written, and the report is then not in the file the owner
  sends back. Nothing else can be hit, because all three sectors must hold
  this ID's exact blank text for that slot and part; games, saves and
  file-system sectors cannot. The sentence 'no other sector of the card can be
  taken for it' (include/kui/toy_survey.h:15-16) is true only while the card
  holds one copy of a given ID.

Suggested:
  No code change is needed for the card's safety. State the precondition in
  the Test 83 document and the tool's help: one blank file per ID per card,
  and a fresh blank from tools/toy_survey.py (new random ID) for every refill
  rather than the same master copied again, so stale copies carry another ID
  and are passed over.

Done: Corrected in `3bd4bed` (the sector not read); the condition on copies stated in the header, the document and the README.

Checked and found in order:

- Scope: the change was committed (55ed2f8) and revised (3059447, 06:13 UTC)
  while I reviewed; everything below was re-run on HEAD 3059447
  (toy_survey_stage.inc sha256 1ff5b162..., toy_survey.c bc48adeb...,
  toy_survey.h 87fee333...). I changed nothing in the repository; my
  experiments are in <scratch>/card-safety-review/ (model.inc, e2e.c to
  e2e5.c, build.sh, *.log).
- Method: the production stage text (toy_movie_iso_stage.inc,
  toy_survey_stage.inc, toy_suite_report_stage.inc), toy_survey.c,
  toy_survey_fat.c/wce_probe_fat.c, retail_storage.c, retail_sd.c,
  sd_reader.c, sci_sd_storage.c, vmu_backing.c and the worker's
  toy_suite_report_write.inc were compiled for the host (ASan+UBSan) against a
  byte-level SPI SD card model; only the SCI port and the card are modelled. A
  host model, not a console run.
- The stage issues exactly one write command, CMD24 for claim.lba[0]. That
  sector was read in the same call (wire CRC-16 checked) as the exact 512-byte
  blank text of this file ID, slot and part 0; toy_survey_place holds it
  non-zero, inside the partition, below card_sectors and the card's block
  count and outside every extent of the manifest, for all three sectors before
  the write and again inside toy_survey_write.
- Lookup geometry: mount() keeps every data-cluster sector inside the
  partition for FAT32 and exFAT (including a wrapping exFAT
  FatOffset+FatLength, which can only misdirect bounds-checked reads); cluster
  numbers are validated; no run can reach the boot sector, a FAT or another
  partition.
- Zero-length file (FAT32: lookup result 4; exFAT: missing), file shorter or
  longer than 512*(1+3*slots) (NOT_FILE), chain shorter than the size or
  looping (CHAIN), more than 128 runs (FRAGMENTED), a folder named like the
  file (MISSING), edited first sector (NOT_FILE), slots of another ID or a
  shifted chain (passed over, FILE FULL): nothing is written in any of them.
- A file bigger than expected is refused by its size before any slot is read;
  the lookup maps its whole chain first, which costs time on a multi-gigabyte
  file but writes nothing.
- Partition at LBA 0 and behind an MBR, FAT32 and exFAT, block-addressed and
  byte-addressed (SDSC) cards: the launch record lands on the slot's part 0
  and the seed names its parts 1 and 2.
- 32-bit arithmetic: the same card presented with its partition ending at
  sector 0xFFFFFFFF: the stage's CMD24 and the worker's two go to the right
  sectors; a partition ending at 2^32 is refused (NOT THE SCI CARD) before any
  card traffic (e2e5.c).
- Slot counts at the limits: 1 and 999 parse, the highest file sector (2997)
  is inside the file, a count of 0, 1000, a leading zero or a fourth digit is
  refused, and the widest launch record (505 bytes with MAPPED) fits its
  sector.
- Single faults at every wire byte of the stage's traffic (port fault; one
  wrong bit from the card; one wrong bit to the card; 4 file systems x 8 card
  variants, about 1.3 million runs): never a sector changed outside the slot's
  part 0, never a seed without a read-back record and two still-blank report
  sectors, never a write through the block path, never a byte moved without
  the port, never a command sent into a programming card.
- Seed: built only after the read-back; its two sectors cannot be the record's
  sector (the lookup refuses overlapping runs and the three texts differ);
  toy_suite_report_prepare re-checks partition and extents; with no survey
  seed the Card Tools rule is unchanged.
- The worker's later write (unchanged toy_suite_report_write.inc) with the
  stage's seed, under the same three fault kinds at every byte of its traffic
  (9,903 runs per configuration): only the seed's two sectors ever change; it
  reports saved only with both read back; it re-checks seed CRC, capacity
  class, bounds, card ID and each sector's blank CRC-32 before the first
  write. That is enough: nothing writes the card between the stage's exact
  compare and the gesture (reader-only mode, no virtual VMU).
- Power lost at every byte of the stage's traffic: only the slot's part 0 can
  differ afterwards; the next launch takes the same slot (still blank) or the
  next one (record, or garbage) - except the unreadable case of finding 1.
- Odd answers to the write (data response 0x0D or 0x0B, no or late answer to
  CMD24, busy longer than one or than all waits): no seed; the card is either
  idle and kept or left not ready and the launch stops; never kept with a
  write pending; no command is sent into a busy card. The CARD WRITE line now
  shows the write's own response (taken before the status command).
- toy_survey_settle after a failed write: where a port fault left the card
  inside the data block, the status command's bytes are taken as data of that
  one block for lba[0]; the card's CRC-16 check rejects it (CRC is on since
  CMD59 at init) or the block stays pending and the launch stops. No path
  reaches another sector.
- toy_survey_settle after a failed identity (card ID all 00 or FF, CRC error,
  port fault): nothing written, the card is asked for status and kept, the
  real kui_retail_storage_adopt accepts it; the launch goes on as Test 81.
- Settle after a read: in 55ed2f8 it ran after any failure, and in my model a
  read whose CMD12 did not reach the card (port fault, wrong bit) was then
  answered 'status OK' from the read's own bytes (493 of 524 fault positions
  in the boot-sector read with a card that takes only CMD12 while sending), so
  the launch went on with the card still sending. In 3059447 it runs only
  after identity and write; the same runs now stop the launch. Deliver a
  module built from 3059447 or later.
- Random damage to boot sector, FATs, folders and the file itself (600,000
  damaged cards): no crash, no out-of-bounds access, and every sector written
  held this ID's blank part 0 just before (the survey file's own in all but
  the copy cases of finding 2).
- The game's image: a claimed sector inside any extent gives FILE PLACE and no
  write (the tree's test); the image reader's one-block cache holds only
  extent sectors, so it cannot hold a stale survey sector.
- Each survey read stops its stream before the port is released, so no stream
  is open at the identity command or the write; the write goes by the byte
  path with transfer_block NULL, restored afterwards.
- The reads now go into 32-byte-aligned buffers and so take the DMA block
  path: purge_buffer covers exactly the buffer's 16 cache lines, so
  neighbouring statics (claim, layout, seed) cannot lose cached data.
- No file-system sector is touched: the three sectors are data clusters of an
  existing file; size, FAT, bitmap and directory entry stay as they were;
  exFAT requires ValidDataLength equal to the size, so the PC reads what was
  written.
- The tree's own tests pass on the reviewed state: HOST_CC=cc python3
  tests/test_toy_survey.py and tests/test_toy_test_switches.py (30 shared
  sources keep their production text, wce_probe_fat.c among them).

## A launch that Test 81 boots

### A failed card-ID read (CMD10) in the survey leaves the card marked not ready; the launch then stops at RETAIL RESIDENT INIT FAILED where Test 81 boots (major)

Where: `src/loader/toy_survey_stage.inc:110`

Scenario:
  New (X) reader, SCI card, virtual VMU off, KUI/SURVEY/STREAM.TXT on the card
  with a blank slot. The lookup and the slot search succeed.
  toy_survey_prepare then acquires the card and calls
  kui_vmu_sd_identity_read(&card.device.sd,&identity) (line 110). The card's
  CMD10 fails in any of these ways: the CID's CRC-16 does not match, no data
  token arrives within the wait, R1 is not 0, or the CID is sixteen 0x00 or
  sixteen 0xFF bytes. In every one of them kui_vmu_sd_identity_read leaves
  through its done: label and runs `else card->ready=false;`
  (src/loader/vmu_backing.c:70). The stage releases the card, shows "SURVEY
  OFF / CARD IDENTITY FFFFFFFB" (or FFFFFFFD) and returns (lines 111-112).
  Nothing sets ready back; nothing has been written to the card yet.
  kui_retail_stage_main goes on to install_resident(); the low resident's
  kui_retail_resident_init calls kui_retail_storage_adopt, which refuses a
  card with ready false (src/loader/retail_storage_impl.h:82), returns
  KUI_RETAIL_RESIDENT_SD, and src/loader/retail_stage.c:1306 stops with
  "RETAIL RESIDENT INIT FAILED / DETAIL 00000003".

Consequence:
  A launch Test 81 boots is stopped. With the virtual VMU off Test 81 never
  sends CMD10 (card init uses CMD0/8/55/41/58/16/59/9; the worker's CMD10 is
  behind a report seed it never has from Games). This contradicts the file's
  own header, "Any failure leaves the launch as Test 81 runs it". No slot is
  used, so each retry repeats for as long as CMD10 fails and the file is on
  the card. The owner's card answers CMD10 (the virtual VMU and Card Tools
  reports work on it), so there it needs a one-off transfer error; a card with
  an odd CID would stop every launch. Reproduced on the host with the real
  vmu_backing.c, sd_reader.c, retail_storage.c and sci_sd_storage.c over a
  byte-level SPI card model (<scratch>/launch-lens/harness.c, run with
  build.sh beside it): all four CMD10 failures end with ready 0 and the
  adoption refused, while the write failures the stage does handle
  (write-error response, write-protect violation in status, write not kept)
  end with ready 1 and the launch going on.

Suggested:
  After a failed identity read, while the card is still acquired, apply the
  rule toy_survey_write already uses: if ready was true before the call, set
  sd->ready=true and ask kui_sci_sd_sync(); if that fails, set it and ask once
  more; keep what the second answer leaves. One small helper can serve the
  identity path and the write path.

### A survey read whose stream cannot be confirmed stopped leaves the card not ready and stops the launch; a bad sector in the survey's own file or folder would do it on every launch (minor)

Where: `src/loader/toy_survey_stage.inc:32`

Scenario:
  toy_survey_read (lines 32-39) returns -1 for any read or stop failure; its
  callers show "SURVEY OFF / FILE LOOKUP n", "FILE CHECK 00000001" or "WRITE
  NOT READ BACK" and return. The reader underneath may already have cleared
  card.device.sd.ready: src/loader/sd_reader.c:423 (CMD12 not answered with 0,
  or busy not released), :113 (block receive failed), :453 (card not idle
  before CMD18), src/loader/retail_storage_impl.h:113 (port fault). Host run:
  a looked-up sector answered with a data error token, and the following CMD12
  answered "illegal command" (R1 0x04), ends with ready 0 and RETAIL RESIDENT
  INIT FAILED; a port fault in the receive does the same. With the same error
  token and CMD12 answered 0, with a wrong data CRC, or with no token and
  CMD12 answered 0, ready stays 1 and the launch goes on. The sectors involved
  are ones Test 81's stage never reads: the partition's boot sector, FAT
  sectors, the root, KUI and KUI/SURVEY directories, STREAM.TXT itself. If the
  sector is in KUI/SURVEY or STREAM.TXT (for example one a power cut left
  unreadable during an earlier launch's write), every later launch reads it
  again.

Consequence:
  A card whose game image is sound boots under Test 81 and stops under Test
  83, with only the "SURVEY OFF / FILE ..." line above the stop to say why; in
  the survey-file case, on every launch until the file is taken off the card.
  It needs a card that does not answer the stop cleanly after a read error, or
  a port fault: a card that follows the specification keeps the launch
  (checked).

Suggested:
  The same single recovery at the survey's exit: if card.device.sd.ready went
  false during toy_survey_prepare, acquire, set ready, ask the status (twice,
  as after a failed write), release. A card that answers goes on as Test 81;
  one that does not stops as now.

### The stage host test's stand-ins for the identity read and for a failed read do not change sd.ready as the real functions do, so the test cannot see either defect above (minor)

Where: `tests/test_toy_survey_stage.c:100`

Scenario:
  The test's kui_vmu_sd_identity_read (lines 100-106) returns
  KUI_VMU_BACKING_IO for io.no_identity and leaves sd->ready true; the real
  one clears it on every failure (vmu_backing.c:70). The case that uses it
  ("SURVEY OFF / CARD IDENTITY FFFFFFFB", about line 308) checks the message
  only. The test's kui_retail_storage_read_run (lines 71-77) returns TIMEOUT
  with ready left true and its kui_retail_storage_stop always answers OK;
  off() and the fail_read_at/fail_acquire_at loops never look at
  card.device.sd.ready. Only the write cases assert ready.

Consequence:
  "every refusal leaves the card as it was" is printed for a card model that
  cannot leave ready false on those paths. The test passes today (HOST_CC=cc
  python3 tests/test_toy_survey.py) while the real stage, on a failed CMD10,
  hands the low resident a card it refuses.

Suggested:
  Make the stand-in identity read clear sd->ready on failure, let a failed
  read optionally clear ready and fail the stop, and assert
  card.device.sd.ready in off() for every refusal after which the card still
  answers its status.

Done: Corrected before the first commit (the identity command) and in `3059447` (the read); the test's card corrected with them.

Checked and found in order:

- Card never left acquired: every acquire in toy_survey_read, toy_survey_write
  and the identity step has its release on every path; in the host run over
  the real storage code, acquisitions equal releases and the card is
  deselected after every scenario, Toy Commander included.
- No read stream left open: each survey read is a one-block CMD18 that stops
  itself; CMD18 and CMD12 counts are equal in every run without a port fault,
  and card.stream.active is false at exit.
- Block-transfer pointer: saved, cleared and restored on the single path of
  toy_survey_write (build/t83 disassembly 0x8ce05064 to 0x8ce0507e shows the
  store and the restore around the call); no block transmit happens; no stage
  code uses the card after the survey and the resident binds its own bus.
- Failed write: ready is restored when the card answers its status at the
  first or second asking (write-error response, write-protect violation in
  status, write accepted but not kept all end with the launch going on); it
  stays false only when the status fails twice (card stuck busy, port fault),
  which the stage's comment and test state.
- Image reader and other stage state: the survey never touches image,
  raw_boot, last_card_result or blocks_read; the one sector it writes is
  checked to lie in no extent, so the image's cached block cannot go stale.
- Statics used later: manifest, toy_suite_source, toy_suite_seed and
  toy_movie_ranges are only read; card changes only in sd.ready, last_command,
  last_response and the stream's work fields, as Test 81's own reads change
  them; toy_survey_seed lives in stage BSS like manifest until
  toy_suite_report_prepare reads it.
- Stack: r15 is 0x8cff0000, set by the module entry (retail_entry.S) and not
  changed by the stage's _start; stage BSS ends at 0x8cec3448 (linker bound
  0x8cfe0000). kui_retail_stage_main's frame grew from 176 to 300 bytes; the
  deepest survey chain is about 1 KiB (300 + 100 + 544 + 32 + 32).
  native_passive_run grew from 2,460 to 2,464 bytes on the 64 KiB installer
  stack.
- No unbounded loop: a directory scan stops at 4,096 sectors and 65,536
  clusters per level, the file's chain at its cluster count and 128 runs, the
  slot search at the slot count, and every card wait at its work-tick budget
  (a stuck card costs a few seconds, then the stop).
- Sector counts for a 100-slot file: the slot search reads 1 + used slots + 3
  (301 at most, if every slot were half blank); the lookup's theoretical
  ceiling is 1 + 3 x 8,192 for directories of 65,536 entries. On a model card
  (FAT32, 40 game folders in the root, 60 modules in KUI): no file 19 reads,
  blank file 27, 99 used 126, full 123; 400 game folders ahead of KUI add 168;
  a 999-slot file nearly full is 1,027.
- Each survey read is its own acquire, CMD18, CMD12 and release, received by
  the polled path rather than DMA (the two buffers are 4-aligned: 0x8ce954b8
  and 0x8ce952b8 in build/t83), about 537 wire bytes; my estimate is 1 to 3 ms
  each on the console, not measured, so K-UI-to-bootstrap times of Test 83 are
  not comparable with Test 81's.
- File absent (module copied alone): reads only (boot sector, root directory,
  KUI directory, FAT sectors); no CMD10, CMD13 or CMD24; the card is
  unchanged, ready stays true, "SURVEY OFF / NO KUI/SURVEY/STREAM.TXT" is
  shown and the launch goes on.
- Legacy reader: "SURVEY OFF / NOT THE NEW (X) READER" before any card access.
  Virtual VMU on: the stage stops in native_persist_storage_init ("THIS TEST
  BUILD NEEDS VIRTUAL VMU OFF", retail_stage.c:1467) before toy_movie_scan at
  :1581, as Test 81 does, and no slot is used. IDE or SCIF: select_resident
  stops ("DUAL NATIVE READER NEEDS SCI X OR Y") before the survey, as Test 81.
- Toy Commander itself: run on the host through the real image reader: its own
  scan keeps its source and movie ranges, stops its stream and releases the
  card before the survey, image.cache_valid is 0 as before, then the survey
  claims its slot and the worker's seed is test 66; with no file the card is
  unchanged.
- Cooked images and titles that are no source: the survey runs and gives a
  seed; the worker reads config.toy_report only at the gesture
  (vmu_native_passive.c:830, toy_suite_report_save.inc,
  toy_suite_report_write.inc), so play is as Test 81.
- Wrapper order in toy_movie_iso_stage.inc: the scan first (source decided,
  Toy Commander's card use finished), the survey second, on every return path
  of the scan; the renaming macro is undone at the end of the file.
- Other Toy tests and production text: the four changed shared sources equal
  HEAD once the KUI_TOY_WIRE_SAVE and KUI_TOY_WIRE_SAVE_PATH blocks are
  removed; with Test 81's exact flags the stage translation unit and
  wce_probe_fat.c preprocess to the same tokens from HEAD and from the working
  tree; build/base81's module has the delivered Test 81's SHA-256
  (5ee3273b...2db4).
- Parts of build/t83 against build/base81 (one build label): both low
  residents, the auxiliary and both workers with their relocations are
  byte-identical; only stage.bin differs. All 15 mutation-test patterns on the
  changed files still match exactly once; tests/test_toy_test_switches.py and
  tests/test_toy_survey.py pass.
- Stage growth: stage.bin 291,840 to 298,892 bytes, BSS +2,176,
  __retail_stage_bss_end 0x8cec1028 to 0x8cec3448 against the 0x8cfe0000
  assert; module 300,096 to 307,148 bytes (limits 1 MiB and 4 MiB). The
  run-time uses of the end symbol (allocation_safe, the SDK table's
  disjointness, the preclear span) only exclude 9,248 more bytes, 0x8cec1028
  to 0x8cec3448; the cache sync covers all RAM; no new libgcc routine is
  linked.
- Seed template: the eight bytes KUITRS01 occur twice in the stage, as in Test
  81 (a literal pool and the template); the new seed builder's two magic words
  are not adjacent, so a patcher sees no new match.
- Notes, not defects of the stage: docs/toy-wire-stream-test-83.md, named in
  the Makefile and in toy_survey.h, does not exist yet; a stop after the
  launch record is written still ends with the unchanged line "STORAGE WAS
  READ ONLY" from retail_stage.c.
- Working tree during the review: tests/test_toy_survey_stage.c was edited at
  05:10 UTC and a commit (05ac704) and edits to
  docs/universal-loader-roadmap.md and tools/kui-library-startup-bundle.py
  appeared, none by me; the stage sources are unchanged since 04:39 UTC and
  build/t83 matches them. I wrote only under <scratch>/launch-lens/.

## The stage's seed and the unchanged worker

### toy_survey_settle asks a card that is still inside the survey's CMD18 read for its status: after a faulted block read the launch stops, or goes on, by what the stream's data bytes happen to be (minor)

Where: `src/loader/toy_survey_stage.inc:80`

Scenario:
  Outside my lens (stage and card, not the worker), but run, so reported.
  Commit 55ed2f8. Card with KUI/SURVEY/STREAM.TXT, New reader, SCI, VMU off.
  One of the survey's sector reads (toy_survey_read ->
  kui_retail_storage_read_run: a one-block CMD18 through the bus's block path)
  fails part-way with a bus fault (sci_sd_bus.c transfer_block returns false
  and latches port.fault). sd_reader.c read_payload sets ready=false;
  stream_finish's CMD12 is never clocked, because transfer() -> prepare()
  refuses while port.fault is set; kui_retail_storage_stop returns TIMEOUT and
  the port is released. The card is still inside CMD18 (sd_reader.c:355 'CMD12
  must instead be sent directly into the read stream';
  toy_wire_ahead.inc:205-207 says the same of a halted stream).
  toy_survey_settle then acquires again (kui_sci_sd_acquire clears
  port.fault), sets sd->ready=true (line 83) and sends CMD13 (line 84). The
  card does not hear a command that is not CMD12; the two bytes taken for R1
  and the status are the next bytes of the stream. Host experiment with the
  repository's own sd_reader.c, sci_sd_storage.c, vmu_backing.c and the
  stage's text over a card model that stays in CMD18 until CMD12
  (<scratch>/handshake-reviewer/, build.sh, log in out/e2e.log): FAT32 card,
  each of a launch's 14 reads faulted in turn after 100 bytes. 12 times both
  status attempts read non-zero data, ready stays false and install_resident's
  kui_retail_storage_adopt refuses the card (RETAIL RESIDENT INIT FAILED).
  Twice (the volume's boot sector, 'SURVEY OFF / FILE LOOKUP 00000002', and
  the FAT sector, 'FILE LOOKUP 00000004', where the sectors behind are zeros)
  the zeros pass for a clean status: ready=true and the launch goes on with
  the card still inside CMD18. exFAT: 9 stop, 4 go on. No seed was given and
  no further write made in any of them.

Consequence:
  Lines 18-22 and the commit message say only a card that does not answer
  stops the launch. Here a card that is well, and would answer once its read
  was ended, stops a launch Test 81 would boot in most positions; in the
  others the low resident adopts as idle a card that is in the middle of a
  read stream, and its first command is not heard (what the resident then does
  was not run on the host; its own failure path sends CMD12). Nothing is
  written in either case. It needs a bus fault during one of the survey's
  extra reads, so it is rare.

Suggested:
  Before the status command, end whatever read the card may be in: on the
  fresh acquisition send CMD12 into the stream and wait out its busy (the low
  reader's stop_card does this and notes that an idle card answers 'illegal
  command' and is stopped all the same), then CMD13. Or do not revive the card
  after a read or stop failure at all, only after the identity and write
  failures, which leave it idle. Give tests/test_toy_survey_stage.c's card a
  stream that outlives a stop that was never clocked, so the case is held.

### docs/toy-wire-stream-test-83.md, which the commit and the roadmap cite, does not exist; nothing yet tells the owner what the screens say (minor)

Where: `docs/universal-loader-roadmap.md:145`

Scenario:
  Makefile.vmu-native-toy-suite:411 and include/kui/toy_survey.h:4 (both in
  commit 55ed2f8) and the uncommitted docs/universal-loader-roadmap.md:145
  ('Built for it: Test 83 (`toy-wire-stream-test-83.md`)') name a document
  that is not in the tree (ls docs at 06:04 UTC; git status shows no untracked
  file). So the question whether the documented text matches the code could
  not be answered: there is no Test 83 text to compare. It may be meant for
  the separate documentation commit; it is not there yet.

Consequence:
  The owner-facing steps exist nowhere, and the unchanged worker shows things
  a reader would not guess. From the code and the strings of
  build/t83/native-worker.bin, what a document or package README must say:
  after a save the screen is not the report's text but Test 81's two fixed
  pages, 'TOY TEST 81 1/2 PLAYBACK / 781250 HZ' and 'TOY TEST 81 2/2 / ID0
  HEX' (ID1 for Toy Commander), each with 'REPORT SAVED TO SD', the first also
  'START / REBOOT TO K-UI CD'; nothing on them or in the saved report says 83
  (the report says TEST 81 and the build label; only the launch record says
  SURVEY 83). After a refused save it is Test 81's pages under 'REPORT NOT
  SAVED / FILM EVERY PAGE'; 'SAVE FAILED / FILM BOTH PAGES' is not in this
  worker's binary at all. A game still reading a second after the gesture is
  not saved (Test 79's known limit 12). The 'SURVEY ON / OFF ...' line is on
  the first launch page, which the relay clears
  (vmu_native_heap_probe_stage.c:691) when bootstrap 2 returns, with no
  handoff pause in this build.

Suggested:
  Write the document (and the package README) before delivery, with the screen
  text above taken from the worker as built, the blank file's place and how to
  make it (tools/toy_survey.py blank), VMU off and New (X), and the gesture at
  a moment the game is not reading.

Done: Corrected in `3059447`; the document and the README written.

Checked and found in order:

- Snapshot: the tree changed three times while under review and was committed
  as 55ed2f8 at 05:54 UTC (only docs/universal-loader-roadmap.md is still
  uncommitted). Everything below was re-checked against that commit. Two
  problems of the first snapshot (a failed identity read left the card not
  ready and stopped the launch; a damaged part 0 ended the slot search) had
  been changed by the author before the commit.
- Handshake end to end, run on the host (my own experiment, not a repository
  test: <scratch>/handshake-reviewer/): the stage's own text, then the
  worker's own toy_suite_report_save.inc and toy_suite_report_write.inc built
  with Test 81's switches, over the repository's real SD command code and a
  byte-level card model. The worker saves with the stage's seed on FAT32
  (block-addressed) and exFAT (byte-addressed) cards, with and without a
  partition table; FatFs and tools/toy_survey.py read the slots back as
  'report saved'; only slot sectors of the file changed.
- A title that is not Toy Commander: nothing in the save path is
  title-specific. What can still refuse is inherited from Test 81: worker not
  armed or a transfer pending, interrupts not masked, lease_valid or
  stack_valid false, the loan BUSY (a disc command still pending or executing
  after the gesture's 60 extra frames, quiesce refused, SCI stream busy), the
  loan FAULT (port fault, stream not stopped, the SDK's vector no longer the
  one bound), the SCI port not acquirable. state.error is 0x4f by the
  gesture's own path; reader_only() is constant true in this build.
- loan->slow, high_capacity and blocks: slow is true only inside card
  initialisation; kui_retail_storage_adopt copies the stage's card (blocks,
  high_capacity) into the resident and refuses a slow one, and the loan is a
  copy of that. The seed's high_capacity and the stage's lba<blocks test come
  from the same struct, so they agree by construction. The experiment's
  refusals with the real code: another card's CID, a changed sector, the other
  addressing mode, a bent seed; nothing was written in any.
- A disc that is no source (toy_suite_source 0): the auxiliary is bound for
  every New-reader launch whatever the source (native_dual_reader_bind ->
  native_readahead_bind), so movie_aux is non-NULL as it was for Test 81's
  pages; toy_wire_reset(true) at the auxiliary's init gives the report block
  its magic and zeroed counters, so toy_report_format reads zeros, not
  garbage, and nothing can fault. Saved in the experiment (slot 1, SOURCE 0).
- No write with an unvalidated seed: config.toy_report is filled only by
  toy_suite_report_prepare's '*out=*seed' after every check, into a cfg that
  was memset to zero. The worker checks test, the seed's CRC, blocks,
  addressing, the CID and both sectors' CRC again, and toy_suite_report_write
  is the only writer a reader-only worker has. Confirmed in the linked stage
  (build/t83/stage.elf, native_passive_run at 0x8ce0369c:
  toy_survey_seed.test, then source, then the same checks, then a 64-byte
  copy).
- 'SURVEY ON' cannot be followed by a refused seed: every rule of
  toy_suite_report_prepare is one toy_survey_look already held (SCI, place in
  partition/card/no extent, distinct sectors because no sector is two parts'
  blank text), and KUI_TOY_SURVEY_SEED_TEST is asserted equal to
  KUI_TOY_SUITE_MODE.
- seed->test (66): the stage compares it with KUI_TOY_SUITE_MODE (66); the
  worker only asks that it is non-zero (toy_suite_report_write.inc:8; the two
  other tests of it are compiled out by TOY_WIRE_TITLES); toy_report_format
  overrides the number to 81 in this build.
- The seed's CRC and the blank sectors' CRC: the stage checks with
  kui_stage_crc32 (byte table), toy_survey.c builds with retail_image.c's
  kui_retail_crc32 (nibble table). They are one function: 0 differences over
  20,000 random buffers.
- The seed outlives the wait: it is stage BSS written in toy_movie_scan,
  before install_resident's kui_retail_stage_sync writes back every RAM line,
  and read in native_passive_run beside toy_suite_source, which Tests 79 to 81
  already relied on across the same span (the two are neighbours in the linked
  BSS, 0x8ce95afc and 0x8ce95b3c).
- The report: 1,024 bytes, text only, ending in '\nEND REPORT\n', so it fills
  the two sectors exactly. The repository's formatter test built with Test
  81's switches passes (582 bytes with nothing counted, 1,010 of 1,012 at
  most, no row cut). The first half's last byte is not a line feed
  (include/kui/toy_survey.h:10 says each sector ends in one; true of the blank
  file and the launch record only), but the returned file is text throughout
  and neither the stage nor the tool relies on it.
- Which test the text names: the report says 'K-UI TOY COMMANDER REPORT / TEST
  81 / BUILD <label>' (unchanged worker), the launch record says 'SURVEY 83
  BUILD <label>'. The tests' made-up reports say TEST 81 and
  tools/toy_survey.py does not parse the report, so they agree. build/t83
  carries Test 81's label 684f485644e5 for the parts comparison; a delivered
  module needs its own.
- The worker is Test 81's: build/t83's residents, auxiliary, worker and
  relocations are byte-identical to build/base81's;
  tests/test_toy_wire_save_parts.py passes on the two; only stage.bin differs
  (291,840 -> 298,904 bytes).
- The fix for a failed identity read, with the real kui_vmu_sd_identity_read:
  a CID with a wrong CRC once, an all-zero CID, and a write the card refuses
  each give no seed, nothing stored, and the card kept ready by
  toy_survey_settle (it is idle in these cases, so the status command is the
  right question there).
- The seed record's magic: 'KUITRS01' followed by 1 and 64 still occurs
  exactly once in the module; the stage's new literal pool holds the two magic
  words apart.
- Repository checks on 55ed2f8: tests/test_toy_survey.py passes (file text,
  FAT32/exFAT lookup, the stage's text, the tool);
  tests/test_toy_test_switches.py passes, and src/core/wce_probe_fat.c with
  the seam resolved is HEAD~1's exact text (sha256 f496f003...).

## The tests and the build

### No host test gives the survey a card with a partition table: a write or a seed counted from the partition's start passes every check (major)

Where: `tests/test_toy_survey_stage.c:208`

Scenario:
  Every card of the two C tests is a volume at sector 0: FM_SFD at
  tests/test_toy_survey_stage.c:177 and tests/test_toy_survey.c:71,
  manifest.partition_start=0u at the stage test's line 208,
  volume={0u,BLOCKS,...} at the core test's line 120; the only other start is
  the direct toy_survey_write(2047u) guard at stage test :334. Each of these
  defects, put alone into a copy of 3bd4bed, passes HOST_CC=cc python3
  tests/test_toy_survey.py from first line to last (my runs): (a)
  src/loader/toy_survey_stage.inc:70
  kui_sci_sd_write(sd,lba-(uint32_t)manifest.partition_start,1u,data); (b) :46
  kui_retail_storage_read_run(&card,lba+(uint32_t)manifest.partition_start,1u,out);
  (c) before :173, toy_survey_claim.lba[1] and .lba[2] lowered by
  manifest.partition_start, so that the worker's seed names sectors counted
  from the partition; (d) :110 the volume begun at 0u; (e)
  src/core/toy_survey.c:129 without *lba<v->partition_start. The Windows CE
  probe's own tests begin at 0 too (tests/test_wce_probe_fat.c:89,
  tests/test_wce_probe_end_to_end.c:140), so the lookup's partition_start+...
  arithmetic, by which this stage now writes, runs with a start other than 0
  in no host test of the repository.

Consequence:
  The standing tests cannot tell a card sector from a sector counted from the
  partition's start, on the one path that writes. The Games map's
  partition_start is the volume's first card sector
  (src/apps/games_retail.c:1376), which is not 0 on a card with a partition
  table. A slip like (a) or (c), in this change or in the next one built on
  it, passes toy-wire-save-host-test and all 62 mutations, and on the console
  puts the launch record or the worker's two report sectors that many sectors
  before the survey file: in the FAT or in another file's data. 'No other
  sector of the card changed' is shown for cards without a partition table
  only. The delivered text has no such slip: with FatFs writing an MBR
  (partition at sector 63) the unmodified stage text and core pass both tests
  on FAT32 and exFAT in my scratch copies, and there (a), (b), (c) and (d) are
  caught; the review record's other reader reports the same from a scratch run
  behind an MBR.

Suggested:
  Format the test cards without FM_SFD (FatFs then writes an MBR), read the
  partition's start and length from the MBR (bytes 454 to 461 of sector 0) and
  use them for manifest.partition_start/partition_end in launch() and for the
  core test's volume. file_lba[] is found by content, so every expectation
  stays a card sector. About four lines a test; my working variant is made by
  <scratch>/tests-plumbing-reviewer/part/make_part.py.

### The stage's two new objects are not rebuilt when their header or the lookup's text changes (minor)

Where: `Makefile.vmu-native-toy-suite:462`

Scenario:
  Lines 462 to 465 add src/core/toy_survey.c and src/core/toy_survey_fat.c to
  STAGE_SRC after every '-include $(STAGE_OBJ:.o=.d)' has already been read
  (Makefile.vmu-toy:343, Makefile.vmu-native-persistent:44), so the two
  objects' .d files are never read; line 465 names the new header for
  vmu_native_heap_probe_stage.o only. In a scratch build of 3bd4bed with the
  pinned GCC 15.2.0 and a fixed BUILD_ID I changed KUI_TOY_SURVEY_UNREAD_MAX
  from 4u to 1u in include/kui/toy_survey.h and ran make for
  retail-toy-test-83.kui in the same BUILD directory: only
  stage/src/loader/vmu_native_heap_probe_stage.o was compiled, make ended 0,
  and the module's sha256 stayed 0db24243..., where a clean build of the
  edited source gives 6b10ee00.... Touching src/core/wce_probe_fat.c, the text
  toy_survey_fat.o is compiled from, rebuilds nothing. Touching
  include/kui/toy_suite_report_wire.h rebuilds the stage's main object and not
  toy_survey.o, which fills that struct.

Consequence:
  An incremental build after an uncommitted change to the survey's header, to
  the report seed's struct or to the other team's lookup links a stage whose
  glue and core come from different texts, or keeps the old lookup, with no
  message, while tests/test_toy_survey.py, compiled fresh each time, passes on
  the new text. A new commit hides it (BUILD_ID is in readonly-config and
  every object depends on that), so a clean delivery build is not affected: my
  rebuild of 3bd4bed gives the packaged module byte for byte. A dev build with
  a fixed BUILD_ID in a kept directory is affected.

Suggested:
  After line 462 add: -include $(BUILD)/stage/src/core/toy_survey.d
  $(BUILD)/stage/src/core/toy_survey_fat.d. Both files are already written by
  -MMD and list wce_probe_fat.c and the headers.

### No test holds the stop screen's line about the survey's write (minor)

Where: `src/loader/retail_stage.c:646`

Scenario:
  With line 646 changed to show "STORAGE WAS READ ONLY" whatever
  toy_survey_wrote says (or with the two texts exchanged),
  tests/test_toy_survey.py passes: it never compiles retail_stage.c, and
  tests/test_toy_survey_stage.c:52 has its own toy_survey_wrote, of which only
  the value is asserted. Built that way in my scratch with label 3bd4bed2fca2,
  the module also passes toy-wire-save-parts-check ('PASS Test 83's parts
  ...') and toy-wire-package-test ('admitted'), with 'ONLY THE SURVEY FILE WAS
  WRITTEN' absent from stage.bin: STAGE_ONLY at
  tests/test_toy_wire_save_parts.py:17 does not name it.

Consequence:
  A launch that has written its launch record and then stops (for instance
  RETAIL RESIDENT INIT FAILED after a survey read left the card not ready)
  could tell the owner that storage was only read, and nothing in the
  repository would notice. The commit's sentence 'The stage's stop screen says
  when the survey file was written' rests on reading that one line.

Suggested:
  Name b"ONLY THE SURVEY FILE WAS WRITTEN" in STAGE_ONLY, and let the stage
  test see the choice: a one-line function in toy_survey_stage.inc that
  returns the stop screen's line, called by stopped(), asserted after the
  cases with and without a write.

### tools/toy_survey.py read lists 'report saved' for report sectors that are no report, and none of its other states or its commands is tested (minor)

Where: `tools/toy_survey.py:121`

Scenario:
  read_file decides a slot's state by 'is not this slot's blank text' alone
  (lines 106 to 123). A returned file whose slot 2 has a good launch record,
  report half 1 saved and half 2 holding 512 zero bytes (a write cut off), or
  both halves holding another slot's blank text, is listed 'slot 002: report
  saved: ...' (my run, <scratch>/tests-plumbing-reviewer/x/states.py). A slot
  whose launch record has one byte damaged and both halves present is listed
  'report saved: ... source ? tracks ?' without the word damaged (lines 149 to
  153; only the JSON carries it). The states the reader gets right are in no
  test: with lines 118 and 119 removed (a cut-off report then reads 'report
  saved'), with 'damaged' made 'empty' at 113, with line 90 removed, with the
  head test of line 84 removed, with launch={} for a blank record at 111, or
  with the first sector's exact-text test at 58 and 59 removed,
  tests/test_toy_survey.py passes. main() is never run: 'blank' making 20
  slots whatever --slots says (141), DEFAULT_SLOTS 1000 (22) and the CD music
  column taken from 'audio' (153) pass too, and so does a slot number printed
  modulo 100 (39), the blank file being compared for 20 slots only
  (tests/test_toy_survey.py:55).

Consequence:
  Whoever reads a returned STREAM.TXT is told that a report was saved where
  its second half is not a report, and is not told in the listing that a
  launch record is damaged. A later slip in the reader's handling of cut-off
  and damaged slots, or in the two commands by which the owner's file is made
  and read, passes the test. As delivered the commands work: the packaged file
  is the C sources' file for its ID and 100 slots, byte for byte.

Suggested:
  Call a report saved only if its 1,024 bytes are text that begins 'K-UI TOY
  COMMANDER REPORT' and ends with the line 'END REPORT'
  (src/loader/toy_suite_report_save.inc:610 and :891), and 'report damaged'
  otherwise; say 'launch record damaged' in the listing; give the test one
  file that holds each state, and one run of the blank and read commands.

### The mutation check counts a mutation that does not compile as caught: 5 of the 62 recorded (minor)

Where: `tests/test_toy_survey_mutations.py:199`

Scenario:
  run() (line 167) calls a mutation caught when tests/test_toy_survey.py ends
  non-zero for any reason, and that script compiles with -Wall -Wextra
  -Werror. In docs/evidence/toy-wire-stream-83/mutation-check.log five of the
  62 'caught' lines show the compiler's message and no test's: 'the block path
  is not given back' (unused variable 'block'), 'the survey is not looked for'
  ('toy_survey_prepare' defined but not used), 'the seed has no card' (unused
  parameter 'cid'), 'the seed's capacity flag is lost' (unused parameter
  'high_capacity'), 'a blank sector does not name its file' (unused parameter
  'file'): MUTATIONS lines 40, 108, 140, 141 and 148. For these five no test
  ran.

Consequence:
  '62 of 62 caught' (docs/toy-wire-stream-test-83.md:263) says more than the
  run shows, and a defect that happens to leave a name unused is reported as
  noticed even where no assertion looks at it. For these five the tests do
  look: written so that they compile ((void)block; if(0)toy_survey_prepare();
  (void)cid; (void)high_capacity; (void)file) each is caught by an assertion
  in my run.

Suggested:
  Write the five so that they compile, and in check() count an output whose
  complaint is the compiler's 'error:' apart, as STALE is, so that it fails
  the check.

### The file-text test never parses a slot count from 990 to 999, though it says the limits are tested (minor)

Where: `tests/test_toy_survey.c:352`

Scenario:
  The loop
  for(slots=1u;slots<=KUI_TOY_SURVEY_SLOTS_MAX;slots+=slots<10u?1u:slots<990u?89u:1u)
  visits 1 to 10, 99, 188, ..., 900, 989 and then 1078: at 989 it adds 89 once
  more, so 990 to 999 are never reached. With src/core/toy_survey.c:75 changed
  to count>989u, or with its upper limit removed, or with the three-digit stop
  at :72 removed, every test passes ('SLOTS 1000' is refused by whichever of
  the two guards is left).

Consequence:
  Nothing holds that the stage accepts the largest files tools/toy_survey.py
  makes (SLOTS_MAX 999): such a file could be refused with 'FILE CHECK' and no
  test would notice. The comment at line 351 and the document's 'slot counts
  at their limits' (docs/toy-wire-stream-test-83.md:257) claim a check that is
  not made. As written the tool and the C sources agree up to 999 (my
  cross-check).

Suggested:
  Step with slots<900u?89u:1u (or an explicit list that ends 989, 990, 998,
  999), so that 999 is parsed and each of the two guards is the only one that
  refuses some input.

### The stage test's map is too plain to hold the extent guard beyond one extent, the title, the track counts, the slot number shown and Card Tools' seed (minor)

Where: `tests/test_toy_survey_stage.c:221`

Scenario:
  launch() always gives one extent (line 221), tracks that are audio, raw Mode
  1 or cooked (line 251; MODE2_DATA at 204 is never used), no audio track at
  session_lba, the 12-character title 'EXAMPLE GAME' (212) and a file of 4
  slots (38); its Card Tools seed __toy_suite_report_seed is always blank.
  Each of these changes to 3bd4bed passes every test:
  src/loader/toy_survey_stage.inc:56 the place loop stopped after the first
  extent (j<manifest.extent_count && j<1u) or begun at the last; :138
  .title_bytes=16u or sizeof(manifest.product), and src/core/toy_survey.c:100
  the title cut at 40 for 56; :154 and 155 the raw count without the header
  test (a Mode 2 track counted RAW); :157 else ++launch.cooked; :152
  start_lba>manifest.session_lba; :96 the shown slot's tens digit taken from
  slot/100u, or its hundreds digit '0';
  src/loader/toy_suite_report_stage.inc:24 bool survey=true, or :25 the
  survey's seed taken only when Card Tools patched none.

Consequence:
  Not held by any test: the 'in no extent of the game's image' guard for a map
  of more than one extent (a map has one for each run of each track file); the
  game's title beyond twelve characters, which is what the launch record is
  for; the RAW, COOKED, OTHER and HDAUDIO counts for a disc with a Mode 2
  track or with audio at the session's first sector; the slot number on the
  launch screen from the eleventh launch on (the delivered file has 100
  slots); and, under the switch, Test 81's own route by a seed Card Tools
  patched. The delivered text is right in the first four: my scratch run of
  the unmodified stage text with a partition table, 14 launches into a 20-slot
  file, three extents, a 128-character title, a Mode 2 track and an audio
  track at session_lba passes on FAT32 and exFAT, and that map catches each of
  the changes above but 'begun at the last extent', the hundreds digit and the
  two of the seed's choice.

Suggested:
  Give good() such a map (<scratch>/tests-plumbing-reviewer/part/make_rich.py
  builds one), put the covering extent first and last in turn, and add one
  launch with a patched Card Tools seed and no survey file.

### The card model never fails the end of a stream after a good read, so a survey read that ignores it passes (minor)

Where: `tests/test_toy_survey_stage.c:93`

Scenario:
  The model's kui_retail_storage_stop fails only after a read that itself
  failed (io.stop_failed is set at line 86 together with read_kills). The real
  one returns TIMEOUT and clears sd.ready after any read when the SCI port is
  not well (src/loader/retail_storage_impl.h:112 to 115). With
  src/loader/toy_survey_stage.inc:49 changed to (void)stop;return
  read==KUI_LOADER_SD_OK?0:-1; every test passes.

Consequence:
  Stage text that took such a read for good would go on with a card marked not
  ready, from the last blank-sector read to the identity and the write, or
  from the read-back to 'SURVEY ON' and a seed for a launch that then stops at
  RETAIL RESIDENT INIT FAILED, and the tests would notice neither.

Suggested:
  A model switch that makes the stop after the n-th good read fail (ready
  false, TIMEOUT), run for every n as the read failures are, expecting no
  later write and no seed.

Done: in `87e380b` and `f8afab2`, tests only: no source of the module changed, and the module built from them with the delivered label is the delivered module, byte for byte. Both C tests run every case on a card with a partition table too; the Makefile names the two objects' dependency files; the parts check names the stop screen's line and holds the two buffers to a cache line in the linked stage; the tool calls a report saved only if it is one and lists a damaged launch record, and a test runs each of its states and both commands; the five defects compile, and one the compiler refuses fails the check; every slot count from 1 to 999 is parsed; the stage test has a fuller map, twelve slots and a run as a module whose seed Card Tools patched; its card can fail the end of any read. The findings' own defects are in the mutation check: 120 of 120 caught. This reader's own script, run again with its 105 defects on the new tests: 92 caught where 53 were. Left open: which of the stop screen's two lines is shown is held by no host test. The function suggested for it was tried and changes the module's bytes, so it waits for the next build.

Checked and found in order:

- State reviewed: the change was committed while I read (55ed2f8, then 3059447
  and 3bd4bed; HEAD is now 8a986d9, documents and evidence only). Every run
  below was made on copies of 3bd4bed's files, which equal the working tree,
  under <scratch>/tests-plumbing-reviewer/ (mymut.py and mymut2.log: my 105
  mutations and their results; repo-mutations2.log; part/make_part.py;
  part/make_rich.py). Nothing in the repository was changed; file names
  in this report are relative to it.
- HOST_CC=cc python3 tests/test_toy_survey.py passes on 3bd4bed, and
  tests/test_toy_survey_mutations.py gives 62 caught, 0 missed, 0 stale in my
  copy as in the recorded log.
- The task's own list is noticed by a test assertion: the read-back removed or
  not compared, the seed naming part 0 or the wrong checksums, the report's
  halves exchanged (lba[2-i]), no extent check, an extent one sector short or
  free at its first sector, extents looked for among the tracks,
  transfer_block not given back, a slot with a non-blank part 1 or 2 claimed,
  the record written before the identity is read, the seed not zeroed, the
  seed built before a failed read-back, the place of part 0 or part 2 not
  asked, the partition's start not asked at the write. Of my 105 further
  mutations 58 are caught; the 47 that survive are all named in the findings
  or in the last two lines here.
- src/core/wce_probe_fat.c without KUI_TOY_WIRE_SAVE_PATH: the output of cc -E
  -P and of sh-elf-gcc -E -P is byte-identical to that of origin/main
  4441e92's file, and so are the object's code and read-only data under the
  pinned GCC 15.2.0 at -Os (and the whole object under GCC 13 without -g).
  With the path defined only the three-entry path table differs.
- tests/fixtures/toy-switch-production-text.json: the sha256 recorded for
  src/core/wce_probe_fat.c (f496f003...81f5, mode exact) is the sha256 of
  origin/main:src/core/wce_probe_fat.c and of the file before the change;
  tests/test_toy_test_switches.py passes (30 sources), and
  tests/test_wce_probe_fat.py and tests/test_wce_probe_end_to_end.py pass.
- No Makefile globs src/core/*.c: toy_survey_fat.c, which defines every
  external function of wce_probe_fat.c a second time, is linked only into Test
  83's stage and the survey's host tests.
- Makefile.vmu-native-toy-suite with TOY_WIRE_SAVE=1 against Test 81's
  switches, every compile and link command of a dry run compared:
  -DKUI_TOY_WIRE_SAVE=1 is on the stage's objects only (stage/*, stage_asm,
  native_worker_call.o, preclear_tail_asm.o, all linked into stage.elf alone);
  every resident, legacy, auxiliary, native and worker command is the same;
  stage.elf's link gains only toy_survey.o and toy_survey_fat.o. The linked
  stage has kui_toy_survey_*, kui_wce_probe_fat_find and kui_wce_probe_lba
  from them, the survey's texts, and no 'WCEDATA'.
- Switch combinations: the test number is 83 only for Test 81's switches with
  TOY_WIRE_VMU=0 and TOY_WIRE_SAVE=1 (also with one part of Test 74, 75 or 76
  switched, as for every earlier number). SAVE with VMU=1, without TIMER,
  without STREAM, with TICK, with mode 65 or FMV_COMPARE, SAVE=2, empty or
  'yes', and joined values (VMU=01 SAVE=, VMU= SAVE=01) all stop with an
  error; toy-wire-save-parts-check without SAVE and toy-wire-vmu-parts-check
  with it stop.
- toy-wire-save-parts-check passes in my own build with the pinned GCC 15.2.0
  and toy-wire-package-test admits the module; the packaged .kui holds
  stage.bin and the six compared resident, auxiliary and worker files verbatim
  (worker.bin, also compared, is not in the module, which is harmless).
- Independent reproduction: 3bd4bed built with BUILD_ID=3bd4bed2fca2 gives
  sha256 a7b1c1cb...11d6, the packaged KUI/apps/games/retail-native-dual.kui;
  Test 81's recipe at 3bd4bed with BUILD_ID=684f485644e5 gives
  5ee3273b...2db4, the module delivered as Test 81; with one label the seven
  non-stage files of the two builds are byte-identical.
- tools/toy_survey.py against the C sources: the blank files are identical for
  1, 2, 9, 10, 11, 20, 99, 100, 101, 255, 256, 500, 989, 990, 998 and 999
  slots and four IDs; the packaged KUI/SURVEY/STREAM.TXT (100 slots, ID
  B674BC23...63FF, sha256 3e32647c...14e3) is the C sources' file for that ID
  byte for byte and reads as 0 of 100 slots used.
- The tool's reader on the state the task names: a good launch record with
  report half 1 saved and half 2 still blank is listed 'launched, report cut
  off' (likewise half 2 saved and half 1 blank); a blank launch record with
  report halves present is 'damaged'; a truncated file and a first sector with
  a changed ID digit are refused.
- tests/test_asm_audit.py reads src/**/*.[ch] and include/**/*.h, not .inc
  files, and the new statement (src/loader/toy_survey_stage.inc:29) has no
  operands, so nothing needs pinning. 9 of its 10 tests pass; the tenth fails
  on origin/main as well (raw __DREAMCAST__ and __sh__ in
  src/core/bba_sockets.c and src/dreamcast/shell_font.c).
- unittest discovery over tests/test_*.py in two scratch clones (1,654 tests,
  the slow splash-asset test left out): the same 3 failures and 10 errors at
  05ac704, before the change, and at 3bd4bed; none is new. The three new
  test_*.py import cleanly and define no test case, so make test
  (Makefile:524) and CI do not run them: they run only from the toy-suite
  Makefile's toy-wire-save-* targets, as the other Toy tests do.
- On my scratch copies the unmodified stage text and core pass both C tests on
  cards with a partition table (FatFs MBR, partition at sector 63) on FAT32
  and exFAT, and a fuller run passes too: 14 launches into a 20-slot file with
  slots 000 to 013 shown, a slot sector in the third extent refused, a
  128-character title cut at 56, a Mode 2 track counted OTHER, an audio track
  at session_lba counted HDAUDIO.
- In the linked stage (label 3bd4bed2fca2) toy_survey_text and toy_survey_work
  lie at 0x8ce95320 and 0x8ce95520, both 32-byte aligned; removing the aligned
  attribute passes the host tests, which cannot see it.
- Surviving mutations that change nothing today: reader and transport
  exchanged in the record (both are 1 whenever the survey runs); the blank
  checksum taken of the expected text (the bytes were just compared equal); a
  survey seed excused from the SCI check (the survey runs on the SCI card
  only); FRAGMENTED taken as found (layout.bytes stays 0 and the first read
  refuses); the rest of a slot still read after a sector that is not read;
  one-digit hex fields read as decimal in the tool; KUI_TOY_SURVEY_UNREAD_MAX
  5 for 4 (the test reads the macro).
- Surviving mutations that another guard covers today: toy_survey_place
  without the partition's end or one sector late at it, and claim_read one
  sector late at it or without its sector-0 test (claim_read, the place and
  card_sectors refuse the same sectors, and the Games map sets card_sectors to
  partition_end); the look without its start>=end test or with a volume that
  has no end (the lookup's mount and the place refuse).

## A second reader on one finding

Finding: verify:launch-regression:A failed card-ID read (CMD10) in the sur

Real: False (none)

  The defect is not in the code Test 83 ships. It was true of an earlier
  uncommitted draft, and the committed code already contains the fix the claim
  itself suggests.  **What the claim was written against.** The claimant's own
  preprocessed snapshot shows a draft where the identity read sat inside
  toy_survey_prepare with no status question after it, and toy_survey_write
  carried the "set ready, sync, sync again" rule inline. The cited lines
  (identity read at 110, stop at retail_stage.c:1306) match that draft, not
  the current files (130 and 1309). On that draft the claim is correct.
  **What the code does now.** The change is no longer uncommitted: it landed
  as 55ed2f8, then 3059447, and HEAD moved to 3bd4bed while I was reviewing (a
  comment-only change in this file). In all three, a failed identity read is
  followed by toy_survey_settle(). That function re-acquires the port, sets
  ready, asks CMD13, asks once more if the first answer is bad, and keeps the
  card unless the port is unwell. kui_sci_sd_acquire does not look at
  sd->ready, so the re-acquire is not blocked by the flag the identity read
  just cleared. "Nothing sets ready back" is therefore false.  **What ships.**
  The package ZIP is built from 3059447, and its module was byte-identical to
  the clean build of that commit when I hashed it. Disassembly of that stage,
  and of the 3bd4bed rebuild, shows the call to toy_survey_settle immediately
  after the failed identity read and release.  **Experiment.** I ran the stage
  text over the repository's real port driver, card init, stream reader,
  storage layer, status/write code and identity read, with only the SCI
  registers and an SPI card modelled. The claim's four CMD10 failures (wrong
  CRC-16, no data token, R1 not 0, CID all 0x00 or all 0xFF), plus four
  variants of them, each ended with ready 1, adoption accepted, the card still
  reading and no sector changed. With the one settle call removed from the
  identity branch, every one ended with ready 0 and adoption refused, which is
  the claim's result.  **What remains, and why it is not this claim.** If
  CMD10 fails and the card also gives no clean CMD13 answer on two asks, or
  the port is unwell, the launch still stops where Test 81 would boot. That is
  the rule the claim's suggested fix prescribes ("keep what the second answer
  leaves"), it is stated in the file header, and the repository's stage test
  and mutation list cover it.  Limits: this is a host model, not a console
  result, and 512-byte receives took the driver's programmed path rather than
  DMA, which does not touch the single-byte CMD10/CMD13 exchange.

## A second reader on one finding

Finding: verify:tests-and-build:No host test gives the survey a card wit

Real: True (minor)

  I could not refute it. The claim is true as a gap in the change's own host
  tests; it is not a fault in the module that was delivered. Two parts of its
  consequence are overstated.  State reviewed: the working tree is clean. The
  change is committed (55ed2f8, 3059447, 3bd4bed); HEAD 8a986d9 adds documents
  only, and its sources equal 3bd4bed's. I changed nothing in the repository.
  **What holds**  - **No test card has a partition table.** Both survey C
  tests format with FM_SFD and start the volume at sector 0. The only other
  start in them is a refused direct write (2048 set, sector 2047 asked) that
  never reaches the card. - **No host test in the repository runs the lookup
  from a start other than 0.** Only four test files call it; the two Windows
  CE probe tests also format with FM_SFD and start at 0. - **The Games map's
  start is not 0 on a partitioned card.** It is the volume's first card
  sector, taken from the MBR entry. - **All five slips pass.** Each of (a) to
  (e), put alone into an exact copy of the sources, passes
  tests/test_toy_survey.py from first line to last. - **The mutation check
  does not notice either.** The repository's own mutation script, run on a
  copy carrying (a) and on one carrying (c), reports 62 of 62 caught, none
  missed or stale. - **The lookup's own offsets are unguarded too.** Four more
  slips of mine (FAT and data start without the partition's start, FAT32 and
  exFAT) pass the survey test and both probe tests. - **The delivered text is
  right behind a partition table.** The unmodified sources pass both C tests
  on FAT32 and exFAT with the partition at sector 63 (FatFs's MBR) and at
  sector 2048 (an MBR I wrote by hand). On those cards (a), (b), (c) and (d)
  are caught.  **What is not right in the claim**  - **(c) would not write a
  foreign sector.** The unchanged worker reads both seed sectors and compares
  their CRC-32 with the blank ones before its first write. I ran the worker's
  own write code with a seed shifted by the partition's start on MBR cards:
  the stage's preparation hands the seed on, the worker returns -1 after one
  read, and nothing is written. On a console that is "REPORT NOT SAVED", not
  damage to the FAT or another file. - **(e) is not a partition-table
  matter.** It still passes on the MBR cards. In the stage the removed
  comparison cannot be reached: the layout's sectors are computed from the
  same volume and are never below its start. - **(b) and (d) end harmlessly.**
  Both show "SURVEY OFF / FILE LOOKUP 00000002" and write nothing. - **Only
  (a) would reach a foreign sector.** It would put the launch record that many
  sectors before the slot, then show "WRITE NOT READ BACK".  **Severity**
  Minor. Nothing in the delivered module is wrong, and no console behaviour
  changes. The sentence "no other sector of the card had changed" in the Test
  83 document is shown by standing tests only for cards without a partition
  table. A later change on this ground could carry a slip like (a) past the
  host test and the mutation check.  The suggested fix works. My variant
  needed a few more lines than four a test, because the no-volume case and the
  core test's "small" volume also need the start.
