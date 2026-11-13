#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "kui/toy_demand_trace.h"

enum { KUI_GD_CHECK=1, KUI_GD_EXEC=2, RA_DMA=2, KUI_NATIVE_RA_DEMAND_GD_ENTER=11 };
struct ra_clock { uint32_t counter,valid; };
struct kui_retail_gd { uint32_t variant; };
typedef struct {
    struct {
        struct kui_retail_gd service;
        struct { uint32_t state; } stream;
    } shared;
    struct { uint32_t token; bool active,failed; } engine;
} kui_retail_async_region_type;
static struct {
    struct kui_toy_demand_trace demand;
    struct { uint32_t bad_clock; } playback[2];
} C;
static struct ra_clock clock_sample;
static struct ra_clock clock_read(void) { return clock_sample; }
static unsigned playback_variant(const struct kui_retail_gd *s) { return s->variant; }
#include "toy_demand_trace.inc"

int main(void) {
    kui_retail_async_region_type region={.shared.service.variant=4u,
        .engine={.token=7u,.active=true}};
    struct kui_toy_demand_movie_trace *intro=&C.demand.movie[1];
    const uint32_t enter=KUI_NATIVE_RA_DEMAND_GD_ENTER,leave=12u;

    clock_sample=(struct ra_clock){1000u,1u};
    assert(toy_demand_gd_trace(enter,KUI_GD_EXEC,&region,2u));
    clock_sample.counter=900u;
    assert(toy_demand_gd_trace(leave,KUI_GD_EXEC,&region,2u));
    assert(intro->exec.count==1u && intro->exec.ticks==100u && intro->exec.max==100u);
    assert(toy_demand_gd_trace(enter,KUI_GD_CHECK,&region,2u));
    region.shared.service.variant=0u; /* CHECK acknowledged terminal command. */
    clock_sample.counter=880u;
    assert(toy_demand_gd_trace(leave,KUI_GD_CHECK,&region,2u));
    assert(intro->check.count==1u && intro->check.ticks==20u);
    assert(toy_demand_gd_trace(enter,KUI_GD_CHECK,&region,2u));
    assert(toy_demand_gd_trace(leave,KUI_GD_CHECK,&region,2u));
    assert(intro->check.count==1u);

    region.shared.service.variant=4u;
    clock_sample.counter=0u;
    assert(toy_demand_gd_trace(enter,KUI_GD_CHECK,&region,2u));
    clock_sample.counter=UINT32_MAX-9u;
    assert(toy_demand_gd_trace(leave,KUI_GD_CHECK,&region,2u));
    assert(intro->check.count==2u && intro->check.ticks==30u && intro->check.max==20u);
    assert(toy_demand_gd_trace(enter,KUI_GD_CHECK,&region,2u));
    clock_sample.valid=0u;
    assert(toy_demand_gd_trace(leave,KUI_GD_CHECK,&region,2u));
    assert(intro->check.count==2u && C.playback[1].bad_clock==1u);

    assert(intro->polls[2]==4u && intro->polls[0]==0u);
    /* Classification increments on movie poll entry, even without valid clock data. */
    assert(toy_demand_gd_trace(enter,KUI_GD_CHECK,&region,0u));
    assert(toy_demand_gd_trace(enter,KUI_GD_CHECK,&region,1u));
    assert(toy_demand_gd_trace(enter,KUI_GD_CHECK,&region,3u));
    assert(toy_demand_gd_trace(enter,KUI_GD_CHECK,&region,UINT32_MAX));
    assert(intro->polls[0]==2u && intro->polls[1]==1u && intro->polls[3]==1u);
    intro->polls[3]=UINT32_MAX;
    assert(toy_demand_gd_trace(enter,KUI_GD_CHECK,&region,3u));
    assert(intro->polls[3]==UINT32_MAX);

    struct ra_clock sample={.counter=800u,.valid=1u};
    assert(!toy_demand_before(4u,&region,sample));
    toy_demand_after(4u,false,false,&region,sample);
    sample.counter=725u;
    assert(!toy_demand_before(4u,&region,sample));
    assert(intro->no_arm.count==1u && intro->no_arm.ticks==75u);
    region.shared.stream.state=RA_DMA;
    toy_demand_after(4u,true,false,&region,sample);
    assert(intro->wakes==1u && intro->wake_arms==1u);
    assert(toy_demand_before(4u,&region,sample));
    toy_demand_after(4u,true,true,&region,sample);
    assert(intro->wakes==1u && intro->wake_arms==1u);

    region.shared.stream.state=0u;
    toy_demand_after(4u,false,true,&region,sample);
    ++region.engine.token;
    sample.counter=650u;
    assert(!toy_demand_before(4u,&region,sample));
    assert(intro->no_arm.count==1u && intro->no_arm.max==75u);
    toy_demand_after(4u,false,false,&region,sample);
    region.shared.stream.state=RA_DMA; /* Arm bypassed observed delivery. */
    assert(toy_demand_before(4u,&region,sample));
    assert(intro->no_arm.count==1u);

    intro->exec.count=UINT32_MAX;
    intro->exec.ticks=UINT32_MAX-1u;
    toy_demand_timing_add(&intro->exec,2u);
    assert(intro->exec.count==UINT32_MAX && intro->exec.ticks==UINT32_MAX);
    assert(!memcmp(&C.demand.movie[0],&(struct kui_toy_demand_movie_trace){0},
        sizeof(C.demand.movie[0])));
}
