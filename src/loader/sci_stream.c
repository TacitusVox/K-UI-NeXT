/* SPDX-License-Identifier: GPL-3.0-only
 * Original K-UI implementation of the receive-only SCI stream that the SCI
 * async probe proved on the console (513-byte DMA, RDR tail byte, MSTP0 reset
 * between blocks; docs/evidence/sci-async-cmd18-stream-2026-10-02.md). */
#include "sci_stream.h"
#include "sci_sd_bus.h"
#include "kui/general_sci_options.h"
#if KUI_TOY_SUITE_TABLE_CRC16
#include "kui/toy_sd_crc16.h"
#endif
#if KUI_TOY_PIPELINE_HIGH_STREAM
#include "kui/toy_loader_data_pipeline.h"
#endif
#ifndef KUI_TOY_PILOT_WORD_REVERSE_PROFILE
#define KUI_TOY_PILOT_WORD_REVERSE_PROFILE 0
#endif
#if KUI_TOY_PILOT_WORD_REVERSE_PROFILE < 0 || KUI_TOY_PILOT_WORD_REVERSE_PROFILE > 1
#error Toy word reversal profile must be 0 or 1
#endif
#if KUI_TOY_PILOT_WORD_REVERSE_PROFILE && \
    (!KUI_TOY_PIPELINE_HIGH_STREAM || KUI_TOY_PILOT_DATA_PIPELINE_PROFILE != 2)
#error Toy word reversal requires the exact high-only AP pipeline
#endif

#define SMR UINT32_C(0xffe00000)
#define BRR UINT32_C(0xffe00004)
#define SCR UINT32_C(0xffe00008)
#define SSR UINT32_C(0xffe00010)
#define RDR UINT32_C(0xffe00014)
#define SCMR UINT32_C(0xffe00018)
#define SPTR UINT32_C(0xffe0001c)
#define STBCR UINT32_C(0xffc00004)
#define PDTR UINT32_C(0xff800030)
#define SAR UINT32_C(0xffa00010)
#define DAR UINT32_C(0xffa00014)
#define TCR UINT32_C(0xffa00018)
#define CHCR UINT32_C(0xffa0001c)
#define DMAOR UINT32_C(0xffa00040)
/* Channel 1: SCI receive request, byte units, destination incrementing; no
 * completion interrupt (the game's DMAC level stays as it is). RIE+RE then
 * clock the card without a transmitter; with SPTR.EIO only the receiver's
 * error (ERI) reaches the CPU, at the overrun that ends every reception. */
#define RX_DMA UINT32_C(0x4911)
#define RDRF 0x40u
#define ORER 0x20u
#define FLAGS 0x38u
#define TEND 0x04u
#define DMA_BYTES 513u
#define AREA_BYTES KUI_SCI_STREAM_AREA_BYTES
#define STRICT_REREAD (KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || \
    KUI_TOY_SUITE_MODE == 28 || KUI_TOY_SUITE_MODE == 31 || KUI_TOY_SUITE_ASYNC_DEMAND)
#if KUI_NATIVE_GD_DMA_STREAM
#define STREAM_OBSERVE(...) do {} while(0)
#else
#define STREAM_OBSERVE(...) __VA_ARGS__
#endif

#ifdef KUI_SCI_STREAM_TEST
extern uint32_t kui_sci_stream_test_read(uint32_t address, unsigned width);
extern void kui_sci_stream_test_write(uint32_t address, uint32_t value, unsigned width);
extern void kui_sci_stream_test_settle(unsigned count);
extern uint32_t kui_sci_stream_test_physical(const void *area);
extern void kui_sci_stream_test_purge(const void *area, unsigned bytes);
extern void kui_sci_stream_test_fence(void);
#define rd8(a) ((uint8_t)kui_sci_stream_test_read(a, 1))
#define rd16(a) ((uint16_t)kui_sci_stream_test_read(a, 2))
#define rd32(a) kui_sci_stream_test_read(a, 4)
#define wr8(a,v) kui_sci_stream_test_write(a, (uint8_t)(v), 1)
#define wr32(a,v) kui_sci_stream_test_write(a, v, 4)
#define settle kui_sci_stream_test_settle
#define physical kui_sci_stream_test_physical
#define purge kui_sci_stream_test_purge
#define fence(area) kui_sci_stream_test_fence()
#else
#define rd8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define rd16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define rd32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define wr8(a,v) (rd8(a) = (uint8_t)(v))
#define wr32(a,v) (rd32(a) = (uint32_t)(v))
static void settle(unsigned n) {
    __asm__ __volatile__("1: dt %0\n\tbf 1b" : "+r"(n) : : "t", "memory");
}
static uint32_t physical(const void *area) { return (uint32_t)(uintptr_t)area & UINT32_C(0x1fffffff); }
static void purge(const void *area, unsigned bytes) {
    for(uintptr_t p = (uintptr_t)area; p < (uintptr_t)area + bytes; p += 32u)
        __asm__ __volatile__("ocbp @%0" : : "r"(p) : "memory");
}
/* An uncached read of main memory waits for the external bus, and the DMAC
 * goes before the CPU there: a transfer the channel had begun (RDR read, the
 * write held off by the game's own DMA) has landed when it returns. Stopping
 * the channel does not cancel such a transfer, so finish fences both before
 * the stop (requests it still takes) and after it (before counting). */
static void fence(const void *area) {
    (void)*(volatile uint32_t *)(((uintptr_t)area & UINT32_C(0x1fffffff)) | UINT32_C(0xa0000000));
}
#endif

typedef uint32_t alias_word __attribute__((__may_alias__));
/* RESUME: stopped mid-block by an overrun in an interrupt, to be resumed. */
enum { CLOSED, PAUSED, DMA=KUI_SCI_STREAM_DMA_STATE, LOST, RESUME
#if KUI_SCI_STREAM_TOKEN_SLICED
    , TOKEN_WAIT
#endif
#if KUI_TOY_SUITE_ASYNC_DEMAND
    , STOP_WAIT, START_WAIT
#endif
};
#if KUI_TOY_SUITE_MODE == 31
enum { TOY_FAST, TOY_RETRY_SLOW, TOY_SLOW_DMA };
#endif
#if KUI_TOY_SUITE_SLOW_RETRY
enum { TOY_RETRY_WAITING = 1u, TOY_RETRY_ARMED = KUI_TOY_SUITE_SLOW_MOVIE };
#define TOY_SLOW_WIRE 2u /* wire[area] also marks the actual DMA rate */
#endif
#if KUI_TOY_SUITE_PIO_WINDOW
/* Reserve the final three byte values. Incrementing after each receive
 * returns to zero; the separate receive-area marker identifies PIO CRCs. */
enum { TOY_PIO_WINDOW = KUI_TOY_SCI_PIO_WINDOW_FIRST, TOY_PIO_WINDOW_WIRE = 4u };
#endif
#ifdef KUI_RETAIL_ASYNC
/* The game reader keeps this state between its interrupt vectors. */
#include "retail_async.h"
#define s (kui_retail_async_region.shared.stream)
#else
static struct kui_sci_stream_state s;
#endif

#if KUI_TOY_SUITE_VERIFIED_RESUME
#if !defined(KUI_RETAIL_ASYNC)
#error "Toy verified resume requires the retained asynchronous request token"
#endif
#define TOY_RESUMED_WIRE 8u
/* The receive DMA ends at byte 512. Keep provenance in the same area,
 * independent of the token-framing fields in the retained stream ABI. */
struct __attribute__((__may_alias__)) toy_resume_meta {
    uint32_t token, lost;
};
#define TOY_RESUME_META_OFFSET 516u
_Static_assert(TOY_RESUME_META_OFFSET+sizeof(struct toy_resume_meta)<=AREA_BYTES,
    "Toy resume metadata must fit outside the wire bytes");
static inline uint8_t *resume_area(unsigned area) {
    uintptr_t p=(uintptr_t)s.area[area];
#ifndef KUI_SCI_STREAM_TEST
    p=(p&UINT32_C(0x1fffffff))|UINT32_C(0xa0000000);
#endif
    return (uint8_t *)p;
}
static inline volatile struct toy_resume_meta *resume_meta(unsigned area) {
    return (volatile struct toy_resume_meta *)(void *)(resume_area(area)+TOY_RESUME_META_OFFSET);
}
static inline bool resume_admitted(unsigned area) {
    uint32_t token=kui_retail_async_region.engine.token;
    return token && (s.toy_suite_active&KUI_TOY_SUITE_RESUME_ACTIVE) &&
        resume_meta(area)->token==token;
}
#endif

static uint8_t byte(uint8_t value) { return s.card->bus.transfer(s.card->bus.ctx, value, false); }
static void select(bool selected) { s.card->bus.select(s.card->bus.ctx, selected); }
static uint32_t reverse(uint32_t x) {
    /* Each byte's bits in reverse order, bytes in place: SCI is LSB first. */
    uint32_t t = (x ^ (x >> 1)) & UINT32_C(0x55555555);
    x ^= t ^ (t << 1);
    t = (x ^ (x >> 2)) & UINT32_C(0x33333333);
    x ^= t ^ (t << 2);
    t = (x ^ (x >> 4)) & UINT32_C(0x0f0f0f0f);
    return x ^ t ^ (t << 4);
}
#if KUI_TOY_PILOT_WORD_REVERSE_PROFILE
/* Only the main 128 data words avoid call/stack traffic. Keep the existing
 * out-of-line reversal for the two CRC tail bytes and every other reader. */
static inline __attribute__((__always_inline__)) uint32_t reverse_data_word(uint32_t x) {
    uint32_t t = (x ^ (x >> 1)) & UINT32_C(0x55555555);
    x ^= t ^ (t << 1);
    t = (x ^ (x >> 2)) & UINT32_C(0x33333333);
    x ^= t ^ (t << 2);
    t = (x ^ (x >> 4)) & UINT32_C(0x0f0f0f0f);
    return x ^ t ^ (t << 4);
}
#endif
#if defined(KUI_RETAIL_CE) && KUI_CE_SCI_FAST_TAKE
/* The bit-0-folded table and the block check shared with the SH-4 test. */
#include "sci_stream_check.h"
#elif defined(KUI_RETAIL_CE)
/* The byte update is linear: table[x ^ 128] = table[x] XOR 0x9188.
 * Keep the first 128 entries and reconstruct the high-bit contribution.
 * Every byte still receives the complete 0x1021 CRC update. This saves
 * 256 ROM bytes at the cost of a conditional XOR, not a throughput claim.
 * Native readers retain the table-free update below. */
static const uint16_t ce_crc16_table[128] __attribute__((aligned(32))) = {
    0x0000u, 0x1021u, 0x2042u, 0x3063u, 0x4084u, 0x50a5u, 0x60c6u, 0x70e7u,
    0x8108u, 0x9129u, 0xa14au, 0xb16bu, 0xc18cu, 0xd1adu, 0xe1ceu, 0xf1efu,
    0x1231u, 0x0210u, 0x3273u, 0x2252u, 0x52b5u, 0x4294u, 0x72f7u, 0x62d6u,
    0x9339u, 0x8318u, 0xb37bu, 0xa35au, 0xd3bdu, 0xc39cu, 0xf3ffu, 0xe3deu,
    0x2462u, 0x3443u, 0x0420u, 0x1401u, 0x64e6u, 0x74c7u, 0x44a4u, 0x5485u,
    0xa56au, 0xb54bu, 0x8528u, 0x9509u, 0xe5eeu, 0xf5cfu, 0xc5acu, 0xd58du,
    0x3653u, 0x2672u, 0x1611u, 0x0630u, 0x76d7u, 0x66f6u, 0x5695u, 0x46b4u,
    0xb75bu, 0xa77au, 0x9719u, 0x8738u, 0xf7dfu, 0xe7feu, 0xd79du, 0xc7bcu,
    0x48c4u, 0x58e5u, 0x6886u, 0x78a7u, 0x0840u, 0x1861u, 0x2802u, 0x3823u,
    0xc9ccu, 0xd9edu, 0xe98eu, 0xf9afu, 0x8948u, 0x9969u, 0xa90au, 0xb92bu,
    0x5af5u, 0x4ad4u, 0x7ab7u, 0x6a96u, 0x1a71u, 0x0a50u, 0x3a33u, 0x2a12u,
    0xdbfdu, 0xcbdcu, 0xfbbfu, 0xeb9eu, 0x9b79u, 0x8b58u, 0xbb3bu, 0xab1au,
    0x6ca6u, 0x7c87u, 0x4ce4u, 0x5cc5u, 0x2c22u, 0x3c03u, 0x0c60u, 0x1c41u,
    0xedaeu, 0xfd8fu, 0xcdecu, 0xddcdu, 0xad2au, 0xbd0bu, 0x8d68u, 0x9d49u,
    0x7e97u, 0x6eb6u, 0x5ed5u, 0x4ef4u, 0x3e13u, 0x2e32u, 0x1e51u, 0x0e70u,
    0xff9fu, 0xefbeu, 0xdfddu, 0xcffcu, 0xbf1bu, 0xaf3au, 0x9f59u, 0x8f78u,
};
#endif
#if defined(KUI_RETAIL_CE) && KUI_CE_SCI_FAST_TAKE
static inline uint32_t crc16(uint32_t crc, uint8_t data) {
    uint32_t index=(crc>>8)^data;
    uint32_t value=kui_sci_check_table[(index>>1)&127u];
    if(index&1u)value^=0x1021u;
    return ((crc<<8)&0xffffu)^value;
}
#elif defined(KUI_RETAIL_CE)
/* Keep the normalized accumulator in a full SH register, avoiding per-byte
 * uint16_t spills. The masked shifted state, table value and high-bit XOR
 * are all 16-bit, so the next table index remains within 0..127. */
static inline uint32_t crc16(uint32_t crc, uint8_t data) {
    uint32_t index=(crc>>8)^data;
    uint32_t value=ce_crc16_table[index&127u];
    if(index&128u)value^=0x9188u;
    return ((crc<<8)&0xffffu)^value;
}
#else
static inline uint16_t crc16(uint16_t crc, uint8_t data) {
    uint32_t x = (crc >> 8) ^ data;
    x ^= x >> 4;
    return (uint16_t)((crc << 8) ^ (x << 12) ^ (x << 5) ^ x);
}
#endif
#if KUI_CE_SCI_FAST_TAKE && !defined(KUI_RETAIL_CE)
#error "The fast block check is the CE stream reader's"
#endif
#if KUI_TOY_SUITE_FAST_CRC16 && !KUI_TOY_SUITE_TABLE_CRC16 && defined(__sh__)
/* The low 16 bits carry the full CRC16 state. swap.b selects its high
 * byte, so discarded upper bits can accumulate until the end of the block. */
static inline __attribute__((always_inline)) uint32_t toy_crc16_word(uint32_t crc, uint32_t word) {
    const int shift4=-4, shift5=5, shift12=12;
    unsigned bytes=4;
    uint32_t index, term;
    __asm__("1: swap.b %[crc],%[index]\n\t"
            "xor %[word],%[index]\n\t"
            "extu.b %[index],%[index]\n\t"
            "mov %[index],%[term]\n\t"
            "shld %[shift4],%[term]\n\t"
            "xor %[term],%[index]\n\t"
            "mov %[index],%[term]\n\t"
            "shll8 %[crc]\n\t"
            "xor %[index],%[crc]\n\t"
            "shld %[shift12],%[term]\n\t"
            "shld %[shift5],%[index]\n\t"
            "xor %[term],%[crc]\n\t"
            "xor %[index],%[crc]\n\t"
            "dt %[bytes]\n\t"
            "bf/s 1b\n\t"
            "shlr8 %[word]"
            : [crc] "+&r"(crc), [word] "+&r"(word), [bytes] "+&r"(bytes),
              [index] "=&r"(index), [term] "=&r"(term)
            : [shift4] "r"(shift4), [shift5] "r"(shift5), [shift12] "r"(shift12)
            : "t");
    return crc;
}
#endif
static uint8_t crc7(const uint8_t *p) {
    unsigned c = 0;
    for(unsigned i = 0; i < 5; ++i) for(unsigned j = 0; j < 8; ++j) {
        c <<= 1;
        if(((p[i] >> (7u - j)) ^ (c >> 7)) & 1u) c ^= 9u;
    }
    return (uint8_t)((c << 1) | 1u);
}
/* Bus faults latch: the card is then unusable until reinitialized. */
static bool healthy(void) { return kui_sci_sd_healthy(); }
static bool channel_idle(void) { return !(rd32(CHCR) & 7u) && (rd32(DMAOR) & 7u) == 1u; }

enum kui_sci_stream_result kui_sci_stream_open(const struct kui_loader_sd *card,
    uint8_t *area0, uint8_t *area1, bool unknown) {
    s.card = card;
    s.area[0] = area0; s.area[1] = area1;
    s.state = unknown ? LOST : CLOSED;
    s.ready[0] = s.ready[1] = 0; s.kept = 0; s.arrived = 0;
#if !KUI_TOY_SUITE_MOVIE_POLL
    s.unrepaired = 0;
#endif
#if KUI_TOY_SUITE_MODE == 31
    s.toy_suite_rate = TOY_FAST;
#elif KUI_TOY_SUITE_SLOW_RETRY
    s.toy_suite_slow_pending = 0u;
#endif
#if KUI_CE_SCI_SLOW_AFTER_OVERRUN
    s.slow = 0;
#endif
    s.sptr = rd8(SPTR);
    /* A deselected idle byte at the fast rate: the bus leaves its slow
     * acquisition rate and the card sees whole bytes. */
    select(false);
    (void)byte(0xff);
    if(!healthy() || (s.sptr & 0x0au) || rd8(SCR) != 0x30u || rd8(BRR) != 0)
        return KUI_SCI_STREAM_RESET;
    return KUI_SCI_STREAM_OK;
}
bool kui_sci_stream_busy(void) {
#if (KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28)
    _Static_assert(DMA==2u && RESUME==4u && KUI_TOY_SCI_BURST_TOKEN==5u &&
        KUI_TOY_SCI_BURST_DMA==6u && !KUI_SCI_STREAM_TOKEN_SLICED,
        "Toy burst reuses the low SCI interrupt route");
    return (s.state&~4u)==DMA;
#else
    return s.state == DMA;
#endif
}
bool kui_sci_stream_ready(uint32_t lba) { return s.arrived == lba + 1u; }
#if KUI_SCI_STREAM_TOKEN_SLICED
void kui_sci_stream_token_budget(bool bounded) {
    s.token_bounded = bounded;
    s.token_budget = KUI_SCI_STREAM_TOKEN_SLICE;
}
bool kui_sci_stream_token_pending(void) { return s.state == TOKEN_WAIT; }
#endif
#if KUI_CE_SCI_SLOW_AFTER_OVERRUN
#define SLOW_RX 0x80u
#define SLOW_LEFT 0x7fu
_Static_assert(KUI_SCI_STREAM_SLOW_BLOCKS <= SLOW_LEFT, "slow window fits seven bits");
bool kui_sci_stream_slow_pending(void) { return (s.slow & SLOW_LEFT) != 0; }
/* After a completed reception: one fewer still to run slowly. */
static void __attribute__((noinline)) slow_done(void) { if(s.slow & SLOW_LEFT) --s.slow; }
#endif
const struct kui_sci_stream_stats *kui_sci_stream_stats(void) { return &s.stats; }
void kui_sci_stream_discard(void) {
    s.ready[0] = s.ready[1] = 0; s.kept = 0; s.arrived = 0;
#if KUI_TOY_SUITE_SLOW_RETRY
    s.toy_suite_slow_pending = 0u;
#endif
}

/* Data token after the card's wait (0xff); TEND before the DMA starts. */
#if KUI_SCI_STREAM_TOKEN_SLICED && !KUI_TOY_SUITE_ASYNC_DEMAND
static void token_count(uint32_t bytes) {
    s.stats.token_bytes += bytes;
    if(bytes > s.stats.token_max) s.stats.token_max = bytes;
}
#endif
static enum kui_sci_stream_result token(uint32_t limit) {
#if KUI_SCI_STREAM_TOKEN_SLICED
    if(s.state != TOKEN_WAIT) {
#if !KUI_TOY_SUITE_ASYNC_DEMAND
        s.token_limit = limit;
#endif
        s.token_used = 0;
        s.state = TOKEN_WAIT;
    }
#if KUI_TOY_SUITE_ASYNC_DEMAND
    uint32_t remaining = s.token_used<limit ? limit-s.token_used : 0u, run = remaining;
#else
    uint32_t remaining = s.token_limit - s.token_used, run = remaining;
#endif
    if(s.token_bounded && run > s.token_budget) run = s.token_budget;
    uint32_t n = 0;
    uint8_t value = 0xff;
    while(n < run) {
        value = byte(0xff);
        ++n;
        if(value != 0xff) break;
    }
    s.token_used += n;
    if(s.token_bounded) s.token_budget -= n;
    if(value == 0xfe) {
        for(unsigned k = 0; k < 64u && !(rd8(SSR) & TEND); ++k) {}
#if !KUI_TOY_SUITE_ASYNC_DEMAND
        token_count(s.token_used);
#endif
        return KUI_SCI_STREAM_OK;
    }
    if(value == 0xff && n < remaining && healthy()) {
        select(false); /* no more clocks until the same search resumes */
        if(!healthy()) {s.state = LOST; return KUI_SCI_STREAM_RESET;}
#if !KUI_TOY_SUITE_ASYNC_DEMAND
        if(n) ++s.stats.token_yields; /* count work paused, not zero-budget probes */
#endif
        return KUI_SCI_STREAM_PENDING;
    }
#if !KUI_TOY_SUITE_ASYNC_DEMAND
    token_count(s.token_used);
#endif
    s.state = LOST;
#else
    for(uint32_t n = 0; n < limit; ++n) {
        uint8_t value = byte(0xff);
        if(value == 0xfe) {
            for(unsigned k = 0; k < 64u && !(rd8(SSR) & TEND); ++k) {}
            return KUI_SCI_STREAM_OK;
        }
        if(value != 0xff) break;
    }
#endif
    STREAM_OBSERVE(++s.stats.token_errors);
    return healthy() ? KUI_SCI_STREAM_TOKEN : KUI_SCI_STREAM_RESET;
}
/* R1, or 0xff if none came within the response window. */
static uint8_t command(const uint8_t packet[6], bool stop) {
    for(unsigned i = 0; i < 6; ++i) (void)byte(packet[i]);
    if(stop) (void)byte(0xff); /* CMD12's stuff byte */
    uint8_t value = 0xff;
    for(unsigned i = 0; i < 20u && (value & 0x80u); ++i) value = byte(0xff);
    return value;
}
#if KUI_TOY_PILOT_SHARED_SCI || KUI_TOY_PILOT_DATA_SCHEDULE_MODE == 2
/* Shared-worker visits must not clock an entire legacy busy timeout while
 * the game's decoder is paused. Keep the first hardware trial's idle and
 * CMD12 R1b windows bounded; a timeout is a transport fault, never a grant
 * to raw audio. Ordinary and CE readers retain their admitted windows. */
#define START_IDLE_LIMIT 4096u
#define STOP_BUSY_LAST 4095u
#else
#define START_IDLE_LIMIT 50000u
#define STOP_BUSY_LAST 200000u
#endif
#if KUI_TOY_SUITE_ASYNC_DEMAND
/* Resume the same busy/idle phase across visits, sharing the current
 * token budget. The full transaction limit still applies across visits. */
static enum kui_sci_stream_result command_ready(uint32_t limit) {
    while(s.token_used<limit && (!s.token_bounded || s.token_budget)) {
        uint8_t value=byte(0xff);
        ++s.token_used;
        if(s.token_bounded) --s.token_budget;
        if(!healthy()) return KUI_SCI_STREAM_RESET;
        if(value==0xffu) return KUI_SCI_STREAM_OK;
    }
    return s.token_used<limit ? KUI_SCI_STREAM_PENDING : KUI_SCI_STREAM_COMMAND;
}
#endif
/* CMD12 into the stream, its R1b busy, deselection and an idle byte. An
 * idle card answers "illegal command" (0x04): it is stopped all the same. */
static enum kui_sci_stream_result stop_card(void) {
    static const uint8_t stop[6] = {0x4c, 0, 0, 0, 0, 0x61};
#if KUI_TOY_SUITE_ASYNC_DEMAND
    if(s.state==START_WAIT) {s.state=CLOSED;return KUI_SCI_STREAM_OK;}
    select(true);
    if(s.state!=STOP_WAIT) {
        s.stop_failed=(command(stop,true)&~0x04u)!=0u;
        s.state=STOP_WAIT;s.token_used=0u;
    }
    enum kui_sci_stream_result result=command_ready(STOP_BUSY_LAST+1u);
    select(false);
    if(result==KUI_SCI_STREAM_PENDING) return result;
    (void)byte(0xff);
    if(result==KUI_SCI_STREAM_OK && s.stop_failed) result=KUI_SCI_STREAM_COMMAND;
#else
#if KUI_SCI_STREAM_TOKEN_SLICED
    if(s.state == TOKEN_WAIT) token_count(s.token_used);
    ++s.stats.stops;
#endif
    select(true);
    enum kui_sci_stream_result result = command(stop, true) & ~0x04u ?
        KUI_SCI_STREAM_COMMAND : KUI_SCI_STREAM_OK;
    for(uint32_t n = 0; byte(0xff) != 0xffu; ++n)
        if(n >= STOP_BUSY_LAST || !healthy()) {result = KUI_SCI_STREAM_COMMAND; break;}
    select(false);
    (void)byte(0xff);
#endif
    s.state = result == KUI_SCI_STREAM_OK ? CLOSED : LOST;
    return healthy() ? result : KUI_SCI_STREAM_RESET;
}
static enum kui_sci_stream_result start(uint32_t lba, uint32_t limit) {
    uint32_t address = s.card->high_capacity ? lba : lba << 9;
    uint8_t packet[6] = {0x52, (uint8_t)(address >> 24), (uint8_t)(address >> 16),
        (uint8_t)(address >> 8), (uint8_t)address, 0};
    packet[5] = crc7(packet);
    select(true);
#if KUI_TOY_SUITE_ASYNC_DEMAND
    if(s.state!=START_WAIT) {s.state=START_WAIT;s.position=lba;s.token_used=0u;}
    enum kui_sci_stream_result ready=command_ready(START_IDLE_LIMIT);
    if(ready!=KUI_SCI_STREAM_OK) {
        select(false);
        if(ready!=KUI_SCI_STREAM_PENDING) s.state=CLOSED;
        return ready;
    }
#else
    for(uint32_t n = 0; byte(0xff) != 0xffu;)
        if(++n >= START_IDLE_LIMIT || !healthy()) return KUI_SCI_STREAM_COMMAND;
#endif
    s.state = LOST; /* from here on, CMD18 may be running */
    STREAM_OBSERVE(++s.stats.starts);
    uint8_t r1 = command(packet, false);
    if(!healthy()) return KUI_SCI_STREAM_RESET;
#if KUI_SCI_STREAM_TOKEN_SLICED
    s.position = lba;
#endif
    return r1 ? KUI_SCI_STREAM_COMMAND : token(limit);
}
static enum kui_sci_stream_result module_reset(void) {
    uint8_t before = rd8(STBCR);
    if((before & 1u) || rd8(SCR) || (rd8(SSR) & (RDRF | FLAGS)) ||
       (rd8(SPTR) & 0x8au) != 0x82u || !(rd16(PDTR) & 0x80u)) return KUI_SCI_STREAM_RESET;
    wr8(STBCR, before | 1u);
    bool stopped = false, resumed = false;
    for(unsigned n = 0; n < 8u && !stopped; ++n) stopped = rd8(STBCR) & 1u;
    settle(64);
    wr8(STBCR, rd8(STBCR) & ~1u);
    for(unsigned n = 0; n < 8u && !resumed; ++n) resumed = !(rd8(STBCR) & 1u);
    if(!resumed || !stopped) return KUI_SCI_STREAM_RESET;
    settle(64);
    if(rd8(SCR) || rd8(SMR) || rd8(BRR) != 0xffu || rd8(SCMR) ||
       (rd8(SSR) & 0xfcu) != 0x84u || (rd8(SPTR) & 0x8au) != 0x82u) {
        wr8(SCR, 0);
        return KUI_SCI_STREAM_RESET;
    }
    return KUI_SCI_STREAM_OK;
}
/* After reception: deselect, reset the module after an overrun, and restore
 * the synchronous fast settings the bus uses for command bytes. */
static enum kui_sci_stream_result handoff(bool overrun) {
    select(false);
    if(!healthy() || !(rd16(PDTR) & 0x80u)) return KUI_SCI_STREAM_RESET;
    if(overrun) {
        enum kui_sci_stream_result result = module_reset();
        if(result != KUI_SCI_STREAM_OK) return result;
    }
    wr8(SCR, 0); wr8(SCMR, 0); wr8(SMR, 0x80u); wr8(BRR, 0);
    settle(64); /* more than one bit time at 12.5 MHz */
    wr8(SPTR, s.sptr); wr8(SCR, 0x30u);
    uint8_t status = rd8(SSR);
    if(rd8(SCR) != 0x30u || rd8(SMR) != 0x80u || rd8(BRR) != 0 ||
       (status & (RDRF | FLAGS)) || (status & 0x84u) != 0x84u) return KUI_SCI_STREAM_RESET;
    return KUI_SCI_STREAM_OK;
}
/* Receive the area's bytes from offset on by DMA: RIE+RE clock the card
 * without a transmitter, the channel takes each byte, and the receiver keeps
 * the one after the last in RDR and stops on the next one's overrun. */
static enum kui_sci_stream_result start_dma(uint32_t area, uint32_t offset) {
    /* TxD latched high as GPIO before the transmitter is turned off. */
    wr8(SPTR, 0x83u);
    if((rd8(SPTR) & 0x8au) != 0x82u) return KUI_SCI_STREAM_RESET;
#if KUI_GENERAL_SCI_RESUME_FIX || KUI_CE_SCI_RESUME_FIX
    /* Snapshot at every actual acquisition, including a resumed block.
     * receive/fetch admit an idle channel; finish restores it before
     * continuing a repaired block with interrupts still masked. */
    for(unsigned j = 0; j < 4; ++j) s.saved[j] = rd32(SAR + 4u * j);
#endif
    wr8(SCR, 0);
#if KUI_TOY_SUITE_MODE == 31
    if(s.toy_suite_rate == TOY_RETRY_SLOW) {
        s.toy_suite_rate = TOY_SLOW_DMA;
        ++s.toy_suite_retries;
        wr8(BRR, 1u);
        settle(64);
    }
#elif KUI_TOY_SUITE_SLOW_RETRY
    if(s.toy_suite_slow_pending == TOY_RETRY_WAITING) {
        s.wire[area] |= TOY_SLOW_WIRE;
        ++s.stats.toy_retry_slow;
        wr8(BRR, 1u);
        settle(64); /* 6.25 MHz for this data receive only */
    }
    s.toy_suite_slow_pending = s.toy_suite_active & KUI_TOY_SUITE_SLOW_MOVIE;
#endif
#if KUI_CE_SCI_SLOW_AFTER_OVERRUN
    /* 6.25 MHz for this reception only; handoff restores 12.5 MHz. The
     * settle exceeds one bit before the receiver is enabled. */
    s.slow &= SLOW_LEFT;
    if(s.slow) {s.slow |= SLOW_RX; wr8(BRR, 1u); settle(64);}
#endif
    wr32(CHCR, 0);
    wr32(SAR, RDR & UINT32_C(0x1fffffff));
    wr32(DAR, physical(s.area[area]) + offset);
    wr32(TCR, DMA_BYTES - offset);
    wr32(CHCR, RX_DMA);
    wr8(SCR, 0x50u);
    s.state = DMA;
    return KUI_SCI_STREAM_OK;
}
/* The block whose token was just read, into the area not holding the last
 * block taken (an untaken block there is dropped), unless the last block
 * received still awaits taking: then into the other area, the kept block
 * giving way. By DMA when channel 1 is idle and polled is false, otherwise
 * by programmed transfers (514 bytes, so the card's gap byte is left for
 * the next token search), which cannot overrun. */
static enum kui_sci_stream_result receive(bool polled) {
    uint32_t area = s.fill ^ 1u;
    if(s.kept == area + 1u) {
        if(s.arrived) s.kept = 0;
        else area ^= 1u;
    }
    uint8_t *p = s.area[area];
    s.fill = area;
    s.arrived = 0;
    s.ready[area] = 0;
#if !KUI_TOY_SUITE_ASYNC_DEMAND
    s.lost[area] = 0;
#endif
    settle(64); /* the token byte's last edge */
#if KUI_TOY_SUITE_MODE == 3
    /* Switch this fresh block before clocking its payload. CHCR2 and the
     * Holly Channel 2 start bit are observations of competing traffic. */
    if(!polled && s.toy_suite_active && !kui_retail_async_region.engine.in_irq &&
       channel_idle() && ((rd32(UINT32_C(0xffa0002c)) & 3u) == 1u ||
                         (rd32(UINT32_C(0xa05f6808)) & 1u))) {
        ++s.toy_suite_events;
        polled = true;
    }
#endif
#if KUI_TOY_SUITE_DIRECT_PIO
    const bool pio_window = s.toy_suite_active & KUI_TOY_SUITE_SLOW_MOVIE;
    if(pio_window || polled || !channel_idle()) {
#elif KUI_TOY_SUITE_PIO_WINDOW
    bool pio_window = (int8_t)s.toy_suite_slow_pending < 0;
    const bool dma_idle = (pio_window || !polled) && channel_idle();
    pio_window = pio_window && dma_idle;
    if(polled || pio_window || !dma_idle) {
#else
    if(polled ||
#if KUI_TOY_SUITE_MODE == 34
       s.toy_suite_pio_retry ||
#endif
       !channel_idle()) {
#endif
#if KUI_TOY_SUITE_MODE == 34
        /* Complete this fresh block by programmed reads. A CRC failure
         * consumes the ordinary retry limit before anything is delivered. */
        s.toy_suite_pio_retry=0u;
#elif KUI_TOY_SUITE_PIO_WINDOW
        s.wire[area] = pio_window ? TOY_PIO_WINDOW_WIRE : 0u;
#if KUI_TOY_SUITE_DIRECT_PIO
        s.toy_suite_slow_pending = 0u;
        if(pio_window) ++s.stats.toy_pio_window_blocks;
#else
        if(pio_window) {
            ++s.toy_suite_slow_pending;
            ++s.stats.toy_pio_window_blocks;
        } else s.toy_suite_slow_pending = 0u;
#endif
#elif KUI_TOY_SUITE_SLOW_RETRY
        /* Keep the ordinary fallback when the guest owns the channel. */
        s.toy_suite_slow_pending=0u;
#endif
#if KUI_TOY_SUITE_PIPELINED_PIO
        _Static_assert(KUI_SCI_SD_PIO_PAYLOAD_BYTES == DMA_BYTES + 1u &&
            KUI_SCI_SD_PIO_PAYLOAD_BYTES <= AREA_BYTES, "Toy window buffer includes both CRC bytes");
        if(pio_window) {
            if(!kui_sci_sd_pio_payload(p)) {
                select(false);
                return KUI_SCI_STREAM_RESET;
            }
            s.rdr[area] = p[DMA_BYTES];
        } else
#endif
        {
            for(unsigned i = 0; i < DMA_BYTES; ++i) p[i] = byte(0xff);
            s.rdr[area] = byte(0xff);
        }
        select(false);
        if(!healthy()) return KUI_SCI_STREAM_RESET;
#if !KUI_TOY_SUITE_PIO_WINDOW
        s.wire[area] = 0;
#endif
        s.ready[area] = 1;
        s.ready_lba[area] = s.position++;
        s.arrived = s.position;
        s.state = PAUSED;
        ++s.stats.polled;
#if KUI_CE_SCI_SLOW_AFTER_OVERRUN
        slow_done();
#endif
        return KUI_SCI_STREAM_OK;
    }
    purge(p, AREA_BYTES);
#if !KUI_GENERAL_SCI_RESUME_FIX && !KUI_CE_SCI_RESUME_FIX
    for(unsigned i = 0; i < 4; ++i) s.saved[i] = rd32(SAR + 4u * i);
#endif
    s.wire[area] = 1;
    return start_dma(area, 0);
}
enum kui_sci_stream_result kui_sci_stream_fetch(uint32_t lba, uint32_t token_limit, bool polled) {
#if (KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28)
    if(s.state >= KUI_TOY_SCI_BURST_TOKEN) return KUI_SCI_STREAM_BUSY;
#endif
    if(s.state == DMA) return KUI_SCI_STREAM_BUSY;
#if KUI_TOY_SUITE_MODE == 34
    if(lba!=s.position) s.toy_suite_pio_retry=0u;
#elif KUI_TOY_SUITE_SLOW_RETRY
    if(lba!=s.position || !(s.toy_suite_active&KUI_TOY_SUITE_SLOW_MOVIE))
        s.toy_suite_slow_pending=0u;
#endif
#if KUI_TOY_SUITE_MODE == 31
    if(!s.toy_suite_active || polled || lba != s.position || !channel_idle())
        s.toy_suite_rate = TOY_FAST;
#endif
    enum kui_sci_stream_result result;
#if KUI_TOY_SUITE_VERIFIED_RESUME
    if(s.state==RESUME) {
        const bool wake=KUI_TOY_SUITE_RESUME_WAKE && kui_retail_async_region.engine.in_irq==2u;
        if(s.position==lba && resume_admitted(s.fill)) {
            if(!(s.toy_suite_active&KUI_TOY_SUITE_RESUME_FOREGROUND)) return KUI_SCI_STREAM_PENDING;
            uint32_t lost=resume_meta(s.fill)->lost;
            if(lost && lost<=512u && !polled && channel_idle()) {
                select(true);
                result=start_dma(s.fill,lost);
                if(result!=KUI_SCI_STREAM_OK) {
                    select(false);
                    s.state=LOST;
                    if(wake)return KUI_SCI_STREAM_PENDING;
                }
                return result;
            }
        }
        /* The independent controller reply may only restart the parked
         * suffix. Leave command stop/reopen to a foreground visit. */
        if(wake)return KUI_SCI_STREAM_PENDING;
    }
#elif !KUI_TOY_SUITE_ASYNC_DEMAND
    if(s.state == RESUME && s.position == lba) {
        /* Resumed where an overrun in an interrupt stopped it. */
#if KUI_GENERAL_SCI_RESUME_FIX || KUI_CE_SCI_RESUME_FIX
        /* A busy guest channel falls through to the normal stop/restart
         * path below, which rereads this block by PIO. */
        if(channel_idle()) {
#elif KUI_TOY_PIPELINE_HIGH_STREAM && KUI_TOY_PIPELINE_RESUME_FIX
        /* finish already restored DMA1 before leaving RESUME. The guest
         * may have changed its idle tuple or started a transfer meanwhile.
         * Never resume over that transfer, or restore the old saved tuple. */
        if(!channel_idle()) {
            if((result = stop_card()) != KUI_SCI_STREAM_OK) return result;
            /* CLOSED falls through to CMD18 at this unchanged block. The
             * normal receive path uses PIO while the guest owns DMA1. */
        } else {
            for(unsigned i = 0; i < 4; ++i) s.saved[i] = rd32(SAR + 4u * i);
#endif
        select(true);
        if((result = start_dma(s.fill, s.lost[s.fill])) != KUI_SCI_STREAM_OK) s.state = LOST;
        return result;
#if (KUI_TOY_PIPELINE_HIGH_STREAM && KUI_TOY_PIPELINE_RESUME_FIX) || KUI_GENERAL_SCI_RESUME_FIX || KUI_CE_SCI_RESUME_FIX
        }
#endif
    }
#endif
#if KUI_SCI_STREAM_TOKEN_SLICED
    bool pending = s.state == TOKEN_WAIT && s.position == lba;
    if(!pending) s.token_polled = polled;
    else polled = s.token_polled;
    if(pending || (s.state == PAUSED && s.position == lba)) {
#else
    if(s.state == PAUSED && s.position == lba) {
#endif
        select(true);
        result = token(token_limit);
        if(result != KUI_SCI_STREAM_OK) {
#if KUI_SCI_STREAM_TOKEN_SLICED
            if(result != KUI_SCI_STREAM_PENDING)
#endif
                s.state = LOST;
            goto framing_result;
        }
    } else {
        if(s.state != CLOSED
#if KUI_TOY_SUITE_ASYNC_DEMAND
           && !(s.state==START_WAIT && s.position==lba)
#endif
           && (result = stop_card()) != KUI_SCI_STREAM_OK) goto framing_result;
        result = start(lba, token_limit);
        if(result != KUI_SCI_STREAM_OK) goto framing_result;
    }
    s.position = lba;
    result = receive(polled);
    if(result != KUI_SCI_STREAM_OK) s.state = LOST;
framing_result:
#if KUI_TOY_SUITE_SLOW_RETRY
    if(result > KUI_SCI_STREAM_PENDING) s.toy_suite_slow_pending=0u;
#endif
    return result;
}
/* End a reception: let the receiver take the second CRC byte and stop on the
 * next byte's overrun, stop the SCI and channel, restore the channel, keep
 * the RDR byte, then hand the SCI back to command bytes. A channel someone
 * else reprogrammed is left alone; the SCI is handed back in every case.
 * later: a mid-block overrun is resumed by the next fetch, not now. */
static enum kui_sci_stream_result
#if KUI_TOY_SUITE_VERIFIED_RESUME
/* Reuse the late Toy receive slot so the low SD lease retains its
 * original firmware admission bound. Mode 48 has no pipelined PIO. */
__attribute__((noipa,section(".toy_pio_payload")))
#endif
finish(bool later) {
    uint32_t area = s.fill, start = physical(s.area[area]), control = 0, count = 1;
    s.state = LOST;
#if KUI_TOY_SUITE_SLOW_RETRY
    const bool toy_retry_armed = s.toy_suite_slow_pending == TOY_RETRY_ARMED;
    s.toy_suite_slow_pending = 0u;
#endif
    if(rd32(CHCR) & 2u)
        for(unsigned n = 0; n < 512u && !(rd8(SSR) & ORER); ++n) {}
    wr8(SCR, 0);
    fence(s.area[area]);
    uint32_t address = rd32(DAR);
    bool ours = rd32(SAR) == (RDR & UINT32_C(0x1fffffff)) && address >= start &&
        address <= start + DMA_BYTES;
    if(ours) {
        wr32(CHCR, rd32(CHCR) & ~UINT32_C(5));
        (void)rd32(CHCR);
        fence(s.area[area]);
        control = rd32(CHCR); count = rd32(TCR);
        ours = rd32(DAR) == start + DMA_BYTES - count;
        if(ours) {
            wr32(CHCR, 0);
            for(unsigned i = 0; i < 4; ++i) wr32(SAR + 4u * i, s.saved[i]);
        }
    }
    uint8_t status = rd8(SSR), tail = 0;
#if KUI_TOY_SUITE_MODE == 31
    const bool slow_receive = s.toy_suite_rate == TOY_SLOW_DMA;
    s.toy_suite_rate = TOY_FAST;
#endif
    if(status & RDRF) tail = rd8(RDR);
    wr8(SSR, status & ~(RDRF | FLAGS));
    enum kui_sci_stream_result result = handoff(status & ORER);
    if(result != KUI_SCI_STREAM_OK) return result;
    if(!ours) {
        STREAM_OBSERVE(++s.stats.foreign);
        return KUI_SCI_STREAM_BUSY;
    }
    if(count || !(control & 2u) || !(status & RDRF)) {
#if KUI_TOY_SUITE_MODE == 31
        /* Reread the whole damaged block with its own CRC. At the slower
         * rate a second cut goes through the existing demand retry limit. */
        if(slow_receive || (s.toy_suite_active && (status & ORER))) {
            if(!slow_receive) s.toy_suite_rate = TOY_RETRY_SLOW;
            STREAM_OBSERVE(++s.stats.overruns);
            return KUI_SCI_STREAM_OVERRUN;
        }
#endif
#ifdef KUI_RETAIL_CE
        unsigned bucket = !count ? 0u : count <= 128u ? 1u : count <= 384u ? 2u : 3u;
        ++s.stats.incomplete[bucket];
        /* The SCI and our channel have already stopped and handed back.
         * These late snapshots cannot identify the exact fault-time owner. */
#if !KUI_CE_SCI_SLOW_AFTER_OVERRUN
        s.stats.incomplete_ch2_active += (rd32(CHCR + 0x10u) & 3u) == 1u;
        s.stats.incomplete_dmaor_bad += (rd32(DMAOR) & 7u) != 1u;
#endif
#endif
#if KUI_TOY_SUITE_VERIFIED_RESUME
        if((status&FLAGS)==ORER && (s.toy_suite_active&KUI_TOY_SUITE_RESUME_ACTIVE) &&
           s.wire[area]==1u && kui_retail_async_region.engine.token) {
            const uint32_t at=DMA_BYTES-count, held=(status&RDRF)!=0u, lost=at+held;
            if(lost<512u) {
                uint8_t *wire=resume_area(area);
                if(held)wire[at]=tail;
                wire[lost]=0u;
                resume_meta(area)->lost=lost+1u;
                resume_meta(area)->token=kui_retail_async_region.engine.token;
                s.wire[area]|=TOY_RESUMED_WIRE;
                /* Re-arm only through the next eligible foreground fetch.
                 * SCI IRQ re-arming failed on the original hardware. */
                s.state=RESUME;
                return KUI_SCI_STREAM_PENDING;
            }
        }
#endif
#if !STRICT_REREAD
        /* The channel was held off the bus (the game's own DMA) and the
         * receiver overran mid-block: the byte that overran is lost and the
         * card stopped after it. The channel has `at` bytes; usually it
         * also took the byte RDR held when it got the bus back, so byte
         * `at` is the lost one. If it had not yet (RDRF), RDR still holds
         * byte `at` and `at + 1` is lost. Once per block, if the lost byte
         * was data, carry on from the byte after it (the card simply
         * waited, deselected) and rebuild the lost one from the CRC when
         * the block is taken. */
        uint32_t at = DMA_BYTES - count, held = status & RDRF ? 1u : 0u, lost = at + held;
#if KUI_CE_SCI_SLOW_AFTER_OVERRUN
        /* An overrun at the fast clock starts the slow window; one at the
         * slow clock ends it, so a bus that holds the channel off longer
         * than 1.28 us gets the existing programmed refetch, not a repeat. */
        if(status & ORER) {
            if(s.slow & SLOW_RX) {++s.stats.slow_overruns; s.slow = 0;}
            else s.slow = KUI_SCI_STREAM_SLOW_BLOCKS;
        }
#endif
        if((status & ORER) && lost < 512u && !s.lost[area] && s.unrepaired < 2u) {
            s.lost[area] = (uint16_t)(lost + 1u);
            s.hold[area] = (uint8_t)held;
            s.held[area] = tail;
            STREAM_OBSERVE(++s.stats.repaired);
            if(later) {
                STREAM_OBSERVE(++s.stats.deferred);
                s.state = RESUME;
                return KUI_SCI_STREAM_PENDING;
            }
            select(true);
            result = start_dma(area, lost + 1u);
            return result == KUI_SCI_STREAM_OK ? KUI_SCI_STREAM_PENDING : result;
        }
#else
        /* Reconstructed blocks cannot be published. Restart at the same
         * LBA and check both CRC bytes from a fresh reception. */
        (void)later;
#if KUI_TOY_SUITE_MODE == 34
        s.toy_suite_pio_retry=s.toy_suite_active;
#elif KUI_TOY_SUITE_SLOW_RETRY
        if((status&ORER) && toy_retry_armed) {
            ++s.stats.toy_retry_orer;
            /* A fresh CMD18 at the unchanged position keeps both CRC
             * bytes. The ordinary retry limit still owns PIO fallback. */
            if(!(s.wire[area]&TOY_SLOW_WIRE))
                s.toy_suite_slow_pending=KUI_TOY_SUITE_FIRST_CUT_PIO &&
                    (s.toy_suite_active&KUI_TOY_SUITE_FIRST_CUT_ACTIVE)?
                    KUI_TOY_SCI_PIO_WINDOW_FIRST:TOY_RETRY_WAITING;
#if KUI_TOY_SUITE_PIO_WINDOW
            else s.toy_suite_slow_pending=TOY_PIO_WINDOW;
#endif
        }
#endif
#endif
        STREAM_OBSERVE(++s.stats.overruns);
        return KUI_SCI_STREAM_OVERRUN;
    }
    s.ready[area] = 1;
    s.ready_lba[area] = s.position++;
    s.arrived = s.position;
    s.rdr[area] = tail;
    s.state = PAUSED;
    STREAM_OBSERVE(++s.stats.blocks);
#if KUI_CE_SCI_SLOW_AFTER_OVERRUN
    if(s.slow & SLOW_RX) ++s.stats.slow_blocks;
    slow_done();
#endif
    return KUI_SCI_STREAM_OK;
}
enum kui_sci_stream_result kui_sci_stream_poll(bool interrupt) {
#if (KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28)
    if(s.state >= KUI_TOY_SCI_BURST_TOKEN) return KUI_SCI_STREAM_BUSY;
#endif
    if(s.state != DMA) return KUI_SCI_STREAM_OK;
    uint32_t control = rd32(CHCR);
    /* Still receiving, unless the channel stopped or the receiver overran. */
    if(!(control & 2u) && (control & 1u) && !(rd8(SSR) & ORER) &&
       (rd32(DMAOR) & 7u) == 1u) return KUI_SCI_STREAM_PENDING;
    return finish(interrupt);
}
enum kui_sci_stream_result kui_sci_stream_wait(void) {
    for(uint32_t n = 0; n < 200000u; ++n) {
        enum kui_sci_stream_result result = kui_sci_stream_poll(false);
        if(result != KUI_SCI_STREAM_PENDING) return result;
    }
    return finish(false); /* a stalled receiver ends as an overrun */
}
/* The byte whose loss leaves this CRC16 syndrome when it stands as zero
 * with `after` bytes behind it, or -1 if no single byte explains it. The
 * CRC (zero start) is linear: undo the `after` zero-byte shifts, bit by bit
 * (x^16+x^12+x^5+1 has its constant term, so each step is invertible). What
 * remains is one byte's own step, t ^ t << 5 ^ t << 12 with t = x ^ x >> 4:
 * its low byte gives t, t gives x, and the whole step must match. */
#if (!STRICT_REREAD || KUI_TOY_SUITE_VERIFIED_RESUME) && \
    !(KUI_TOY_SUITE_TABLE_CRC16 && defined(__sh__))
static int rebuild(uint32_t syndrome, uint32_t after) {
#if KUI_TOY_SUITE_VERIFIED_RESUME
    /* Four inverse bit steps at once. The four low-bit basis vectors
     * become 0x1102 shifted left 0..3, with disjoint result bits. */
    for(uint32_t n=after*2u;n;--n)
        syndrome=(syndrome>>4)^((syndrome&15u)*0x1102u);
#else
    for(uint32_t n = after * 8u; n; --n)
        syndrome = syndrome & 1u ? ((syndrome ^ 0x1021u) >> 1) | 0x8000u : syndrome >> 1;
#endif
    uint32_t t = (syndrome ^ syndrome << 5) & 0xffu, x = t ^ t >> 4;
    return crc16(0, (uint8_t)x) == syndrome ? (int)x : -1;
}
#endif
const uint8_t *kui_sci_stream_take(uint32_t lba, enum kui_sci_stream_result *result) {
    for(unsigned i = 0; i < 2; ++i) {
        if(!s.ready[i] || s.ready_lba[i] != lba) continue;
        s.ready[i] = 0;
        uint8_t *area = s.area[i];
#if KUI_TOY_SUITE_VERIFIED_RESUME
        uint32_t lost=0u;
        if(s.wire[i]&TOY_RESUMED_WIRE) {
            if(s.arrived==lba+1u)s.arrived=0u;
            if(!resume_admitted(i) || !(lost=resume_meta(i)->lost) || lost>512u) {
                *result=KUI_SCI_STREAM_CRC;
                return NULL;
            }
        }
#elif KUI_TOY_SUITE_ASYNC_DEMAND
        const uint32_t lost = 0u; /* strict mode never records a partial block */
#else
        uint32_t lost = s.lost[i];
#endif
#if KUI_TOY_SUITE_VERIFIED_RESUME
        /* The missing wire byte already stands as zero in the area. */
#elif STRICT_REREAD
        if(lost) {
            ++s.stats.crc_errors;
            *result=KUI_SCI_STREAM_CRC;
            return NULL;
        }
#else
        if(lost) {
            if(s.hold[i]) area[lost - 2u] = s.held[i];
            area[lost - 1u] = 0;
        }
#endif
#if !(KUI_TOY_SUITE_TABLE_CRC16 && defined(__sh__))
        uint8_t high = area[512], low = s.rdr[i];
#endif
#if KUI_TOY_SUITE_PIO_WINDOW || KUI_TOY_SUITE_VERIFIED_RESUME
        bool wire = s.wire[i] & 1u;
#else
        bool wire = s.wire[i];
#endif
#if KUI_TOY_SUITE_TABLE_CRC16 && defined(__sh__)
        const kui_toy_sd_crc16_check_fn check=(kui_toy_sd_crc16_check_fn)(uintptr_t)
            kui_retail_async_region.vector600[KUI_TOY_SD_CRC16_SLOT];
        if(check) {
            if(!check(area,s.rdr[i],wire,lost))goto crc_error;
            if(lost)goto recovered;
        } else {
            /* Verified resume only runs after the high helper is installed.
             * Before binding, check intact blocks and reject any lost byte. */
            if(lost)goto crc_error;
            uint16_t crc=0;
            for(unsigned w=0;w<128u;++w) {
                uint32_t word=((alias_word *)area)[w];
                if(wire)((alias_word *)area)[w]=word=reverse(word);
                for(unsigned b=0;b<4u;++b) {
                    crc=crc16(crc,(uint8_t)word);
                    word>>=8;
                }
            }
            uint32_t sent=((uint32_t)area[512]<<8)|s.rdr[i];
            if(wire)sent=(uint16_t)reverse(sent);
            if(crc!=sent)goto crc_error;
        }
        goto intact;
crc_error:
        ++s.stats.crc_errors;
        *result=KUI_SCI_STREAM_CRC;
        return NULL;
recovered:
        ++s.stats.toy_resumed_blocks;
        *result=KUI_SCI_STREAM_OK;
        return area;
intact:
#else
#if defined(KUI_RETAIL_CE) || (KUI_TOY_SUITE_FAST_CRC16 && defined(__sh__))
        uint32_t crc = 0;
#else
        uint16_t crc = 0;
#endif
#if KUI_CE_SCI_FAST_TAKE && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        crc = kui_sci_check_words((kui_sci_check_word *)(void *)area, wire);
#else
        for(unsigned w = 0; w < 128u; ++w) {
            uint32_t word = ((alias_word *)area)[w];
#if KUI_TOY_PILOT_WORD_REVERSE_PROFILE
            if(wire) ((alias_word *)area)[w] = word = reverse_data_word(word);
#else
            if(wire) ((alias_word *)area)[w] = word = reverse(word);
#endif
#if KUI_TOY_SUITE_FAST_CRC16 && defined(__sh__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            crc = toy_crc16_word(crc, word);
#else
            for(unsigned b = 0; b < 4u; ++b) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
                crc = crc16(crc, (uint8_t)word); word >>= 8;
#else
                crc = crc16(crc, (uint8_t)(word >> 24)); word <<= 8;
#endif
            }
#endif
        }
#endif
#if KUI_TOY_SUITE_FAST_CRC16 && defined(__sh__)
        crc = (uint16_t)crc;
#endif
        if(wire) {high = (uint8_t)reverse(high); low = (uint8_t)reverse(low);}
        uint16_t sent = (uint16_t)(high << 8 | low);
#if KUI_TOY_SUITE_VERIFIED_RESUME
        if(lost) {
            int value=crc==sent?0:((low|1u)==0xffu?-1:rebuild(crc^sent,512u-lost));
            if(value<0) {
                ++s.stats.crc_errors;
                *result=KUI_SCI_STREAM_CRC;
                return NULL;
            }
            area[lost-1u]=(uint8_t)value;
            ++s.stats.toy_resumed_blocks;
            /* The high cursor is currently RAW and targets only private
             * scratch. The candidate never becomes a reusable kept block. */
            *result=KUI_SCI_STREAM_OK;
            return area;
        }
#elif !STRICT_REREAD
        if(crc != sent && lost) {
            /* The gap byte (0xff) or the next token (0xfe) where the second
             * CRC byte belongs: the resumed reception ran a byte ahead. */
            if((low | 1u) == 0xffu) STREAM_OBSERVE(++s.stats.ahead);
            else {
                int x = rebuild((uint32_t)(crc ^ sent), 512u - lost);
                if(x >= 0) {area[lost - 1u] = (uint8_t)x; crc = sent;}
                else ++s.unrepaired;
            }
        }
#endif
        /* A reconstructed byte consumes eight bits of the CRC's evidence:
         * broader receive damage can then satisfy the remaining checksum.
         * Never publish a block whose DMA reception lost a byte. Keep the
         * existing CRC failure/retry path so the same block is read afresh,
         * with bounded programmed-I/O fallback under repeated contention. */
        if(((uint32_t)crc ^ sent) | lost) {
            STREAM_OBSERVE(++s.stats.crc_errors);
#if KUI_TOY_SUITE_PIO_WINDOW
            if((int8_t)s.toy_suite_slow_pending < 0)
                s.toy_suite_slow_pending = 0u;
#endif
            *result = KUI_SCI_STREAM_CRC;
            return NULL;
        }
#endif
#if KUI_TOY_SUITE_SLOW_RETRY
        if(s.wire[i]&TOY_SLOW_WIRE) ++s.stats.toy_retry_ok;
#endif
#if KUI_TOY_SUITE_PIO_WINDOW
        if(s.wire[i] == TOY_PIO_WINDOW_WIRE) ++s.stats.toy_pio_window_ok;
#endif
        s.kept = (uint8_t)(i + 1u);
        s.kept_lba = lba;
        *result = KUI_SCI_STREAM_OK;
        return area;
    }
    if(s.kept && s.kept_lba == lba) {
        STREAM_OBSERVE(++s.stats.kept);
        *result = KUI_SCI_STREAM_OK;
        return s.area[s.kept - 1u];
    }
    *result = KUI_SCI_STREAM_PENDING;
    return NULL;
}
enum kui_sci_stream_result kui_sci_stream_stop(void) {
#if KUI_TOY_SUITE_ASYNC_DEMAND
    /* Exclusive shutdown must finish its transaction before releasing SCI. */
    s.token_bounded=0u;
#endif
#if KUI_TOY_SUITE_MODE == 31
    /* Invalidate the retry after stopping, including a newly cut receive. */
    s.toy_suite_active = 0u;
    if(s.toy_suite_rate != TOY_SLOW_DMA) s.toy_suite_rate = TOY_FAST;
#elif KUI_TOY_SUITE_SLOW_RETRY
    s.toy_suite_active &= (uint8_t)~KUI_TOY_SUITE_SLOW_MOVIE;
    s.toy_suite_slow_pending = 0u;
#elif KUI_TOY_SUITE_VERIFIED_RESUME
    s.toy_suite_active &= (uint8_t)~(KUI_TOY_SUITE_RESUME_ACTIVE|KUI_TOY_SUITE_RESUME_FOREGROUND);
#endif
#if (KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28)
    /* Only the auxiliary can finish its sliced token or fence its receive. */
    if(s.state >= KUI_TOY_SCI_BURST_TOKEN) return KUI_SCI_STREAM_BUSY;
#endif
    enum kui_sci_stream_result result = KUI_SCI_STREAM_OK;
#if (KUI_TOY_PIPELINE_HIGH_STREAM && KUI_TOY_PIPELINE_RESUME_FIX) || KUI_GENERAL_SCI_RESUME_FIX || KUI_CE_SCI_RESUME_FIX
    /* Cancellation must fence the receiver, not launch another repair and
     * then clock CMD12 while DMA remains active. Deferred finish restores
     * the channel and leaves a partial block stopped in RESUME. */
    for(uint32_t n = 0; s.state == DMA && n < 200000u; ++n) {
        result = kui_sci_stream_poll(true);
        if(result != KUI_SCI_STREAM_PENDING) break;
    }
    if(s.state == DMA) result = finish(true);
#else
    if(s.state == DMA) result = kui_sci_stream_wait();
#endif
    if(result == KUI_SCI_STREAM_BUSY || result == KUI_SCI_STREAM_RESET) return result;
    s.ready[0] = s.ready[1] = 0;
    s.kept = 0; s.arrived = 0;
    return s.state == CLOSED ? KUI_SCI_STREAM_OK : stop_card();
}
