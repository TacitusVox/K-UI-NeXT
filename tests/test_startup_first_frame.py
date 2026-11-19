#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Execute the production root startup prefix, theme lookup order, and worker.

The frozen 2.0 prefix is the positive lifecycle control. The later early-artwork
workaround is the failing control for a second pre-splash mount. Cold selection
and warm handoff share the real first-frame mount after the first themed draw.
"""
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'src/dreamcast/main.c').read_text()
END = '    kthread_t *io_worker=thd_create_ex(&attrs,worker,NULL);'
start = SOURCE.index('int main(void) {') + len('int main(void) {')
prefix = SOURCE[start:SOURCE.index(END, start) + len(END)]

def body(text, marker):
    at = text.index(marker) + len(marker)
    level, end = 1, at
    while level:
        level += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[at:end - 1]

worker = body(SOURCE, '            if(action==27) {')
worker_body = body(SOURCE, 'static void *worker(void *unused) {')
worker_tail = worker_body[worker_body.rindex('        mutex_lock(&lock);\n        busy = false;'):
                          worker_body.rindex('\n    }\n    return NULL;')]
vmu_marker = 'static void vmu_tools_warm_mounted(void) {'
settings_marker = 'static void settings_operation(bool save,bool first_frame) {'
theme_source = (ROOT / 'src/dreamcast/theme_runtime.inc').read_text()
theme_marker = 'static void theme_first_frame(bool mounted) {'

fixtures = (
    ('v2', 'startup-v2-root-original.inc', '50c9c19064f3451b9e71307c149bea566b9c5d475eed37077eceec6ceb2e1348'),
    ('old_mask', 'startup-f722-preflight-original.inc', 'cf4b86f90dc155573ca6412909781dd9866602fe9974c12f3b17c63f5f8ab335'),
)
controls = []
for label, name, digest in fixtures:
    text = (ROOT / 'tests/fixtures' / name).read_text()
    assert hashlib.sha256(text.encode()).hexdigest() == digest
    controls.append((label, text))

before = r'''
#include "kui/update_handoff.h"
#include "kui/storage.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define KUI_SD_RUNTIME 1
#define KUI_UPDATE_ENGINE_CURRENT 3
#define KUI_IO_WORKER_STACK_BYTES (96u*1024u)
#define KUI_RELEASE_SHORT "test"
#define KUI_ROLE "runtime"
#define KUI_BUILD_ID "012345abcdef"
#define DM_640x480 1
#define DM_MULTIBUFFER 2
#define PM_RGB565 16
#define CT_VGA 1
#define VID_PAL 4
#define VID_INTERLACE 8
#define CONT_Y 8u
#define CONT_START 16u
#define FR_OK 0
static uint8_t kui_update_bank_capability[16] = {[8] = 1};
static uint8_t kui_update_engine_capability[16] = {[8] = KUI_UPDATE_ENGINE_CURRENT};
static volatile uint8_t kui_update_ram_handoff[KUI_UPDATE_HANDOFF_BYTES];
static const uint8_t empty_slot[KUI_UPDATE_HANDOFF_BYTES]=KUI_UPDATE_HANDOFF_INITIALIZER;
struct kui_update_boot_view {char bank[13],build[13],notice[128];bool fallback;};
static struct kui_update_boot_view update_boot_view;
enum kui_update_boot_result {KUI_UPDATE_BOOT_BASELINE,KUI_UPDATE_BOOT_PINNED,KUI_UPDATE_BOOT_CHAIN,KUI_UPDATE_BOOT_ERROR};
struct kui_runtime_image {void *data;struct {uint32_t payload_bytes;} info;};
#define KUI_RUNTIME_ADDRESS 0x8c010000u
#define KUI_RUNTIME_MAX_BYTES (4u*1024u*1024u)
static struct video_mode {unsigned width,height,cable_type,flags;} video = {640,480,CT_VGA,0};
static const struct video_mode *vid_mode = &video;
static void *ui_thread;
static uint16_t *vram_s;
static struct preferences {bool startup_chime,menu_sounds;unsigned music_volume;} system_current,system_pending;
static int disc_identity,disc_snapshot;
struct kui_settings {int marker;};
static struct kui_settings settings_current,settings_pending;
static struct shell_state {int marker;const void *theme_catalog;unsigned vmu_emulation_generation;bool vmu_emulation_background;} shell;
#if CURRENT_COHORT
static int theme_catalog,theme_startup_settings;
static struct theme_bundle {int settings;} theme_saved_bundle;
static const int default_theme=1,card_theme=2;
static bool selected_before_draw;
void theme_init(void) {theme_saved_bundle.settings=default_theme;selected_before_draw=false;}
#endif
static bool safe_video_boot,busy,splash_active,startup_finished;
static unsigned pending;
static uint64_t splash_deadline;
static char destination_current[32],settings_note[128];
static int lock;
static unsigned events[32],event_count,preflights,resets,early_frames;
static bool warm,skip_chime,disable_chime;
static void event(unsigned n) {assert(event_count<32);events[event_count++]=n;}
void kui_storage_boot_begin(void) {}
void *thd_get_current(void) {return (void *)(uintptr_t)1;}
void vid_set_mode(int mode,int format) {
    assert(ui_thread && mode==(DM_640x480|DM_MULTIBUFFER) && format==PM_RGB565);event(1);
}
void kui_log(const char *format,...) {assert(format);}
void kui_clock_start(void (*log)(const char *,...)) {assert(log==kui_log);event(2);}
void kui_update_boot_require_engine(unsigned n) {assert(n==3);}
void kui_update_boot_reset(void) {++resets;}
void update_boot_start(void) {
    ++preflights;
    if(!warm) {fputs("forbidden ordinary-root updater preflight I/O\n",stderr);exit(23);}
}
void kui_splash_draw(uint16_t *frame) {(void)frame;++early_frames;}
void vid_waitvbl(void) {}
void vid_flip(int target) {assert(target==-1);}
void kui_system_settings_default(struct preferences *p) {*p=(struct preferences){true,true,3};}
void kui_music_init(void (*log)(const char *,...)) {assert(log==kui_log);event(3);}
void kui_disc_identity_init(int *p) {*p=1;}
void kui_disc_identity_park(int *p) {assert(*p==1);}
static unsigned recovery_sample;
unsigned controller_buttons(void) {
    return event_count && ((recovery_sample==1 && events[event_count-1]==4) || (recovery_sample==2 && events[event_count-1]==5))?CONT_START:0;
}
void kui_settings_default(struct kui_settings *p) {p->marker=2;}
void kui_shell_init(struct shell_state *p,const struct kui_settings *settings) {assert(settings->marker==2);p->marker=3;event(4);}
void kui_shell_set_system_preferences(struct shell_state *p,const struct preferences *s) {assert(p->marker==3 && s->startup_chime);}
void kui_destination_default(char *p) {strcpy(p,"root");}
uint64_t timer_ms_gettime64(void) {return 10;}
void draw_shell(void) {
    assert(pending==27 && busy && splash_active);
#if CURRENT_COHORT
    assert(selected_before_draw);
#endif
    event(5);
}
bool kui_disc_stop(void) {event(6);return true;}
void kui_vmu_lcd_startup(void) {event(7);}
void mutex_lock(int *p) {assert(p==&lock);}
void mutex_unlock(int *p) {assert(p==&lock);}
#if CURRENT_COHORT
typedef int FATFS;
#define KUI_DEST_ROOT_CAP 32
static unsigned settings_generation,destination_generation,connect_calls,mount_calls,unmounts;
static unsigned storage_failure,selected_transport=KUI_STORAGE_SCI;
static bool chain_requested,refused,release_failure,recovery_seen;
static unsigned executions,blocks,report_writes,disconnects;
static bool destination_ready;
static bool retain_mount=true,failed_preference,vmu_tools_warm_ready;
static struct {unsigned generation,slot;bool ok,enabled;} vmu_tools_preference;
static bool screenshot_frame_pending,storage_test_running,salvage_defer_pending,cancel_requested;
static unsigned worker_active_action,ui_hz_busy,vmu_reads;
static unsigned enter_while_reading;
#define KUI_OPT_UI_FULL 0
bool kui_games_storage_active(void) {assert(!busy && startup_finished);return retain_mount && !storage_failure;}
bool games_idle_cancelled(void) {return false;}
bool kui_cd_audio_owns_drive(void) {return false;}
bool kui_vmu_emulation_settings_load_full(bool *enabled,unsigned *slot,void (*log)(const char *,...)) {
    assert(!busy && !splash_active && log==kui_log && events[event_count-1]==13);
    if(enter_while_reading) {
        shell.vmu_emulation_background=true;shell.vmu_emulation_generation=enter_while_reading;
    }
    ++vmu_reads;*enabled=false;*slot=1;return !failed_preference;
}
static int destination_result;
static char destination_note[128];
bool kui_cancelled(void) {return false;}
unsigned kui_storage_selected(void) {return selected_transport;}
void kui_sd_set_params(unsigned adapter,bool crc) {assert(adapter==selected_transport && crc);}
bool kui_sd_connect(void) {++connect_calls;return storage_failure!=1;}
void kui_sd_disconnect(void) {++disconnects;}
void kui_theme_startup_splash_prepare(struct theme_bundle *bundle,void (*log)(const char *,...)) {
    assert(!storage_failure && mount_calls==1 && !unmounts && event_count==3 && events[2]==4 && log==kui_log);
    bundle->settings=card_theme;
}
void kui_shell_set_theme_preferences(struct shell_state *state,const int *settings) {
    assert(state->marker==3 && !selected_before_draw);
    assert(*settings==(storage_failure?default_theme:card_theme));
    selected_before_draw=true;
}
bool kui_mount(FATFS *fs,void (*log)(const char *,...)) {
    assert(fs && log==kui_log);++mount_calls;return storage_failure!=2;
}
int f_mount(FATFS *fs,const char *path,unsigned immediate) {
    assert(!fs && !strcmp(path,"0:") && !immediate);++unmounts;return release_failure?1:0;
}
bool kui_settings_load(struct kui_settings *p,void (*log)(const char *,...)) {
    assert(p && log==kui_log);event(9);return true;
}
bool kui_settings_save(struct kui_settings *p,void (*log)(const char *,...)) {
    (void)p;(void)log;abort();
}
bool kui_destination_load(char *p,void (*log)(const char *,...)) {
    assert(p && log==kui_log);return true;
}
static bool update_boot_no_cancel(void) { return false; }
static const char *startup_selection_name(enum kui_update_boot_result result) {(void)result;return "test";}
void kui_update_boot_block(void) {++blocks;}
static void startup_report_save_mounted(void) {assert(!unmounts);++report_writes;}
static void update_boot_timing_save(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,enum kui_update_boot_result r) {
    assert(a<=b && b<=c && d<=e && r==KUI_UPDATE_BOOT_ERROR);
}
static const char *update_runtime_handoff_refusal(uintptr_t source,size_t bytes,uintptr_t stack) {
    assert(source && bytes && stack);return refused?"test staging refusal":NULL;
}
static enum kui_update_boot_result kui_update_boot_startup(const char *build,unsigned transport,
    const struct kui_update_handoff_context *ctx,bool recovery,struct kui_runtime_image *image,
    struct kui_update_boot_view *view,void (*log)(const char *,...),bool (*cancel)(void)) {
    assert(events[event_count-1]==5 && mount_calls==1 && !unmounts && !storage_failure);
    assert(!strcmp(build,KUI_BUILD_ID) && transport==selected_transport && log==kui_log && !cancel());
    assert(warm!=kui_update_handoff_empty(ctx->wire,sizeof(ctx->wire),NULL));recovery_seen=recovery;
    ++preflights;strcpy(view->build,build);
    if(chain_requested) { image->data=malloc(16);assert(image->data);image->info.payload_bytes=16;return KUI_UPDATE_BOOT_CHAIN; }
    return warm?KUI_UPDATE_BOOT_PINNED:KUI_UPDATE_BOOT_BASELINE;
}
static void arch_exec(void *data,uint32_t size) {
    assert(data && size==16 && unmounts==1 && disconnects==1 && !release_failure);++executions;
}
static void kui_runtime_free(struct kui_runtime_image *image) {free(image->data);image->data=NULL;}
#endif
void system_operation(bool save) {assert(!save);system_current.startup_chime=!disable_chime;event(10);}
void games_selection_startup(void) {event(11);}
bool startup_sound_cancelled(void) {return skip_chime;}
void kui_music_play_boot_chime(bool (*cancel)(void)) {assert(cancel==startup_sound_cancelled);event(12);}
bool kui_menu_sound_init(void) {event(13);return true;}
void kui_menu_sound_config(bool on,unsigned volume) {assert(on && volume==3);}
'''
worker_begin = r'''
static void *worker(void *unused) {(void)unused;
'''
middle = r'''
    return NULL;
}
typedef struct {unsigned stack_size;const char *label;} kthread_attr_t;
typedef int kthread_t;
kthread_t *thd_create_ex(const kthread_attr_t *attrs,void *(*entry)(void *),void *argument) {
    assert(attrs->stack_size>=64u*1024u && !strcmp(attrs->label,"kui-io") && entry==worker && !argument);
    event(8);return (void *)(uintptr_t)1;
}
static int startup_prefix(void) {
'''
after = r'''
    assert(io_worker);return 0;
}
static void reset(void) {
    event_count=preflights=resets=early_frames=0;startup_finished=false;warm=false;
#if CURRENT_COHORT
    connect_calls=mount_calls=unmounts=executions=blocks=report_writes=disconnects=0;recovery_seen=false;
    vmu_reads=0;vmu_tools_warm_ready=false;
    shell.vmu_emulation_generation=0;shell.vmu_emulation_background=false;
    memset(&vmu_tools_preference,0,sizeof(vmu_tools_preference));
#endif
    for(unsigned i=0;i<KUI_UPDATE_HANDOFF_BYTES;i++)kui_update_ram_handoff[i]=empty_slot[i];
    memset(&update_boot_view,0,sizeof(update_boot_view));
    /* The frozen 2.0 lifecycle did not yet carry these RAM capability bytes. */
    (void)kui_update_bank_capability;(void)kui_update_engine_capability;(void)vram_s;
}
static void lifecycle(bool skip) {
    reset();skip_chime=skip;assert(startup_prefix()==0);
    assert(!early_frames);
#if CURRENT_COHORT
    assert(preflights==(storage_failure?0u:1u) && resets==(storage_failure?1u:0u));
    assert(recovery_seen==(recovery_sample!=0 && !storage_failure) && !executions && !blocks && !report_writes);
#else
    assert(!preflights);
#endif
    if(CURRENT_COHORT)assert(!strcmp(update_boot_view.build,KUI_BUILD_ID));
    assert(worker(NULL)==NULL && startup_finished && !splash_active);
#if CURRENT_COHORT
    const unsigned initial[]={1,2,4,5};
#else
    const unsigned initial[]={1,2,3,4,5};
#endif
    assert(event_count>=sizeof(initial)/sizeof(initial[0]) && !memcmp(events,initial,sizeof(initial)));
    unsigned n=sizeof(initial)/sizeof(initial[0]);
#if CURRENT_COHORT
    assert(connect_calls==1 && mount_calls==(storage_failure==1?0u:1u));
    assert(unmounts==(storage_failure?0u:1u));
    assert(vmu_reads==(unsigned)(retain_mount && !storage_failure));
    assert(vmu_tools_warm_ready==(vmu_reads && !failed_preference && !enter_while_reading));
    assert(vmu_tools_preference.ok==(vmu_reads && !failed_preference && enter_while_reading<=1));
    if(vmu_tools_preference.ok)assert(vmu_tools_preference.generation==enter_while_reading);
    if(vmu_tools_warm_ready)assert(vmu_tools_preference.ok && !vmu_tools_preference.enabled && vmu_tools_preference.slot==1);
    if(!storage_failure)assert(events[n++]==9); /* already mounted for the first frame */
    assert(events[n++]==3); /* no audio worker while a checked GUI may execute */
#endif
    assert(events[n++]==6 && events[n++]==7 && events[n++]==8);
#if !CURRENT_COHORT
    assert(events[n++]==9);
#endif
    assert(events[n++]==10 && events[n++]==11);
    if(!skip && !disable_chime)assert(events[n++]==12);
    assert(events[n++]==13 && n==event_count);
}
int main(void) {
    lifecycle(false);lifecycle(true);
    disable_chime=true;lifecycle(false);disable_chime=false;
#if CURRENT_COHORT
    for(selected_transport=KUI_STORAGE_SCIF;selected_transport<=KUI_STORAGE_AUTO;selected_transport++)
        lifecycle(false);
    selected_transport=KUI_STORAGE_SCI;
    retain_mount=false;lifecycle(false);retain_mount=true;
    failed_preference=true;lifecycle(false);failed_preference=false;
    enter_while_reading=1;lifecycle(false);
    enter_while_reading=2;lifecycle(false);enter_while_reading=0;
    recovery_sample=1;lifecycle(false);recovery_sample=2;lifecycle(false);recovery_sample=0;
    storage_failure=1;lifecycle(false);
    storage_failure=2;lifecycle(false);
    storage_failure=0;
    chain_requested=true;reset();assert(startup_prefix()==0 && executions==1 && blocks==1 && !report_writes);
    release_failure=true;reset();assert(startup_prefix()==0 && !executions && blocks==1);
    release_failure=false;refused=true;reset();assert(startup_prefix()==0 && !executions && resets==1);
    refused=false;chain_requested=false;
#endif
    if(CURRENT_COHORT) {
        struct kui_update_handoff h={.build=KUI_BUILD_ID,.transport=1,.sequence=1,
            .file_bytes=4096,.memory_bytes=8192,.payload_offset=128,.nonce={1}};
        uint8_t wire[KUI_UPDATE_HANDOFF_BYTES];assert(kui_update_handoff_encode(wire,&h));
        /* A valid explicit Restart context, damaged context, and malformed
         * header all retain checked selection rather than silently becoming root. */
        for(unsigned kind=0;kind<3;kind++) {
            reset();warm=true;
            for(unsigned i=0;i<KUI_UPDATE_HANDOFF_BYTES;i++)kui_update_ram_handoff[i]=wire[i];
            if(kind==1)kui_update_ram_handoff[132]^=1;
            if(kind==2)kui_update_ram_handoff[0]^=1;
            assert(startup_prefix()==0 && preflights==1 && !resets && !early_frames);
        }
#if CURRENT_COHORT
        reset();warm=true;storage_failure=2;memcpy((void *)kui_update_ram_handoff,wire,sizeof(wire));
        assert(startup_prefix()==0 && !preflights && !executions && blocks==1 && !resets);storage_failure=0;
#endif
        reset();kui_update_engine_capability[8]=2;
        assert(startup_prefix()==1 && !event_count && !preflights && !resets);
    }
    puts(CURRENT_COHORT?"PASS themed splash before version selection on the existing mount; checked cold/warm dispatch, physical recovery and storage release before execution":
        "PASS original root splash, chime, disc stop and worker ordering");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='kui-startup-root-') as temporary:
    directory = Path(temporary)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
    for label, code in [('current', prefix), *controls]:
        source, binary = directory / (label + '.c'), directory / label
        settings = ''
        if label == 'current':
            settings = theme_marker + body(theme_source, theme_marker) + '}\n'
            prepare = SOURCE[SOURCE.rindex('static bool update_boot_prepare('):SOURCE.rindex('static void update_boot_finish(')]
            # Replace just the SH-4 stack register read in the host build.
            stack = 'uintptr_t stack;__asm__ __volatile__("mov r15,%0":"=r"(stack));'
            assert prepare.count(stack) == 1
            prepare = prepare.replace(stack, 'uintptr_t stack=1;')
            finish = SOURCE[SOURCE.rindex('static void update_boot_finish('):].split('#endif',1)[0]
            settings += prepare + finish
            settings += settings_marker + body(SOURCE, settings_marker) + '}\n'
            settings += vmu_marker + body(SOURCE, vmu_marker) + '}\n'
        start_worker = ('unsigned action=27;\n' + worker + worker_tail if label == 'current'
                        else 'event(9);\n' + worker)
        source.write_text('#define CURRENT_COHORT '+str(int(label == 'current'))+'\n'+before+settings+
                          worker_begin+start_worker+middle+code+after)
        subprocess.run([*shlex.split(os.environ.get('HOST_CC', 'cc')), '-std=c11', '-O1', '-g',
            '-Wall', '-Wextra', '-Werror', '-Wpedantic', '-fsanitize=address,undefined',
            '-fno-omit-frame-pointer', '-fno-pie', '-no-pie', '-I', str(ROOT / 'include'),
            '-I', str(ROOT / 'config'), '-I', str(ROOT / '.deps/fatfs/source'),
            str(source), str(ROOT / 'src/core/update_handoff.c'), str(ROOT / 'src/core/data.c'),
            '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], env=env, capture_output=True, text=True)
        if label == 'old_mask':
            assert result.returncode == 23 and 'forbidden ordinary-root updater preflight I/O' in result.stderr, result.stdout+result.stderr
            print('PASS frozen f7229 control: early artwork still reaches forbidden ordinary-root preflight and is rejected')
        else:
            assert result.returncode == 0, result.stdout+result.stderr
            print(label+': '+result.stdout.strip())
