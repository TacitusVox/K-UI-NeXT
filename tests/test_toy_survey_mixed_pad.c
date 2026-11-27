/* SPDX-License-Identifier: GPL-3.0-only */
/* The Test 83 worker must finish an A-port hold on the same completion
 * whether the SDK also polled accessories, or took the fast controller path. */
#define KUI_SPLIT_CONTROLLER_TEST_MAIN original_split_tests
#include "test_vmu_native_split_controller.c"

static kui_retail_async_region_type region;
void *kui_vmu_native_reader_test_region(uint32_t address) {
    (void)address;
    return &region;
}
/* Test 84's switch and Test 85's, off where the recipe does not set them (an
 * undefined name is 0 in #if, but not in C). */
#ifndef KUI_TOY_CDDA_SURVEY
#define KUI_TOY_CDDA_SURVEY 0
#endif
#ifndef KUI_TOY_CDDA_CARD
#define KUI_TOY_CDDA_CARD 0
#endif
#if KUI_TOY_CDDA_SURVEY || KUI_TOY_CDDA_CARD
/* The drive, and what both tests count: the completions of each path. */
#include "kui/toy_cdda_player.h"
struct kui_toy_cdda_drive kui_toy_cdda_drive;
static unsigned long surveys,fast_polls,mixed_polls;
#if KUI_NATIVE_COOP_PUMP
/* The worker's cooperative service, built as the worker has it: with no read
 * active in this region it returns before its visit. */
int kui_native_cooperative_test_visit(void) { CHECK(false); return 0; }
#endif
#endif
#if KUI_TOY_CDDA_SURVEY
/* Test 84 (SURVEY): the same with the drive playing, every completion with
 * the controller then surveying the sound chip. Here an idle bus and a sound
 * memory of zeros; each poll a sixtieth of a second after the last. */
#include "kui/toy_cdda_survey.h"
static uint32_t survey_sr=UINT32_C(0x40000000),survey_clock=UINT32_C(0x70000000);
static unsigned long reset_reads,fast_surveys,mixed_surveys;
uint32_t kui_toy_cdda_survey_test_read(uint32_t address,unsigned width) {
    (void)width;
    /* TMU0 in the Toy profile. */
    if(address==UINT32_C(0xffc00000))return 0x0e0au;
    if(address==UINT32_C(0xffd80008))return UINT32_MAX;
    if(address==UINT32_C(0xffd80010))return 0x0002u;
    if(address==UINT32_C(0xffd80004))return 1u;
    if(address==UINT32_C(0xffd8000c))return --survey_clock;
    /* A survey reads the reset bit first, twice, until two reads agree. */
    if(address==UINT32_C(0xa0702c00))++reset_reads;
    surveys=reset_reads/2u;
    return 0u;
}
uint32_t kui_toy_cdda_survey_test_sr(void) { return survey_sr; }
void kui_toy_cdda_survey_test_set_sr(uint32_t value) { survey_sr=value; }
#define SURVEY_TIME() (survey_clock-=13021u)
#define SURVEY_COUNT(kind,before) (kind##_surveys+=surveys-(before))
#else
#define SURVEY_TIME() ((void)0)
#define SURVEY_COUNT(kind,before) ((void)(before))
#endif
#if KUI_TOY_CDDA_CARD
/* Test 85 (CARD): the audio's consumer is called where the survey is, under
 * the same rule, and behind it where the worker has a survey. Here it counts
 * its calls and looks whether the completion's survey has been made
 * (src/loader/toy_cdda_card.c has the real one, tests/test_toy_cdda_card.inc
 * its test). */
static unsigned long consumers,fast_consumers,mixed_consumers,consumer_surveys;
void kui_toy_cdda_consumer_heartbeat(void) {
    ++consumers;
    if(KUI_TOY_CDDA_SURVEY && kui_toy_cdda_drive.state==KUI_TOY_CDDA_PLAYING) CHECK(surveys==consumer_surveys+1u);
}
#define CONSUMERS consumers
#define CONSUMER_MARK() (consumer_surveys=surveys)
#define CONSUMER_COUNT(kind,before) (kind##_consumers+=consumers-(before))
#else
#define CONSUMERS 0ul
#define CONSUMER_MARK() ((void)0)
#define CONSUMER_COUNT(kind,before) ((void)(before))
#endif
#if KUI_TOY_CDDA_SURVEY || KUI_TOY_CDDA_CARD
#define SURVEY_POLL(kind,call) do { const unsigned long before_=surveys,consumers_=CONSUMERS; CONSUMER_MARK(); \
    SURVEY_TIME(); call; ++kind##_polls; SURVEY_COUNT(kind,before_); CONSUMER_COUNT(kind,consumers_); } while(0)
#else
#define SURVEY_POLL(kind,call) call
#endif
static const uint32_t held=UINT32_C(0xfffff9f9);
static const uint32_t condition_reply=UINT32_C(0x03200008);
static const uint32_t controller_fn=UINT32_C(0x01000000);

static void setup_survey(void) {
    split_prepare();
    input.diagnostics=KUI_VMU_NATIVE_DIAGNOSTICS_READER_ONLY;
    memset(&input.durable,0,sizeof(input.durable));
    arm();install_hooks();
    CHECK(reader_only());
}
static void finish_poll(bool terminal_expected) {
    uint8_t original[3][1024];
    const unsigned slots[]={0u,VIRTUAL,6u};
    for(unsigned i=0;i<3u;i++)memcpy(original[i],rx_bytes(slots[i]),sizeof(original[i]));
    if(terminal_expected) {
        if(!setjmp(terminal)) {kui_vmu_native_passive_complete(&frame);CHECK(false);}
        CHECK(terminal_error==0x4fu);
    } else complete_return();
    CHECK(state.submitted==state.completed && !state.pending);
    for(unsigned i=0;i<3u;i++)CHECK(!memcmp(original[i],rx_bytes(slots[i]),sizeof(original[i])));
}
/* Original physical batch: A condition, ancillary A query, then B query. */
static void mixed_poll_once(uint32_t response,uint32_t function,uint32_t controls,bool terminal_expected) {
    stage_batch(1u,0u);
    submit();CHECK(!controller_pair.pending);
    physical_complete();
    put(rx_at(VIRTUAL),4u,UINT32_MAX);
    put(rx_at(0),4u,response);
    put(rx_at(0)+4u,4u,function);
    put(rx_at(0)+8u,4u,controls);
    finish_poll(terminal_expected);
}
static void mixed_poll(uint32_t response,uint32_t function,uint32_t controls,bool terminal_expected) {
    SURVEY_POLL(mixed,mixed_poll_once(response,function,controls,terminal_expected));
}
static void fast_poll_once(uint32_t a,uint32_t b,bool terminal_expected) {
    split_batch(3u,0u,0u);split_submit_ok();split_done(3u,0u,controller_fn);
    put(rx_at(0)+8u,4u,a);put(rx_at(6)+8u,4u,b);finish_poll(terminal_expected);
}
static void fast_poll(uint32_t a,uint32_t b,bool terminal_expected) {
    SURVEY_POLL(fast,fast_poll_once(a,b,terminal_expected));
}
static void other_port_poll(void) {
    stage_batch(1u,0u);
    uint32_t source=value_at(0xa05f6c04u);
    put(source,4u,0x20001u);put(source+4u,4u,rx_at(12u));put(source+8u,4u,0x0180a009u);
    submit();CHECK(!controller_pair.pending);
    physical_complete();
    put(rx_at(12u),4u,0x03a08008u);put(rx_at(12u)+4u,4u,controller_fn);
    put(rx_at(12u)+8u,4u,held);put(rx_at(VIRTUAL),4u,UINT32_MAX);
    finish_poll(false);
}
#if KUI_TOY_CDDA_SURVEY || KUI_TOY_CDDA_CARD
/* The early SDK's drivers: the survey follows the reader's rule on the fast
 * path, a GETCOND parent's completion only, never an INFO or ALLINFO
 * discovery batch's. And so does the audio's consumer. */
static void early_family_survey(void) {
    split_early_setup_mode(1u,1u,KUI_VMU_NATIVE_DIAGNOSTICS_READER_ONLY);
    submit_sr=complete_sr=0x60000101u;
    unsigned long before=surveys;
    const unsigned long consumers_before=CONSUMERS;
    CONSUMER_MARK();
    SURVEY_TIME();
    split_batch(0u,15u,0u);split_submit_ok();split_done(0u,15u,controller_fn);complete_return();
    CHECK(surveys==before && CONSUMERS==consumers_before);
    SURVEY_TIME();
    split_batch(1u,0u,0u);split_submit_ok();split_done(1u,0u,controller_fn);complete_return();
    CHECK(surveys==before+(KUI_TOY_CDDA_SURVEY?1u:0u) && CONSUMERS==consumers_before+(KUI_TOY_CDDA_CARD?1u:0u));
    held_report=0u;
}
/* The fast path's survey comes after the controller's completion: the
 * completion that finishes the hold hands over to the terminal and makes no
 * survey. */
static void fast_terminal_survey(void) {
    setup_survey();
    for(unsigned i=0;i<HELD_REPORT_FRAMES-1u;i++) fast_poll(held,UINT32_MAX,false);
    CHECK(held_report==HELD_REPORT_FRAMES-1u);
    const unsigned long before=surveys,consumers_before=CONSUMERS;
    SURVEY_TIME();
    fast_poll_once(held,UINT32_MAX,true);
    CHECK(surveys==before && CONSUMERS==consumers_before);
    held_report=0u;
}
#endif
int main(void) {
    setup_survey();
    stage_batch(1u,0u);
    uint32_t second=value_at(0xa05f6c04u)+16u;
    put(second+4u,4u,rx_at(0));put(second+8u,4u,0x00002001u);
    reject_submit();CHECK(!held_report);

    setup_survey();
#if KUI_TOY_CDDA_SURVEY || KUI_TOY_CDDA_CARD
    /* Paused, no survey on either path; then the drive plays for the rest. */
    kui_toy_cdda_drive.state=KUI_TOY_CDDA_PAUSED;
    fast_poll(UINT32_MAX,UINT32_MAX,false);
    mixed_poll(condition_reply,controller_fn,UINT32_MAX,false);
    CHECK(!surveys && !held_report);
#if KUI_TOY_CDDA_CARD
    /* The audio's consumer is called all the same: whether a play stands is
     * its own to ask. */
    CHECK(consumers==2u && fast_consumers==1u && mixed_consumers==1u);
#endif
    kui_toy_cdda_drive.state=KUI_TOY_CDDA_PLAYING;
    early_family_survey();
    fast_terminal_survey();
    setup_survey();
    surveys=0u;
    fast_polls=mixed_polls=0u;
#if KUI_TOY_CDDA_SURVEY
    reset_reads=0u;
    fast_surveys=mixed_surveys=0u;
#endif
#if KUI_TOY_CDDA_CARD
    consumers=fast_consumers=mixed_consumers=0u;
#endif
#endif
    for(unsigned i=0;i<185u;i++)fast_poll(UINT32_MAX,held,false);
    CHECK(!held_report);
    for(unsigned i=0;i<179u;i++)mixed_poll(condition_reply,controller_fn,held,false);
    CHECK(held_report==179u);
#if KUI_TOY_CDDA_SURVEY || KUI_TOY_CDDA_CARD
    /* A batch without the port-A controller: no survey, its time due or not;
     * and no call of the audio's consumer. */
    {
        const unsigned long before=surveys,consumers_before=CONSUMERS;
        SURVEY_TIME();
        other_port_poll();
        CHECK(surveys==before && CONSUMERS==consumers_before);
    }
#else
    other_port_poll();
#endif
    CHECK(held_report==179u);
    mixed_poll(condition_reply,controller_fn,UINT32_MAX,false);CHECK(!held_report);
    mixed_poll(condition_reply,controller_fn,held,false);CHECK(held_report==1u);
    mixed_poll(condition_reply,UINT32_C(0x02000000),held,false);CHECK(!held_report);
    mixed_poll(condition_reply,controller_fn,held,false);
    mixed_poll(UINT32_MAX,controller_fn,held,false);CHECK(!held_report);
    mixed_poll(condition_reply,controller_fn,held,false);
    mixed_poll(UINT32_C(0x02200008),controller_fn,held,false);CHECK(!held_report);
    for(unsigned i=0;i<179u;i++) {
        if(i&1u)fast_poll(held,UINT32_MAX,false);
        else mixed_poll(condition_reply,controller_fn,held,false);
        CHECK(held_report==i+1u);
    }
    region.shared.service.pending=1u;
    mixed_poll(condition_reply,controller_fn,held,false);
    region.shared.service.pending=0u;
    mixed_poll(condition_reply,controller_fn,held,true);
#if KUI_TOY_CDDA_SURVEY
    /* Every fast completion and every mixed one with the controller made one
     * survey, the last before its terminal handoff excepted. */
    CHECK(fast_polls && fast_surveys==fast_polls && mixed_surveys==mixed_polls-1u && surveys==fast_surveys+mixed_surveys);
    CHECK(kui_toy_cdda_survey.family==KUI_TOY_CDDA_FAMILY_UNKNOWN && kui_toy_cdda_survey.identified &&
          !kui_toy_cdda_survey.timeouts && !kui_toy_cdda_survey.skipped_bl && !kui_toy_cdda_survey.skipped_dma);
    printf("PASS mixed and fast Test 83 controller hold, unchanged game input and retired terminal handoff, "
           "with the drive playing: %lu fast and %lu mixed completions each surveyed the sound chip\n",
           fast_surveys,mixed_surveys);
#endif
#if KUI_TOY_CDDA_CARD
    /* Every fast completion and every mixed one with the controller called
     * the audio's consumer once, the last before its terminal handoff
     * excepted, and no other completion did; with a survey, behind it (the
     * consumer has looked each time). */
    CHECK(fast_polls && fast_consumers==fast_polls && mixed_consumers==mixed_polls-1u &&
          consumers==fast_consumers+mixed_consumers);
    printf("PASS the same with Test 85's consumer%s: %lu fast and %lu mixed completions each called it once%s\n",
           KUI_TOY_CDDA_SURVEY?"":" and no survey",fast_consumers,mixed_consumers,
           KUI_TOY_CDDA_SURVEY?", behind the survey":"");
#endif
#if !KUI_TOY_CDDA_SURVEY && !KUI_TOY_CDDA_CARD
    puts("PASS mixed and fast Test 83 controller hold, unchanged game input and retired terminal handoff");
#endif
    return 0;
}
