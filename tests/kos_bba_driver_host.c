/* SPDX-License-Identifier: GPL-3.0-only */
/* Compile the patched driver's actual routines with a simulated interrupt
 * line, worker, G2 DMA completion, PHY link and descriptor ownership. */
#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct { unsigned flags; } netif_t;
enum { NETIF_INITIALIZED=4, NETIF_RUNNING=8, BBA_TX_OK=0, BBA_TX_AGAIN=-2, BBA_TX_WAIT=1,
    BBA_TX_NOWAIT=0, TX_SEMA=1, RT_CMD_RX_BUF_EMPTY=1, RT_TX_HOST_OWNS=2, RT_TXSTATUS0=10,
    RT_CHIPCMD=11, TX_NB_BUFFERS=4, DMA_THRESHOLD=128 };
static netif_t bba_if;
static struct { unsigned cur_tx; } rtl;
static uint32_t desc_mem[4][512];
static uintptr_t txdesc[4];
static const void *bba_rx_worker_attr;
static int *rx_worker, worker;
static bool fail_worker, rx_enabled, link_stable, link, masked=true, installed;
static bool descriptor_ready=true, fail_dma, fail_sem;
static unsigned pending_dma, joins, enqueued, dropped, rx_drains, sleeps, submissions, releases, pio, launched;
static uint8_t *next_dst, *next_src;
static int next_len;
static atomic_int dma_used;

#define irq_disable_scoped() (void)0
#define irq_inside_int() false
#define dbglog(...) (void)0
#define ASIC_EVT_EXP_PCI 5
#define BBA_ASIC_IRQ 6
#define bba_irq_hnd 0
#define bba_rx_worker 0
#define g2_fifo_wait() (void)0
#define __align_up(n,a) (((n)+(a)-1)&~((a)-1))
#define __align_down(n,a) ((n)&~((a)-1))
#define __is_aligned(x,a) (!((x)&((a)-1)))
#define __is_defined(x) (x)
#define NIC(x) (x)
#define G2_DMA_TO_G2 1
#define G2_DMA_TO_SH4 0
#define G2_DMA_CHAN_BBA 1
#define RT_TX_ABORTED 4
#define dcache_wback_range(...) (void)0
#define tx_sema 0
#define sem_trywait(x) (fail_sem ? -1 : 0)
#define sem_wait_irqsafe(x) (fail_sem ? -1 : 0)
#define sem_signal(x) (++releases)

static void asic_evt_set_handler(int id,int handler,void *data) {
    (void)id;(void)handler;(void)data; assert(rx_enabled && rx_worker && masked); installed=true;
}
static void asic_evt_remove_handler(int id) { (void)id; assert(masked && !rx_enabled); installed=false; }
static void asic_evt_enable(int id,int level) {
    (void)id;(void)level; assert(installed && rx_enabled && rx_worker); masked=false;
}
static void asic_evt_disable(int id,int level) { (void)id;(void)level; masked=true; }
static int *thd_worker_create_ex(const void *attr,int fn,void *data) {
    (void)attr;(void)fn;(void)data; assert(masked && !installed); return fail_worker ? NULL : &worker;
}
static void thd_worker_destroy(int *w) {
    assert(w && w==rx_worker && masked && !installed && !rx_enabled && !dma_used); ++joins;
}
static void bba_rx(void) { assert(rx_worker && rx_enabled); ++rx_drains; }
static void rx_finish_enq(int ok) { assert(rx_worker && rx_enabled); if(ok) ++enqueued; else ++dropped; }
static uint8_t g2_read_8(int where) { assert(where==RT_CHIPCMD); return 0; }
static uint32_t g2_read_32(unsigned where) {
    assert(where==RT_TXSTATUS0+4*rtl.cur_tx); return descriptor_ready ? RT_TX_HOST_OWNS : 0;
}
static void g2_write_32(unsigned where, int value) {
    assert(where==RT_TXSTATUS0+4*rtl.cur_tx && value>=60); ++launched;
}
static void g2_write_block_32(const uint32_t *src,uintptr_t dst,int words) {
    memcpy((void *)dst,src,(size_t)words*4); ++pio;
}
static void g2_memset_8(uintptr_t dst,int value,int bytes) { memset((void *)dst,value,(size_t)bytes); }
static int g2_dma_transfer(void *sh,void *g2,size_t bytes,unsigned block,void (*cb)(void *),void *ctx,
                           int dir,int mode,int channel,int sh_channel) {
    (void)ctx;(void)mode;(void)channel;(void)sh_channel; assert(dma_used);
    if(fail_dma) return -1;
    ++submissions;
    if(block) { assert(dir==G2_DMA_TO_G2 && !cb); memcpy(g2,sh,bytes); }
    else { assert(dir==G2_DMA_TO_SH4 && cb); ++pending_dma; }
    return 0;
}
static int bba_can_tx(void *unused) { (void)unused; return descriptor_ready; }
static int thd_poll(int (*check)(void *),void *ctx,unsigned timeout) {
    if(check==bba_can_tx) assert(timeout==100);
    else { assert(timeout==10000 && rx_enabled && installed && !masked); link_stable=link; }
    return check(ctx);
}
static void bba_dma_cb(void *);
static void thd_sleep(unsigned ms) {
    assert(ms==1 && pending_dma==1 && !rx_enabled && !installed && rx_worker);
    --pending_dma; ++sleeps; bba_dma_cb(NULL);
}

#include "kos_bba_routines.h"

int main(void) {
    bba_if.flags=NETIF_INITIALIZED;
    fail_worker=true; assert(bba_if_start(&bba_if)<0 && masked && !installed && !rx_worker && !joins);
    fail_worker=false; link=false; assert(bba_if_start(&bba_if)<0 && joins==1 && !rx_worker);
    link=true;
    for(unsigned i=0;i<4;++i) txdesc[i]=(uintptr_t)desc_mem[i];
    for(unsigned i=0;i<3;++i) {
        assert(bba_if_start(&bba_if)==0 && (bba_if.flags & NETIF_RUNNING));
        assert(bba_if_start(&bba_if)==0);
        /* IRQ started one copy with another fragment queued at stop. */
        pending_dma=dma_used=1; next_len=256;
        assert(bba_if_stop(&bba_if)==0 && bba_if_stop(&bba_if)==0);
        assert(!rx_worker && !pending_dma && !dma_used && !next_len && !enqueued && !rx_drains);
    }
    assert(sleeps==3 && joins==4);
    assert(bba_if_start(&bba_if)==0);
    _Alignas(32) uint8_t payload[256];
    for(unsigned i=0;i<sizeof(payload);++i) payload[i]=(uint8_t)(i+20);
    fail_sem=true; assert(bba_tx(payload,192,BBA_TX_NOWAIT)==BBA_TX_AGAIN && !launched && !releases);
    fail_sem=false; descriptor_ready=false;
    assert(bba_tx(payload,192,BBA_TX_WAIT)==BBA_TX_AGAIN && !launched && releases==1);
    descriptor_ready=true; fail_dma=true;
    assert(bba_tx(payload,192,BBA_TX_NOWAIT)==BBA_TX_OK);
    assert(!dma_used && pio==1 && launched==1 && !memcmp(payload,desc_mem[0],192));
    fail_dma=false; assert(bba_tx(payload,200,BBA_TX_NOWAIT)==BBA_TX_OK);
    assert(!dma_used && pio==2 && launched==2 && submissions==1 && !memcmp(payload,desc_mem[1],200));
    /* Failure to schedule the second RX transfer drops that frame. */
    dma_used=1; next_len=32; fail_dma=true; bba_dma_cb(NULL);
    assert(!dma_used && !next_len && dropped==1 && !enqueued);
    fail_dma=false; dma_used=1; next_len=32; bba_dma_cb(NULL);
    assert(dma_used && !next_len && pending_dma==1);
    --pending_dma; bba_dma_cb(NULL); assert(!dma_used && enqueued==1);
    assert(bba_if_stop(&bba_if)==0 && !rx_worker && joins==5);
    puts("PASS pinned BBA driver: worker failure, cable timeout, IRQ ordering, repeated start/stop, "
         "pending G2 RX completion, semaphore pressure, TX DMA success/PIO fallback and chained RX failure");
}
