/* SPDX-License-Identifier: GPL-3.0-only */

#if KUI_NATIVE_GD_DMA_STREAM || KUI_NATIVE_GD_MMU_BUFFERS || KUI_NATIVE_SESSION_CONTROLS || defined(KUI_RETAIL_CE)
#error "Toy's fast publish path requires the native main-RAM reader"
#endif

/* Integer-only copy for the private aligned staging arena and the native
 * mapper's uncached destination. No store-queue, DMA or FPU state changes. */
static void __attribute__((noipa)) toy_resume_copy(uint8_t *out,const uint8_t *in,uint32_t bytes) {
#ifdef __sh__
    if(!(((uintptr_t)out|(uintptr_t)in)&3u)) {
        const uint32_t chunks=bytes/32u;
        if(chunks) {
            uint32_t left=chunks,word;
            __asm__("1: mov.l @%[in]+,%[word]\n\t"
                    "mov.l %[word],@(0,%[out])\n\t"
                    "mov.l @%[in]+,%[word]\n\t"
                    "mov.l %[word],@(4,%[out])\n\t"
                    "mov.l @%[in]+,%[word]\n\t"
                    "mov.l %[word],@(8,%[out])\n\t"
                    "mov.l @%[in]+,%[word]\n\t"
                    "mov.l %[word],@(12,%[out])\n\t"
                    "mov.l @%[in]+,%[word]\n\t"
                    "mov.l %[word],@(16,%[out])\n\t"
                    "mov.l @%[in]+,%[word]\n\t"
                    "mov.l %[word],@(20,%[out])\n\t"
                    "mov.l @%[in]+,%[word]\n\t"
                    "mov.l %[word],@(24,%[out])\n\t"
                    "mov.l @%[in]+,%[word]\n\t"
                    "mov.l %[word],@(28,%[out])\n\t"
                    "dt %[left]\n\t"
                    "bf/s 1b\n\t"
                    "add #32,%[out]"
                : [in] "+&r"(in),[out] "+&r"(out),[left] "+&r"(left),[word] "=&r"(word)
                : : "t","memory");
            bytes-=chunks*32u;
        }
    }
#endif
    if(bytes)memcpy(out,in,bytes);
}

static bool __attribute__((noipa)) toy_resume_fast_publish(kui_retail_async_region_type *r,
    const struct kui_retail_cursor *original,const uint8_t *raw) {
    const struct kui_retail_gd *s=&r->shared.service;
    const uint32_t address=r->engine.destination;
    const uint32_t bytes=original->count*KUI_GAME_DATA_BYTES;
    if((address&3u) || !s->ops.map ||
       address!=((s->destination&UINT32_C(0x00ffffff))|UINT32_C(0x8c000000)) ||
       bytes!=s->request_bytes || (s->command!=KUI_GD_PIOREAD && s->command!=KUI_GD_DMAREAD))
        return false;
    /* The same native map used by write_out guards the complete contiguous
     * span and purges its P1 alias before the first byte is published. */
    uint8_t *out=s->ops.map(s->ops.context,address,bytes,1);
    if(!out || ((uintptr_t)out&3u))return false;
#ifndef KUI_NATIVE_READAHEAD_AUX_HOST_TEST
    if((uintptr_t)out!=(address|UINT32_C(0x20000000)))return false;
#endif
    for(uint32_t sector=0u;sector<original->count;++sector) {
        toy_resume_copy(out,raw+16u,KUI_GAME_DATA_BYTES);
        out+=KUI_GAME_DATA_BYTES;
        raw+=KUI_GAME_RAW_BYTES;
    }
    return true;
}
