#include "lookup_tables.h"
#include "common.h"

#include <float.h>
#include <math.h>
#include <string.h>

#define TAU 6.28318530717958647692f

static float taylor_sin_cos(int is_sin, float x) {
    float fraction = is_sin ? x : 1.0f, fac = is_sin ? 1.0f : 0.0f;
    float neg_x2 = -(x * x), sum = 0.0f;
    for (int i = 0; i < 25; i++) {
        float term = fraction;
        fraction *= neg_x2;
        fraction /= (fac + 1.0f) * (fac + 2.0f);
        fac += 2.0f;
        sum += term;
    }
    return sum;
}

static void build_reciprocal_table(ByteBuffer *b, int n, int half, int num_frac, int out_frac) {
    for (int i = 0; i < n; i++) {
        int32_t v = float_to_fixed((float)(1 << num_frac) / ((float)i - (float)half), out_frac);
        buffer_append(b, &v, 4);
    }
}

static uint32_t float_bits(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

static float float_from_bits(uint32_t u) {
    float f;
    memcpy(&f, &u, 4);
    return f;
}

static int32_t exponent_of(float x) { return (int32_t)((float_bits(x) & 0x7F800000u) >> 23) - 127; }

static float set_exponent(float x, int32_t e) {
    return float_from_bits((float_bits(x) & ~0x7F800000u) | ((uint32_t)(e + 127) << 23));
}

static float copysign_bits(float x, float sign) {
    return float_from_bits((float_bits(x) & 0x7FFFFFFFu) | (float_bits(sign) & 0x80000000u));
}

static float approx_fract(float x) {
    int32_t e = exponent_of(x);
    if (e < 0) return x;
    uint32_t frac = (float_bits(x) << e) & 0x7FFFFFu;
    if (!frac) return copysign_bits(0.0f, x);
    uint32_t shift = (uint32_t)(__builtin_clz(frac) - 9) + 1;
    uint32_t normal = (frac << shift) & 0x7FFFFFu;
    return copysign_bits(float_from_bits(normal | (127u - shift) << 23), x);
}

static float approx_trunc(float x) {
    int32_t e = exponent_of(x);
    if (e < 0) return copysign_bits(0.0f, x);
    uint32_t frac = (float_bits(x) << e) & 0x7FFFFFu;
    if (!frac) return x;
    return float_from_bits(float_bits(x) & ~(frac >> e));
}

static float approx_ln(float x) {
    if (fabsf(x - 1.0f) < FLT_EPSILON) return 0.0f;
    int less = x < 1.0f;
    float w = less ? float_from_bits(0x7F000000u - float_bits(x)) : x;
    uint32_t e = (uint32_t)exponent_of(w);
    w = w / float_from_bits(float_bits(w) & 0x7F800000u);
    float poly = -1.7417939f + (2.8212026f + (-1.4699568f + (0.44717955f - 0.056570851f * w) * w) * w) * w;
    float r = (float)e * 0.693147180559945309417f + poly;
    return less ? -r : r;
}

static float approx_exp(float x) {
    if (x == 0.0f) return 1.0f;
    if (fabsf(x - 1.0f) < FLT_EPSILON) return 2.71828182845904523536f;
    if (fabsf(x - -1.0f) < FLT_EPSILON) return 1.0f / 2.71828182845904523536f;
    float y = x * 1.44269504088896340736f;
    float fr = approx_fract(y) * 0.693147180559945309417f;
    float tr = approx_trunc(y);
    float total = 1.0f;
    for (int i = 4; i >= 1; i--) total = 1.0f + ((fr / (float)i) * total);
    int64_t e = (int64_t)exponent_of(total) + float_to_i32_saturated(tr);
    if (e > INT32_MAX) e = INT32_MAX;
    if (e < -127) return 0.0f;
    if (e > 128) return INFINITY;
    return set_exponent(total, (int32_t)e);
}

static float approx_powf(float x, float n) { return approx_exp(n * approx_ln(x)); }

static float cubic_ease_out(float x) { return 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x); }

Tables tables_build(void) {
    Tables t = { 0 };

    for (int i = 0; i < 1024; i++) {
        float a = (float)i / 1024.0f * TAU;
        int16_t sc[2] = { float_to_i16_saturated(taylor_sin_cos(1, a) * 256.0f),
            float_to_i16_saturated(taylor_sin_cos(0, a) * 256.0f) };
        buffer_append(&t.sin_cos, sc, 4);
    }

    build_reciprocal_table(&t.recip8, 1 << 15, 1 << 14, 8, 8);
    build_reciprocal_table(&t.recip_clip, 1 << 15, 1 << 14, 6, 12);
    build_reciprocal_table(&t.recip_sprite, 1 << 11, 1 << 10, 8, 12);
    build_reciprocal_table(&t.recip_area, 1 << 15, 0, 8, 12);

    for (int i = 0; i < 1 << 16; i++) {
        int32_t v = float_to_fixed(64.0f / (float)i, 14);
        buffer_append(&t.project, &v, 4);
    }
    for (int i = 0; i < 1 << 15; i++) {
        int32_t v = float_to_fixed(256.0f / (float)i, 15);
        if ((v >> 7) > 256) v = 1 << 15;
        buffer_append(&t.delta, &v, 4);
    }
    for (int i = 0; i < 4096; i++) {
        int16_t v = float_to_i16_saturated(sqrtf((float)i) * 256.0f);
        buffer_append(&t.sqrt, &v, 2);
    }
    for (int x = 0; x < 64; x++)
        for (int y = 0; y < 64; y++) {
            int16_t v = float_to_i16_saturated(atan2f((float)x - 32.0f, (float)y - 32.0f) * 4096.0f);
            buffer_append(&t.atan2, &v, 2);
        }

    uint8_t zbin[2048];
    for (int i = 0; i < 2048; i++) {
        float bin = 128.0f - 128.0f * cubic_ease_out((float)i / 4.0f / 256.0f);
        zbin[i] = bin < 0.0f ? 0 : bin >= 128.0f ? 127 : (uint8_t)bin;
    }
    buffer_append(&t.zbin, zbin, sizeof zbin);
    for (int max_bin = 0; max_bin <= 128; max_bin++) {
        int32_t drop = INT32_MAX;
        if (max_bin)
            for (int i = 0; i < 2048; i++)
                if (zbin[i] < max_bin) {
                    drop = i << 6;
                    break;
                }
        buffer_append(&t.zdrop, &drop, 4);
    }

    static const float bases[4] = { 0.010f, 0.05f, 0.0000005f, 0.8f };
    for (int b = 0; b < 4; b++)
        for (int k = 0; k <= 32; k++) {
            int32_t v = float_to_fixed(approx_powf(bases[b], (float)k * (1.0f / 256.0f)), 8);
            buffer_append(&t.pow, &v, 4);
        }
    return t;
}
