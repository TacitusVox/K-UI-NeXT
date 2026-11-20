#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Defects put into Test 83's own sources one at a time: the survey's host tests must notice each.

usage: test_toy_survey_mutations.py [--work DIRECTORY] [--only TEXT]

The repository is not changed: each mutation is applied to a copy of the sources
(include, src, tests, tools, config) and tests/test_toy_survey.py is run there.
Run by make -f Makefile.vmu-native-toy-suite toy-wire-save-mutation-check.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STAGE, CORE, REPORT, SCAN, FAT = ('src/loader/toy_survey_stage.inc', 'src/core/toy_survey.c',
                                  'src/loader/toy_suite_report_stage.inc', 'src/loader/toy_movie_iso_stage.inc',
                                  'src/core/toy_survey_fat.c')
# name, file, text as it stands, text with the defect
MUTATIONS = [
    ('the launch record is not read back', STAGE,
     'if(toy_survey_read(NULL,c->lba[0],toy_survey_work) ||\n       memcmp(toy_survey_work,toy_survey_text,KUI_TOY_SURVEY_SECTOR)) {',
     'if(0) {'),
    ('the read-back is not compared', STAGE,
     'if(toy_survey_read(NULL,c->lba[0],toy_survey_work) ||\n       memcmp(toy_survey_work,toy_survey_text,KUI_TOY_SURVEY_SECTOR)) {',
     'if(toy_survey_read(NULL,c->lba[0],toy_survey_work)) {'),
    ('a sector in an extent of the game is taken', STAGE,
     'if(lba>=e->card_lba && lba-e->card_lba<e->blocks) return false;', '(void)e;'),
    ('an extent is checked one sector short', STAGE,
     'lba-e->card_lba<e->blocks) return false;', 'lba-e->card_lba+1u<e->blocks) return false;'),
    ('the map\'s card size is not asked', STAGE, '       lba>=manifest.card_sectors || lba>=card.device.sd.blocks) return false;',
     '       lba>=card.device.sd.blocks) return false;'),
    ('the card\'s size is not asked', STAGE, '       lba>=manifest.card_sectors || lba>=card.device.sd.blocks) return false;',
     '       lba>=manifest.card_sectors) return false;'),
    ('the block path is left on for the write', STAGE, '    sd->bus.transfer_block=NULL;\n', ''),
    ('the block path is not given back', STAGE, '    sd->bus.transfer_block=block;\n', ''),
    ('no status is asked after a failed identity', STAGE,
     '        toy_survey_settle();\n        retail_display_hex("SURVEY OFF / CARD IDENTITY",(uint32_t)known);return;',
     '        retail_display_hex("SURVEY OFF / CARD IDENTITY",(uint32_t)known);return;'),
    ('no status is asked after a failed write', STAGE,
     '        toy_survey_settle();\n        retail_display_hex("SURVEY OFF / CARD WRITE",response);return;',
     '        retail_display_hex("SURVEY OFF / CARD WRITE",response);return;'),
    ('a card inside a read is asked for its status', STAGE, '    toy_survey_look();\n}', '    toy_survey_look();\n    toy_survey_settle();\n}'),
    ('one status read decides for a card left not ready', STAGE,
     'if(kui_sci_sd_sync(sd)!=KUI_LOADER_SD_OK) {sd->ready=true;(void)kui_sci_sd_sync(sd);}', '(void)kui_sci_sd_sync(sd);'),
    ('a card that answers nothing is kept', STAGE,
     'if(kui_sci_sd_sync(sd)!=KUI_LOADER_SD_OK) {sd->ready=true;(void)kui_sci_sd_sync(sd);}',
     'if(kui_sci_sd_sync(sd)!=KUI_LOADER_SD_OK) {sd->ready=true;(void)kui_sci_sd_sync(sd);sd->ready=true;}'),
    ('a card on an unwell port is kept', STAGE, '    if(!kui_sci_sd_healthy() || kui_sci_sd_faulted()) sd->ready=false;\n', ''),
    ('a card on a port with a latched fault is kept', STAGE,
     'if(!kui_sci_sd_healthy() || kui_sci_sd_faulted()) sd->ready=false;', 'if(!kui_sci_sd_healthy()) sd->ready=false;'),
    ('a card that is ready is asked for its status', STAGE,
     'if(sd->ready || kui_retail_storage_acquire(&card)!=KUI_LOADER_SD_OK) return;',
     'if(kui_retail_storage_acquire(&card)!=KUI_LOADER_SD_OK) return;'),
    ('a card not ready at the start is surveyed', STAGE,
     '    if(!card.device.sd.ready) {retail_display_line("SURVEY OFF / CARD NOT READY");return;}\n', ''),
    ('the stop screen is not told of the write', STAGE, '    toy_survey_wrote=true;\n', ''),
    ('the stop screen keeps an earlier launch\'s write', STAGE, '    toy_survey_wrote=false;\n', ''),
    ('audio tracks without their files count as mapped', STAGE,
     'if(track->extent_count) ++launch.audio_mapped;', '++launch.audio_mapped;'),
    ('an unwell port passes', STAGE, 'bool healthy=kui_sci_sd_healthy() && !kui_sci_sd_faulted();', 'bool healthy=true;'),
    ('a latched fault passes', STAGE, 'bool healthy=kui_sci_sd_healthy() && !kui_sci_sd_faulted();', 'bool healthy=kui_sci_sd_healthy();'),
    ('a failed write gives a seed', STAGE,
     'retail_display_hex("SURVEY OFF / CARD WRITE",response);return;',
     'retail_display_hex("SURVEY OFF / CARD WRITE",response);'),
    ('a record not read back gives a seed', STAGE,
     'retail_display_line("SURVEY OFF / WRITE NOT READ BACK");return;', 'retail_display_line("SURVEY OFF / WRITE NOT READ BACK");'),
    ('the record goes to the slot\'s second sector', STAGE,
     'if(toy_survey_write(c->lba[0],toy_survey_text)) {', 'if(toy_survey_write(c->lba[1],toy_survey_text)) {'),
    ('the Legacy reader is surveyed', STAGE,
     'if(manifest.reader!=KUI_RETAIL_READER_ASYNC || native_runtime_mode()!=KUI_VMU_NATIVE_MODE_READER_ONLY) {',
     'if(native_runtime_mode()!=KUI_VMU_NATIVE_MODE_READER_ONLY) {'),
    ('a launch with the virtual VMU is surveyed', STAGE,
     'if(manifest.reader!=KUI_RETAIL_READER_ASYNC || native_runtime_mode()!=KUI_VMU_NATIVE_MODE_READER_ONLY) {',
     'if(manifest.reader!=KUI_RETAIL_READER_ASYNC) {'),
    ('the map\'s transport is not asked', STAGE,
     'if(manifest.storage_transport!=KUI_STORAGE_SCI || card.transport!=KUI_STORAGE_SCI ||',
     'if(card.transport!=KUI_STORAGE_SCI ||'),
    ('the card\'s transport is not asked', STAGE,
     'if(manifest.storage_transport!=KUI_STORAGE_SCI || card.transport!=KUI_STORAGE_SCI ||',
     'if(manifest.storage_transport!=KUI_STORAGE_SCI ||'),
    ('a partition past 32 bits is taken', STAGE, '       manifest.partition_end>UINT32_MAX || manifest.partition_start>=manifest.partition_end) {',
     '       manifest.partition_start>=manifest.partition_end) {'),
    ('the slot\'s places are not asked', STAGE,
     'if(!toy_survey_place(c->lba[i])) {retail_display_hex("SURVEY OFF / FILE PLACE",c->lba[i]);return;}',
     '(void)i;'),
    ('the write does not ask the place', STAGE,
     'if(!toy_survey_place(lba) || kui_retail_storage_acquire(&card)!=KUI_LOADER_SD_OK) return -1;',
     'if(kui_retail_storage_acquire(&card)!=KUI_LOADER_SD_OK) return -1;'),
    ('a card without an identity gets a seed', STAGE,
     'retail_display_hex("SURVEY OFF / CARD IDENTITY",(uint32_t)known);return;',
     'retail_display_hex("SURVEY OFF / CARD IDENTITY",(uint32_t)known);'),
    ('an earlier launch\'s seed stays', STAGE, '    memset(&toy_survey_seed,0,sizeof(toy_survey_seed));\n', ''),
    ('the executable\'s checksum is the map\'s', STAGE, '.exec_crc32=boot_crc,', '.exec_crc32=manifest.boot_crc32,'),
    ('audio tracks below the session count as high', STAGE,
     'if(track->start_lba>=manifest.session_lba) ++launch.audio_high;', '++launch.audio_high;'),
    ('a cooked track counts as raw', STAGE,
     '} else if(kui_retail_track_sector_bytes(track)==KUI_GAME_RAW_BYTES &&\n                  kui_retail_track_header_bytes(track)==16u) ++launch.raw;',
     '} else if(kui_retail_track_header_bytes(track)!=24u) ++launch.raw;'),
    ('the survey\'s seed is not handed on', REPORT, '    if(survey)seed=&toy_survey_seed;\n', ''),
    ('a disc that is no source gets no seed', REPORT,
     'if((!toy_suite_source && !survey) || card.transport!=KUI_STORAGE_SCI || seed->test!=test ||',
     'if(!toy_suite_source || card.transport!=KUI_STORAGE_SCI || seed->test!=test ||'),
    ('the survey is not looked for', SCAN,
     'static void toy_movie_scan(void) {toy_movie_scan_source();toy_survey_prepare();}',
     'static void toy_movie_scan(void) {toy_movie_scan_source();}'),
    ('the survey is looked for before the source is known', SCAN,
     'static void toy_movie_scan(void) {toy_movie_scan_source();toy_survey_prepare();}',
     'static void toy_movie_scan(void) {toy_survey_prepare();toy_movie_scan_source();}'),
    ('the probe\'s file is looked for', FAT, '{"SURVEY",{\'S\',\'U\',\'R\',\'V\',\'E\',\'Y\',\' \',\' \',\' \',\' \',\' \'}}',
     '{"WCEDATA",{\'W\',\'C\',\'E\',\'D\',\'A\',\'T\',\'A\',\' \',\' \',\' \',\' \'}}'),
    ('a slot with one blank sector is claimed', CORE, 'if(blank==KUI_TOY_SURVEY_PARTS) {', 'if(blank>=1u) {'),
    ('a slot with two blank sectors is claimed', CORE, 'if(blank==KUI_TOY_SURVEY_PARTS) {', 'if(blank>=2u) {'),
    ('a slot\'s sector that is not read ends the search', CORE,
     'if(++unread==KUI_TOY_SURVEY_UNREAD_MAX) return KUI_TOY_SURVEY_CLAIM_IO;\n                break;',
     'return KUI_TOY_SURVEY_CLAIM_IO;'),
    ('a card that gives nothing is asked for every slot', CORE,
     'if(++unread==KUI_TOY_SURVEY_UNREAD_MAX) return KUI_TOY_SURVEY_CLAIM_IO;\n                break;',
     '++unread;break;'),
    ('a slot whose sector is not read is claimed', CORE,
     'if(++unread==KUI_TOY_SURVEY_UNREAD_MAX) return KUI_TOY_SURVEY_CLAIM_IO;\n                break;',
     'if(++unread==KUI_TOY_SURVEY_UNREAD_MAX) return KUI_TOY_SURVEY_CLAIM_IO;\n                ++blank;at[part]=lba;crc[part]=0u;continue;'),
    ('a file with sectors not read is called full', CORE,
     'return unread?KUI_TOY_SURVEY_CLAIM_IO:KUI_TOY_SURVEY_CLAIM_FULL;', 'return KUI_TOY_SURVEY_CLAIM_FULL;'),
    ('the file\'s size is not asked', CORE,
     'if(kui_toy_survey_file_parse(work,&file) || layout->bytes!=kui_toy_survey_bytes(file.slots))',
     'if(kui_toy_survey_file_parse(work,&file))'),
    ('the first sector is not held to its exact text', CORE,
     'if(memcmp(sector,expect,sizeof(expect))) {memset(out,0,sizeof(*out));return -1;}', ''),
    ('the seed names the launch record\'s sector', CORE,
     'seed->lba[i]=claim->lba[1u+i];seed->placeholder_crc[i]=claim->blank_crc[1u+i];',
     'seed->lba[i]=claim->lba[i];seed->placeholder_crc[i]=claim->blank_crc[i];'),
    ('the seed\'s checksums are the wrong sectors\'', CORE,
     'seed->lba[i]=claim->lba[1u+i];seed->placeholder_crc[i]=claim->blank_crc[1u+i];',
     'seed->lba[i]=claim->lba[1u+i];seed->placeholder_crc[i]=claim->blank_crc[i];'),
    ('the seed has no card', CORE, '    memcpy(seed->cid,cid,sizeof(seed->cid));\n', ''),
    ('the seed\'s capacity flag is lost', CORE, 'seed->high_capacity=high_capacity?1u:0u;', 'seed->high_capacity=0u;'),
    ('the seed is not sealed', CORE, 'seed->crc32=kui_retail_crc32(0u,seed,KUI_TOY_REPORT_CRC_OFFSET);', ''),
    ('a blank sector does not name its part', CORE,
     'struct text t;text_begin(&t,out);\n    slot_line(&t,file,slot,part,"EMPTY");',
     'struct text t;text_begin(&t,out);(void)part;\n    slot_line(&t,file,slot,0u,"EMPTY");'),
    ('a blank sector does not name its slot', CORE,
     'puts_(t,"==== SLOT ");put_decimal(t,slot,3u);', 'puts_(t,"==== SLOT ");put_decimal(t,0u*slot,3u);'),
    ('a blank sector does not name its file', CORE,
     'puts_(t," ==== ID ");put_id(t,file);puts_(t," ==== ");', 'puts_(t," ==== ID ");puts_(t," ==== ");'),
    ('a sector outside the partition is read', CORE,
     'return !*lba || *lba<v->partition_start || *lba>=v->partition_end || v->read(v->context,*lba,work)?-1:0;',
     'return !*lba || v->read(v->context,*lba,work)?-1:0;'),
    ('a count of the map can run the record over', CORE,
     'static void put_count(struct text *t,uint32_t value) {put_decimal(t,value>999u?999u:value,1u);}',
     'static void put_count(struct text *t,uint32_t value) {put_decimal(t,value,1u);}'),
    ('text that is no text goes into the record', CORE, "put(t,c>=0x20u && c<0x7fu?(char)c:'?');", 'put(t,(char)c);'),
    ('the claim is of the slot after', CORE, 'out->file=file;out->slot=slot;out->used=slot;', 'out->file=file;out->slot=slot+1u;out->used=slot;'),
]



def run(tree):
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
    env.setdefault('HOST_CC', 'cc')
    result = subprocess.run([sys.executable, str(tree / 'tests/test_toy_survey.py')], capture_output=True, text=True,
                            env=env)
    return result.returncode == 0, result.stdout + result.stderr


def check(work, only):
    tree = work / 'tree'
    for part in ('include', 'src', 'tests', 'tools', 'config'):
        shutil.copytree(ROOT / part, tree / part)
    for dependency in ('.deps', 'fatfs'):
        if (ROOT / dependency).exists():
            (tree / dependency).symlink_to(ROOT / dependency)
    names = [mutation[0] for mutation in MUTATIONS]
    assert len(set(names)) == len(names), 'two mutations share a name'
    passed, output = run(tree)
    if not passed:
        print('FAILS   the tests without a mutation\n' + output[-800:], flush=True)
        return 1
    print('passes  the tests without a mutation', flush=True)
    caught = missed = stale = 0
    for name, path, old, new in MUTATIONS:
        if only and only not in name:
            continue
        source = tree / path
        text = source.read_text()
        if text.count(old) != 1:
            print('STALE   %s (%d places in %s)' % (name, text.count(old), path), flush=True)
            stale += 1
            continue
        source.write_text(text.replace(old, new))
        try:
            passed, output = run(tree)
        finally:
            source.write_text(text)
        if passed:
            missed += 1
            print('MISSED  ' + name, flush=True)
        else:
            caught += 1
            lines = [line for line in output.split('\n') if 'FAIL' in line or 'error:' in line or 'ssert' in line]
            print('caught  %-52s %s' % (name, lines[0].strip()[:90] if lines else 'the test stopped'), flush=True)
    print('%d mutations: %d caught, %d missed, %d stale' % (caught + missed + stale, caught, missed, stale))
    return 1 if missed or stale else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--work', type=Path, help='an empty or absent directory to copy the sources into')
    parser.add_argument('--only', default='', help='run the mutations whose name contains this text')
    args = parser.parse_args()
    if args.work:
        if args.work.exists():
            shutil.rmtree(args.work)
        args.work.mkdir(parents=True)
        return check(args.work.resolve(), args.only)
    with tempfile.TemporaryDirectory() as scratch:
        return check(Path(scratch), args.only)


if __name__ == '__main__':
    sys.exit(main())
