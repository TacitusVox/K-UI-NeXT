/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/toy_fmv_compare.h"
#include "kui/gd_service.h"
#include <string.h>

static struct kui_toy_fmv_bin *bin_at(struct kui_toy_fmv_movie *movie, uint32_t tick) {
    uint32_t elapsed=movie->first_tick-tick;
    unsigned bin=0u;
    while(bin+1u<KUI_TOY_FMV_BINS && elapsed>=KUI_TOY_FMV_BIN_TICKS) {
        elapsed-=KUI_TOY_FMV_BIN_TICKS;
        ++bin;
    }
    return &movie->bins[bin];
}

void kui_toy_fmv_request(struct kui_toy_fmv_trace *trace, const struct kui_toy_movie_range ranges[4],
    uint32_t token, uint32_t lba, uint32_t sectors, uint32_t tick) {
    if(!token || !sectors) return;
    unsigned variant=0u;
    for(;variant<4u;++variant) {
        const struct kui_toy_movie_range *range=&ranges[variant];
        /* Toy reads the AVI headers through sector 385 before streaming. */
        if(lba>=range->lba && lba-range->lba>=386u && lba-range->lba<range->count) break;
    }
    if(variant==4u) return;
    struct kui_toy_fmv_movie *movie=&trace->movie[variant/2u];
    if(!movie->requests) movie->first_tick=tick;
    if(movie->first_tick-tick>=UINT32_C(0x80000000)) {
        ++trace->clock_lost;
        return;
    }
    ++movie->requests;
    movie->last_lba=lba;
    bin_at(movie,tick)->requested+=sectors;
    struct kui_toy_fmv_pending *slot=NULL;
    for(unsigned i=0u;i<KUI_TOY_FMV_PENDING;++i) {
        struct kui_toy_fmv_pending *pending=&trace->pending[i];
        if(pending->token==token) {
            slot=pending;
            ++trace->movie[pending->movie].unmatched;
            break;
        }
        if(!pending->token && !slot) slot=pending;
    }
    if(slot) *slot=(struct kui_toy_fmv_pending){token,tick,lba,sectors,variant/2u};
    else ++movie->unmatched;
}

void kui_toy_fmv_status(struct kui_toy_fmv_trace *trace, uint32_t token, uint32_t tick,
    int32_t status, uint32_t bytes) {
    if(!token || (status!=KUI_GD_COMPLETED && status!=KUI_GD_FAILED)) return;
    for(unsigned i=0u;i<KUI_TOY_FMV_PENDING;++i) {
        struct kui_toy_fmv_pending *pending=&trace->pending[i];
        if(pending->token!=token) continue;
        pending->token=0u;
        struct kui_toy_fmv_movie *movie=&trace->movie[pending->movie];
        uint32_t elapsed=pending->tick-tick;
        if(elapsed>=UINT32_C(0x80000000) || movie->first_tick-tick>=UINT32_C(0x80000000)) {
            ++trace->clock_lost;
            return;
        }
        struct kui_toy_fmv_bin *bin=bin_at(movie,tick);
        if(elapsed>bin->longest) bin->longest=elapsed;
        if(movie->acks) {
            uint32_t gap=movie->last_ack-tick;
            if(gap>bin->gap) bin->gap=gap;
        }
        ++movie->acks;
        movie->last_ack=tick;
        if(status==KUI_GD_FAILED) ++movie->failed;
        else {
            /* Status word 2 is the number of bytes reported by GD CHECK.
             * DATA reads for this comparison are logical 2048-byte sectors. */
            uint32_t sectors=bytes/2048u;
            if(sectors!=pending->sectors || bytes%2048u) ++movie->short_bytes;
            if(sectors>pending->sectors) sectors=pending->sectors;
            bin->completed+=sectors;
            if(sectors) movie->end_lba=pending->lba+sectors;
        }
        return;
    }
}

static void append(char **at, const char *text) {
    while(*text) *(*at)++=*text++;
}

static void hex(char **at, uint32_t value, unsigned digits) {
    if(digits==4u && value>UINT32_C(0xffff)) value=UINT32_C(0xffff);
    while(digits) {
        --digits;
        *(*at)++="0123456789ABCDEF"[(value>>(digits*4u))&15u];
    }
}

static uint32_t milliseconds(uint32_t ticks) {
    return (ticks/3125u)*4u+((ticks%3125u)*4u+3124u)/3125u;
}

void kui_toy_fmv_format(char text[KUI_TOY_REPORT_FILE_BYTES], const struct kui_toy_fmv_trace *trace,
    const char *source, const char *build) {
    char *at=text;
    memset(text,' ',KUI_TOY_REPORT_FILE_BYTES);
    append(&at,"K-UI TOY FMV COMPARE V1 / ");
    for(unsigned i=0u;i<8u && source[i];++i) *at++=source[i];
    *at++=' ';
    for(unsigned i=0u;i<12u && build[i];++i) *at++=build[i];
    append(&at,"\nHEX / FIRST STREAM REQUEST = 0 / 500 MS BINS / LAST BIN 6S+\n");
    append(&at,"REQ ACK FAIL SIZE UNMATCHED OPEN LAST_LBA ACK_END_LBA\n");
    append(&at,"PER BIN: REQ_SECT ACK_SECT MAX_REQ_MS MAX_ACK_GAP_MS\n");
    for(unsigned i=0u;i<2u;++i) {
        const struct kui_toy_fmv_movie *movie=&trace->movie[i];
        append(&at,i?"INTRO\n":"BUMPER\n");
        uint32_t open=0u;
        for(unsigned j=0u;j<KUI_TOY_FMV_PENDING;++j)
            if(trace->pending[j].token && trace->pending[j].movie==i) ++open;
        const uint32_t values[]={movie->requests,movie->acks,movie->failed,movie->short_bytes,
            movie->unmatched,open,movie->last_lba,movie->end_lba};
        for(unsigned j=0u;j<8u;++j) {
            hex(&at,values[j],j<6u?4u:8u);
            *at++=j==7u?'\n':' ';
        }
        for(unsigned j=0u;j<KUI_TOY_FMV_BINS;++j) {
            const struct kui_toy_fmv_bin *bin=&movie->bins[j];
            hex(&at,bin->requested,4u);*at++=' ';
            hex(&at,bin->completed,4u);*at++=' ';
            hex(&at,milliseconds(bin->longest),4u);*at++=' ';
            hex(&at,milliseconds(bin->gap),4u);*at++='\n';
        }
    }
    append(&at,"CLOCK LOST ");hex(&at,trace->clock_lost,8u);*at++='\n';
    memcpy(text+KUI_TOY_REPORT_FILE_BYTES-12u,"\nEND REPORT\n",12u);
}
