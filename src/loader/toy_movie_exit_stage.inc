/* SPDX-License-Identifier: GPL-3.0-only */
/* Only the exact raw Toy owner reaches this cold, masked publication. The
 * player keeps its original call sites and argument/return conventions. */
static bool toy_movie_probe_install(uint32_t literal,uint32_t original,uint32_t entry) {
    const uint32_t uncached=literal|UINT32_C(0x20000000);
    if(native_word(uncached)!=original)return false;
    *(volatile uint32_t *)(uintptr_t)literal=entry;
    native_publish(literal);
    return native_word(uncached)==entry;
}
static bool toy_movie_exit_install(uint32_t entry) {
    return toy_movie_probe_install(UINT32_C(0x8c06d428),UINT32_C(0x8c08d638),entry);
}
