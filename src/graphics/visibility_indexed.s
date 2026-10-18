    .syntax unified
    .arm
    .macro COLLECT_INDEXED name, direct
    .global \name
    .type \name, %function
\name:
    push    {r4-r11, lr}
    sub     sp, sp, #12
    mov     r11, r0
    ldr     r4, [r11, #20]
    mov     r4, r4, lsl #16
    add     r5, r11, #24
    ldmia   r5, {r5-r7}
    ldr     r0, [r11, #0]
    ldr     r1, [r11, #4]
    add     r1, r0, r1, lsl #1
    stmia   sp, {r0, r1}
1:  ldmia   sp, {r0, r1}
    cmp     r0, r1
    bhs     9f
    .if \direct
    ldr     r9, [r11, #56]
    ldr     r10, [r11, #60]
    cmp     r9, r10
    bhs     9f
    .endif
    ldrh    r9, [r0], #2
    str     r0, [sp]
    ldr     r10, [r11, #8]
    add     r10, r10, r9, lsl #1
    ldrh    r1, [r10]
    ldrh    r2, [r10, #2]
    cmp     r1, r2
    beq     1b
    mov     r3, r1
    ldr     r10, [r11, #16]
    add     r2, r10, r2, lsl #1
    add     r1, r10, r1, lsl #1
2:  ldrh    r9, [r1], #2
    add     r10, r3, r3, lsl #1
    ldr     lr, [r11, #72]
    add     lr, lr, r10, lsl #3
    ldrh    r10, [lr]
    add     r10, r10, r10, lsl #1
    ldr     r12, [r11, #68]
    add     r8, r12, r10, lsl #1
    ldrsh   r10, [r8]
    sub     r10, r10, r5
    ldrsb   r12, [lr, #21]
    mul     r0, r10, r12
    mov     r0, r0, asr #7
    ldrsh   r10, [r8, #2]
    sub     r10, r10, r6
    ldrsb   r12, [lr, #22]
    mul     r12, r10, r12
    add     r0, r0, r12, asr #7
    ldrsh   r10, [r8, #4]
    sub     r10, r10, r7
    ldrsb   r12, [lr, #23]
    mul     r12, r10, r12
    adds    r0, r0, r12, asr #7
    bpl     5f

    ldr     r12, [r11, #80]
    ldrsh   r0, [r8, r12]
    ldr     r12, [r11, #84]
    eor     r0, r0, r12
    sub     r0, r0, r12
    cmp     r0, r4, asr #16
    bgt     6f
7:
    .if \direct

    str     r9, [sp, #8]
    ldr     r0, [r11, #36]
    ldr     r8, [r11, #40]
    ldr     r10, [r11, #56]
    .irp k, 0, 6, 12
    ldrh    r12, [lr, #\k]
    ldrb    r9, [r0, r12]
    cmp     r9, r8
    strbne  r8, [r0, r12]
    strhne  r12, [r10], #2
    .endr
    str     r10, [r11, #56]
    ldr     r9, [sp, #8]
    .endif
    mov     r12, r9, lsr #5
    and     r9, r9, #31
    mov     r10, #1
    mov     r10, r10, lsl r9
    ldr     lr, [r11, #44]
    ldr     r0, [lr, r12, lsl #2]
    orr     r0, r0, r10
    str     r0, [lr, r12, lsl #2]
    ldr     r0, [r11, #48]
    cmp     r12, r0
    strlo   r12, [r11, #48]
    ldr     r0, [r11, #52]
    cmp     r12, r0
    strhi   r12, [r11, #52]
5:  add     r3, r3, #1
    cmp     r1, r2
    blo     2b
    b       1b
9:  add     sp, sp, #12
    pop     {r4-r11, lr}
    bx      lr

6:  ldr     r8, [r11, #68]
    .irp k, 6, 12
    ldrh    r10, [lr, #\k]
    add     r10, r10, r10, lsl #1
    add     r10, r8, r10, lsl #1
    ldr     r12, [r11, #80]
    ldrsh   r10, [r10, r12]
    ldr     r12, [r11, #84]
    eor     r10, r10, r12
    sub     r10, r10, r12
    cmp     r0, r10
    movgt   r0, r10
    .endr
    cmp     r0, #32768
    beq     7b
    cmp     r0, r4, asr #16
    ble     7b
    b       5b

    .size \name, . - \name
    .endm

    .section .visibilityoverlay, "ax", %progbits
    COLLECT_INDEXED geometry_collect_indexed, 0
    .section .visibilityoverlay_direct, "ax", %progbits
    COLLECT_INDEXED geometry_collect_indexed_direct, 1
