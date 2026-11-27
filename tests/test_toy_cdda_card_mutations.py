#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Whether Test 85's tests notice a defect in the audio's card work: each of a
list put into a copy of the sources, and the test that should notice built and
run there. The repository's own files are never written.

usage: test_toy_cdda_card_mutations.py [--jobs N] [--work DIR] [--only TEXT]

The auxiliary's part (src/loader/toy_cdda_aux_card.inc and its two places in
the dispatcher), the worker's (src/loader/toy_cdda_card.c) and the shared
block's place (include/kui/toy_cdda_mail.h) are held against the audio's
scenarios on the integration model (toy-cdda-card-host-test); the consumer's
two calls in the worker's completion (src/loader/vmu_native_passive.c) against
the gesture test (toy-survey-mixed-pad-host-test). Every mutation must still
compile and must make its test fail (Makefile.vmu-native-toy-suite,
toy-cdda-card-mutation-check).

Left out as equivalent, or as no rule of this test's:
- take-over without the test of the resident's hook (hooked): in this reader
  hook() always sets it, and the armed level is tested beside it;
- the error of a declined stream kept although the interrupt's route was not
  there: the stream is not asked then, and its last error is none;
- halt without forgetting the blocks the low reader kept: as toy_wire_halt
  does, and the stream never writes the low reader's areas;
- halt giving up a token search at another block than the audio's: the
  audio's search stands at its own block;
- a segment's count of failed reads not begun again with the segment: every
  segment begins with a read that gets through, which begins it;
- the claim asked with no audio (the dispatcher's test of busy), a delivery
  kept while only a search of the audio's waits, and the delivery that ends
  the audio's stream for a read kept too: nothing of a visit's own work is
  left out or added, only its time (the read is begun by the game's next
  call);
- a stream that cannot be followed read on: the block is then not one the
  stream gave, and its kind is not defined;
- the disc's table read without asking whether one was seen: none seen has
  no tracks.
"""
import argparse
import concurrent.futures
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECIPE = ("TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 "
          "TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 "
          "TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 TOY_WIRE_SAVE=1 KUI_NATIVE_BACKGROUND_MAP160=1 TOY_SURVEY_MIXED_PAD=1 "
          "TOY_CDDA_ROOM=1 TOY_CDDA_HOOK=1 TOY_CDDA_DRIVE=1 TOY_CDDA_KEEP=1 TOY_CDDA_SURVEY=1 TOY_CDDA_CARD=1").split()
AUX = "src/loader/toy_cdda_aux_card.inc"
DISPATCH = "src/loader/native_readahead_aux.c"
WORKER = "src/loader/toy_cdda_card.c"
MAIL = "include/kui/toy_cdda_mail.h"
PASSIVE = "src/loader/vmu_native_passive.c"
CARD = ("toy-cdda-card-host-test", "test-toy-wire-integration-card")
SITES = ("toy-survey-mixed-pad-host-test", "test-survey-mixed-pad-card")

WANTED = "(s->pending && (s->command==KUI_GD_PIOREAD || s->command==KUI_GD_DMAREAD))"
KIND = "if(t->control&(4u|KUI_RETAIL_TRACK_COOKED|KUI_RETAIL_TRACK_2336|KUI_RETAIL_TRACK_MODE2))"
PIECES = "if(first<map->track_count || first>=used || t->extent_count>used-first)"
RANGE = "if(m->want_lba<t->start_lba || m->want_end>t->end_lba || m->want_end<=m->want_lba)"
GUARDS = ("    if(!m->want_blocks || toy_cdda_wanted(r) || e->in_irq || !e->opened || r->shared.port.fault ||\n"
          "       !r->shared.port.acquired || r->shared.stream.toy_suite_active)return;")
FOREIGN = "if(C.config.ops->busy() || (r->shared.stream.state>RA_RESUME && !CD.busy))return;"
KINDS = "if(parse!=KUI_WIRE_BLOCK || (block.kind!=KUI_WIRE_CLEAN && block.kind!=KUI_WIRE_REBUILT)) {"
ON = ("        if(!toy_cdda_feed(m,block.data) || CD.cursor.block!=next) {\n"
      "            toy_cdda_halt(r,&m->other_halts);\n            break;\n        }")
MORE = "        if(B.state!=KUI_TOY_CDDA_CARD_STREAMING || B.elapsed+M.round_longest>B.window)break;"
FAST = ("        if(!kui_vmu_native_early_server(NATIVE_HOT_CONFIG.driver.family) ||\n"
        "           (controller_pair.frames[controller_pair.parent].header&255u)==9u)\n"
        "            kui_toy_cdda_consumer_heartbeat();")
MIXED = "        if(pad_sample>=0) kui_toy_cdda_consumer_heartbeat();"
SURVEY_FAST = ("        if(!kui_vmu_native_early_server(NATIVE_HOT_CONFIG.driver.family) ||\n"
               "           (controller_pair.frames[controller_pair.parent].header&255u)==9u)\n"
               "            kui_toy_cdda_survey_heartbeat();")
SURVEY_MIXED = "        if(pad_sample>=0) kui_toy_cdda_survey_heartbeat();"
COMPLETE = "        if(!controller_complete(f)) return;"


def aux(name, old, new):
    return (name, CARD, [(AUX, old, new)])


def worker(name, old, new):
    return (name, CARD, [(WORKER, old, new)])


MUTATIONS = [
    # ---- the auxiliary: when a read of the game's stands ----
    aux("a read the engine has begun does not end the audio", "    return r->engine.active || s->executing ||\n",
        "    return s->executing ||\n"),
    aux("a read being executed does not", "    return r->engine.active || s->executing ||\n",
        "    return r->engine.active ||\n"),
    aux("a read the service has accepted does not", WANTED, "0"),
    aux("a DMA read is no read", WANTED, "(s->pending && s->command==KUI_GD_PIOREAD)"),
    aux("a PIO read is no read", WANTED, "(s->pending && s->command==KUI_GD_DMAREAD)"),
    aux("any pending command holds the audio back", WANTED, "(s->pending!=0u)"),
    # ---- a stream the reader ended ----
    aux("a stream the reader ended goes unnoticed",
        "    if(!(CD.busy&TOY_CDDA_OWNS) || toy_wire_keep()->on)return;\n    CD.busy=0u;", "    if(1)return;\n    CD.busy=0u;"),
    aux("an ended stream noticed, and still said to stream",
        "    CD.busy=0u;\n    if(CD.state==KUI_TOY_CDDA_CARD_STREAMING)CD.state=KUI_TOY_CDDA_CARD_SEEKING;\n    ++m->external_halts;",
        "    CD.busy=0u;\n    ++m->external_halts;"),
    aux("an ended stream not counted", "    ++m->external_halts;\n", "    (void)m;\n"),
    # ---- ending the audio's use of the card ----
    aux("halt: the stream is left running", "    if(!kui_wire_stop(toy_wire_stream()))r->shared.port.fault=true;\n", ""),
    aux("halt: a stream that does not stop is no fault",
        "    if(!kui_wire_stop(toy_wire_stream()))r->shared.port.fault=true;\n", "    (void)kui_wire_stop(toy_wire_stream());\n"),
    aux("halt: the low reader is not told the card's place is lost",
        "    stream->state=RA_LOST;\n    stream->ready[0]=stream->ready[1]=0u;", "    stream->ready[0]=stream->ready[1]=0u;"),
    aux("halt: the stream's flag is left set", "    toy_wire_keep()->on=0u;\n}", "}"),
    aux("halt: a search left waiting stands in the read's way",
        "        if(stream->state==AHEAD_TOKEN_WAIT && stream->position==CD.cursor.block)stream->state=RA_LOST;\n", ""),
    aux("halt: still said to stream",
        "    if(CD.state==KUI_TOY_CDDA_CARD_STREAMING)CD.state=KUI_TOY_CDDA_CARD_SEEKING;\n    /* The SCI did not return",
        "    /* The SCI did not return"),
    aux("halt: not counted", "    ++*counted;\n", "    (void)counted;\n"),
    aux("halt: the audio still thinks the card its own", "    if(!busy)return;\n    CD.busy=0u;\n", "    if(!busy)return;\n"),
    # ---- the sink ----
    aux("bytes written past the sink's end", "count>KUI_TOY_CDDA_SINK_BYTES-at || ", ""),
    aux("a sink that says it is over-full written to", "at>KUI_TOY_CDDA_SINK_BYTES || ", ""),
    aux("bytes that do not follow on written", " || offset!=m->sink_offset+at", " || (offset!=m->sink_offset+at && !count)"),
    aux("a write refused, and the segment goes on",
        "C.config.ops->feed(&CD.cursor,data)!=KUI_GAME_OK || CD.state==KUI_TOY_CDDA_CARD_REFUSED",
        "C.config.ops->feed(&CD.cursor,data)!=KUI_GAME_OK"),
    aux("a block the map does not hold, and the segment goes on",
        "C.config.ops->feed(&CD.cursor,data)!=KUI_GAME_OK || CD.state==KUI_TOY_CDDA_CARD_REFUSED",
        "(C.config.ops->feed(&CD.cursor,data),CD.state==KUI_TOY_CDDA_CARD_REFUSED)"),
    aux("a segment refused in its course gives no reason", "        m->why=KUI_TOY_CDDA_WHY_FEED;\n", ""),
    aux("a segment refused in its course is not counted",
        "        m->why=KUI_TOY_CDDA_WHY_FEED;\n        ++m->refused;\n", "        m->why=KUI_TOY_CDDA_WHY_FEED;\n"),
    aux("a segment never done", "    if(CD.cursor.done==CD.cursor.count) {", "    if(0) {"),
    # ---- the segment wanted ----
    aux("a track number beyond the map", "if(index>=map->track_count || used>KUI_RETAIL_MANIFEST_SLOTS)",
        "if(used>KUI_RETAIL_MANIFEST_SLOTS)"),
    aux("a map with more slots than there are", "if(index>=map->track_count || used>KUI_RETAIL_MANIFEST_SLOTS)",
        "if(index>=map->track_count)"),
    aux("a data track's sectors", KIND, KIND.replace("4u|", "")),
    aux("sectors of 2,048 bytes", KIND, KIND.replace("KUI_RETAIL_TRACK_COOKED|", "")),
    aux("sectors of 2,336 bytes", KIND, KIND.replace("KUI_RETAIL_TRACK_2336|", "")),
    aux("mode 2 sectors", KIND, KIND.replace("|KUI_RETAIL_TRACK_MODE2", "")),
    aux("a track without its file", "    if(!t->extent_count)return KUI_TOY_CDDA_WHY_UNMAPPED;\n", ""),
    aux("pieces among the tracks' slots", PIECES, "if(first>=used || t->extent_count>used-first)"),
    aux("pieces behind the map's own", PIECES, "if(first<map->track_count || t->extent_count>used-first)"),
    aux("more pieces than the map has", PIECES, "if(first<map->track_count || first>=used)"),
    aux("sectors before the track", RANGE, "if(m->want_end>t->end_lba || m->want_end<=m->want_lba)"),
    aux("sectors behind the track", RANGE, "if(m->want_lba<t->start_lba || m->want_end<=m->want_lba)"),
    aux("no sectors", RANGE, "if(m->want_lba<t->start_lba || m->want_end>t->end_lba)"),
    aux("a cursor that could not be begun", "==KUI_GAME_OK?KUI_TOY_CDDA_WHY_NONE:KUI_TOY_CDDA_WHY_BEGIN;",
        "==KUI_GAME_OK?KUI_TOY_CDDA_WHY_NONE:KUI_TOY_CDDA_WHY_NONE;"),
    # ---- taking the stream over ----
    aux("a stream begun without the reader's interrupt route",
        "    if(!r->engine.hooked || r->engine.release.armed!=0x10u ||\n", "    if(!r->engine.hooked ||\n"),
    aux("a declined stream's error not kept",
        "            m->first_error=w->error;\n            m->first_detail=w->detail;\n        }\n        ++m->declined;",
        "        }\n        ++m->declined;"),
    aux("a declined stream not counted", "        ++m->declined;\n", ""),
    aux("the resident not told that the stream receives", "    r->shared.stream.state=RA_DMA;\n", ""),
    aux("the stream's flag not set", "    toy_wire_keep()->on=1u;\n", ""),
    aux("the audio does not know the stream its own", "    CD.busy=TOY_CDDA_OWNS;\n", ""),
    aux("the card not said to stream",
        "    CD.busy=TOY_CDDA_OWNS;\n    CD.state=KUI_TOY_CDDA_CARD_STREAMING;\n", "    CD.busy=TOY_CDDA_OWNS;\n"),
    # ---- bringing the card to the block ----
    aux("a block by programmed transfers the worker did not allow", "        if(!m->seek_ok)return false;\n", ""),
    aux("the allowance not spent", "        m->seek_ok=0u;\n", ""),
    aux("the card's wait not cut into slices",
        "            stream->token_bounded=1u;\n            stream->token_budget=KUI_SCI_STREAM_TOKEN_SLICE;\n", ""),
    aux("a slice four times as long", "            stream->token_budget=KUI_SCI_STREAM_TOKEN_SLICE;\n",
        "            stream->token_budget=4u*KUI_SCI_STREAM_TOKEN_SLICE-1u;\n"),
    aux("the low reader's bound not put back", "        stream->token_bounded=bounded;\n", "        (void)bounded;\n"),
    aux("the low reader's budget not put back", "        stream->token_budget=budget;\n", "        (void)budget;\n"),
    aux("a block received by the channel, not by programmed transfers",
        "result=ops->fetch(block,AHEAD_TOKEN_LIMIT,true);", "result=ops->fetch(block,AHEAD_TOKEN_LIMIT,false);"),
    aux("the audio forgets that its read waits",
        "        CD.busy=!data && result==KUI_SCI_STREAM_PENDING?TOY_CDDA_SEEKS:0u;", "        CD.busy=0u;"),
    aux("a read that waits counted as failed",
        "            if(CD.busy)++m->seek_waits;\n            else if(++CD.fails>=TOY_CDDA_SEEK_FAILS) {",
        "            if(++CD.fails>=TOY_CDDA_SEEK_FAILS) {"),
    aux("a wait not counted", "            if(CD.busy)++m->seek_waits;\n", "            if(CD.busy)(void)0;\n"),
    aux("given up at the first failed read", "enum { TOY_CDDA_SEEK_FAILS=8u };", "enum { TOY_CDDA_SEEK_FAILS=1u };"),
    aux("given up one failed read late", "enum { TOY_CDDA_SEEK_FAILS=8u };", "enum { TOY_CDDA_SEEK_FAILS=9u };"),
    aux("failed reads never forgotten", "        CD.fails=0u;\n        ++m->seek_blocks;", "        ++m->seek_blocks;"),
    aux("a block given up gives no reason", "                m->why=KUI_TOY_CDDA_WHY_CARD;\n", ""),
    aux("a block given up, and the segment not refused",
        "                CD.state=KUI_TOY_CDDA_CARD_REFUSED;\n                m->why=KUI_TOY_CDDA_WHY_CARD;",
        "                m->why=KUI_TOY_CDDA_WHY_CARD;"),
    aux("a block read by programmed transfers not counted", "        ++m->seek_blocks;\n", ""),
    aux("the time of a programmed read not counted", "        m->seek_ticks+=tick-toy_wire_phase();\n", "        (void)tick;\n"),
    aux("the stream takes over wherever the card stands",
        "    if(stream->state!=RA_PAUSED || stream->position!=CD.cursor.block) {\n        if(!m->seek_ok)return false;",
        "    if(stream->state!=RA_PAUSED) {\n        if(!m->seek_ok)return false;"),
    aux("the stream takes over whatever the low reader does",
        "    if(stream->state!=RA_PAUSED || stream->position!=CD.cursor.block) {\n        if(!m->seek_ok)return false;",
        "    if(stream->position!=CD.cursor.block) {\n        if(!m->seek_ok)return false;"),
    aux("the stream takes over behind a block the file does not go on from",
        "        if(!toy_cdda_feed(m,data) || stream->state!=RA_PAUSED || stream->position!=CD.cursor.block)\n",
        "        if(!toy_cdda_feed(m,data))\n"),
    aux("a stream begun behind the segment's last block",
        "        if(!toy_cdda_feed(m,data) || stream->state!=RA_PAUSED || stream->position!=CD.cursor.block)\n",
        "        (void)toy_cdda_feed(m,data);\n        if(stream->state!=RA_PAUSED || stream->position!=CD.cursor.block)\n"),
    # ---- a heartbeat's visit ----
    aux("the shared block taken on trust", "    if(!m || m->magic!=KUI_TOY_CDDA_MAIL_MAGIC)return;", "    if(!m)return;"),
    aux("another segment, and the stream goes on",
        "        toy_cdda_halt(r,&m->other_halts);\n        CD.generation=m->have_generation=m->want_generation;",
        "        CD.generation=m->have_generation=m->want_generation;"),
    aux("the sink not emptied for another segment", "        m->sink_offset=m->sink_bytes=0u;\n", ""),
    aux("a segment not answered for", "CD.generation=m->have_generation=m->want_generation;",
        "CD.generation=m->want_generation;"),
    aux("a refused segment not counted",
        "                CD.state=KUI_TOY_CDDA_CARD_REFUSED;\n                ++m->refused;\n            } else",
        "                CD.state=KUI_TOY_CDDA_CARD_REFUSED;\n            } else"),
    aux("a refused segment read all the same",
        "            if(m->why) {\n                CD.state=KUI_TOY_CDDA_CARD_REFUSED;",
        "            if(m->why) {\n                CD.state=KUI_TOY_CDDA_CARD_SEEKING;"),
    aux("a finished or refused segment worked on",
        "    if(CD.state!=KUI_TOY_CDDA_CARD_SEEKING && CD.state!=KUI_TOY_CDDA_CARD_STREAMING) {\n"
        "        m->card=CD.state;\n        return;\n    }\n", ""),
    aux("the state of a finished segment not answered",
        "        m->card=CD.state;\n        return;\n    }\n    /* The card is the audio's", "        return;\n    }\n    /* The card is the audio's"),
    aux("card work with no block to hand on", GUARDS, GUARDS.replace("!m->want_blocks || ", "")),
    aux("card work with a read of the game's standing", GUARDS, GUARDS.replace("toy_cdda_wanted(r) || ", "")),
    aux("card work in an interrupt's visit", GUARDS, GUARDS.replace("e->in_irq || ", "")),
    aux("card work with the bus not opened", GUARDS, GUARDS.replace("!e->opened || ", "")),
    aux("card work on a bus at fault", GUARDS, GUARDS.replace("r->shared.port.fault ||\n       ", "")),
    aux("card work on a port that is not the reader's", GUARDS, GUARDS.replace("!r->shared.port.acquired || ", "")),
    aux("card work behind a movie request of the low reader's",
        GUARDS, GUARDS.replace(" || r->shared.stream.toy_suite_active", "")),
    aux("a window is no hindrance", "        if(KUI_TOY_AHEAD.variant)return;\n", ""),
    aux("a reception of the low reader's in flight taken for nothing", FOREIGN,
        "if(r->shared.stream.state>RA_RESUME && !CD.busy)return;"),
    aux("a wait of the low reader's own gone on with", FOREIGN, "if(C.config.ops->busy())return;"),
    aux("the audio's own wait taken for the low reader's", FOREIGN,
        "if(C.config.ops->busy() || r->shared.stream.state>RA_RESUME)return;"),
    aux("the interrupt's level not looked after", "    if(CD.busy&TOY_CDDA_OWNS)C.config.ops->hook();\n", ""),
    aux("a run that ended not answered between blocks", "        kui_wire_service(w);\n        if(w->state==KUI_WIRE_FAULT) {",
        "        if(w->state==KUI_WIRE_FAULT) {"),
    aux("a stream at fault read on",
        "            toy_cdda_halt(r,&m->other_halts);\n            break;\n        }\n        if(blocks>=m->want_blocks",
        "            break;\n        }\n        if(blocks>=m->want_blocks"),
    aux("a stream's fault not kept",
        "            if(!m->first_error) {\n                m->first_error=w->error;\n                m->first_detail=w->detail;\n"
        "            }\n            toy_cdda_halt(r,&m->other_halts);", "            toy_cdda_halt(r,&m->other_halts);"),
    aux("more blocks than asked for", "        if(blocks>=m->want_blocks || m->sink_bytes>KUI_TOY_CDDA_SINK_BYTES-512u)break;",
        "        if(m->sink_bytes>KUI_TOY_CDDA_SINK_BYTES-512u)break;"),
    aux("a block for a sink with no room", "        if(blocks>=m->want_blocks || m->sink_bytes>KUI_TOY_CDDA_SINK_BYTES-512u)break;",
        "        if(blocks>=m->want_blocks)break;"),
    aux("a block the stream did not check handed on", KINDS, "if(parse!=KUI_WIRE_BLOCK) {"),
    aux("a rebuilt block read again", KINDS, "if(parse!=KUI_WIRE_BLOCK || block.kind!=KUI_WIRE_CLEAN) {"),
    aux("a clean block read again", KINDS, "if(parse!=KUI_WIRE_BLOCK || block.kind!=KUI_WIRE_REBUILT) {"),
    aux("a block read again not counted", "            ++m->rereads;\n", ""),
    aux("a block to read again, and the stream goes on",
        "            ++m->rereads;\n            toy_cdda_halt(r,&m->other_halts);\n            break;", "            ++m->rereads;\n            break;"),
    aux("the stream goes on where the file does not", "        if(!toy_cdda_feed(m,block.data) || CD.cursor.block!=next) {",
        "        if(!toy_cdda_feed(m,block.data) || (CD.cursor.block!=next && !block.data)) {"),
    aux("the stream goes on behind the segment's end", ON,
        "        if(!toy_cdda_feed(m,block.data)) {\n"
        "            if(CD.state!=KUI_TOY_CDDA_CARD_DONE)toy_cdda_halt(r,&m->other_halts);\n            break;\n        }\n"
        "        if(CD.cursor.block!=next) {\n            toy_cdda_halt(r,&m->other_halts);\n            break;\n        }"),
    aux("blocks handed on not counted", "        ++m->blocks;\n", ""),
    aux("rebuilt blocks not counted", "        if(block.kind==KUI_WIRE_REBUILT)++m->rebuilt;\n", ""),
    aux("the card's state not answered", "    }\n    m->card=CD.state;\n}", "    }\n}"),
    # ---- a delivery while the audio has the card ----
    aux("data first: a read does not end the audio's stream",
        "        toy_cdda_halt(r,&m->data_halts);\n        return false;", "        return false;"),
    aux("the stream's interrupt not answered in its delivery",
        "    kui_wire_service(toy_wire_stream());\n    return true;", "    return true;"),
    aux("a delivery of the audio's stream passed on to the low reader",
        "    kui_wire_service(toy_wire_stream());\n    return true;", "    kui_wire_service(toy_wire_stream());\n    return false;"),
    aux("a stream the reader ended not noticed at a delivery",
        "    struct kui_toy_cdda_mail *m=C.config.cdda_mail;\n    toy_cdda_seen(m);\n    if(toy_cdda_wanted(r)) {",
        "    struct kui_toy_cdda_mail *m=C.config.cdda_mail;\n    if(toy_cdda_wanted(r)) {"),
    # ---- the dispatcher ----
    ("every visit claimed for the audio's stream", CARD,
     [(DISPATCH, "    if(event==KUI_NATIVE_RA_DELIVER && toy_cdda.busy && toy_cdda_claim(r))return 1u;",
       "    if(toy_cdda.busy && toy_cdda_claim(r))return 1u;")]),
    ("no delivery claimed", CARD,
     [(DISPATCH, "    if(event==KUI_NATIVE_RA_DELIVER && toy_cdda.busy && toy_cdda_claim(r))return 1u;",
       "    if(0 && toy_cdda.busy && toy_cdda_claim(r))return 1u;")]),
    ("the audio's work at any heartbeat argument", CARD,
     [(DISPATCH, "        if(arg==KUI_NATIVE_RA_TICK_DEFAULT)toy_cdda_fill(r);", "        toy_cdda_fill(r);")]),
    ("the audio's state kept across a new binding", CARD,
     [(DISPATCH, "    /* Test 85 (CARD): the audio has nothing of the card. */\n    toy_cdda_reset();\n",
       "    (void)toy_cdda_reset;\n")]),
    # ---- the shared block's place ----
    ("a shared block off the cache lines", CARD, [(MAIL, "    return !(address & 31u) && address >= begin", "    return address >= begin")]),
    ("a shared block that begins before the span", CARD,
     [(MAIL, "address >= begin && address <= end &&", "(begin || !begin) && address <= end &&")]),
    ("a shared block that begins behind the span", CARD, [(MAIL, "address >= begin && address <= end &&", "address >= begin &&")]),
    ("a shared block that ends behind the span", CARD,
     [(MAIL, "        end - address >= (uint32_t)sizeof(struct kui_toy_cdda_mail);", "        end - address >= 32u;")]),
    # ---- the worker's heartbeat ----
    worker("asked with no auxiliary bound", "    if(!B.window)return KUI_TOY_CDDA_CARD_IDLE;\n", ""),
    worker("nothing wanted, and a visit every heartbeat", "        if(M.have_generation) {\n            M.want_generation=0u;",
           "        {\n            M.want_generation=0u;"),
    worker("nothing wanted, and the auxiliary not told", "            M.want_generation=0u;\n            ++M.visits;", "            ++M.visits;"),
    worker("the timer's profile not asked", "    if(!clock_now(&B.begun))return KUI_TOY_CDDA_CARD_IDLE;", "    (void)clock_now(&B.begun);"),
    worker("the shared block not claimed", "    M.magic=KUI_TOY_CDDA_MAIL_MAGIC;\n", ""),
    worker("the track not passed on", "    M.want_track=want->track;\n", ""),
    worker("the first sector not passed on", "    M.want_lba=want->lba;\n", ""),
    worker("the end not passed on", "    M.want_end=want->end;\n", ""),
    worker("what the consumer says of itself not passed on", "    M.yield_ok=want->yield_ok;\n", ""),
    worker("no block by programmed transfers allowed", "    M.seek_ok=1u;\n", ""),
    worker("another segment's answer believed before a visit",
           "    B.state=M.have_generation==M.want_generation?M.card:KUI_TOY_CDDA_CARD_IDLE;", "    B.state=M.card;"),
    worker("more blocks asked for than the sink holds",
           "        if(blocks>KUI_TOY_CDDA_SINK_BYTES/512u)blocks=KUI_TOY_CDDA_SINK_BYTES/512u;\n", ""),
    worker("a visit with room for no block", "        if(!blocks)break;\n", ""),
    worker("a seeking visit's time reckoned as a reading one's",
           "        if(B.state==KUI_TOY_CDDA_CARD_STREAMING && spent>M.round_longest)M.round_longest=spent;",
           "        if(spent>M.round_longest)M.round_longest=spent;"),
    worker("a visit's time counted from the heartbeat's start",
           "        const uint32_t spent=B.begun-read_hw(TCNT0,4u)-B.elapsed;", "        const uint32_t spent=B.begun-read_hw(TCNT0,4u);"),
    worker("another segment's answer believed after a visit",
           "        if(M.have_generation!=M.want_generation || M.sink_bytes>B.room)break;", "        if(M.sink_bytes>B.room)break;"),
    worker("more handed on than there is room for",
           "        if(M.have_generation!=M.want_generation || M.sink_bytes>B.room)break;",
           "        if(M.have_generation!=M.want_generation)break;"),
    worker("a visit behind one that handed nothing on", "        if(!M.sink_bytes)break;\n",
           "        if(!M.sink_bytes) {\n            if(B.state!=KUI_TOY_CDDA_CARD_STREAMING || "
           "B.elapsed+M.round_longest>B.window)break;\n            continue;\n        }\n"),
    worker("the room not counted down", "        B.room-=M.sink_bytes;\n", ""),
    worker("the sink's place not moved on", "        M.sink_offset+=M.sink_bytes;\n", ""),
    worker("the sink not emptied", "        M.sink_offset+=M.sink_bytes;\n        M.sink_bytes=0u;\n", "        M.sink_offset+=M.sink_bytes;\n"),
    worker("a visit behind one that did not stream", MORE, "        if(B.elapsed+M.round_longest>B.window)break;"),
    worker("visits without end of time", MORE, "        if(B.state!=KUI_TOY_CDDA_CARD_STREAMING)break;"),
    worker("the longest visit not reckoned with", MORE, "        if(B.state!=KUI_TOY_CDDA_CARD_STREAMING || B.elapsed>B.window)break;"),
    worker("visits not counted", "        M.visits+=B.rounds;\n", ""),
    worker("the visits' time not counted", "        M.visit_ticks+=B.elapsed;\n", ""),
    worker("heartbeats not counted", "        ++M.heartbeats;\n", ""),
    worker("the longest heartbeat not kept", "        if(B.elapsed>M.longest)M.longest=B.elapsed;\n", ""),
    # ---- the disc's table ----
    worker("a data track named", "        if(kui_retail_track_control(t))break;\n", ""),
    worker("the sector behind a track its own", "        if(lba>=t->end_lba)continue;", "        if(lba>t->end_lba)continue;"),
    worker("a track's first sector not its own", "        if(lba<t->start_lba)break;", "        if(lba<=t->start_lba)break;"),
    # ---- the consumer with no sound output ----
    worker("a heartbeat with SR.BL set", "    if(sr_read()&UINT32_C(0x10000000)) {++L.skipped;return;}", "    (void)sr_read();"),
    worker("asked while the drive does not play", "    bool playing=D.state==KUI_TOY_CDDA_PLAYING && clock_now(&now);",
           "    bool playing=D.state!=KUI_TOY_CDDA_IDLE && clock_now(&now);"),
    worker("the play's place without its passes", "        position=D.done*length+(uint32_t)at;", "        position=(uint32_t)at;"),
    worker("a part of three sectors left out of the play's place",
           "            ticks%KUI_TOY_CDDA_TICKS_3_SECTORS*3u/KUI_TOY_CDDA_TICKS_3_SECTORS);", "            0u);"),
    worker("another play not begun at", "        if(!L.following || L.play!=D.generation || L.releases!=D.releases) {",
           "        if(!L.following || L.releases!=D.releases) {"),
    worker("a release not begun at", "        if(!L.following || L.play!=D.generation || L.releases!=D.releases) {",
           "        if(!L.following || L.play!=D.generation) {"),
    worker("a play picked up where it was left", "    } else L.following=0u;\n", "    }\n"),
    worker("begun in the pass the player has counted", "            L.done=D.done+divide(&offset,length);",
           "            (void)divide(&offset,length);L.done=D.done;"),
    worker("begun at a sector behind the play's end", "            L.done=D.done+divide(&offset,length);\n            L.sector=D.first+offset;",
           "            L.done=D.done+divide(&offset,length);\n            L.sector=D.first+(uint32_t)at;"),
    worker("what was held before a start counted on", "            L.ahead=0u;\n", ""),
    worker("the old segment kept across a start", "            L.ahead=0u;\n            L.want.generation=0u;", "            L.ahead=0u;"),
    worker("the last pass asked for again", "    if(!playing || (D.passes && L.done>=D.passes)) {", "    if(!playing) {"),
    worker("the audio not told to leave the card",
           "            L.want.generation=0u;\n            (void)kui_toy_cdda_card_heartbeat(&L.want,0u,listener_take,0);",
           "            L.want.generation=0u;"),
    worker("no end to the consumer's reach",
           "        lead<(int32_t)KUI_TOY_CDDA_LISTENER_LEAD?KUI_TOY_CDDA_LISTENER_LEAD-(uint32_t)lead:0u;",
           "        KUI_TOY_CDDA_LISTENER_LEAD;"),
    worker("a pass not begun at the play's first sector", "            L.sector=D.first;\n", "            L.sector=D.first+1u;\n"),
    worker("a pass too many", "            if(D.passes && ++L.done>=D.passes)return;", "            ++L.done;"),
    worker("silence not counted as held", "            L.ahead+=2352u;\n            ++L.silent;", "            ++L.silent;"),
    worker("silent sectors not counted", "            L.ahead+=2352u;\n            ++L.silent;", "            L.ahead+=2352u;"),
    worker("silence behind the play's end", "            if(++L.sector>=D.end)return;", "            ++L.sector;"),
    worker("silence with no room for it", "            if(room<2352u)return;\n", ""),
    worker("the segment that asks for nothing given out", "        if(!++L.generation)++L.generation;", "        ++L.generation;"),
    worker("a segment beyond the play's end", "        L.want.end=end<D.end?end:D.end;", "        L.want.end=end;"),
    worker("a segment's bytes counted on from the last one's", "        L.taken=0u;\n        ++L.segments;", "        ++L.segments;"),
    worker("never said to run low", "    L.want.yield_ok=lead<(int32_t)KUI_TOY_CDDA_LISTENER_LOW;", "    L.want.yield_ok=false;"),
    worker("always said to run low", "    L.want.yield_ok=lead<(int32_t)KUI_TOY_CDDA_LISTENER_LOW;", "    L.want.yield_ok=true;"),
    worker("a refused segment asked for again", ">=KUI_TOY_CDDA_CARD_DONE) {", "==KUI_TOY_CDDA_CARD_DONE) {"),
    worker("a finished segment asked for again", ">=KUI_TOY_CDDA_CARD_DONE) {", ">KUI_TOY_CDDA_CARD_DONE) {"),
    worker("what is left of a refused segment not passed over",
           "        L.ahead+=(L.want.end-L.want.lba)*2352u-L.taken;\n", ""),
    worker("the next segment begun where the last one began", "        L.sector=L.want.end;\n", ""),
    worker("bytes out of their place not noticed", "    if(offset!=L.taken)++L.wrong;\n", ""),
    worker("bytes taken not counted as held", "    L.taken+=count;\n    L.ahead+=count;", "    L.taken+=count;"),
    # ---- the consumer's two calls ----
    ("the fast path calls the consumer on early discovery batches", SITES,
     [(PASSIVE, FAST, "        kui_toy_cdda_consumer_heartbeat();")]),
    ("the mixed path calls the consumer on batches without the controller", SITES,
     [(PASSIVE, MIXED, "        kui_toy_cdda_consumer_heartbeat();")]),
    ("the fast path calls the consumer before the controller's completion", SITES,
     [(PASSIVE, FAST, ""), (PASSIVE, COMPLETE, "#if KUI_TOY_CDDA_CARD\n" + FAST + "\n#endif\n" + COMPLETE)]),
    ("no consumer on the fast path", SITES, [(PASSIVE, FAST, "")]),
    ("no consumer on the mixed path", SITES, [(PASSIVE, MIXED, "")]),
    ("the consumer before the survey on the fast path", SITES,
     [(PASSIVE, FAST, ""), (PASSIVE, SURVEY_FAST, "#if KUI_TOY_CDDA_CARD\n" + FAST + "\n#endif\n" + SURVEY_FAST)]),
    ("the consumer before the survey on the mixed path", SITES,
     [(PASSIVE, MIXED, ""), (PASSIVE, SURVEY_MIXED, "#if KUI_TOY_CDDA_CARD\n" + MIXED + "\n#endif\n" + SURVEY_MIXED)]),
]


def copy_tree(work):
    ignore = shutil.ignore_patterns("__pycache__", "*.py", "fixtures")
    for name in ("include", "src", "tests"):
        shutil.copytree(ROOT / name, work / name, ignore=ignore)
    for makefile in ROOT.glob("Makefile*"):
        shutil.copy2(makefile, work / makefile.name)


def run(index, base, env, timeout):
    name, (target, binary), edits = MUTATIONS[index]
    work = base / ("m%03d" % index)
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    texts = {}
    for path, old, new in edits:
        text = texts.get(path)
        if text is None:
            text = (ROOT / path).read_text()
        if text.count(old) != 1:
            return index, "not applied (%d matches in %s)" % (text.count(old), path)
        texts[path] = text.replace(old, new)
    copy_tree(work)
    for path, text in texts.items():
        (work / path).write_text(text)
    try:
        done = subprocess.run(["make", "-f", "Makefile.vmu-native-toy-suite", *RECIPE,
                               "HOST_CC=" + env.get("HOST_CC", "cc"), "BUILD_ID=000000000000",
                               "BUILD=" + str(work / "build"), target], cwd=work, env=env, capture_output=True,
                              text=True, errors="replace", timeout=timeout)
        log, code = done.stdout + done.stderr, done.returncode
    except subprocess.TimeoutExpired as e:
        log = ((e.stdout or b"").decode(errors="replace") if isinstance(e.stdout, bytes) else (e.stdout or "")) + \
            "\nTIMEOUT after %d s\n" % timeout
        code = None
    built = (work / "build" / binary).exists()
    for name_ in ("include", "src", "tests", "build"):
        shutil.rmtree(work / name_, ignore_errors=True)
    (work / "log.txt").write_text(log)
    if code == 0:
        return index, "SURVIVED"
    if code is None:
        # A test that never ends has not passed: the reader it drives no longer serves.
        return index, "caught" if built else "did not build"
    if built and ("Assertion" in log or "AddressSanitizer" in log or "runtime error" in log):
        return index, "caught"
    return index, "did not build"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--work")
    parser.add_argument("--only", action="append", help="run the mutations whose name holds this text (may be repeated)")
    parser.add_argument("--timeout", type=int, default=900)
    args = parser.parse_args()
    base = Path(args.work or tempfile.mkdtemp(prefix="toy-cdda-card-mutations-"))
    base.mkdir(parents=True, exist_ok=True)
    print("mutations in", base)
    env = dict(os.environ)
    env.pop("MAKEFLAGS", None)
    env.pop("MFLAGS", None)
    chosen = [i for i in range(len(MUTATIONS)) if not args.only or any(text in MUTATIONS[i][0] for text in args.only)]
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        results = sorted(pool.map(lambda i: run(i, base, env, args.timeout), chosen))
    for i, r in results:
        print("%-9s %s" % (r, MUTATIONS[i][0]))
    bad = [(i, r) for i, r in results if r != "caught"]
    if bad:
        sys.exit("FAIL %d of %d mutations not caught: %s" % (len(bad), len(results),
                 "; ".join("%s (%s)" % (MUTATIONS[i][0], r) for i, r in bad)))
    print("PASS Test 85 card mutations: all %d caught, %d by the audio's scenarios and %d by the gesture test" % (
        len(results), sum(1 for i, _ in results if MUTATIONS[i][1] is CARD),
        sum(1 for i, _ in results if MUTATIONS[i][1] is SITES)))


if __name__ == "__main__":
    main()
