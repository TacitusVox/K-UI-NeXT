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

uint32_t measure_gd(uint32_t event,uint32_t function,const kui_retail_async_region_type *r,struct ra_clock clock) {
    clock_sample=clock;
    return toy_demand_gd_trace(event,function,r);
}
bool measure_before(unsigned variant,const kui_retail_async_region_type *r,struct ra_clock clock) {
    return toy_demand_before(variant,r,clock);
}
void measure_after(unsigned variant,bool wake,bool armed,const kui_retail_async_region_type *r,struct ra_clock clock) {
    toy_demand_after(variant,wake,armed,r,clock);
}
