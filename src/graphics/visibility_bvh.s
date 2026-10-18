    .syntax unified
    .arm
    .section .iwram.render, "ax", %progbits

    .global geometry_collect_leaves
    .type geometry_collect_leaves, %function
geometry_collect_leaves:
    push    {r4-r11, lr}
    sub     sp, sp, #8
    mov     r11, r0

    ldr     r1, [r11, #64]
    cmp     r1, #0
    ldreq   r2, =0xE1A00000
    ldrne   r2, collect_skip_branch
    ldr     r3, =collect_point_patch
    str     r2, [r3]
    ldr     r4, [r11, #20]
    mov     r4, r4, lsl #16
    add     r5, r11, #24
    ldmia   r5, {r5-r7}
    ldr     r8, [r11, #40]
    ldr     r0, [r11, #0]
    ldr     r1, [r11, #4]
    add     r1, r0, r1, lsl #1
    stmia   sp, {r0, r1}
1:  ldmia   sp, {r0, r1}
    cmp     r0, r1
    bhs     9f
    ldr     r9, [r11, #56]
    ldr     r10, [r11, #60]
    cmp     r9, r10
    bhs     9f
    ldrh    r9, [r0], #2
    str     r0, [sp]
    ldr     r10, [r11, #8]
    add     r10, r10, r9, lsl #1
    ldrh    r1, [r10]
    ldrh    r2, [r10, #2]
    cmp     r1, r2
    beq     1b
    ldr     r10, [r11, #16]
    add     r3, r1, r1, lsl #2
    add     r2, r10, r2, lsl #1
    add     r1, r10, r1, lsl #1
    ldr     r10, [r11, #12]
    add     r3, r10, r3, lsl #2
2:  ldrh    r9, [r1], #2
    ldrsh   r10, [r3, #6]
    sub     r10, r10, r5
    ldrsh   r12, [r3, #12]
    mul     lr, r10, r12
    mov     r0, lr, asr #8
    ldrsh   r10, [r3, #8]
    sub     r10, r10, r6
    ldrsh   r12, [r3, #14]
    mul     lr, r10, r12
    add     r0, r0, lr, asr #8
    ldrsh   r10, [r3, #10]
    sub     r10, r10, r7
    ldrsh   r12, [r3, #16]
    mul     lr, r10, r12
    adds    r0, r0, lr, asr #8
    bpl     5f

    ldr     r12, [r11, #80]
    add     r10, r3, #6
    ldrsh   r0, [r10, r12]
    ldr     r12, [r11, #84]
    eor     r0, r0, r12
    sub     r0, r0, r12
    cmp     r0, r4, asr #16
    bgt     collect_axis_corners
collect_axis_back:

collect_point_patch:
    mov     r0, r0
    ldr     r10, [r11, #36]
    ldr     lr, [r11, #56]
    .irp k, 0, 2, 4
    ldrh    r12, [r3, #\k]
    ldrb    r0, [r10, r12]
    cmp     r0, r8
    strbne  r8, [r10, r12]
    strhne  r12, [lr], #2
    .endr
    str     lr, [r11, #56]
collect_big_back:

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
5:  add     r3, r3, #20
    cmp     r1, r2
    blo     2b
    b       1b
9:  add     sp, sp, #8
    pop     {r4-r11, lr}
    bx      lr

collect_axis_corners:
    ldr     lr, [r11, #68]
    .irp k, 2, 4
    ldrh    r10, [r3, #\k]
    add     r10, r10, r10, lsl #1
    add     r10, lr, r10, lsl #1
    ldr     r12, [r11, #80]
    ldrsh   r10, [r10, r12]
    ldr     r12, [r11, #84]
    eor     r10, r10, r12
    sub     r10, r10, r12
    cmp     r0, r10
    movgt   r0, r10
    .endr
    cmp     r0, #32768
    beq     collect_axis_back
    cmp     r0, r4, asr #16
    ble     collect_axis_back
    b       5b

collect_skip_branch:
    .word   0xEA000000 | (((collect_big_back - collect_point_patch - 8) >> 2) & 0xFFFFFF)
    .pool
    .size geometry_collect_leaves, . - geometry_collect_leaves
    .global geometry_collect_leaves_end
geometry_collect_leaves_end:

    .equ CULL_DOUBLE_CENTER, 0
    .equ CULL_DOUBLE_EXTENT, 12
    .equ CULL_NODE_STACK, 24
    .equ CULL_STACK_SIZE, 24 + 32 * 8

    .global geometry_cull_chunks
    .type geometry_cull_chunks, %function
geometry_cull_chunks:
    push    {r4-r11, lr}
    sub     sp, sp, #CULL_STACK_SIZE
    mov     r11, r0
    mov     r0, #0
    mov     r2, #31
    mov     r3, #0
    mov     r4, #0
    str     r4, [r11, #168]
1:  ldr     r4, [r11, #4]
    cmp     r0, r4
    bhs     9f
2:  cmp     r3, #0
    beq     3f
    add     r4, sp, #CULL_NODE_STACK
    add     r4, r4, r3, lsl #3
    ldr     r5, [r4, #-8]
    cmp     r0, r5
    blo     3f
    ldr     r2, [r4, #-4]
    sub     r3, r3, #1
    b       2b
3:  ldr     r1, [r11, #0]
    add     r1, r1, r0, lsl #5
    ldrsh   r12, [r1, #0]
    ldrsh   lr, [r1, #6]
    add     r4, r12, lr
    sub     r7, lr, r12
    ldrsh   r12, [r1, #2]
    ldrsh   lr, [r1, #8]
    add     r5, r12, lr
    sub     r8, lr, r12
    ldrsh   r12, [r1, #4]
    ldrsh   lr, [r1, #10]
    add     r6, r12, lr
    sub     r9, lr, r12
    stmia   sp, {r4-r9}
    mov     r10, r2
    mov     r9, #1
    add     r8, r11, #8
4:  tst     r2, r9
    beq     7f
    ldmia   sp, {r4-r6}
    ldmia   r8, {r7, r12, lr}
    mul     r7, r4, r7
    mla     r7, r5, r12, r7
    mla     r7, r6, lr, r7
    add     r4, sp, #CULL_DOUBLE_EXTENT
    ldmia   r4, {r4-r6}
    ldr     r12, [r8, #12]
    mul     lr, r4, r12
    ldr     r12, [r8, #16]
    mla     lr, r5, r12, lr
    ldr     r12, [r8, #20]
    mla     lr, r6, r12, lr
    mov     r12, r7, asr #1
    mov     lr, lr, asr #1
    ldr     r6, [r8, #24]
    sub     r4, r12, lr
    add     r4, r6, r4, asr #8
    add     r5, r12, lr
    add     r5, r6, r5, asr #8
    cmp     r9, #16
    beq     5f
    cmp     r4, #64
    bgt     8f
    cmn     r5, #64
    biclt   r10, r10, r9
    b       7f
5:  ldr     r6, [r11, #148]
    sub     r4, r4, #64
    cmp     r4, r6
    bge     8f
    ldr     r6, [r11, #172]
    cmp     r6, #0
    beq     12f
    cmp     r5, r6
    blt     8f
    b       7f
12: ldr     r6, [r11, #148]
    add     r5, r5, #64
    cmp     r5, r6
    biclt   r10, r10, #16
7:  add     r8, r8, #28
    mov     r9, r9, lsl #1
    cmp     r9, #32
    blo     4b

    ldr     r4, [r1, #28]
    mvn     r5, #0x80000000
    cmp     r4, r5
    beq     6f
    ldrsh   r5, [r1, #22]
    ldr     r6, [r11, #152]
    sub     r5, r6, r5
    ldrsh   r6, [r1, #24]
    ldr     r7, [r11, #156]
    sub     r6, r7, r6
    ldrsh   r7, [r1, #26]
    ldr     r8, [r11, #160]
    sub     r7, r8, r7
    mov     r8, #0x20000
    cmp     r5, r8
    bge     6f
    cmn     r5, r8
    ble     6f
    cmp     r6, r8
    bge     6f
    cmn     r6, r8
    ble     6f
    cmp     r7, r8
    bge     6f
    cmn     r7, r8
    ble     6f
    ldrsb   r8, [r1, #16]
    mul     r12, r8, r5
    ldrsb   r8, [r1, #17]
    mla     r12, r8, r6, r12
    ldrsb   r8, [r1, #18]
    mla     r12, r8, r7, r12
    cmp     r5, #0
    rsblt   r5, r5, #0
    cmp     r6, #0
    rsblt   r6, r6, #0
    cmp     r7, #0
    rsblt   r7, r7, #0
    ldrb    r8, [r1, #19]
    mul     lr, r8, r5
    ldrb    r8, [r1, #20]
    mla     lr, r8, r6, lr
    ldrb    r8, [r1, #21]
    mla     lr, r8, r7, lr
    add     r12, r12, lr
    add     r4, r4, r12, lsl #1
    cmn     r4, #768
    ble     8f

6:  ldrh    r4, [r1, #14]
    mvn     r5, #0
    cmp     r4, r5, lsr #16
    beq     10f
    ldr     r5, [r11, #164]
    ldr     r6, [r11, #168]
    add     r7, r5, r6, lsl #1
    strh    r4, [r7]
    add     r6, r6, #1
    str     r6, [r11, #168]
    add     r0, r0, #1
    b       1b
10: cmp     r10, r2
    beq     11f
    add     r4, sp, #CULL_NODE_STACK
    add     r4, r4, r3, lsl #3
    ldrh    r5, [r1, #12]
    str     r5, [r4]
    str     r2, [r4, #4]
    add     r3, r3, #1
    mov     r2, r10
11: add     r0, r0, #1
    b       1b
8:  ldrh    r0, [r1, #12]
    b       1b
9:  ldr     r0, [r11, #168]
    add     sp, sp, #CULL_STACK_SIZE
    pop     {r4-r11, lr}
    bx      lr
    .size geometry_cull_chunks, . - geometry_cull_chunks

    .global geometry_claim_points
    .type geometry_claim_points, %function
geometry_claim_points:
    push    {r4-r11, lr}
    mov     lr, r0
    ldmia   lr, {r0-r3}
    add     r1, r0, r1, lsl #1
    ldr     r5, [lr, #20]
    ldr     r6, [lr, #24]
    ldr     r7, [lr, #28]
    ldr     r8, [lr, #32]
    ldr     r9, [lr, #36]
    ldr     r10, [lr, #40]
    push    {r0, lr}
1:  cmp     r0, r1
    bhs     9f
    ldr     r11, [sp, #4]
    ldr     r11, [r11, #44]
    cmp     r10, r11
    bhs     9f
    ldrh    r4, [r0], #2
    add     r4, r4, r4, lsl #1
    add     r4, r2, r4, lsl #3
    .irp k, 0, 6, 12
    ldrh    r12, [r4, #\k]
    add     r11, r3, r12, lsl #1
    ldrh    r11, [r11]
    ldrb    lr, [r5, r11]
    cmp     lr, r8
    blne    claim_leaf
    add     lr, r6, r11, lsl #1
    ldrsh   lr, [lr]
    add     lr, lr, r12
    add     lr, r7, lr, lsl #4
    ldrb    r11, [lr, #13]
    cmp     r11, r8
    strbne  r8, [lr, #13]
    strhne  r12, [r9], #2
    .endr
    b       1b
9:  pop     {r2, lr}
    str     r9, [lr, #36]
    str     r10, [lr, #40]
    sub     r0, r0, r2
    mov     r0, r0, lsr #1
    pop     {r4-r11, lr}
    bx      lr
claim_leaf:
    push    {r0, r1, lr}
    strb    r8, [r5, r11]
    ldr     lr, [sp, #16]
    ldr     lr, [lr, #16]
    add     lr, lr, r11, lsl #1
    ldrh    r0, [lr]
    ldrh    r1, [lr, #2]
    sub     r1, r1, r0
    sub     r0, r10, r0
    add     lr, r6, r11, lsl #1
    strh    r0, [lr]
    add     r10, r10, r1
    pop     {r0, r1, lr}
    bx      lr
    .size geometry_claim_points, . - geometry_claim_points
