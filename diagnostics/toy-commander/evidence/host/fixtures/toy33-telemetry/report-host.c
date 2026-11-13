#include <assert.h>
#include <stdio.h>
#include <string.h>

#define KUI_TOY_SUITE_MODE 33
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
#define KUI_TOY_TEST_TITLE "TOY TEST 33"

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

static unsigned display_lines;
void retail_display_line(const char *label) {
    assert(strlen(label)<=49u);++display_lines;
}
void retail_display_hex(const char *label,uint32_t value) {
    (void)value;assert(strlen(label)<=40u);++display_lines;
}
void retail_display_values(const char *label,const uint32_t *values,unsigned count) {
    assert(count>0u && count<=5u);
    if(!strcmp(label,"INTRO NOARM N SUM MAX WAKE N ARM"))
        assert(count==5u && values[0]==17u && values[4]==21u);
    if(!strcmp(label,"BMP EXEC SUM MAX CHECK SUM MAX"))
        assert(count==4u && values[0]==2u && values[3]==6u);
    if(!strcmp(label,"BMP POLL OTHER 900 MOVIE BLOCK"))
        assert(count==4u && values[0]==22u && values[3]==25u);
    if(!strcmp(label,"INTRO POLL OTHER 900 MOVIE BLOCK"))
        assert(count==4u && values[0]==26u && values[3]==29u);
    retail_display_line(label);++display_lines;
}
#include "screen-extract.inc"

int main(void) {
    kui_retail_async_region_type region={0};
    struct kui_native_ra_public ra;
    memset(&ra,0xff,sizeof(ra));
    memset(&region.shared.stream.stats,0xff,sizeof(region.shared.stream.stats));
    ra.demand.movie[0]=(struct kui_toy_demand_movie_trace){
        .exec={1,2,3},.check={4,5,6},.no_arm={7,8,9},.wakes=10,.wake_arms=11,.polls={22,23,24,25}};
    ra.demand.movie[1]=(struct kui_toy_demand_movie_trace){
        .exec={UINT32_MAX,12,13},.check={14,15,16},.no_arm={17,18,19},.wakes=20,.wake_arms=21,.polls={26,27,28,29}};
    char buffer[KUI_TOY_REPORT_FILE_BYTES+2u];
    memset(buffer,'!',sizeof(buffer));
    char *text=buffer+1u;
    toy_report_format(text,&ra,&region.shared.stream.stats,33u);
    assert(buffer[0]=='!' && buffer[sizeof(buffer)-1u]=='!');
    assert(!memcmp(text+KUI_TOY_REPORT_FILE_BYTES-12u,"\nEND REPORT\n",12u));
    buffer[sizeof(buffer)-1u]=0;
    assert(strstr(text,"\nTEST 33\nBUILD DEADBEEF0123\n"));
    assert(strstr(text,"BMP EXEC N SUM MAX CHECK N SUM MAX\n"
        "00000001 00000002 00000003 00000004 00000005 00000006\n"));
    assert(strstr(text,"BMP NOARM N SUM MAX WAKE N ARM\n"
        "00000007 00000008 00000009 0000000A 0000000B\n"));
    assert(strstr(text,"INTRO EXEC N SUM MAX CHECK N SUM MAX\n"
        "FFFFFFFF 0000000C 0000000D 0000000E 0000000F 00000010\n"));
    assert(strstr(text,"INTRO NOARM N SUM MAX WAKE N ARM\n"
        "00000011 00000012 00000013 00000014 00000015\n"));
    const char *poll=strstr(text,"POLL OTHER 900 MOVIE BLOCK / BMP INTRO\n"
        "00000016 00000017 00000018 00000019 0000001A 0000001B 0000001C 0000001D\n");
    assert(poll);
    const char *end = strchr(strchr(poll, '\n')+1, '\n')+1;
    assert(end<=text+1012);
    for(const char *pad=end;pad<text+1012;pad++)assert(*pad==' ');
    assert(!toy_suite_save_report(&ra,&region));
    toy_playback_page(&ra);assert(display_lines==14u);
    display_lines=0u;
    toy_storage_page(&ra,&region);assert(display_lines==13u);
    printf("Mode33 report: %td body bytes in 1024-byte file; screen bodies 14 and 13 rows\n",end-text);
}
