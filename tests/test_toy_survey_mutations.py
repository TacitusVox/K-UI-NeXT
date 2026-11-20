#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Defects put one at a time into Test 83's own sources, the lookup it shares and its tool: the survey's
host tests must notice each.

usage: test_toy_survey_mutations.py [--work DIRECTORY] [--only TEXT] [--jobs N]

The repository is not changed: each mutation is applied to a copy of the sources
(include, src, tests, tools, config) and tests/test_toy_survey.py is run there;
with --jobs, in as many copies at once.
A mutation the compiler refuses shows nothing about the tests: it is counted
apart, as BROKEN, and fails the check as a missed one does.
Run by make -f Makefile.vmu-native-toy-suite toy-wire-save-mutation-check.
"""
import argparse
import concurrent.futures
import os
import queue
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STAGE, CORE, REPORT, SCAN, FAT = ('src/loader/toy_survey_stage.inc', 'src/core/toy_survey.c',
                                  'src/loader/toy_suite_report_stage.inc', 'src/loader/toy_movie_iso_stage.inc',
                                  'src/core/toy_survey_fat.c')
# The lookup the survey shares with the Windows CE data probe, as the survey uses it; and the tool.
LOOKUP, TOOL = 'src/core/wce_probe_fat.c', 'tools/toy_survey.py'
PLACE = 'if(!lba || lba<manifest.partition_start || lba>=manifest.partition_end ||'
EXTENTS = 'for(uint32_t j=0u;j<manifest.extent_count;j++) {'
RAW = ('} else if(kui_retail_track_sector_bytes(track)==KUI_GAME_RAW_BYTES &&\n'
       '                  kui_retail_track_header_bytes(track)==16u) ++launch.raw;')
CARD_WRITE = 'enum kui_loader_sd_result result=kui_sci_sd_write(sd,lba,1u,data);'
CARD_READ = 'enum kui_loader_sd_result read=kui_retail_storage_read_run(&card,lba,1u,out);'
SEED = 'kui_toy_survey_seed_build(c,identity.cid,identity.high_capacity,&toy_survey_seed);'
REPORT_TEXT = 'data.startswith(REPORT_HEAD) and data.endswith(REPORT_TAIL)'
LAUNCH_HEAD = "if not data.startswith(head) or data[-1:] != b'\\n':"
CLAIM_READ = 'return !*lba || *lba<v->partition_start || *lba>=v->partition_end || v->read(v->context,*lba,work)?-1:0;'
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
    ('the block path is not given back', STAGE, '    sd->bus.transfer_block=block;\n', '    (void)block;\n'),
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
     'static void toy_movie_scan(void) {toy_movie_scan_source();(void)toy_survey_prepare;}'),
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
    ('the seed has no card', CORE, '    memcpy(seed->cid,cid,sizeof(seed->cid));\n', '    (void)cid;\n'),
    ('the seed\'s capacity flag is lost', CORE, 'seed->high_capacity=high_capacity?1u:0u;',
     'seed->high_capacity=0u*high_capacity;'),
    ('the seed is not sealed', CORE, 'seed->crc32=kui_retail_crc32(0u,seed,KUI_TOY_REPORT_CRC_OFFSET);', ''),
    ('a blank sector does not name its part', CORE,
     'struct text t;text_begin(&t,out);\n    slot_line(&t,file,slot,part,"EMPTY");',
     'struct text t;text_begin(&t,out);(void)part;\n    slot_line(&t,file,slot,0u,"EMPTY");'),
    ('a blank sector does not name its slot', CORE,
     'puts_(t,"==== SLOT ");put_decimal(t,slot,3u);', 'puts_(t,"==== SLOT ");put_decimal(t,0u*slot,3u);'),
    ('a blank sector does not name its file', CORE,
     'puts_(t," ==== ID ");put_id(t,file);puts_(t," ==== ");', 'puts_(t," ==== ID ");(void)file;puts_(t," ==== ");'),
    ('a sector outside the partition is read', CORE,
     'return !*lba || *lba<v->partition_start || *lba>=v->partition_end || v->read(v->context,*lba,work)?-1:0;',
     'return !*lba || v->read(v->context,*lba,work)?-1:0;'),
    ('a count of the map can run the record over', CORE,
     'static void put_count(struct text *t,uint32_t value) {put_decimal(t,value>999u?999u:value,1u);}',
     'static void put_count(struct text *t,uint32_t value) {put_decimal(t,value,1u);}'),
    ('text that is no text goes into the record', CORE, "put(t,c>=0x20u && c<0x7fu?(char)c:'?');", 'put(t,(char)c);'),
    ('the claim is of the slot after', CORE, 'out->file=file;out->slot=slot;out->used=slot;', 'out->file=file;out->slot=slot+1u;out->used=slot;'),
    # A card with a partition table: the volume does not begin at the card's first sector.
    ('the record is written at its place in the partition', STAGE,
     'if(toy_survey_write(c->lba[0],toy_survey_text)) {',
     'if(toy_survey_write(c->lba[0]-(uint32_t)manifest.partition_start,toy_survey_text)) {'),
    ('the record is read back from its place in the partition', STAGE,
     'if(toy_survey_read(NULL,c->lba[0],toy_survey_work) ||',
     'if(toy_survey_read(NULL,c->lba[0]-(uint32_t)manifest.partition_start,toy_survey_work) ||'),
    ('the lookup is given the card, not the partition', STAGE,
     'const struct kui_wce_probe_volume volume={(uint32_t)manifest.partition_start,',
     'const struct kui_wce_probe_volume volume={0u,'),
    ('the lookup places an exFAT file from the card\'s first sector', LOOKUP,
     'f->data_lba=v->partition_start+le32(b+0x58u);', 'f->data_lba=le32(b+0x58u);'),
    ('the lookup places a FAT32 file from the card\'s first sector', LOOKUP,
     'f->data_lba=v->partition_start+data;', 'f->data_lba=data;'),
    ('the card\'s first sector is taken', STAGE, PLACE, 'if(lba<manifest.partition_start || lba>=manifest.partition_end ||'),
    ('a sector before the partition is taken', STAGE, PLACE, 'if(!lba || lba>=manifest.partition_end ||'),
    ('a sector behind the partition is taken', STAGE, PLACE, 'if(!lba || lba<manifest.partition_start ||'),
    ('a sector before the partition is read', CORE, CLAIM_READ,
     'return !*lba || *lba>=v->partition_end || v->read(v->context,*lba,work)?-1:0;'),
    ('a sector behind the partition is read', CORE, CLAIM_READ,
     'return !*lba || *lba<v->partition_start || v->read(v->context,*lba,work)?-1:0;'),
    ('the card is written at the sector\'s place in the partition', STAGE, CARD_WRITE,
     'enum kui_loader_sd_result result=kui_sci_sd_write(sd,lba-(uint32_t)manifest.partition_start,1u,data);'),
    ('the card is read behind the sector by the partition\'s start', STAGE, CARD_READ,
     'enum kui_loader_sd_result read=kui_retail_storage_read_run(&card,lba+(uint32_t)manifest.partition_start,1u,out);'),
    ('the seed names places counted from the partition', STAGE, SEED,
     'toy_survey_claim.lba[1]-=(uint32_t)manifest.partition_start;'
     'toy_survey_claim.lba[2]-=(uint32_t)manifest.partition_start;\n    ' + SEED),
    ('the partition\'s end is one sector late for a write', STAGE, PLACE,
     'if(!lba || lba<manifest.partition_start || lba>manifest.partition_end ||'),
    ('the partition\'s end is one sector late for a read', CORE, CLAIM_READ,
     'return !*lba || *lba<v->partition_start || *lba>v->partition_end || v->read(v->context,*lba,work)?-1:0;'),
    ('the partition\'s start is one sector late for a read', CORE, CLAIM_READ,
     'return !*lba || *lba<=v->partition_start || *lba>=v->partition_end || v->read(v->context,*lba,work)?-1:0;'),
    # A map with more than one extent, and tracks of every kind.
    ('only the game\'s last extent is asked', STAGE, EXTENTS,
     'for(uint32_t j=manifest.extent_count?manifest.extent_count-1u:0u;j<manifest.extent_count;j++) {'),
    ('every data track that is not raw counts as cooked', STAGE,
     'else if(kui_retail_track_sector_bytes(track)==KUI_GAME_DATA_BYTES) ++launch.cooked;\n        else ++launch.other;',
     'else ++launch.cooked;'),
    ('the title is cut at 40 characters', CORE, 'put_field(&t,launch->title,launch->title_bytes,56u);',
     'put_field(&t,launch->title,launch->title_bytes,40u);'),
    ('the launch line\'s tens are the slot\'s hundreds', STAGE, "line[n++]=(char)('0'+slot/10u%10u);",
     "line[n++]=(char)('0'+slot/100u%10u);"),
    ('a slot count above 989 is refused', CORE, 'if(!count || count>KUI_TOY_SURVEY_SLOTS_MAX) return -1;',
     'if(!count || count>989u) return -1;'),
    ('only the game\'s first extent is asked', STAGE, EXTENTS, 'for(uint32_t j=0u;j<manifest.extent_count && j<1u;j++) {'),
    ('the game\'s last extent is not asked', STAGE, EXTENTS, 'for(uint32_t j=0u;j+1u<manifest.extent_count;j++) {'),
    ('an audio track at the session\'s first sector is no CD music', STAGE,
     'if(track->start_lba>=manifest.session_lba) ++launch.audio_high;',
     'if(track->start_lba>manifest.session_lba) ++launch.audio_high;'),
    ('a Mode 2 track counts as raw', STAGE, RAW,
     '} else if(kui_retail_track_sector_bytes(track)==KUI_GAME_RAW_BYTES) ++launch.raw;'),
    ('a track of another sector size counts as cooked', STAGE,
     'else if(kui_retail_track_sector_bytes(track)==KUI_GAME_DATA_BYTES) ++launch.cooked;',
     'else if(kui_retail_track_sector_bytes(track)!=KUI_GAME_RAW_BYTES) ++launch.cooked;'),
    ('the title is given the product\'s length', STAGE,
     '.title=manifest.title,.title_bytes=sizeof(manifest.title),', '.title=manifest.title,.title_bytes=sizeof(manifest.product),'),
    ('the title is not cut', CORE, 'put_field(&t,launch->title,launch->title_bytes,56u);',
     'put_field(&t,launch->title,launch->title_bytes,128u);'),
    ('the boot file\'s name is not cut', CORE, 'put_field(&t,launch->bootfile,launch->bootfile_bytes,16u);',
     'put_field(&t,launch->bootfile,launch->bootfile_bytes,24u);'),
    ('the region is not cut', CORE, 'put_field(&t,launch->region,launch->region_bytes,8u);',
     'put_field(&t,launch->region,launch->region_bytes,16u);'),
    ('the launch line has no tens of the slot', STAGE, "line[n++]=(char)('0'+slot/10u%10u);", "line[n++]='0';"),
    ('the launch line has no hundreds of the slot', STAGE, "line[n++]=(char)('0'+slot/100u%10u);", "line[n++]='0';"),
    ('a file in more pieces than the lookup maps is taken', STAGE, 'if(found!=KUI_WCE_PROBE_FAT_OK) {',
     'if(found!=KUI_WCE_PROBE_FAT_OK && found!=KUI_WCE_PROBE_FAT_FRAGMENTED) {'),
    ('the search gives up at five sectors not read', 'include/kui/toy_survey.h',
     '#define KUI_TOY_SURVEY_UNREAD_MAX 4u', '#define KUI_TOY_SURVEY_UNREAD_MAX 5u'),
    # A read whose stream the card does not end.
    ('a read counts whose stop the card did not take', STAGE,
     'return read==KUI_LOADER_SD_OK && stop==KUI_LOADER_SD_OK?0:-1;', 'return (void)stop,read==KUI_LOADER_SD_OK?0:-1;'),
    ('a read counts that failed and was stopped', STAGE,
     'return read==KUI_LOADER_SD_OK && stop==KUI_LOADER_SD_OK?0:-1;', 'return (void)read,stop==KUI_LOADER_SD_OK?0:-1;'),
    # Started from Card Tools, its seed patched into the module.
    ('a seed from Card Tools is refused without the survey\'s file', REPORT,
     'if((!toy_suite_source && !survey) || card.transport!=KUI_STORAGE_SCI || seed->test!=test ||',
     'if(!survey || card.transport!=KUI_STORAGE_SCI || seed->test!=test ||'),
    ('a disc that is no source takes a seed from Card Tools', REPORT,
     'if((!toy_suite_source && !survey) || card.transport!=KUI_STORAGE_SCI || seed->test!=test ||',
     'if(card.transport!=KUI_STORAGE_SCI || seed->test!=test ||'),
    ('a seed from Card Tools comes before the survey\'s', REPORT,
     '    if(survey)seed=&toy_survey_seed;\n', '    if(survey && !toy_report_seed.test)seed=&toy_survey_seed;\n'),
    ('every launch is taken for a surveyed one', REPORT, 'bool survey=toy_survey_seed.test!=0u;', 'bool survey=true;'),
    # tools/toy_survey.py, which reads the file the owner sends back.
    ('the tool calls a cut-off report saved', TOOL,
     "                elif blank[1] or blank[2]:\n                    entry['state'] = 'launched, report cut off'\n", ''),
    ('the tool calls any two sectors a report', TOOL, 'elif is_report(parts[1] + parts[2]):', 'elif True:'),
    ('the tool asks no report for its first line', TOOL, REPORT_TEXT, 'data.endswith(REPORT_TAIL)'),
    ('the tool asks no report for its last line', TOOL, REPORT_TEXT, 'data.startswith(REPORT_HEAD)'),
    ('the tool lists a damaged slot as empty', TOOL, "                entry['state'] = 'damaged'",
     "                entry['state'] = 'empty'"),
    ('the tool takes a report without a launch record for a launch', TOOL,
     'launch = None if blank[0] else parse_launch(file_id, slot, parts[0])',
     'launch = {} if blank[0] else parse_launch(file_id, slot, parts[0])'),
    ('the tool takes any slot\'s launch record', TOOL, LAUNCH_HEAD, "if data[-1:] != b'\\n':"),
    ('the tool takes a launch record that does not end its line', TOOL, LAUNCH_HEAD, 'if not data.startswith(head):'),
    ('the tool does not say where a launch record stops', TOOL, "            record['damaged'] = name\n", ''),
    ('the tool does not list a damaged launch record', TOOL,
     "        if 'damaged' in launch:\n            line += '; LAUNCH RECORD DAMAGED at its %s line' % launch['damaged']\n", ''),
    ('the tool does not hold the first sector to its text', TOOL,
     "    if data != file_sector(file_id, slots):\n"
     "        raise ValueError('the first sector is not what its ID and slot count give')\n", ''),
    ('the tool does not ask the file\'s size', TOOL, '    if len(data) != SECTOR * (1 + PARTS * slots):', '    if False:'),
    ('the tool makes 20 slots whatever is asked', TOOL, 'data = blank_file(file_id, args.slots)', 'data = blank_file(file_id, 20)'),
    ('the tool\'s blank file has 1000 slots unless told', TOOL, 'DEFAULT_SLOTS = 100\n', 'DEFAULT_SLOTS = 1000\n'),
    ('the tool counts slots modulo 100', TOOL, '% (slot, part, file_id.hex().upper(), state)',
     '% (slot % 100, part, file_id.hex().upper(), state)'),
    ('the tool\'s CD music column is every audio track', TOOL, "launch.get('audio_high', '?'))", "launch.get('audio', '?'))"),
    ('the tool gives every blank file one ID', TOOL,
     'file_id = bytes.fromhex(args.id) if args.id else secrets.token_bytes(16)',
     'file_id = bytes.fromhex(args.id) if args.id else bytes(16)'),
    ('the tool takes an ID of any length', TOOL,
     "            if args.id is not None and not re.fullmatch('[0-9A-Fa-f]{32}', args.id):\n"
     "                raise ValueError('--id takes 32 hex digits')\n", ''),
]



def run(tree):
    # No bytecode kept: every run reads the tool's text as it stands in its copy.
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', PYTHONDONTWRITEBYTECODE='1')
    env.setdefault('HOST_CC', 'cc')
    result = subprocess.run([sys.executable, str(tree / 'tests/test_toy_survey.py')], capture_output=True, text=True,
                            env=env)
    return result.returncode == 0, result.stdout + result.stderr


# A line of the compiler's, the linker's or Python's own refusal, not of a test that ran
# (UBSan says "runtime error:").
COMPILER = re.compile(r':\d+(:\d+)?: (fatal )?error: |collect2: error: |ld: error: |clang: error: |ld returned '
                      r'|^SyntaxError|^IndentationError')


def copy_tree(tree):
    for part in ('include', 'src', 'tests', 'tools', 'config'):
        shutil.copytree(ROOT / part, tree / part)
    for dependency in ('.deps', 'fatfs'):
        if (ROOT / dependency).exists():
            (tree / dependency).symlink_to(ROOT / dependency)


def check(work, only, jobs):
    names = [mutation[0] for mutation in MUTATIONS]
    assert len(set(names)) == len(names), 'two mutations share a name'
    chosen = [mutation for mutation in MUTATIONS if not only or only in mutation[0]]
    jobs = max(1, min(jobs, len(chosen) or 1))
    trees = queue.Queue()
    for index in range(jobs):
        copy_tree(work / ('tree%d' % index))
        trees.put(work / ('tree%d' % index))
    first = trees.get()
    passed, output = run(first)
    trees.put(first)
    if not passed:
        print('FAILS   the tests without a mutation\n' + output[-800:], flush=True)
        return 1
    print('passes  the tests without a mutation', flush=True)

    def one(mutation):
        name, path, old, new = mutation
        text = (ROOT / path).read_text()
        if text.count(old) != 1:
            return 'STALE', '%d places in %s' % (text.count(old), path)
        tree = trees.get()
        try:
            (tree / path).write_text(text.replace(old, new))
            try:
                passed, output = run(tree)
            finally:
                (tree / path).write_text(text)
        finally:
            trees.put(tree)
        lines = [line.replace(str(tree) + '/', '') for line in output.split('\n')]
        refused = [line for line in lines if COMPILER.search(line)]
        if passed:
            return 'MISSED', ''
        if refused:
            return 'BROKEN', 'does not compile: ' + refused[0].strip()
        said = [line for line in lines if 'FAIL' in line or 'error:' in line or 'ssert' in line]
        return 'caught', said[0].strip() if said else 'the test stopped'

    counts = {'caught': 0, 'MISSED': 0, 'STALE': 0, 'BROKEN': 0}
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        for mutation, (verdict, said) in zip(chosen, pool.map(one, chosen)):
            counts[verdict] += 1
            print(('%-7s %-58s %s' % (verdict, mutation[0], said[:150])).rstrip(), flush=True)
    print('%d mutations: %d caught, %d missed, %d stale, %d that do not compile'
          % (len(chosen), counts['caught'], counts['MISSED'], counts['STALE'], counts['BROKEN']))
    return 1 if counts['MISSED'] or counts['STALE'] or counts['BROKEN'] else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--work', type=Path, help='an empty or absent directory to copy the sources into')
    parser.add_argument('--only', default='', help='run the mutations whose name contains this text')
    parser.add_argument('--jobs', type=int, default=min(4, os.cpu_count() or 1),
                        help='copies of the sources to test at once (default: %(default)s)')
    args = parser.parse_args()
    if args.work:
        if args.work.exists():
            shutil.rmtree(args.work)
        args.work.mkdir(parents=True)
        return check(args.work.resolve(), args.only, args.jobs)
    with tempfile.TemporaryDirectory() as scratch:
        return check(Path(scratch), args.only, args.jobs)


if __name__ == '__main__':
    sys.exit(main())
