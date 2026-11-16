/* SPDX-License-Identifier: GPL-3.0-only */
#include "disc_toy_trace.h"
#include "kui/gd_service.h"
#include "kui/toy_fmv_compare.h"
#include "kui/toy_movie_exit.h"
#include "kui/toy_pilot_clock.h"
#include "kui/vmu_backing.h"
#include "sci_sd_bus.h"
#include "stage_fast_crc32.h"
#include "../dreamcast/sci_sd_storage.h"
#include <string.h>
#ifndef KUI_TOY_WIRE_PROBE
#define KUI_TOY_WIRE_PROBE 0
#endif
#if KUI_TOY_WIRE_PROBE
#include "toy_wire_probe.h"
#endif

struct kui_disc_toy_trace_config kui_disc_toy_config;
uint32_t kui_disc_toy_gd_active;
static struct kui_toy_fmv_trace trace;
static struct {
    uint32_t pr, function, argument, output, fad, sectors;
} gd;
static _Alignas(32) char report_text[KUI_TOY_REPORT_FILE_BYTES];
static _Alignas(32) uint8_t verify[KUI_TOY_REPORT_SECTOR_BYTES];

/* Keep the physical and SCI measurement code identical. The private
 * prefix holds the two game-hook records at the same linked offsets. */
struct kui_toy_movie_present_trace {
    uint32_t submitted, on_time, first_gap, max_gap, max_gap_frame;
    uint32_t tail_gap, last_frame, last_tick, clock_lost;
    uint32_t render_entries, scene_hits;
};
struct {
    uint8_t prefix[KUI_TOY_MOVIE_EXIT_OFFSET];
    struct kui_toy_movie_exit_witness movie_exit[2];
    uint8_t spare[1184u-KUI_TOY_MOVIE_EXIT_OFFSET-2u*KUI_TOY_MOVIE_EXIT_BYTES];
    struct kui_toy_movie_present_trace present[2];
} kui_native_readahead_aux_context;
#define C kui_native_readahead_aux_context
_Static_assert(offsetof(__typeof__(C),movie_exit)==KUI_TOY_MOVIE_EXIT_OFFSET,"Toy exit offset");
_Static_assert(offsetof(__typeof__(C),present)==1184u,"Toy present offset");

void kui_disc_toy_gd_proxy(void);
void kui_disc_toy_gd_return(void);

static uint32_t word(uint32_t address) {
    return *(volatile const uint32_t *)(uintptr_t)address;
}

static const volatile uint32_t *guest_words(uint32_t address, uint32_t bytes) {
    uint32_t area=address&0xff000000u;
    uint32_t physical=address&0x00ffffffu;
    if((area!=0x0c000000u && area!=0x8c000000u && area!=0xac000000u) ||
       (physical&3u) || physical<0x8000u || physical>0xfd0000u-bytes) return NULL;
    if(area==0x0c000000u) address|=0x80000000u;
    return (const volatile uint32_t *)(uintptr_t)address;
}

struct ra_clock {uint32_t counter,valid;};
static struct ra_clock clock_read(void) {
    const uint32_t frqcr=*(volatile const uint16_t *)0xffc00000u;
    const uint32_t tcor=word(0xffd80008u);
    const uint32_t tcr=*(volatile const uint16_t *)0xffd80010u;
    const uint32_t tstr=*(volatile const uint8_t *)0xffd80004u;
    return (struct ra_clock){word(0xffd8000cu),kui_toy_pilot_clock_profile(frqcr,tcor,tcr,tstr)};
}
#if KUI_TOY_WIRE_PROBE
/* The probe follows the same two player boundaries and scene submissions the
 * frame trace already observes. The trace's own records are unchanged. */
#define kui_toy_movie_present_boundary toy_present_boundary_traced
#define kui_toy_movie_scene_observe toy_scene_observe_traced
void toy_present_boundary_traced(uint32_t ctx,uintptr_t witness,unsigned phase);
void toy_scene_observe_traced(void);
#endif
#include "toy_movie_present_trace.inc"
#if KUI_TOY_WIRE_PROBE
#undef kui_toy_movie_present_boundary
#undef kui_toy_movie_scene_observe
void kui_toy_movie_present_boundary(uint32_t ctx,uintptr_t witness,unsigned phase) {
    const unsigned intro=witness==(uintptr_t)&C.movie_exit[1];
    if(phase==KUI_TOY_MOVIE_EXIT_STARTED) {
        toy_present_boundary_traced(ctx,witness,phase);
        if(C.movie_exit[intro].start_status==0x07dbu)kui_wire_movie_start(intro);
        return;
    }
    kui_wire_movie_end(intro);
    toy_present_boundary_traced(ctx,witness,phase);
}

void kui_toy_movie_scene_observe(void) {
    toy_scene_observe_traced();
    if(toy_present.owner && toy_present.trace->submitted)
        kui_wire_frame(toy_present.trace==&C.present[1]);
}
#endif

static bool movie_request(uint32_t fad, uint32_t count) {
    if(fad<KUI_GD_FAD_OFFSET || !count) return false;
    uint32_t lba=fad-KUI_GD_FAD_OFFSET;
    for(unsigned i=0;i<4u;i++) {
        const struct kui_toy_movie_range *movie=&kui_disc_toy_config.movie[i];
        if(lba>=movie->lba && lba-movie->lba>=386u && lba-movie->lba<movie->count) return true;
    }
    return false;
}

static bool tracked_token(uint32_t token) {
    if(!token) return false;
    for(unsigned i=0;i<KUI_TOY_FMV_PENDING;i++)
        if(trace.pending[i].token==token) return true;
    return false;
}

void kui_disc_toy_gd_before(uint32_t frame[21]) {
#if KUI_TOY_WIRE_PROBE
    kui_wire_poll();
#endif
    if(frame[14]!=0u) return;
    uint32_t function=frame[13], argument=frame[16], output=frame[15];
    gd.fad=0u;
    if(function==KUI_GD_REQUEST) {
        if(argument!=KUI_GD_PIOREAD && argument!=KUI_GD_DMAREAD) return;
        const volatile uint32_t *parameters=guest_words(output,16u);
        if(!parameters) return;
        uint32_t fad=parameters[0], sectors=parameters[1];
        if(!movie_request(fad,sectors)) return;
        gd.fad=fad;
        gd.sectors=sectors;
    } else if(function!=KUI_GD_CHECK || !tracked_token(argument)) return;
    gd.pr=frame[5];
    gd.function=function;
    gd.argument=argument;
    gd.output=output;
    kui_disc_toy_gd_active=1u;
    frame[5]=(uint32_t)(uintptr_t)kui_disc_toy_gd_return;
}

void kui_disc_toy_gd_after(uint32_t frame[21]) {
    frame[5]=gd.pr;
    kui_disc_toy_gd_active=0u;
    int32_t result=(int32_t)frame[20];
    if(gd.function==KUI_GD_REQUEST && result>0 && gd.fad) {
#if KUI_TOY_WIRE_PROBE
        kui_wire_gd(true);
#endif
        const struct ra_clock now=clock_read();
        if(!now.valid) {++trace.clock_lost;return;}
        kui_toy_fmv_request(&trace,kui_disc_toy_config.movie,(uint32_t)result,
            gd.fad-KUI_GD_FAD_OFFSET,gd.sectors,now.counter);
    } else if(gd.function==KUI_GD_CHECK && gd.argument &&
              (result==KUI_GD_COMPLETED || result==KUI_GD_FAILED)) {
#if KUI_TOY_WIRE_PROBE
        kui_wire_gd(false);
#endif
        const volatile uint32_t *status=guest_words(gd.output,16u);
        if(!status) return;
        const struct ra_clock now=clock_read();
        if(!now.valid) {++trace.clock_lost;return;}
        kui_toy_fmv_status(&trace,gd.argument,now.counter,result,status[2]);
    }
}

static const char *write_verified(struct kui_loader_sd *card) {
    const struct kui_toy_report_wire *seed=&kui_disc_toy_config.report;
    struct kui_vmu_sd_identity identity;
    if(!card->ready || card->slow || card->high_capacity!=!!seed->high_capacity ||
       seed->lba[0]>=card->blocks || seed->lba[1]>=card->blocks) return "SD GEOMETRY";
    if(kui_vmu_sd_identity_read(card,&identity)!=KUI_VMU_BACKING_OK) return "SD ID READ";
    if(memcmp(identity.cid,seed->cid,sizeof(identity.cid))) return "SD ID MISMATCH";
    for(unsigned i=0;i<2u;i++) {
        if(kui_loader_sd_read(card,seed->lba[i],1u,verify)!=KUI_LOADER_SD_OK) return "PLACEHOLDER READ";
        if(kui_stage_crc32(0u,verify,sizeof(verify))!=seed->placeholder_crc[i]) return "PLACEHOLDER CRC";
    }

    /* The retail SCI block callback accepts reads only. Use the same byte
     * write path as the SCI trace and physical-disc VMU writer. */
    bool (*transfer_block)(void *,const uint8_t *,uint8_t *,size_t,bool,uint16_t *)=card->bus.transfer_block;
    card->bus.transfer_block=NULL;
    enum kui_loader_sd_result result=KUI_LOADER_SD_OK;
    for(unsigned i=0;i<2u && result==KUI_LOADER_SD_OK;i++) {
        result=kui_sci_sd_write(card,seed->lba[i],1u,(const uint8_t *)report_text+i*sizeof(verify));
    }
    card->bus.transfer_block=transfer_block;
    if(result!=KUI_LOADER_SD_OK) return "SD DATA WRITE";
    if(kui_sci_sd_sync(card)!=KUI_LOADER_SD_OK) return "SD SYNC";
    for(unsigned i=0;i<2u;i++) {
        if(kui_loader_sd_read(card,seed->lba[i],1u,verify)!=KUI_LOADER_SD_OK) return "SD READBACK";
        if(memcmp(verify,report_text+i*sizeof(verify),sizeof(verify))) return "SD VERIFY MISMATCH";
    }
    return NULL;
}

static const char *save_report(void) {
    if(!kui_disc_toy_config.report.test)return "PHOTO ONLY / NO SD REPORT RESERVED";
    if(kui_sci_sd_acquire()!=KUI_LOADER_SD_OK) return "SCI PORT ACQUIRE";
    struct kui_loader_sd card;
    const char *error=kui_loader_sd_init_bus(&card,kui_sci_sd_bus())==KUI_LOADER_SD_OK ?
        write_verified(&card) : "SD INIT";
    if(!error && !kui_sci_sd_healthy()) error="SCI PORT FAULT";
    kui_sci_sd_release();
    if(!error && kui_sci_sd_faulted()) error="SCI PORT RESTORE";
    return error;
}

static char *report_string(char *at,const char *value) {
    while(*value)*at++=*value++;
    return at;
}

static void report_row(char **at,const char *label,const uint32_t *values,unsigned count) {
    if(!at) {
#if KUI_TOY_WIRE_PROBE
        /* The screen shows five values a row. */
        if(count>5u) {
            retail_display_values(label,values,3u);
            retail_display_values("...",values+3u,count-3u);
            return;
        }
#endif
        retail_display_values(label,values,count);
        return;
    }
    char *out=report_string(*at,label);
    *out++='\n';
    for(unsigned i=0;i<count;i++) {
        for(unsigned shift=32u;shift;shift-=4u)
            *out++="0123456789ABCDEF"[(values[i]>>(shift-4u))&15u];
        *out++=i+1u==count?'\n':' ';
    }
    *at=out;
}

#if !KUI_TOY_WIRE_PROBE
static void movie_rows(char **at,unsigned i) {
    const struct kui_toy_movie_exit_witness *w=&C.movie_exit[i];
    const struct kui_toy_movie_present_trace *p=&C.present[i];
    report_row(at,i?"I SUBMIT CADENCE MAX AT TAIL":"B SUBMIT CADENCE MAX AT TAIL",
        (uint32_t[]){p->submitted,p->on_time,p->max_gap,p->max_gap_frame,p->tail_gap},5u);
    report_row(at,i?"I FIRST LAST CLK OUT SKIP":"B FIRST LAST CLK OUT SKIP",
        (uint32_t[]){p->first_gap,p->last_frame,p->clock_lost,w->output_frames,w->skipped_frames},5u);
    report_row(at,i?"I ENTER SCENE EXIT FRAME":"B ENTER SCENE EXIT FRAME",
        (uint32_t[]){p->render_entries,p->scene_hits,w->status,w->frame},4u);
}

static void transport_rows(char **at,unsigned i) {
    const struct kui_toy_fmv_movie *m=&trace.movie[i];
    uint32_t total=0u,longest=0u,gap=0u;
    for(unsigned j=0;j<KUI_TOY_FMV_BINS;j++) {
        const struct kui_toy_fmv_bin *bin=&m->bins[j];
        total+=bin->completed;
        if(bin->longest>longest)longest=bin->longest;
        if(bin->gap>gap)gap=bin->gap;
    }
    report_row(at,i?"I REQ ACK FAIL SHORT END":"B REQ ACK FAIL SHORT END",
        (uint32_t[]){m->requests,m->acks,m->failed,m->short_bytes,m->end_lba},5u);
    report_row(at,i?"I SECT MAXREQ MAXGAP":"B SECT MAXREQ MAXGAP",
        (uint32_t[]){total,longest,gap},3u);
}
#endif

#if KUI_TOY_WIRE_PROBE
/* Every row is a fixed label and fixed-width values, so the report has one
 * length whatever the run measured. sizeof counts the label's newline. */
#define WIRE_HEAD "K-UI TOY COMMANDER REPORT\nTEST 67\nBUILD " KUI_BUILD_ID \
    "\nHEX / 781250 HZ\n"
#define WIRE_STATE "ST ERR AUX MODE SEGS"
#define WIRE_FRAMES "SUB CAD MAXGAP SKIP EXIT MAXREQ"
#define WIRE_BYTES "SPAN BYTES CUTS IRQ ISR"
#define WIRE_PAUSE "PAUSE TICKS MAX LATE"
#define WIRE_DISC "GD TICKS BYTES CUTS"
#define WIRE_RUNS "RUN 16 64 256 1K 4K MORE"
#define WIRE_CODE "PC N PC N"
#define WIRE_TAIL "TAIL BYTES OK BAD HOLE"
#define WIRE_ROW(label,values) (sizeof(label)+9u*(values))
#define WIRE_MOVIE_ROW(label,values) (2u+WIRE_ROW(label,values))
_Static_assert(sizeof(WIRE_HEAD)-1u+WIRE_ROW(WIRE_STATE,5u)+
    2u*(WIRE_MOVIE_ROW(WIRE_FRAMES,6u)+WIRE_MOVIE_ROW(WIRE_BYTES,5u)+WIRE_MOVIE_ROW(WIRE_PAUSE,4u)+
        WIRE_MOVIE_ROW(WIRE_DISC,3u)+WIRE_MOVIE_ROW(WIRE_RUNS,6u)+WIRE_MOVIE_ROW(WIRE_CODE,4u))+
    WIRE_ROW(WIRE_TAIL,4u)+12u<=KUI_TOY_REPORT_FILE_BYTES,
    "Toy wire probe report must fit its two reserved sectors");

static uint32_t longest_request(unsigned i) {
    uint32_t longest=0u;
    for(unsigned j=0;j<KUI_TOY_FMV_BINS;j++)
        if(trace.movie[i].bins[j].longest>longest)longest=trace.movie[i].bins[j].longest;
    return longest;
}

/* part 0: frames, 1: wire totals, 2: waits, 3: disc share, 4: run lengths,
 * 5: code. */
static void wire_row(char **at,unsigned i,unsigned part) {
    const struct kui_toy_movie_exit_witness *w=&C.movie_exit[i];
    const struct kui_toy_movie_present_trace *p=&C.present[i];
    const struct kui_wire_window *v=&kui_wire_summary()->window[i];
    switch(part) {
    case 0u:report_row(at,i?"I " WIRE_FRAMES:"B " WIRE_FRAMES,(uint32_t[]){p->submitted,p->on_time,
        p->max_gap,w->skipped_frames,w->status,longest_request(i)},6u);break;
    case 1u:report_row(at,i?"I " WIRE_BYTES:"B " WIRE_BYTES,(uint32_t[]){v->span,v->total.bytes,
        v->total.cuts,v->total.irqs,v->total.isr_ticks},5u);break;
    case 2u:report_row(at,i?"I " WIRE_PAUSE:"B " WIRE_PAUSE,(uint32_t[]){v->total.pauses,
        v->total.paused_ticks,v->pause_max,v->total.late_ticks},4u);break;
    case 3u:report_row(at,i?"I " WIRE_DISC:"B " WIRE_DISC,(uint32_t[]){v->gd_ticks,v->gd_bytes,
        v->gd_cuts},3u);break;
    case 4u:report_row(at,i?"I " WIRE_RUNS:"B " WIRE_RUNS,v->total.run,KUI_WIRE_RUN_BUCKETS);break;
    default:report_row(at,i?"I " WIRE_CODE:"B " WIRE_CODE,(uint32_t[]){v->top_pc[0],v->top_count[0],
        v->top_pc[1],v->top_count[1]},4u);break;
    }
}

static void wire_state_row(char **at) {
    const struct kui_wire_summary *s=kui_wire_summary();
    report_row(at,WIRE_STATE,(uint32_t[]){s->state,s->error,s->detail,s->mode,s->segments},5u);
}

static void wire_tail_rows(char **at) {
    const struct kui_wire_summary *s=kui_wire_summary();
    report_row(at,WIRE_TAIL,(uint32_t[]){s->tail_bytes,s->tail_ok,s->tail_bad,s->tail_holed},4u);
}

static void format_report(void) {
    memset(report_text,' ',sizeof(report_text));
    char *at=report_string(report_text,WIRE_HEAD);
    wire_state_row(&at);
    for(unsigned i=0;i<2u;i++)
        for(unsigned part=0;part<6u;part++)wire_row(&at,i,part);
    wire_tail_rows(&at);
    memcpy(report_text+sizeof(report_text)-12u,"\nEND REPORT\n",12u);
}
#else
static void format_report(void) {
    memset(report_text,' ',sizeof(report_text));
    char *at=report_string(report_text,"K-UI TOY COMMANDER REPORT\nTEST 63\nBUILD " KUI_BUILD_ID
        "\nHEX / 781250 HZ\nPHYSICAL GD-ROM\n");
    for(unsigned i=0;i<2u;i++) {
        movie_rows(&at,i);
        transport_rows(&at,i);
    }
    report_row(&at,"GD CLOCK LOST",(uint32_t[]){trace.clock_lost},1u);
    report_row(&at,"I LAG LIMIT",(uint32_t[]){C.movie_exit[1].lag,C.movie_exit[1].limit},2u);
    memcpy(report_text+sizeof(report_text)-12u,"\nEND REPORT\n",12u);
}
#endif

void kui_disc_toy_terminal(void) __attribute__((noreturn));
void kui_disc_toy_terminal(void) {
    volatile uint32_t *vector=(volatile uint32_t *)KUI_DISC_TOY_TRACE_GD_VECTOR;
    if(*vector==(uint32_t)(uintptr_t)kui_disc_toy_gd_proxy)
        *vector=kui_disc_toy_config.firmware;
#if KUI_TOY_WIRE_PROBE
    /* Stop the stream and give the SCI port back before the report uses it. */
    kui_wire_shutdown();
#endif
    format_report();
    const char *error=save_report();
    retail_display_restore(&kui_disc_toy_config.display);
    if(!error) {
#if KUI_TOY_WIRE_PROBE
        retail_display_line("TOY COMMANDER GD-ROM + SCI WIRE PROBE");
#else
        retail_display_line("TOY COMMANDER PHYSICAL GD-ROM");
#endif
        retail_display_line("REPORT SAVED TO SD");
        retail_display_line("POWER OFF AFTER CAPTURE");
#if KUI_TOY_WIRE_PROBE
    } else {
        /* The same rows as the file, at most twelve lines a screen. */
        unsigned frames=(kui_disc_toy_config.display.regs[8]>>16)>=600u?250u:300u;
        for(unsigned page=0;;page=page==4u?0u:page+1u) {
            static const char *const title[5]={"TOY TEST 67 / 1 OF 5 / HEX","TOY TEST 67 / 2 OF 5 / HEX",
                "TOY TEST 67 / 3 OF 5 / HEX","TOY TEST 67 / 4 OF 5 / HEX","TOY TEST 67 / 5 OF 5 / HEX"};
            retail_display_line(title[page]);
            if(!page) {
                retail_display_line(error);
                wire_state_row(NULL);
                wire_tail_rows(NULL);
            } else {
                const unsigned movie=(page-1u)>>1;
                if(page&1u)for(unsigned part=0;part<3u;part++)wire_row(NULL,movie,part);
                else for(unsigned part=3u;part<6u;part++)wire_row(NULL,movie,part);
            }
            retail_display_line("FILM ALL FIVE PAGES");
            retail_display_pause(frames);
            retail_display_restore(&kui_disc_toy_config.display);
        }
    }
#else
    } else {
        unsigned frames=(kui_disc_toy_config.display.regs[8]>>16)>=600u?250u:300u;
        for(;;) {
            retail_display_line("TOY GD-ROM TEST 63 / 1 OF 2 / HEX");
            retail_display_line(error);
            for(unsigned i=0;i<2u;i++)movie_rows(NULL,i);
            retail_display_line("FILM BOTH PAGES");
            retail_display_pause(frames);
            retail_display_restore(&kui_disc_toy_config.display);
            retail_display_line("TOY GD-ROM TEST 63 / 2 OF 2 / HEX");
            for(unsigned i=0;i<2u;i++)transport_rows(NULL,i);
            retail_display_values("GD CLOCK LOST",(uint32_t[]){trace.clock_lost},1u);
            retail_display_values("I LAG LIMIT",
                (uint32_t[]){C.movie_exit[1].lag,C.movie_exit[1].limit},2u);
            retail_display_line("PHOTO THEN POWER OFF");
            retail_display_pause(frames);
            retail_display_restore(&kui_disc_toy_config.display);
        }
    }
#endif
    for(;;) __asm__ volatile("nop");
}
