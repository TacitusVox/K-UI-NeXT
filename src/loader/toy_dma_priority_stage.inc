/* SPDX-License-Identifier: GPL-3.0-only */
/* Only the SHA-verified Toy owner calls this during the masked cold bind.
 * Both original C2 DMA setup literals select CH2 ahead of the SCI's CH1.
 * Keep the owner's CHCR2 and DDT programming. The original experiment gave
 * CH1 fixed priority over CH2. The round-robin test rotates priority after
 * a transfer unit (SH7750 hardware manual section 14.3.3). */
static bool toy_dma_priority_install(void) {
    static const uint32_t address[2]={UINT32_C(0x8c094f98),UINT32_C(0x8c095a6c)};
    const uint32_t uncached=UINT32_C(0x20000000);
    const uint32_t before=UINT32_C(0x8201);
#if KUI_TOY_SUITE_DMA_ROUND_ROBIN
    const uint32_t after=UINT32_C(0x8301);
#else
    const uint32_t after=UINT32_C(0x8001);
#endif
    for(unsigned i=0;i<2u;i++)
        if(native_word(address[i]|uncached)!=before)return false;
    for(unsigned i=0;i<2u;i++) {
        *(volatile uint32_t *)(uintptr_t)address[i]=after;
        native_publish(address[i]);
    }
    for(unsigned i=0;i<2u;i++)
        if(native_word(address[i]|uncached)!=after)return false;
    return true;
}
