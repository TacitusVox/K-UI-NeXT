#include <assert.h>
#include <stdio.h>
#include <string.h>

#define KUI_TOY_SUITE_MODE @MODE@
#define KUI_NATIVE_TOY_MOVIE_CAPTURE 1
#define KUI_NATIVE_TOY_PLAYBACK_CAPTURE 1
#define KUI_RETAIL_ASYNC 1
#define KUI_RETAIL_GD_ASYNC 1
#define KUI_RETAIL_ASYNC_TEST 1
#define KUI_RETAIL_TRANSPORT 1
#define KUI_SCI_SD_NO_BLOCK 1
#define KUI_GENERAL_SCI_RESUME_FIX 1
#define KUI_VMU_NATIVE_BACKGROUND 1
#define KUI_BUILD_ID "DEADBEEF0123"
#define KUI_TOY_TEST_TITLE "TOY TEST @MODE@"

#include "kui/native_readahead.h"
#include "kui/toy_suite_report_wire.h"
#include "kui/vmu_sd_lease.h"

static struct { struct kui_toy_report_wire toy_report; uintptr_t sd_lease; } config;
static struct { bool armed,pending; unsigned submitted,completed; } state;
static unsigned char issued_chain[3u*KUI_TOY_REPORT_SECTOR_BYTES];
static bool reader_only(void) { return true; }
static bool lease_valid(void) { return true; }
static bool stack_valid(void) { return true; }
static uint32_t current_sr(void) { return 0xf0u; }
static int kui_vmu_native_persistent_sd_transaction(void *c, struct kui_loader_sd *l) {
    (void)c;
    (void)l;
    assert(!"report must be disarmed in this formatter test");
    return 0;
}
static void toy_ahead_recovery_report(const struct kui_native_ra_public *ra,
    uint32_t recovery[4],uint32_t first[4]);
#include "toy_suite_report_save.inc"

#define F UINT32_MAX
struct row {
    const char *file_label, *screen_label;
    unsigned page, words;
    uint32_t values[5];
};
static const struct row rows[] = {
    {"B SUBMIT CADENCE MAX AT TAIL", "B SUBMIT CADENCE MAX AT TAIL", 1, 5, {0x112,0x110,0x12abc,0xf6,0}},
    {"I SUBMIT CADENCE MAX AT TAIL", "I SUBMIT CADENCE MAX AT TAIL", 1, 5, {0x98,0x8f,0x92345,0x124,0x878a}},
    {"B FIRST LAST CLK OUT SKIP", "B FIRST LAST CLK OUT SKIP", 1, 5, {0x19807,0x14b,0,0x112,0x39}},
    {"I FIRST LAST CLK OUT SKIP", "I FIRST LAST CLK OUT SKIP", 1, 5, {0x23576,0x224,1,0x98,0x18d}},
    {"B WAIT USED NEED SKIP LBA", "B WAIT USED NEED SKIP LBA", 2, 5, {0x7c,0x11000,0x400,0x95,0x69263}},
    {"I WAIT USED NEED SKIP LBA", "I WAIT USED NEED SKIP LBA", 2, 5, {0xbb,0x8789,0xa34,0xc4,0x6e450}},
    {"B READ N VERIFIED TICK AGE DONE", "B READ N VERIFIED TICK AGE DONE", 2, 5, {0x10,0xc,0x235678,0x1345,0x1ce1}},
    {"I READ N VERIFIED TICK AGE DONE", "I READ N VERIFIED TICK AGE DONE", 2, 5, {0x11,0x3,0x2e4567,F,F}},
    {"B ENTER SCENE EXIT FRAME", "B ENTER SCENE EXIT FRAME", 1, 4, {0x130,0x115,0x7dc,0x14b}},
    {"I ENTER SCENE EXIT FRAME", "I ENTER SCENE EXIT FRAME", 1, 4, {0x9f,0x99,0xfffff843,0x225}},
    {"FIX POLL TAKE FETCH", "FIX POLL TAKE FETCH", 2, 4, {0x201,0x19a,0x1223,0x2345}},
    {"FIRST CARD WHY RAW SERVED", "FIRST CARD WHY RAW SERVED", 2, 4, {0x695ff,0x105,0x9,0x34}},
    {"AHEAD B I HIT EDC STOP", "AHEAD B I REQS VERIFIED FALLBACK", 2, 5, {0x5a,0x90,0x11f,0x345,0x234}},
    {"I LAG LIMIT", NULL, 0, 2, {0x9,0x7}},
};
static bool shown[sizeof(rows)/sizeof(rows[0])];
static unsigned page, display_lines;

void retail_display_line(const char *label) {
    assert(strlen(label)<=49u);
    ++display_lines;
}
void retail_display_hex(const char *label, uint32_t value) {
    (void)value;
    assert(strlen(label)<=40u);
    ++display_lines;
}
void retail_display_values(const char *label, const uint32_t *values, unsigned count) {
    assert(count>0u && count<=5u);
    for (unsigned i=0; i<sizeof(rows)/sizeof(rows[0]); ++i) {
        const struct row *row=&rows[i];
        if (!row->screen_label || strcmp(label,row->screen_label)) continue;
        assert(row->page==page && !shown[i]);
        assert(row->words==count && !memcmp(row->values,values,count*sizeof(*values)));
        shown[i]=true;
    }
    retail_display_line(label);
    ++display_lines;
}
#include "screen-extract.inc"

int main(int argc, char **argv) {
    kui_retail_async_region_type region={0};
    struct kui_native_ra_public ra;
    memset(&ra,0xff,sizeof(ra));
    memset(&region.shared.stream.stats,0xff,sizeof(region.shared.stream.stats));
    ra.present[0]=(struct kui_toy_movie_present_trace){
        .submitted=0x112,.on_time=0x110,.max_gap=0x12abc,.max_gap_frame=0xf6,
        .first_gap=0x19807,.last_frame=0x14b,.render_entries=0x130,.scene_hits=0x115};
    ra.present[1]=(struct kui_toy_movie_present_trace){
        .submitted=0x98,.on_time=0x8f,.max_gap=0x92345,.max_gap_frame=0x124,
        .tail_gap=0x878a,.first_gap=0x23576,.last_frame=0x224,.clock_lost=1,
        .render_entries=0x9f,.scene_hits=0x99};
    ra.movie_exit[0]=(struct kui_toy_movie_exit_witness){
        .flags=11,.start_status=0x7db,.output_frames=0x112,.skipped_frames=0x39,
        .status=0x7dc,.frame=0x14b,.samples=0x10000,.lag=0x8,.limit=0x6};
    ra.movie_exit[1]=(struct kui_toy_movie_exit_witness){
        .flags=3,.start_status=0x7db,.output_frames=0x98,.skipped_frames=0x18d,
        .status=0xfffff843,.frame=0x225,.samples=0x20000,.lag=0x9,.limit=0x7};
    ra.onset[0]=(struct kui_toy_movie_onset_trace){
        .wait_frame=0x7c,.wait_used=0x11000,.wait_missing=0x400,.skip_frame=0x95,
        .quiet_defers=0x5a,.quiet_tick=0x90,.quiet_end_frame=0x11f,.quiet_reason=0x345};
    ra.onset[1]=(struct kui_toy_movie_onset_trace){
        .wait_frame=0xbb,.wait_used=0x8789,.wait_missing=0xa34,.skip_frame=0xc4,
        .quiet_defers=0x25,.quiet_reason=3,.quiet_end_frame=0x34};
    ra.suite[4]=9;
    ra.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][0]=0x5a;
    ra.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][1]=0x90;
    ra.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][3]=0x201;
    ra.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][4]=0x34;
    uint32_t (*counts)[KUI_NATIVE_RA_RESULT_COUNT]=ra.diagnostics.stage_result;
    memset(counts[KUI_NATIVE_RA_STAGE_POLL],0,sizeof(counts[KUI_NATIVE_RA_STAGE_POLL]));
    memset(counts[KUI_NATIVE_RA_STAGE_TAKE],0,sizeof(counts[KUI_NATIVE_RA_STAGE_TAKE]));
    memset(counts[KUI_NATIVE_RA_STAGE_FETCH],0,sizeof(counts[KUI_NATIVE_RA_STAGE_FETCH]));
    memset(counts[KUI_NATIVE_RA_STAGE_APPEND],0,sizeof(counts[KUI_NATIVE_RA_STAGE_APPEND]));
    counts[KUI_NATIVE_RA_STAGE_POLL][1]=0x190;
    counts[KUI_NATIVE_RA_STAGE_POLL][5]=0xa;
    counts[KUI_NATIVE_RA_STAGE_TAKE][2]=0x1200;
    counts[KUI_NATIVE_RA_STAGE_TAKE][6]=0x23;
    counts[KUI_NATIVE_RA_STAGE_FETCH][3]=0x2000;
    counts[KUI_NATIVE_RA_STAGE_FETCH][7]=0x345;
    counts[KUI_NATIVE_RA_STAGE_APPEND][5]=9;
    ra.diagnostics.first.operation_lba=0x695ff;
    ra.diagnostics.first.stage=KUI_NATIVE_RA_STAGE_POLL;
    ra.diagnostics.first.result=5;
    ra.stats.hit_requests=0x11f;
    ra.stats.producer_blocks=0x345;
    ra.stats.abandoned=0x234;
    ra.wait[0]=(struct kui_toy_movie_wait_trace){0x69263,0x10,0xc,0x235678,0x1345,0x1ce1};
    ra.wait[1]=(struct kui_toy_movie_wait_trace){0x6e450,0x11,0x3,0x2e4567,F,F};

    char buffer[KUI_TOY_REPORT_FILE_BYTES+2u];
    memset(buffer,'!',sizeof(buffer));
    char *text=buffer+1u;
    toy_report_format(text,&ra,&region.shared.stream.stats,@MODE@u);
    assert(buffer[0]=='!' && buffer[sizeof(buffer)-1u]=='!');
    assert(!memcmp(text+KUI_TOY_REPORT_FILE_BYTES-12u,"\nEND REPORT\n",12u));
    buffer[sizeof(buffer)-1u]=0;
    assert(strstr(text,"\nTEST @MODE@\nBUILD DEADBEEF0123\n"));
    assert(strstr(text,"\nRAW ID1\n"));
    for (unsigned i=0; i<sizeof(rows)/sizeof(rows[0]); ++i) {
        const struct row *row=&rows[i];
        char line[96];
        char *next=line+sprintf(line,"%s\n",row->file_label);
        for (unsigned j=0; j<row->words; ++j)
            next+=sprintf(next,"%08X%c",row->values[j],j+1u==row->words?'\n':' ');
        const char *observed=strstr(text,line);
        if(!observed || observed+strlen(line)>text+1012)
            fprintf(stderr,"Missing or truncated file row: %s\n%.*s\n",line,1024,text);
        assert(observed && observed+strlen(line)<=text+1012);
    }
    const char *end=text+1012;
    while (end>text && end[-1]==' ') --end;
    assert(end>text && end[-1]=='\n');
    assert(!toy_suite_save_report(&ra,&region));

    page=1;
    toy_playback_page(&ra);
    unsigned playback_lines=display_lines;
    assert(playback_lines==13u);
    page=2;
    display_lines=0;
    toy_storage_page(&ra,&region);
    assert(display_lines==15u);
    for (unsigned i=0; i<sizeof(rows)/sizeof(rows[0]); ++i)
        assert(shown[i]==(rows[i].screen_label!=NULL));
    if (argc==2) {
        FILE *out=fopen(argv[1],"wb");
        assert(out && fwrite(text,1u,KUI_TOY_REPORT_FILE_BYTES,out)==KUI_TOY_REPORT_FILE_BYTES);
        assert(fclose(out)==0);
    }
    printf("Mode @MODE@ report: %td body bytes in 1024-byte file, %u and %u screen lines\n",
           end-text,playback_lines,display_lines);
}
