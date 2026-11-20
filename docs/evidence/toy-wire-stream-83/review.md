# Test 83: the review's record

Four readers beside the author on 20 November 2026, each with one question, on the
change as it stood while they read (`55ed2f8`, then `3059447`). Their findings and
their checks are given as they wrote them. None of it is a console result.

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

This reader's result was not in when the build was delivered.

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
