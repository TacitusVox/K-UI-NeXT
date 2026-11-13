#include <assert.h>
#include <stdio.h>
#include <string.h>

#define KUI_TOY_SUITE_MODE 49
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
#define KUI_TOY_TEST_TITLE "TOY TEST 49"

#include "kui/native_readahead.h"
#include "kui/toy_suite_report_wire.h"
#include "kui/vmu_sd_lease.h"

static struct { struct kui_toy_report_wire toy_report; uintptr_t sd_lease; } config;
static struct { bool armed,pending; unsigned submitted,completed; } state;
static unsigned char issued_chain[3u*KUI_TOY_REPORT_SECTOR_BYTES];
static bool reader_only(void) { return true; }
static uint32_t current_sr(void) { return 0xf0u; }
static bool lease_valid(void) { return true; }
static bool stack_valid(void) { return true; }
static int kui_vmu_native_persistent_sd_transaction(void *c,struct kui_loader_sd *l) {
    (void)c;(void)l;assert(!"report must be disarmed in this formatter test");return 0;
}
#include "toy_suite_report_save.inc"

static unsigned display_lines, checked_witness_rows, checked_window_rows;
void retail_display_line(const char *label) {
    assert(strlen(label)<=49u);
    ++display_lines;
}
void retail_display_hex(const char *label,uint32_t value) {
    (void)value;
    assert(strlen(label)<=40u);
    ++display_lines;
}
void retail_display_values(const char *label,const uint32_t *values,unsigned count) {
    assert(count>0u && count<=5u);
    if(!strcmp(label,"BMP FLG START OUT SKIP")) {
        assert(count==4u && values[0]==3u && values[1]==0x7d2u && values[2]==0u && values[3]==0x12u);
        ++checked_witness_rows;
    }
    if(!strcmp(label,"BMP EXIT CODE FRAME SAMP LAG LIM")) {
        assert(count==5u && values[0]==0xfffff843u && values[1]==0x34u && values[2]==0x56u &&
            values[3]==0x78u && values[4]==0x9au);
        ++checked_witness_rows;
    }
    if(!strcmp(label,"INTRO FLG START OUT SKIP")) {
        assert(count==4u && values[0]==7u && values[1]==0x7d2u && values[2]==0u && values[3]==0x123u);
        ++checked_witness_rows;
    }
    if(!strcmp(label,"INTRO EXIT CODE FRAME SAMP LAG LIM")) {
        assert(count==5u && values[0]==0x7dcu && values[1]==0x456u && values[2]==0x789u &&
            values[3]==0u && values[4]==0u);
        ++checked_witness_rows;
    }
    if(!strcmp(label,"BMP RDY WGD RSM RET EXT")) {
        assert(count==5u && values[0]==UINT32_MAX && values[1]==0x19u &&
            values[2]==0x5au && values[3]==0x5bu && values[4]==0x5cu);
        ++checked_window_rows;
    }
    if(!strcmp(label,"INTRO RDY WGD RSM RET EXT")) {
        assert(count==5u && values[0]==UINT32_MAX && values[1]==0x39u &&
            values[2]==0x6au && values[3]==0x6bu && values[4]==0x6cu);
        ++checked_window_rows;
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
    ra.movie_exit[0]=(struct kui_toy_movie_exit_witness){
        .flags=3u,.start_status=0x7d2u,.output_frames=0u,.skipped_frames=0x12u,
        .status=0xfffff843u,.frame=0x34u,.samples=0x56u,.lag=0x78u,.limit=0x9au};
    ra.movie_exit[1]=(struct kui_toy_movie_exit_witness){
        .flags=7u,.start_status=0x7d2u,.output_frames=0u,.skipped_frames=0x123u,
        .status=0x7dcu,.frame=0x456u,.samples=0x789u};
    ra.movie_chunk[0]=(struct kui_toy_movie_chunk_trace){
        .ready=UINT32_MAX,.wait_gd=0x19u,.wait_no_gd=0x11u,.other=0x17u};
    ra.movie_chunk[1]=(struct kui_toy_movie_chunk_trace){
        .ready=UINT32_MAX,.wait_gd=0x39u,.wait_no_gd=0x35u,.other=0x29u};
    ra.suite[0]=0x5au; ra.suite[1]=0x6au; ra.suite[2]=0x5bu; ra.suite[3]=0x6bu;
    ra.demand.movie[0].slice_blocks=0x5cu; ra.demand.movie[1].slice_blocks=0x6cu;
    char buffer[KUI_TOY_REPORT_FILE_BYTES+2u];
    memset(buffer,'!',sizeof(buffer));
    char *text=buffer+1u;
    toy_report_format(text,&ra,&region.shared.stream.stats,49u);
    assert(buffer[0]=='!' && buffer[sizeof(buffer)-1u]=='!');
    assert(!memcmp(text+KUI_TOY_REPORT_FILE_BYTES-12u,"\nEND REPORT\n",12u));
    buffer[sizeof(buffer)-1u]=0;
    assert(strstr(text,"\nTEST 49\nBUILD DEADBEEF0123\n"));
    assert(strstr(text,"BUMPER SECT SPAN END FAIL CLK\nFFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF\n"));
    assert(strstr(text,"INTRO SECT SPAN END FAIL CLK\nFFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF\n"));
    assert(strstr(text,"SD-BLOCKS PIO START OVR CRC\nFFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF\n"));
    assert(strstr(text,"BMP RDY WGD RSM RET EXT\nFFFFFFFF 00000019 0000005A 0000005B 0000005C\n"));
    assert(strstr(text,"INTRO RDY WGD RSM RET EXT\nFFFFFFFF 00000039 0000006A 0000006B 0000006C\n"));
    assert(strstr(text,"BMP FLG START OUT SKIP\n00000003 000007D2 00000000 00000012\n"));
    assert(strstr(text,"BMP EXIT CODE FRAME SAMP LAG LIM\nFFFFF843 00000034 00000056 00000078 0000009A\n"));
    assert(strstr(text,"INTRO FLG START OUT SKIP\n00000007 000007D2 00000000 00000123\n"));
    const char *target=strstr(text,"INTRO EXIT CODE FRAME SAMP LAG LIM\n000007DC 00000456 00000789 00000000 00000000\n");
    assert(target);
    const char *end=strchr(strchr(target,'\n')+1,'\n')+1;
    assert(end<=text+1012);
    for(const char *pad=end;pad<text+1012;pad++)assert(*pad==' ');
    assert(!toy_suite_save_report(&ra,&region));
    toy_playback_page(&ra);
    assert(display_lines==14u);
    display_lines=0u;
    toy_storage_page(&ra,&region);
    assert(display_lines==13u && checked_witness_rows==4u && checked_window_rows==2u);
    if(argc==2) {
        FILE *out=fopen(argv[1],"wb");
        assert(out && fwrite(text,1u,KUI_TOY_REPORT_FILE_BYTES,out)==KUI_TOY_REPORT_FILE_BYTES);
        assert(fclose(out)==0);
    }
    printf("Mode 49 report: %td body bytes in 1024-byte file; screen bodies 14 and 13 rows\n",end-text);
}
