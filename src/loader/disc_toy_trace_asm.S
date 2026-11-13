/* SPDX-License-Identifier: GPL-3.0-only */
#define JOIN_(a,b) a##b
#define JOIN(a,b) JOIN_(a,b)
#define C(name) JOIN(__USER_LABEL_PREFIX__,name)

    .section .exports,"a"
    .long C(kui_disc_toy_gd_proxy),C(kui_disc_toy_finish),C(kui_disc_toy_config)
    .long __disc_toy_trace_binary_end,__disc_toy_trace_bss_end,C(kui_disc_toy_finish)

/* Keep the firmware call on its original stack and with the complete caller
 * context. Only the outermost tracked call gets a return-address relay. */
    .macro TRACE_SAVE
    mov.l r0,@-r15
    mov.l r1,@-r15
    mov.l r2,@-r15
    mov.l r3,@-r15
    mov.l r4,@-r15
    mov.l r5,@-r15
    mov.l r6,@-r15
    mov.l r7,@-r15
    mov.l r8,@-r15
    mov.l r9,@-r15
    mov.l r10,@-r15
    mov.l r11,@-r15
    mov.l r12,@-r15
    mov.l r13,@-r15
    mov.l r14,@-r15
    sts.l pr,@-r15
    stc.l sr,@-r15
    stc.l vbr,@-r15
    stc.l gbr,@-r15
    sts.l mach,@-r15
    sts.l macl,@-r15
    stc sr,r0
    or #0xf0,r0
    ldc r0,sr
    .endm

    .macro TRACE_RESTORE
    mov r8,r15
    lds.l @r15+,macl
    lds.l @r15+,mach
    ldc.l @r15+,gbr
    ldc.l @r15+,vbr
    add #4,r15
    lds.l @r15+,pr
    mov.l @r15+,r14
    mov.l @r15+,r13
    mov.l @r15+,r12
    mov.l @r15+,r11
    mov.l @r15+,r10
    mov.l @r15+,r9
    mov.l @r15+,r8
    mov.l @r15+,r7
    mov.l @r15+,r6
    mov.l @r15+,r5
    mov.l @r15+,r4
    mov.l @r15+,r3
    mov.l @r15+,r2
    mov.l @r15+,r1
    mov r15,r0
    add #-64,r0
    mov.l @r0,r0
    ldc r0,sr
    .endm

    .text
    .align 2
    .global C(kui_disc_toy_gd_proxy)
C(kui_disc_toy_gd_proxy):
    /* EXEC and every unrelated operation tail-delegate with only two
     * temporary words. Native firmware still receives the original SR. */
    mov.l r0,@-r15
    stc.l sr,@-r15
    mov r7,r0
    cmp/eq #0,r0
    bt .Ltrack
    cmp/eq #1,r0
    bt .Ltrack
    ldc.l @r15+,sr
    mov.l .Lconfig,r0
    mov.l @r0,r0
    jmp @r0
    mov.l @r15+,r0
.Ltrack:
    ldc.l @r15+,sr
    mov.l @r15+,r0
    TRACE_SAVE
    mov r15,r8
    mov.l .Lactive,r9
    mov.l @r9,r0
    tst r0,r0
    bf .Ldelegate
    mov.l .Lstack,r15
    mov r8,r4
    mov.l .Lbefore,r0
    jsr @r0
    nop
.Ldelegate:
    TRACE_RESTORE
    mov.l .Lconfig,r0
    mov.l @r0,r0
    jmp @r0
    mov.l @r15+,r0

    .align 2
    .global C(kui_disc_toy_gd_return)
C(kui_disc_toy_gd_return):
    TRACE_SAVE
    mov r15,r8
    mov.l .Lstack,r15
    mov r8,r4
    mov.l .Lafter,r0
    jsr @r0
    nop
    TRACE_RESTORE
    rts
    mov.l @r15+,r0

    .align 2
    .global C(kui_disc_toy_finish)
C(kui_disc_toy_finish):
    stc sr,r0
    or #0xf0,r0
    ldc r0,sr
    mov.l .Lstack,r15
    mov.l .Lterminal,r0
    jmp @r0
    nop
    .align 2
.Lactive: .long C(kui_disc_toy_gd_active)
.Lconfig: .long C(kui_disc_toy_config)
.Lstack: .long __disc_toy_trace_stack_top
.Lbefore: .long C(kui_disc_toy_gd_before)
.Lafter: .long C(kui_disc_toy_gd_after)
.Lterminal: .long C(kui_disc_toy_terminal)
