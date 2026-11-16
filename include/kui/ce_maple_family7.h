/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_CE_MAPLE_FAMILY7_H
#define KUI_CE_MAPLE_FAMILY7_H
/* The Windows CE kernels of maple.dll family 7 (docs/wince-vmu-driver-families.md):
 * Armada, Wild Metal, Biohazard 2 Value Plus and Hello Kitty Lovely Fruit Park.
 * They carry the same nk.exe build and the same maple.dll and mapledev.dll and
 * differ only in placement: where the kernel's data block lies, as a shift
 * from Armada's, and where each driver's ROM table entry and nominal base are.
 * tools/wince_family7_profiles.py derives these from each kernel, and
 * docs/evidence/wince-family7-profiles-2026-11-17.json records them.
 *
 * Plain integers: linker scripts read this header too. Armada is the default,
 * so every existing build keeps its values. */
#define KUI_CE_FAMILY7_ARMADA 0
#define KUI_CE_FAMILY7_WILD_METAL 1
#define KUI_CE_FAMILY7_BIOHAZARD2_VALUE_PLUS 2
#define KUI_CE_FAMILY7_HELLO_KITTY_LOVELY_FRUIT_PARK 3
#ifndef KUI_CE_FAMILY7_KERNEL
#define KUI_CE_FAMILY7_KERNEL KUI_CE_FAMILY7_ARMADA
#endif

#if KUI_CE_FAMILY7_KERNEL == KUI_CE_FAMILY7_ARMADA
#define KUI_CE_FAMILY7_DATA_SHIFT 0x0
#define KUI_CE_FAMILY7_MAPLE_BASE 0x01dd0000
#define KUI_CE_FAMILY7_MAPLE_TOC 0x8c141614
#define KUI_CE_FAMILY7_MAPLEDEV_BASE 0x01d20000
#define KUI_CE_FAMILY7_MAPLEDEV_TOC 0x8c1416f4
#elif KUI_CE_FAMILY7_KERNEL == KUI_CE_FAMILY7_WILD_METAL
#define KUI_CE_FAMILY7_DATA_SHIFT 0x200000
#define KUI_CE_FAMILY7_MAPLE_BASE 0x01b40000
#define KUI_CE_FAMILY7_MAPLE_TOC 0x8c341efc
#define KUI_CE_FAMILY7_MAPLEDEV_BASE 0x01a90000
#define KUI_CE_FAMILY7_MAPLEDEV_TOC 0x8c341fdc
#elif KUI_CE_FAMILY7_KERNEL == KUI_CE_FAMILY7_BIOHAZARD2_VALUE_PLUS
#define KUI_CE_FAMILY7_DATA_SHIFT (-0x10000)
#define KUI_CE_FAMILY7_MAPLE_BASE 0x01df0000
#define KUI_CE_FAMILY7_MAPLE_TOC 0x8c130d94
#define KUI_CE_FAMILY7_MAPLEDEV_BASE 0x01d60000
#define KUI_CE_FAMILY7_MAPLEDEV_TOC 0x8c130e34
#elif KUI_CE_FAMILY7_KERNEL == KUI_CE_FAMILY7_HELLO_KITTY_LOVELY_FRUIT_PARK
#define KUI_CE_FAMILY7_DATA_SHIFT 0x0
#define KUI_CE_FAMILY7_MAPLE_BASE 0x01dc0000
#define KUI_CE_FAMILY7_MAPLE_TOC 0x8c143628
#define KUI_CE_FAMILY7_MAPLEDEV_BASE 0x01d10000
#define KUI_CE_FAMILY7_MAPLEDEV_TOC 0x8c143708
#else
#error "Unknown maple.dll family 7 kernel"
#endif
#endif
