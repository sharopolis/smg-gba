    .syntax unified
    .arm
    .section .iwram.fastmath, "ax", %progbits

    .global isqrt32_arm
    .type isqrt32_arm, %function
isqrt32_arm:
    mov     r1, #3 << 30
    mov     r2, #1 << 30
    cmp     r0, r2
    subhs   r0, r0, r2
    adc     r2, r1, r2, lsl #1
    .set n, 1
    .rept 15
    cmp     r0, r2, ror #(2 * n)
    subhs   r0, r0, r2, ror #(2 * n)
    adc     r2, r1, r2, lsl #1
    .set n, n + 1
    .endr
    bic     r0, r2, #3 << 30
    bx      lr
    .size isqrt32_arm, . - isqrt32_arm

    .global recip30_arm
    .type recip30_arm, %function
recip30_arm:
    mov     r1, #1 << 30
    mov     r2, #0
    .set b, 16
    .rept 17
    cmp     r1, r0, lsl #b
    subhs   r1, r1, r0, lsl #b
    adc     r2, r2, r2
    .set b, b - 1
    .endr
    mov     r0, r2
    bx      lr
    .size recip30_arm, . - recip30_arm
