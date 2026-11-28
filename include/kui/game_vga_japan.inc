/* SPDX-License-Identifier: GPL-3.0-only */
/* Executable VGA patches by Rolly (SEGA-SKY), except as credited. Each package
 * contains the IP header and one DCP boot-file delta per disc. Literal
 * regions were checked against the bundled complete target. */

/* deSPIRIA (T14402M V1.001). The corrected October 2026 retail DCP
 * recommended by ConsoleMods writes 31 bytes in 1ST_READ.BIN. The
 * decoded stock SHA-256 also reproduces the Redump RetroAchievements ID
 * f62c7829ee12acf9586aba1b910275d7 with the unmodified 256-byte header.
 * https://retroachievements.org/game/4624/hashes
 * https://drive.google.com/uc?export=download&id=1d01vJSCD5SZXolb1lrGsw1xojmnuOJjv
 * DCP SHA-256 17106af557d92b35d74472a2930093b995605b20e25be691b2f539bb644d34a9. */
static const uint8_t kui_vga_despiria_fixed_060378[] = {
    0xa0, 0x47, 0x10,
};
static const uint8_t kui_vga_despiria_fixed_0f47a0[] = {
    0x04, 0xd0, 0x01, 0x60, 0x19, 0x40, 0x03, 0xc8, 0x34, 0xe4, 0x00, 0x8b,
    0x35, 0xe4, 0x02, 0xd0, 0x2b, 0x40, 0x09, 0x00, 0x30, 0x00, 0x80, 0xff,
    0x7c, 0xfb, 0x06, 0x8c,
};
static const struct kui_game_vga_span kui_vga_despiria_fixed_spans[] = {
    {0x060378u, 3, kui_vga_despiria_fixed_060378, 0},
    {0x0f47a0u, 28, kui_vga_despiria_fixed_0f47a0, 0},
};

/* BLACK MATRIX AD (T20101M V1.002). Rolly (SEGA-SKY).
 * https://segasky.itch.io/black-matrix-ad-vga-patch-for-sega-dreamcast
 * DCP SHA-256 bfb635911799a4574920a558a9df95f90cc9d345ae355f74645484c4c16ffe87.
 * Disc 1 DCP d0e854fbbf626dc5f6c6e62b3cb4db807b53b68ed4da6a3d63627194009a2f71.
 * Both disc patches produce the same complete executable. */
static const uint8_t kui_vga_black_matrix_076fd6[] = {
    0x22, 0x4f, 0xec, 0x7f, 0x4a, 0xd1,
};
static const uint8_t kui_vga_black_matrix_092a66[] = {
    0x2d, 0x94,
};
static const uint8_t kui_vga_black_matrix_0add60[] = {
    0x01, 0xd0, 0x2b, 0x40, 0x09, 0x00, 0x09, 0x00, 0x0a, 0x99, 0x0d, 0x8c,
    0x0b, 0x00, 0x03, 0xe0,
};
static const uint8_t kui_vga_black_matrix_0adff4[] = {
    0x60, 0x98,
};
static const uint8_t kui_vga_black_matrix_0c99b4[] = {
    0xcc, 0x5c, 0x2c, 0x8c, 0x18, 0x5a, 0x2c, 0x8c, 0x00, 0x80, 0x5f, 0xa0,
    0x30, 0x00, 0x80, 0xff, 0xa0, 0x5c, 0x2c, 0x8c, 0xff, 0xff, 0x0f, 0xc0,
    0x00, 0xc0, 0x03, 0x00, 0xe4, 0x9a, 0x0d, 0x8c, 0x18, 0x28, 0x0a, 0x8c,
};
static const struct kui_game_vga_span kui_vga_black_matrix_spans[] = {
    {0x076fd6u, 6, kui_vga_black_matrix_076fd6, 0},
    {0x092a66u, 2, kui_vga_black_matrix_092a66, 0},
    {0x092cdeu, 2, kui_vga_plasma_us_mode, 0},
    {0x0a3a58u, 4, kui_vga_plasma_us_cable, 0},
    {0x0add60u, 16, kui_vga_black_matrix_0add60, 0},
    {0x0adff4u, 2, kui_vga_black_matrix_0adff4, 0},
    {0x0c9860u, 340, kui_vga_video_routine, 0},
    {0x0c99b4u, 36, kui_vga_black_matrix_0c99b4, 0},
    {0x0c99d8u, 55, kui_vga_video_constants, 0},
    {0x0c9a0fu, 7, kui_vga_video_parameters, 0},
    {0x0c9a16u, 63, kui_vga_video_trailer, 0},
};

/* CHOUHATSUMEIBOY KANIPAN BOUSOUROBOTO NO NAZO (HDR-0021 V1.005). Rolly (SEGA-SKY).
 * https://segasky.itch.io/cho-hatsumei-boy-kanipan-vga-patch-for-sega-dreamcast
 * DCP SHA-256 57f7b047c8e819eae437704da7cb0154f0689b5d14131ad1b7fa0ea3350b0081. */
static const uint8_t kui_vga_kanipan_129500[] = {
    0x01, 0xd0, 0x2b, 0x40, 0x09, 0x00, 0x09, 0x00, 0x2a, 0x10, 0x15, 0x8c,
    0x0b, 0x00, 0x03, 0xe0,
};
static const uint8_t kui_vga_kanipan_129794[] = {
    0x80, 0x0f,
};
static const uint8_t kui_vga_kanipan_1410d4[] = {
    0x4c, 0x7e, 0x2c, 0x8c, 0x98, 0x7b, 0x2c, 0x8c, 0x00, 0x80, 0x5f, 0xa0,
    0x30, 0x00, 0x80, 0xff, 0x20, 0x7e, 0x2c, 0x8c, 0xff, 0xff, 0x0f, 0xc0,
    0x00, 0xc0, 0x03, 0x00, 0xc4, 0x11, 0x15, 0x8c, 0x14, 0xed, 0x11, 0x8c,
};
static const struct kui_game_vga_span kui_vga_kanipan_spans[] = {
    {0x0dd748u, 4, kui_vga_plasma_us_cable, 0},
    {0x10f1dau, 2, kui_vga_plasma_us_mode, 0},
    {0x129500u, 16, kui_vga_kanipan_129500, 0},
    {0x129794u, 2, kui_vga_kanipan_129794, 0},
    {0x140f80u, 340, kui_vga_video_routine, 0},
    {0x1410d4u, 36, kui_vga_kanipan_1410d4, 0},
    {0x1410f8u, 55, kui_vga_video_constants, 0},
    {0x14112fu, 7, kui_vga_video_parameters, 0},
    {0x141136u, 63, kui_vga_video_trailer, 0},
};

/* DANCE DANCE REVOLUTION 2NDMIX DREAMCAST EDITION (T9506M V1.003). Rolly (SEGA-SKY).
 * https://segasky.itch.io/dance-dance-revolution-vga-patch-for-sega-dreamcast
 * DCP SHA-256 25ca06348a229d251b53ccd6498f78deec74543c36439b986246d3b6d2116a5b. */
static const uint8_t kui_vga_ddr_2nd_mix_11ac80[] = {
    0x01, 0xd0, 0x2b, 0x40, 0x09, 0x00, 0x09, 0x00, 0xea, 0x5b, 0x14, 0x8c,
    0x0b, 0x00, 0x03, 0xe0,
};
static const uint8_t kui_vga_ddr_2nd_mix_11afd0[] = {
    0x40, 0x5b,
};
static const uint8_t kui_vga_ddr_2nd_mix_135c94[] = {
    0xd0, 0xdf, 0x49, 0x8c, 0x10, 0xdd, 0x49, 0x8c, 0x00, 0x80, 0x5f, 0xa0,
    0x30, 0x00, 0x80, 0xff, 0xa4, 0xdf, 0x49, 0x8c, 0xff, 0xff, 0x0f, 0xc0,
    0x00, 0xc0, 0x03, 0x00, 0x84, 0x5e, 0x14, 0x8c, 0x2c, 0xeb, 0x0e, 0x8c,
};
static const struct kui_game_vga_span kui_vga_ddr_2nd_mix_spans[] = {
    {0x0df038u, 2, kui_vga_plasma_us_mode, 0},
    {0x10c9e8u, 4, kui_vga_plasma_us_cable, 0},
    {0x11ac80u, 16, kui_vga_ddr_2nd_mix_11ac80, 0},
    {0x11afd0u, 2, kui_vga_ddr_2nd_mix_11afd0, 0},
    {0x135b40u, 340, kui_vga_video_routine, 0},
    {0x135c94u, 36, kui_vga_ddr_2nd_mix_135c94, 0},
    {0x135cb8u, 55, kui_vga_video_constants, 0},
    {0x135cefu, 7, kui_vga_video_parameters, 0},
    {0x135cf6u, 63, kui_vga_video_trailer, 0},
};

/* DDR CLUB VER 1000 (T9508M V1.000). Rolly (SEGA-SKY).
 * https://segasky.itch.io/dance-dance-revolution-vga-patch-for-sega-dreamcast
 * DCP SHA-256 a944935b5848772924f415a0c7ebedcfee4f149f034693f310c47e24b768a696. */
static const uint8_t kui_vga_ddr_club_126dc0[] = {
    0x01, 0xd0, 0x2b, 0x40, 0x09, 0x00, 0x09, 0x00, 0x2a, 0x1d, 0x15, 0x8c,
    0x0b, 0x00, 0x03, 0xe0,
};
static const uint8_t kui_vga_ddr_club_127110[] = {
    0x80, 0x1c,
};
static const uint8_t kui_vga_ddr_club_141dd4[] = {
    0x9c, 0xa4, 0x4a, 0x8c, 0xdc, 0xa1, 0x4a, 0x8c, 0x00, 0x80, 0x5f, 0xa0,
    0x30, 0x00, 0x80, 0xff, 0x70, 0xa4, 0x4a, 0x8c, 0xff, 0xff, 0x0f, 0xc0,
    0x00, 0xc0, 0x03, 0x00, 0xc4, 0x1f, 0x15, 0x8c, 0xe4, 0xac, 0x0f, 0x8c,
};
static const struct kui_game_vga_span kui_vga_ddr_club_spans[] = {
    {0x0eb1f0u, 2, kui_vga_plasma_us_mode, 0},
    {0x118b34u, 4, kui_vga_plasma_us_cable, 0},
    {0x126dc0u, 16, kui_vga_ddr_club_126dc0, 0},
    {0x127110u, 2, kui_vga_ddr_club_127110, 0},
    {0x141c80u, 340, kui_vga_video_routine, 0},
    {0x141dd4u, 36, kui_vga_ddr_club_141dd4, 0},
    {0x141df8u, 55, kui_vga_video_constants, 0},
    {0x141e2fu, 7, kui_vga_video_parameters, 0},
    {0x141e36u, 63, kui_vga_video_trailer, 0},
};

/* DVINE LUV (T46501M V1.002). Rolly (SEGA-SKY).
 * https://segasky.itch.io/dvine-luv-vga-patch-for-sega-dreamcast
 * DCP SHA-256 8c9d0ae5e3413501223bcc2007f3eb4fe86dd36ba7ade3a6b145b3d1249cebc7. */
static const uint8_t kui_vga_dvine_luv_006df8[] = {
    0x09, 0x00,
};
static const struct kui_game_vga_span kui_vga_dvine_luv_spans[] = {
    {0x006df8u, 2, kui_vga_dvine_luv_006df8, 0},
};

/* JOJO (6107357 V1.002). Rolly (SEGA-SKY).
 * https://segasky.itch.io/jojos-bizarre-adventure-vga-patch-for-sega-dreamcast
 * DCP SHA-256 90bc52716c4200af4590e5fb76949f8bc7a01e8a73ab1072cdfc3d175cac27d6. */
static const uint8_t kui_vga_jojo_trial_01f500[] = {
    0x01, 0xd0, 0x2b, 0x40, 0x09, 0x00, 0x09, 0x00, 0xaa, 0x4a, 0x04, 0x8c,
    0x0b, 0x00, 0x03, 0xe0,
};
static const uint8_t kui_vga_jojo_trial_01f850[] = {
    0x00, 0x4a,
};
static const uint8_t kui_vga_jojo_trial_034b54[] = {
    0x30, 0xe4, 0x0c, 0x8c, 0x70, 0xe1, 0x0c, 0x8c, 0x00, 0x80, 0x5f, 0xa0,
    0x30, 0x00, 0x80, 0xff, 0x04, 0xe4, 0x0c, 0x8c, 0xff, 0xff, 0x0f, 0xc0,
    0x00, 0xc0, 0x03, 0x00, 0x44, 0x4d, 0x04, 0x8c, 0x88, 0xd9, 0x01, 0x8c,
};
static const struct kui_game_vga_span kui_vga_jojo_trial_spans[] = {
    {0x00de94u, 2, kui_vga_plasma_us_mode, 0},
    {0x01574cu, 4, kui_vga_plasma_us_cable, 0},
    {0x01f500u, 16, kui_vga_jojo_trial_01f500, 0},
    {0x01f850u, 2, kui_vga_jojo_trial_01f850, 0},
    {0x034a00u, 340, kui_vga_video_routine, 0},
    {0x034b54u, 36, kui_vga_jojo_trial_034b54, 0},
    {0x034b78u, 55, kui_vga_video_constants, 0},
    {0x034bafu, 7, kui_vga_video_parameters, 0},
    {0x034bb6u, 63, kui_vga_video_trailer, 0},
};

/* JOJO (T1205M V1.001). Rolly (SEGA-SKY).
 * https://segasky.itch.io/jojos-bizarre-adventure-vga-patch-for-sega-dreamcast
 * DCP SHA-256 2813d4d1f146a75e4d6311b214d4c4086e42f6135ba99b5494a6a428eb6eed10. */
static const uint8_t kui_vga_jojo_jp_021b40[] = {
    0x01, 0xd0, 0x2b, 0x40, 0x09, 0x00, 0x09, 0x00, 0x8a, 0x69, 0x04, 0x8c,
    0x0b, 0x00, 0x03, 0xe0,
};
static const uint8_t kui_vga_jojo_jp_021e90[] = {
    0xe0, 0x68,
};
static const uint8_t kui_vga_jojo_jp_036a34[] = {
    0x6c, 0xc6, 0x0f, 0x8c, 0xac, 0xc3, 0x0f, 0x8c, 0x00, 0x80, 0x5f, 0xa0,
    0x30, 0x00, 0x80, 0xff, 0x40, 0xc6, 0x0f, 0x8c, 0xff, 0xff, 0x0f, 0xc0,
    0x00, 0xc0, 0x03, 0x00, 0x24, 0x6c, 0x04, 0x8c, 0xa0, 0xf1, 0x01, 0x8c,
};
static const struct kui_game_vga_span kui_vga_jojo_jp_spans[] = {
    {0x00f6acu, 2, kui_vga_plasma_us_mode, 0},
    {0x016f64u, 4, kui_vga_plasma_us_cable, 0},
    {0x021b40u, 16, kui_vga_jojo_jp_021b40, 0},
    {0x021e90u, 2, kui_vga_jojo_jp_021e90, 0},
    {0x0368e0u, 340, kui_vga_video_routine, 0},
    {0x036a34u, 36, kui_vga_jojo_jp_036a34, 0},
    {0x036a58u, 55, kui_vga_video_constants, 0},
    {0x036a8fu, 7, kui_vga_video_parameters, 0},
    {0x036a96u, 63, kui_vga_video_trailer, 0},
};

/* JOJO S BIZARRE ADVENTURE (T1206N V1.001). Rolly (SEGA-SKY).
 * https://segasky.itch.io/jojos-bizarre-adventure-vga-patch-for-sega-dreamcast
 * DCP SHA-256 50c073e4dd17f83efcb8bf6d6167561c5073ff01b44bf44b32b73e21318e76c9.
 * Original executable SHA-256 from the K-UI US startup survey. */
static const uint8_t kui_vga_jojo_us_01fb80[] = {
    0x01, 0xd0, 0x2b, 0x40, 0x09, 0x00, 0x09, 0x00, 0x6a, 0x4c, 0x04, 0x8c,
    0x0b, 0x00, 0x03, 0xe0,
};
static const uint8_t kui_vga_jojo_us_01fed0[] = {
    0xc0, 0x4b,
};
static const uint8_t kui_vga_jojo_us_034d14[] = {
    0x34, 0x6b, 0x0a, 0x8c, 0x74, 0x68, 0x0a, 0x8c, 0x00, 0x80, 0x5f, 0xa0,
    0x30, 0x00, 0x80, 0xff, 0x08, 0x6b, 0x0a, 0x8c, 0xff, 0xff, 0x0f, 0xc0,
    0x00, 0xc0, 0x03, 0x00, 0x04, 0x4f, 0x04, 0x8c, 0x7c, 0xe1, 0x01, 0x8c,
};
static const struct kui_game_vga_span kui_vga_jojo_us_spans[] = {
    {0x00e740u, 2, kui_vga_plasma_us_mode, 0},
    {0x015fccu, 4, kui_vga_plasma_us_cable, 0},
    {0x01fb80u, 16, kui_vga_jojo_us_01fb80, 0},
    {0x01fed0u, 2, kui_vga_jojo_us_01fed0, 0},
    {0x034bc0u, 340, kui_vga_video_routine, 0},
    {0x034d14u, 36, kui_vga_jojo_us_034d14, 0},
    {0x034d38u, 55, kui_vga_video_constants, 0},
    {0x034d6fu, 7, kui_vga_video_parameters, 0},
    {0x034d76u, 63, kui_vga_video_trailer, 0},
};

/* JOJO S BIZARRE ADVENTURE (T7007D  50 V1.001). Rolly (SEGA-SKY).
 * https://segasky.itch.io/jojos-bizarre-adventure-vga-patch-for-sega-dreamcast
 * DCP SHA-256 592346b2975a86bd646519d0ad17b33bdbd24599e3f4de78ee2596e494db2826.
 * Retain the original PAL-specific check at 0xa08. The rest of the
 * PAL target is byte-identical to the USA target used for verification. */
static const struct kui_game_vga_span kui_vga_jojo_pal_spans[] = {
    {0x00e740u, 2, kui_vga_plasma_us_mode, 0},
    {0x015fccu, 4, kui_vga_plasma_us_cable, 0},
    {0x01fb80u, 16, kui_vga_jojo_us_01fb80, 0},
    {0x01fed0u, 2, kui_vga_jojo_us_01fed0, 0},
    {0x034bc0u, 340, kui_vga_video_routine, 0},
    {0x034d14u, 36, kui_vga_jojo_us_034d14, 0},
    {0x034d38u, 55, kui_vga_video_constants, 0},
    {0x034d6fu, 7, kui_vga_video_parameters, 0},
    {0x034d76u, 63, kui_vga_video_trailer, 0},
};

/* GEKKA NO KENSHI2 FINAL EDITION (T3107M V1.003). Rolly (SEGA-SKY).
 * https://segasky.itch.io/the-last-blade-2-vga-patch-for-sega-dreamcast
 * DCP SHA-256 82c104950e86b194d28eb060184da6008d16e97c0268c082f050b5b0127d52cb. */
static const uint8_t kui_vga_last_blade_jp_12c080[] = {
    0x01, 0xd0, 0x2b, 0x40, 0x09, 0x00, 0x09, 0x00, 0xea, 0x57, 0x15, 0x8c,
    0x0b, 0x00, 0x03, 0xe0,
};
static const uint8_t kui_vga_last_blade_jp_12c3d0[] = {
    0x40, 0x57,
};
static const uint8_t kui_vga_last_blade_jp_145894[] = {
    0xf0, 0xa1, 0xef, 0x8c, 0x30, 0x9f, 0xef, 0x8c, 0x00, 0x80, 0x5f, 0xa0,
    0x30, 0x00, 0x80, 0xff, 0xc4, 0xa1, 0xef, 0x8c, 0xff, 0xff, 0x0f, 0xc0,
    0x00, 0xc0, 0x03, 0x00, 0x84, 0x5a, 0x15, 0x8c, 0x34, 0xf5, 0x11, 0x8c,
};
static const struct kui_game_vga_span kui_vga_last_blade_jp_spans[] = {
    {0x10fa40u, 2, kui_vga_plasma_us_mode, 0},
    {0x122b68u, 4, kui_vga_plasma_us_cable, 0},
    {0x12c080u, 16, kui_vga_last_blade_jp_12c080, 0},
    {0x12c3d0u, 2, kui_vga_last_blade_jp_12c3d0, 0},
    {0x145740u, 340, kui_vga_video_routine, 0},
    {0x145894u, 36, kui_vga_last_blade_jp_145894, 0},
    {0x1458b8u, 55, kui_vga_video_constants, 0},
    {0x1458efu, 7, kui_vga_video_parameters, 0},
    {0x1458f6u, 63, kui_vga_video_trailer, 0},
};

/* MAHJONG TAIKAI 2 SPECIAL (T7601M V1.003). Rolly (SEGA-SKY).
 * https://segasky.itch.io/mahjong-taikai-ii-special-vga-patch
 * DCP SHA-256 7c1168b0f10adb4b7362132a869a8f060c79d531132bab17ecf6985b0bcd1302. */
static const struct kui_game_vga_span kui_vga_mahjong_taikai_spans[] = {
    {0x05f330u, 2, kui_vga_plasma_us_mode, 0},
    {0x05f59cu, 2, kui_vga_plasma_us_mode, 0},
};

/* NIJYUEI (T46505M V1.001). Rolly (SEGA-SKY).
 * https://segasky.itch.io/nijyuei-vga-patch-for-sega-dreamcast
 * DCP SHA-256 fa0cd8a527754e273c6ea69dc3f554452280ae53ddc08212ed65c581ebd9ebed. */
static const struct kui_game_vga_span kui_vga_nijyuei_spans[] = {
    {0x007148u, 2, kui_vga_dvine_luv_006df8, 0},
};

/* PSYCHIC FORCE 2012 (T1101M V1.002). Rolly (SEGA-SKY).
 * https://segasky.itch.io/psychic-force-2012-vga-patch-for-sega-dreamcast
 * DCP SHA-256 16df9d5d4fdbb7078662d0920e5705c27337c592fa85d2e37083e406f01e2fb3. */
static const uint8_t kui_vga_psychic_force_jp_43cae0[] = {
    0x01, 0xd0, 0x2b, 0x40, 0x09, 0x00, 0x09, 0x00, 0x2a, 0xe6, 0x45, 0x8c,
    0x0b, 0x00, 0x03, 0xe0,
};
static const uint8_t kui_vga_psychic_force_jp_43cd74[] = {
    0x80, 0xe5,
};
static const uint8_t kui_vga_psychic_force_jp_44e6d4[] = {
    0x80, 0x6b, 0x80, 0x8c, 0xcc, 0x68, 0x80, 0x8c, 0x00, 0x80, 0x5f, 0xa0,
    0x30, 0x00, 0x80, 0xff, 0x54, 0x6b, 0x80, 0x8c, 0xff, 0xff, 0x0f, 0xc0,
    0x00, 0xc0, 0x03, 0x00, 0xc4, 0xe7, 0x45, 0x8c, 0x8c, 0xd0, 0x43, 0x8c,
};
static const struct kui_game_vga_span kui_vga_psychic_force_jp_spans[] = {
    {0x170a44u, 4, kui_vga_plasma_us_cable, 0},
    {0x42d4b6u, 2, kui_vga_plasma_us_mode, 0},
    {0x43cae0u, 16, kui_vga_psychic_force_jp_43cae0, 0},
    {0x43cd74u, 2, kui_vga_psychic_force_jp_43cd74, 0},
    {0x44e580u, 340, kui_vga_video_routine, 0},
    {0x44e6d4u, 36, kui_vga_psychic_force_jp_44e6d4, 0},
    {0x44e6f8u, 55, kui_vga_video_constants, 0},
    {0x44e72fu, 7, kui_vga_video_parameters, 0},
    {0x44e736u, 63, kui_vga_video_trailer, 0},
};

/* PSYCHIC FORCE 2012 (T-8106D-50 V1.000). Rolly (SEGA-SKY).
 * https://segasky.itch.io/psychic-force-2012-vga-patch-for-sega-dreamcast
 * DCP SHA-256 6582d10cbd42f0436db0d0abfbd12b55b57e8b9e69aff2ed2b0d74ac6a2a2c0f. */
static const uint8_t kui_vga_psychic_force_pal_440560[] = {
    0x01, 0xd0, 0x2b, 0x40, 0x09, 0x00, 0x09, 0x00, 0xaa, 0x20, 0x46, 0x8c,
    0x0b, 0x00, 0x03, 0xe0,
};
static const uint8_t kui_vga_psychic_force_pal_4407f4[] = {
    0x00, 0x20,
};
static const uint8_t kui_vga_psychic_force_pal_452154[] = {
    0xe0, 0xab, 0x80, 0x8c, 0x2c, 0xa9, 0x80, 0x8c, 0x00, 0x80, 0x5f, 0xa0,
    0x30, 0x00, 0x80, 0xff, 0xb4, 0xab, 0x80, 0x8c, 0xff, 0xff, 0x0f, 0xc0,
    0x00, 0xc0, 0x03, 0x00, 0x44, 0x22, 0x46, 0x8c, 0x0c, 0x0b, 0x44, 0x8c,
};
static const struct kui_game_vga_span kui_vga_psychic_force_pal_spans[] = {
    {0x16ea90u, 4, kui_vga_plasma_us_cable, 0},
    {0x430f36u, 2, kui_vga_plasma_us_mode, 0},
    {0x440560u, 16, kui_vga_psychic_force_pal_440560, 0},
    {0x4407f4u, 2, kui_vga_psychic_force_pal_4407f4, 0},
    {0x452000u, 340, kui_vga_video_routine, 0},
    {0x452154u, 36, kui_vga_psychic_force_pal_452154, 0},
    {0x452178u, 55, kui_vga_video_constants, 0},
    {0x4521afu, 7, kui_vga_video_parameters, 0},
    {0x4521b6u, 63, kui_vga_video_trailer, 0},
};

/* RUNE CASTER (T40001M V1.007). Rolly (SEGA-SKY).
 * https://segasky.itch.io/rune-caster-vga-patch
 * DCP SHA-256 44967e43eff2b0b97ee600f466a31072f0193ab2feebd5c718448f9be066dffb. */
static const struct kui_game_vga_span kui_vga_rune_caster_spans[] = {
    {0x08b1feu, 2, kui_vga_plasma_us_mode, 0},
    {0x08b564u, 2, kui_vga_plasma_us_mode, 0},
};

/* NOBUNAGA NO YABOU SHOUSEIROKU with PowerUpKit (T7602M V1.000). Rolly (SEGA-SKY).
 * https://segasky.itch.io/sangokushi-nobunga-vga
 * DCP SHA-256 6c8aef0aecc97ede33edbf2b86c6e7a410333d0c644e508f4793fc904e3944d8.
 * The archive supplies a fixed executable and a reverse DCP that restores
 * both stock mode-load instructions. SHA-256 of that decoded source:
 * 64f21c9bcf821d128ec773aa1935399325cc4e643d3ce1dcb8ca48ee5ed84aca. */
static const struct kui_game_vga_edit kui_vga_nobunaga_shouseiroku_edits[] = {
    {0x14fc88u, 0x942e, 0xe400},
    {0x14fef4u, 0x942e, 0xe400},
};

/* NOBUNAGA NO YABOU REPPUUDEN (T7605M V1.001). Rolly (SEGA-SKY).
 * https://segasky.itch.io/sangokushi-nobunga-vga
 * DCP SHA-256 31569f9a750f6dd3e49c36b9b2f8c8fa7a8e395bfad7f3bbc977a599212df482.
 * The archive supplies a fixed executable and a reverse DCP that restores
 * both stock mode-load instructions. SHA-256 of that decoded source:
 * 1579b966c5ac5b836c26fe81ddd45fe06d76aa38e74d3aef831029a39927d791. */
static const struct kui_game_vga_edit kui_vga_nobunaga_reppuuden_edits[] = {
    {0x277bb2u, 0x942d, 0xe400},
    {0x277e60u, 0x9419, 0xe400},
};

/* SANGOKUSHI6WITHPK (T7607M V1.000). Rolly (SEGA-SKY).
 * https://segasky.itch.io/sangokushi-nobunga-vga
 * DCP SHA-256 dd79e8d6c2d31a86628f2da168cbf712bc38ed85ad267bbbae73534ce73ccd59.
 * The archive supplies a fixed executable and a reverse DCP that restores
 * both stock mode-load instructions. SHA-256 of that decoded source:
 * 9d840eff755b052411b99b5bcd9ca7e64f1772bcac6fa5848dd54bb3eaacb596. */
static const struct kui_game_vga_edit kui_vga_sangokushi_powerup_edits[] = {
    {0x2e89d6u, 0x942d, 0xe400},
    {0x2e8d3cu, 0x9419, 0xe400},
};

/* SANGOKUSHI 6 (T7603M V1.000). Rolly (SEGA-SKY).
 * https://segasky.itch.io/sangokushi-nobunga-vga
 * DCP SHA-256 89ff6417ca5f0f75b98e13d4046c5fe8c4c531073e59b73f59de274e423e230c. */
static const struct kui_game_vga_span kui_vga_sangokushi_spans[] = {
    {0x2c31ccu, 2, kui_vga_plasma_us_mode, 0},
    {0x2c3438u, 2, kui_vga_plasma_us_mode, 0},
};

/* SHINRI GAME DC EDITION (T43403M V1.004). Rolly (SEGA-SKY).
 * https://segasky.itch.io/the-shinri-game-vga-patch
 * DCP SHA-256 5919261bc4d58a03aca96c78dd9a66b5a3357c680c4673ed212bbc4c95a70c2d. */
static const struct kui_game_vga_span kui_vga_shinri_spans[] = {
    {0x0f634eu, 2, kui_vga_plasma_us_mode, 0},
    {0x0f65fcu, 2, kui_vga_plasma_us_mode, 0},
};

/* SUNRISE EIYUUTAN (T41101M V1.000). Rolly (SEGA-SKY).
 * https://segasky.itch.io/sunrise-eiyuutan-vga-patch
 * DCP SHA-256 89916f57064687315544d69b84ac6f12508e3c9b534ca7d305e4040fd81c7fd4. */
static const struct kui_game_vga_span kui_vga_sunrise_spans[] = {
    {0x09853au, 2, kui_vga_plasma_us_mode, 0},
    {0x0987e8u, 2, kui_vga_plasma_us_mode, 0},
};

/* TANAKATORAHIKO NO URUTORARYUUSYOUGI IBISYAANAGUMAHEN (T22001M V1.006). Rolly (SEGA-SKY).
 * https://segasky.itch.io/tanaka-torahiko-no-uru-toraryuu-shougi-ibisha-vga-patch
 * DCP SHA-256 06fd53c5ef2107b0ab8fd5f849bd43e16f03367575463ebfd164b0f8da2984d4. */
static const uint8_t kui_vga_tanaka_shogi_09e0a4[] = {
    0x18, 0x00,
};
static const struct kui_game_vga_span kui_vga_tanaka_shogi_spans[] = {
    {0x078096u, 2, kui_vga_plasma_us_mode, 0},
    {0x07830eu, 2, kui_vga_plasma_us_mode, 0},
    {0x09e0a4u, 2, kui_vga_tanaka_shogi_09e0a4, 0},
};

/* TREASURE STRIKE (T19701M V1.006). Rolly (SEGA-SKY).
 * https://segasky.itch.io/treasure-strike-vga-patch-for-sega-dreamcast
 * DCP SHA-256 42ffe7faeee614223389057a160cc1f11b865a6945fec4079251f027e25e960d. */
static const uint8_t kui_vga_treasure_strike_143300[] = {
    0x01, 0xd0, 0x2b, 0x40, 0x09, 0x00, 0x09, 0x00, 0x6a, 0xe8, 0x17, 0x8c,
    0x0b, 0x00, 0x03, 0xe0,
};
static const uint8_t kui_vga_treasure_strike_16e914[] = {
    0x4c, 0x38, 0xa4, 0x8c, 0x8c, 0x35, 0xa4, 0x8c, 0x00, 0x80, 0x5f, 0xa0,
    0x30, 0x00, 0x80, 0xff, 0x20, 0x38, 0xa4, 0x8c, 0xff, 0xff, 0x0f, 0xc0,
    0x00, 0xc0, 0x03, 0x00, 0x04, 0xeb, 0x17, 0x8c, 0x30, 0x9c, 0x13, 0x8c,
};
static const struct kui_game_vga_span kui_vga_treasure_strike_spans[] = {
    {0x12a1f4u, 2, kui_vga_plasma_us_mode, 0},
    {0x137a20u, 4, kui_vga_plasma_us_cable, 0},
    {0x143300u, 16, kui_vga_treasure_strike_143300, 0},
    {0x143650u, 2, kui_vga_star_gladiator_jp_mode, 0},
    {0x16e7c0u, 340, kui_vga_video_routine, 0},
    {0x16e914u, 36, kui_vga_treasure_strike_16e914, 0},
    {0x16e938u, 55, kui_vga_video_constants, 0},
    {0x16e96fu, 7, kui_vga_video_parameters, 0},
    {0x16e976u, 63, kui_vga_video_trailer, 0},
};

/* WORLD NEVERLAND2 THE WAKTIC REPUBLIC OF PLUTO STORIES (T5303M V1.001). Rolly (SEGA-SKY).
 * https://segasky.itch.io/world-neverland-2-plus-vga-patch-for-sega-dreamcast
 * DCP SHA-256 cd517e03336583102181ca5ca7f7c18d6f8f71512ea15381531a334f09c20fa7. */
static const uint8_t kui_vga_world_neverland_2_1e0c00[] = {
    0x01, 0xd0, 0x2b, 0x40, 0x09, 0x00, 0x09, 0x00, 0xea, 0x17, 0x20, 0x8c,
    0x0b, 0x00, 0x03, 0xe0,
};
static const uint8_t kui_vga_world_neverland_2_1e0f50[] = {
    0x40, 0x17,
};
static const uint8_t kui_vga_world_neverland_2_1f1894[] = {
    0x6c, 0x1a, 0x50, 0x8c, 0xac, 0x17, 0x50, 0x8c, 0x00, 0x80, 0x5f, 0xa0,
    0x30, 0x00, 0x80, 0xff, 0x40, 0x1a, 0x50, 0x8c, 0xff, 0xff, 0x0f, 0xc0,
    0x00, 0xc0, 0x03, 0x00, 0x84, 0x1a, 0x20, 0x8c, 0x98, 0xcf, 0x1f, 0x8c,
};
static const struct kui_game_vga_span kui_vga_world_neverland_2_spans[] = {
    {0x1d03e4u, 4, kui_vga_plasma_us_cable, 0},
    {0x1e0c00u, 16, kui_vga_world_neverland_2_1e0c00, 0},
    {0x1e0f50u, 2, kui_vga_world_neverland_2_1e0f50, 0},
    {0x1ed4a4u, 2, kui_vga_plasma_us_mode, 0},
    {0x1f1740u, 340, kui_vga_video_routine, 0},
    {0x1f1894u, 36, kui_vga_world_neverland_2_1f1894, 0},
    {0x1f18b8u, 55, kui_vga_video_constants, 0},
    {0x1f18efu, 7, kui_vga_video_parameters, 0},
    {0x1f18f6u, 63, kui_vga_video_trailer, 0},
};

/* PERPETUAL BLUE (T27801M V1.003). Rolly (SEGA-SKY).
 * https://segasky.itch.io/yukyu-gensokyoku-3-perpetual-blue-vga
 * DCP SHA-256 667be77420a5ac7beb10140cd3a0837e98d65aa7eeaae76729a3d451364d1a11. */
static const struct kui_game_vga_span kui_vga_perpetual_blue_spans[] = {
    {0x06608au, 2, kui_vga_plasma_us_mode, 0},
    {0x066338u, 2, kui_vga_plasma_us_mode, 0},
};

#define KUI_GAME_VGA_JAPAN_PROFILES     {"T14402M", "V1.001", 'J', 1484260u, 0, NULL, 0, \
        "172a25b9761b3cea8149beb011b2301e1c7a5e1bc988ad95b7337c1157417aad", 0, \
        kui_vga_despiria_fixed_spans, sizeof(kui_vga_despiria_fixed_spans) / sizeof(kui_vga_despiria_fixed_spans[0])}, \
    {"T20101M", "V1.002", 'J', 947300u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("5952f8a2a3a881c6b34e2f0eb1bc5ff01239ddb87371048a5f25f5ec3ad2d920"), 0, \
        kui_vga_black_matrix_spans, sizeof(kui_vga_black_matrix_spans) / sizeof(kui_vga_black_matrix_spans[0])}, \
    {"HDR-0021", "V1.005", 'J', 1419652u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("546c88d9e393d9d07d864092fd8c35f2104ab5ac51a61127aabb20db0957837b"), 0, \
        kui_vga_kanipan_spans, sizeof(kui_vga_kanipan_spans) / sizeof(kui_vga_kanipan_spans[0])}, \
    {"T9506M", "V1.003", 'J', 1406052u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("1120ae4f04ceffb8e2f5649c57fd3d8cd949676bdd197a45c93e61396dca613f"), 0, \
        kui_vga_ddr_2nd_mix_spans, sizeof(kui_vga_ddr_2nd_mix_spans) / sizeof(kui_vga_ddr_2nd_mix_spans[0])}, \
    {"T9508M", "V1.000", 'J', 1455524u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("42e258e1870b133e51474672934fef2d8241dfdc96290be33831bc7dcf269781"), 0, \
        kui_vga_ddr_club_spans, sizeof(kui_vga_ddr_club_spans) / sizeof(kui_vga_ddr_club_spans[0])}, \
    {"T46501M", "V1.002", 'J', 1056740u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("6aafe456b2a3fdaa84d22aebee799cdcd7ef0a00198e6539e52786323cf63795"), 0, \
        kui_vga_dvine_luv_spans, sizeof(kui_vga_dvine_luv_spans) / sizeof(kui_vga_dvine_luv_spans[0])}, \
    {"6107357", "V1.002", 'J', 231456u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("1a2f6977048dc9cf89316e18cfd0aabd69bfcac776c3ac40cc06f7aa032b5b32"), 0, \
        kui_vga_jojo_trial_spans, sizeof(kui_vga_jojo_trial_spans) / sizeof(kui_vga_jojo_trial_spans[0])}, \
    {"T1205M", "V1.001", 'J', 239648u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("bc6548c161a2c2fad65be5ccd8c22184f031cfacb4a0987f262299be41572aa5"), 0, \
        kui_vga_jojo_jp_spans, sizeof(kui_vga_jojo_jp_spans) / sizeof(kui_vga_jojo_jp_spans[0])}, \
    {"T1206N", "V1.001", 'U', 234752u, 0, NULL, 0, \
        "02ead9c68aacf5e9153fedff30c4a141ba6e7842b6535c1d96d79485b30a622d", 0, \
        kui_vga_jojo_us_spans, sizeof(kui_vga_jojo_us_spans) / sizeof(kui_vga_jojo_us_spans[0])}, \
    {"T7007D  50", "V1.001", 'E', 234752u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("781763abffb9b0b58327fd0299f858fd2cd777117e5d9ad004a719488f9336d3"), 0, \
        kui_vga_jojo_pal_spans, sizeof(kui_vga_jojo_pal_spans) / sizeof(kui_vga_jojo_pal_spans[0])}, \
    {"T3107M", "V1.003", 'J', 1455396u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("926377a696856e8410a3f9daed34f7a431a1d115dbb261b5e445b33095c0a6bc"), 0, \
        kui_vga_last_blade_jp_spans, sizeof(kui_vga_last_blade_jp_spans) / sizeof(kui_vga_last_blade_jp_spans[0])}, \
    {"T7601M", "V1.003", 'J', 791744u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("afc8c9c3777a533fe8b038fa5cd6f8c5b3d04cc7b7834943d964b8eaf1cce13e"), 0, \
        kui_vga_mahjong_taikai_spans, sizeof(kui_vga_mahjong_taikai_spans) / sizeof(kui_vga_mahjong_taikai_spans[0])}, \
    {"T46505M", "V1.001", 'J', 855556u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("4105c677794fdc706fd039ca091d5927c95c04d50a50983d961afaa4621ce0d7"), 0, \
        kui_vga_nijyuei_spans, sizeof(kui_vga_nijyuei_spans) / sizeof(kui_vga_nijyuei_spans[0])}, \
    {"T1101M", "V1.002", 'J', 4706080u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("9f3c319c59245e481f0816e676aea89f873260fa87059b9f88d2594a85968882"), 0, \
        kui_vga_psychic_force_jp_spans, sizeof(kui_vga_psychic_force_jp_spans) / sizeof(kui_vga_psychic_force_jp_spans[0])}, \
    {"T-8106D-50", "V1.000", 'E', 4721056u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("782916095848ee5e6d20cc56fedd9aec88ac41c4a27d6d75fbe367116fba1803"), 0, \
        kui_vga_psychic_force_pal_spans, sizeof(kui_vga_psychic_force_pal_spans) / sizeof(kui_vga_psychic_force_pal_spans[0])}, \
    {"T40001M", "V1.007", 'J', 975748u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("02f4091a290ffbf611229ee2a1d6939d2afbca0712d8b028411189bdd1110568"), 0, \
        kui_vga_rune_caster_spans, sizeof(kui_vga_rune_caster_spans) / sizeof(kui_vga_rune_caster_spans[0])}, \
    {"T7602M", "V1.000", 'J', 1874400u, 0x4204b11eu, \
        kui_vga_nobunaga_shouseiroku_edits, sizeof(kui_vga_nobunaga_shouseiroku_edits) / sizeof(kui_vga_nobunaga_shouseiroku_edits[0]), \
        NULL, 0, NULL, 0}, \
    {"T7605M", "V1.001", 'J', 2814960u, 0xa8574e3du, \
        kui_vga_nobunaga_reppuuden_edits, sizeof(kui_vga_nobunaga_reppuuden_edits) / sizeof(kui_vga_nobunaga_reppuuden_edits[0]), \
        NULL, 0, NULL, 0}, \
    {"T7607M", "V1.000", 'J', 3287816u, 0xbcb8eb40u, \
        kui_vga_sangokushi_powerup_edits, sizeof(kui_vga_sangokushi_powerup_edits) / sizeof(kui_vga_sangokushi_powerup_edits[0]), \
        NULL, 0, NULL, 0}, \
    {"T7603M", "V1.000", 'J', 5422272u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("f71058097a28390060b60ad05b97c7290df72ae931e9e1054fa8353962599662"), 0, \
        kui_vga_sangokushi_spans, sizeof(kui_vga_sangokushi_spans) / sizeof(kui_vga_sangokushi_spans[0])}, \
    {"T43403M", "V1.004", 'J', 1309956u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("b990c0f0c37ae45a04681da6cee1eaabee057c1ddf7bc4bb5d9b5fcc7d8a6f6a"), 0, \
        kui_vga_shinri_spans, sizeof(kui_vga_shinri_spans) / sizeof(kui_vga_shinri_spans[0])}, \
    {"T41101M", "V1.000", 'J', 1424004u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("9b9bf1f29dc1ce8339ea0a11a64c4a99a5f8c39bed79245c1dfbdebd4ce5d854"), 0, \
        kui_vga_sunrise_spans, sizeof(kui_vga_sunrise_spans) / sizeof(kui_vga_sunrise_spans[0])}, \
    {"T22001M", "V1.006", 'J', 908448u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("c96d5e5a2cf49a01b5716f8d15a03d4ce2c31d02268ff3e3341654b71429f646"), 0, \
        kui_vga_tanaka_shogi_spans, sizeof(kui_vga_tanaka_shogi_spans) / sizeof(kui_vga_tanaka_shogi_spans[0])}, \
    {"T19701M", "V1.006", 'J', 2177440u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("d91b817d62bc3683555c95229e2ff5930269845348901e876a816b6599d73beb"), 0, \
        kui_vga_treasure_strike_spans, sizeof(kui_vga_treasure_strike_spans) / sizeof(kui_vga_treasure_strike_spans[0])}, \
    {"T5303M", "V1.001", 'J', 2248560u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("d26f2bc45930fe0f96aa928ea7e9c8d7d634e9ec544ea4370c56b8eec2092c6a"), 0, \
        kui_vga_world_neverland_2_spans, sizeof(kui_vga_world_neverland_2_spans) / sizeof(kui_vga_world_neverland_2_spans[0])}, \
    {"T27801M", "V1.003", 'J', 948228u, 0, NULL, 0, \
        KUI_VGA_TARGET_SHA256("43e5058fad3a76bc04084e7e223aa34d86f5d4ffeb4971eaa211b222e10a9e24"), 0, \
        kui_vga_perpetual_blue_spans, sizeof(kui_vga_perpetual_blue_spans) / sizeof(kui_vga_perpetual_blue_spans[0])}
