    .syntax unified
    .arm

    .section .crt0, "ax", %progbits
    .global _start
_start:
    b       reset
    .fill   156, 1, 0
    .ascii  "GALAXYADV   "
    .ascii  "GLXY"
    .ascii  "00"
    .byte   0x96, 0, 0
    .fill   7, 1, 0
    .byte   0, 0
    .fill   2, 1, 0

reset:
    mov     r0, #0x12
    msr     cpsr_c, r0
    ldr     sp, =0x03007FA0
    mov     r0, #0x1F
    msr     cpsr_c, r0
    ldr     sp, =0x03007F00

    mov     r12, #0x04000000
    ldr     r0, =0x4317
    add     r1, r12, #0x200
    strh    r0, [r1, #4]

    ldr     r0, =__iwram_lma
    ldr     r1, =__iwram_start
    ldr     r2, =__iwram_end
    bl      copy_words
    ldr     r0, =__ewram_lma
    ldr     r1, =__ewram_start
    ldr     r2, =__ewram_end
    bl      copy_words

    mov     r0, #0
    ldr     r1, =__bss_start
    ldr     r2, =__bss_end
    bl      fill_words
    mov     r0, #0
    ldr     r1, =__ewram_bss_start
    ldr     r2, =__ewram_bss_end
    bl      fill_words

    strh    r0, [r12, #-8]

    ldr     r0, =irq_handler
    str     r0, [r12, #-4]

    ldr     r0, =main
    mov     lr, pc
    bx      r0
    b       .
    .pool

copy_words:
    cmp     r1, r2
    ldrlo   r3, [r0], #4
    strlo   r3, [r1], #4
    blo     copy_words
    bx      lr

fill_words:
    cmp     r1, r2
    strlo   r0, [r1], #4
    blo     fill_words
    bx      lr

    .thumb
    .thumb_func
    .global display_wait_vblank
display_wait_vblank:
    swi     0x05
    bx      lr

    .arm
    .section .iwram.irq, "ax", %progbits
    .global irq_handler
irq_handler:
    mov     r0, #0x04000000
    ldr     r1, [r0, #0x200]
    and     r1, r1, r1, lsr #16
    add     r3, r0, #0x200
    strh    r1, [r3, #2]
    ldrh    r2, [r0, #-8]
    orr     r2, r2, r1
    strh    r2, [r0, #-8]

    tst     r1, #0x200
    beq     2f
    ldr     r3, =g_audio
    ldrh    r2, [r3, #36]
    add     r2, r2, #16
    cmp     r2, #256
    strhne  r2, [r3, #36]
    bne     2f
    stmfd   sp!, {r1, lr}
    mov     r0, #0
    ldr     r3, =audio_dma
    mov     lr, pc
    bx      r3
    ldmfd   sp!, {r1, lr}
2:
    tst     r1, #0x400
    beq     1f
    ldr     r3, =g_audio
    ldrh    r2, [r3, #42]
    add     r2, r2, #16
    cmp     r2, #288
    strhne  r2, [r3, #42]
    bne     1f
    stmfd   sp!, {r1, lr}
    mov     r0, #1
    ldr     r3, =audio_dma
    mov     lr, pc
    bx      r3
    ldmfd   sp!, {r1, lr}
1:
    tst     r1, #1
    bxeq    lr
    ldr     r3, =g_vblank_count
    ldr     r2, [r3]
    add     r2, r2, #1
    str     r2, [r3]
    stmfd   sp!, {lr}
    ldr     r3, =vblank_service_nested
    mov     lr, pc
    bx      r3
    ldmfd   sp!, {lr}
    mov     r0, #0x04000000
    ldr     r3, =g_input_latch_enabled
    ldr     r3, [r3]
    cmp     r3, #0
    bxeq    lr
    add     r2, r0, #0x100
    ldrh    r2, [r2, #0x30]
    mvn     r2, r2
    lsl     r2, r2, #22
    lsr     r2, r2, #22
    ldr     r3, =g_keys + 4
    ldrh    r1, [r3]
    orr     r1, r1, r2
    strh    r1, [r3]
    bx      lr
    .pool

    .section .text, "ax", %progbits
    .arm
    .global vblank_service_nested
vblank_service_nested:
    stmfd   sp!, {r4-r7, lr}
    mrs     r4, spsr
    mrs     r5, cpsr
    ldr     r6, =0x04000200
    ldrh    r7, [r6]
    ldrh    r2, [r6, #8]
    orr     r7, r7, r2, lsl #16
    and     r2, r7, #0x600
    strh    r2, [r6]
    mov     r2, #1
    strh    r2, [r6, #8]
    bic     r2, r5, #0x9f
    orr     r2, r2, #0x1f
    msr     cpsr_c, r2
    mov     r2, sp
    ldr     sp, =g_vblank_stack+512
    stmfd   sp!, {r2, lr}
    ldr     r3, =vblank_service
    mov     lr, pc
    bx      r3
    ldmfd   sp!, {r2, lr}
    mov     sp, r2
    msr     cpsr_c, r5
    msr     spsr_cxsf, r4
    mov     r2, r7, lsr #16
    strh    r2, [r6, #8]
    ldrh    r2, [r6]
    and     r2, r2, #0x600
    bic     r7, r7, #0x600
    orr     r2, r2, r7
    strh    r2, [r6]
    ldmfd   sp!, {r4-r7, lr}
    bx      lr
    .pool
