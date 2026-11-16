/* SPDX-License-Identifier: GPL-3.0-only */
#define JOIN_(a,b) a##b
#define JOIN(a,b) JOIN_(a,b)
#define C(name) JOIN(__USER_LABEL_PREFIX__,name)
#include "kui/toy_pilot_boot.h"

/* Bootstrap has reached the verified original heap-init call. The known
 * caller has IMASK15. Run the native initializer on its original stack,
 * then keep its post-call state while our installer uses a high stack. */
    .text
    .align 2
    .global C(kui_disc_toy_heap_hook)
C(kui_disc_toy_heap_hook):
    mov.l .Lsaved,r0
    mov.l r4,@(4,r0)
    mov.l r5,@(8,r0)
    mov.l r15,@(12,r0)
    sts pr,r1
    mov.l r1,@r0
    mov.l .Lnative,r3
    jsr @r3
    nop
    sts.l mach,@-r15
    sts.l macl,@-r15
    stc.l sr,@-r15
    mov.l r0,@-r15
    mov.l r1,@-r15
    mov.l r2,@-r15
    mov.l r3,@-r15
    mov.l r4,@-r15
    mov.l r5,@-r15
    mov.l r6,@-r15
    mov.l r7,@-r15
    mov.l r8,@-r15
    mov.l .Lsaved,r4
    mov.l @r4,r0
    mov.l r0,@-r15
    mov r15,r8
    mov.l .Lstage_stack,r15
    mov.l .Linstall,r0
    jsr @r0
    nop
    mov r8,r15
    lds.l @r15+,pr
    mov.l @r15+,r8
    mov.l @r15+,r7
    mov.l @r15+,r6
    mov.l @r15+,r5
    mov.l @r15+,r4
    mov.l @r15+,r3
    mov.l @r15+,r2
    mov.l @r15+,r1
    mov.l @r15+,r0
    ldc.l @r15+,sr
    lds.l @r15+,macl
    lds.l @r15+,mach
    rts
    nop
    .align 2
.Lsaved: .long C(kui_disc_toy_heap_call)
.Lnative: .long KUI_TOY_BOOT_HEAP_INIT
.Linstall: .long C(kui_disc_toy_install)
.Lstage_stack: .long 0x8cff0000
