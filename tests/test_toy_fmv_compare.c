/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/toy_fmv_compare.h"
#include "kui/gd_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const struct kui_toy_movie_range ranges[4]={
    {1000u,600u},{2000u,600u},{3000u,600u},{4000u,600u}
};

static void test_movie_bins(void) {
    struct kui_toy_fmv_trace trace={0};
    const uint32_t begin=256u;
    const uint32_t half=KUI_TOY_FMV_BIN_TICKS;
    struct kui_toy_fmv_movie *bumper=&trace.movie[0], *intro=&trace.movie[1];

    /* The header, the end of an extent, and invalid requests are excluded. */
    kui_toy_fmv_request(&trace,ranges,1u,1385u,1u,begin);
    kui_toy_fmv_request(&trace,ranges,1u,1600u,1u,begin);
    kui_toy_fmv_request(&trace,ranges,0u,1386u,1u,begin);
    kui_toy_fmv_request(&trace,ranges,1u,1386u,0u,begin);
    assert(!bumper->requests && !intro->requests);

    /* Requests are attributed to their issue bin and completions to their
     * CHECK bin. Two handles can overlap and complete out of issue order. */
    kui_toy_fmv_request(&trace,ranges,1u,1386u,4u,begin);
    kui_toy_fmv_request(&trace,ranges,2u,1390u,2u,begin-1u);
    kui_toy_fmv_status(&trace,1u,begin-2u,KUI_GD_PROCESSING,2048u);
    kui_toy_fmv_status(&trace,3u,begin-3u,KUI_GD_COMPLETED,2048u);
    assert(bumper->requests==2u && bumper->bins[0].requested==6u && !bumper->acks);
    kui_toy_fmv_status(&trace,2u,begin-(half-1u),KUI_GD_COMPLETED,4096u);
    assert(bumper->bins[0].completed==2u && bumper->bins[0].longest==half-2u);
    assert(bumper->end_lba==1392u);
    kui_toy_fmv_status(&trace,1u,begin-half,KUI_GD_COMPLETED,6145u);
    kui_toy_fmv_status(&trace,1u,begin-half-1u,KUI_GD_COMPLETED,8192u);
    assert(bumper->acks==2u && bumper->short_bytes==1u && !bumper->failed);
    assert(bumper->bins[1].completed==3u && bumper->bins[1].longest==half);
    assert(bumper->bins[1].gap==1u && bumper->end_lba==1389u && !trace.clock_lost);

    /* Each movie has its own zero. A failed read ends its request without
     * adding successful sectors, even if CHECK reports a byte count. */
    const uint32_t intro_begin=begin-17u*half;
    kui_toy_fmv_request(&trace,ranges,3u,4386u,5u,intro_begin);
    kui_toy_fmv_status(&trace,3u,intro_begin-half,KUI_GD_FAILED,4096u);
    assert(intro->first_tick==intro_begin && intro->bins[0].requested==5u);
    assert(intro->acks==1u && intro->failed==1u && !intro->short_bytes);
    assert(!intro->bins[1].completed && intro->bins[1].longest==half);
    kui_toy_fmv_request(&trace,ranges,4u,4391u,1u,intro_begin-20u*half);
    kui_toy_fmv_status(&trace,4u,intro_begin-20u*half-1u,KUI_GD_COMPLETED,2048u);
    assert(intro->bins[12].requested==1u && intro->bins[12].completed==1u);
    assert(intro->bins[12].gap==19u*half+1u && intro->end_lba==4392u);
}

static void test_pending_and_clock(void) {
    struct kui_toy_fmv_trace trace={0};
    struct kui_toy_fmv_movie *bumper=&trace.movie[0], *intro=&trace.movie[1];
    const uint32_t begin=1000u;

    for(uint32_t token=1u;token<=KUI_TOY_FMV_PENDING+1u;++token)
        kui_toy_fmv_request(&trace,ranges,token,2386u,1u,begin-token);
    assert(bumper->requests==KUI_TOY_FMV_PENDING+1u && bumper->unmatched==1u);
    kui_toy_fmv_status(&trace,KUI_TOY_FMV_PENDING+1u,begin-8u,KUI_GD_COMPLETED,2048u);
    assert(!bumper->acks);

    kui_toy_fmv_request(&trace,ranges,2u,3386u,2u,begin-10u);
    assert(bumper->unmatched==2u && intro->requests==1u);
    kui_toy_fmv_status(&trace,2u,begin-11u,KUI_GD_COMPLETED,4096u);
    assert(intro->acks==1u && intro->bins[0].completed==2u && !bumper->acks);

    kui_toy_fmv_status(&trace,1u,begin+1u,KUI_GD_COMPLETED,2048u);
    kui_toy_fmv_request(&trace,ranges,7u,2387u,1u,begin+1u);
    assert(trace.clock_lost==2u && !bumper->acks);
    assert(bumper->requests==KUI_TOY_FMV_PENDING+1u);
    kui_toy_fmv_request(&trace,ranges,7u,2387u,1u,begin-13u);
    kui_toy_fmv_status(&trace,7u,begin-14u,KUI_GD_COMPLETED,4096u);
    assert(bumper->acks==1u && bumper->short_bytes==1u);
    assert(bumper->bins[0].completed==1u);
}

static void test_format(void) {
    struct kui_toy_fmv_trace trace={0};
    struct kui_toy_fmv_movie *bumper=&trace.movie[0];
    char guarded[KUI_TOY_REPORT_FILE_BYTES+3u];
    memset(guarded,'!',sizeof(guarded));
    char *const report=guarded+1u;
    const char end[]="\nEND REPORT\n";
    const size_t end_bytes=sizeof(end)-1u;

    bumper->requests=UINT32_MAX;
    bumper->bins[0]=(struct kui_toy_fmv_bin){UINT32_MAX,5u,781u,1562u};
    trace.pending[0]=(struct kui_toy_fmv_pending){5u,100u,4386u,1u,1u};
    trace.clock_lost=UINT32_MAX;
    kui_toy_fmv_format(report,&trace,"PHYSICAL-LONG","123456789abc-extra");
    assert(guarded[0]=='!' && report[KUI_TOY_REPORT_FILE_BYTES]=='!');
    assert(report[KUI_TOY_REPORT_FILE_BYTES+1u]=='!');
    assert(memcmp(report+KUI_TOY_REPORT_FILE_BYTES-end_bytes,end,end_bytes)==0);
    report[KUI_TOY_REPORT_FILE_BYTES]='\0';
    assert(strstr(report,"K-UI TOY FMV COMPARE V1 / PHYSICAL 123456789abc\n"));
    assert(strstr(report,"BUMPER\nFFFF 0000 0000 0000 0000 0000 00000000 00000000\n"));
    assert(strstr(report,"FFFF 0005 0001 0002\n"));
    assert(strstr(report,"INTRO\n0000 0000 0000 0000 0000 0001 00000000 00000000\n"));
    assert(strstr(report,"CLOCK LOST FFFFFFFF\n"));
}

int main(void) {
    test_movie_bins();
    test_pending_and_clock();
    test_format();
    puts("Toy FMV comparison bins, pending handles, clock and report passed");
    return 0;
}
