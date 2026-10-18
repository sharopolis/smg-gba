#include "sound_codec.h"
#include "../../src/assets/asset_format.h"

#include <math.h>
#include <string.h>

static const int16_t sound_filters[16][2] = SOUND_FILTERS;

#define RS_HALF 24
#define RS_FINE 512
static double bessel_i0(double x) {
    double sum = 1, term = 1;
    for (int k = 1; k < 40; k++) {
        term *= (x / (2 * k)) * (x / (2 * k));
        sum += term;
    }
    return sum;
}

static size_t round_block(double v) {
    size_t r = (size_t)llround(v / SOUND_BLOCK) * SOUND_BLOCK;
    return r ? r : SOUND_BLOCK;
}

float *sound_resample(const int8_t *pcm, size_t n, size_t loop, int out_rate, int in_rate, size_t *out_n,
                      size_t *out_loop) {
    double ratio = (double)out_rate / in_rate;
    int looping = loop < n;
    if (!looping) loop = n;

    size_t L = loop ? round_block((double)loop * ratio) : 0;
    size_t P = round_block((double)(n - loop) * ratio);
    if (!looping) P = 0;
    size_t N = L + P;
    double cutoff = 0.5 * ratio * 0.92;
    double beta = 8.0;
    double *h = xmalloc(sizeof(double) * (RS_HALF * RS_FINE + 2));
    for (int i = 0; i <= RS_HALF * RS_FINE + 1; i++) {
        double t = (double)i / RS_FINE;
        double s = t == 0 ? 2 * cutoff : sin(2 * M_PI * cutoff * t) / (M_PI * t);
        double w = t >= RS_HALF ? 0
                                : bessel_i0(beta * sqrt(1 - (t / RS_HALF) * (t / RS_HALF))) / bessel_i0(beta);
        h[i] = s * w;
    }
    float *y = xcalloc(N ? N : 1, sizeof(float));
    for (size_t k = 0; k < N; k++) {
        double t = k < L ? (double)k * (double)loop / (double)L
                         : (double)loop + (double)(k - L) * (double)(n - loop) / (double)P;
        long c = (long)floor(t);
        double acc = 0;
        for (long j = c - RS_HALF + 1; j <= c + RS_HALF; j++) {
            double d = fabs(t - (double)j) * RS_FINE;
            size_t di = (size_t)d;
            if (di >= (size_t)RS_HALF * RS_FINE) continue;
            double f = d - (double)di, hv = h[di] * (1 - f) + h[di + 1] * f;
            long src = j;
            if (src < 0) continue;
            if (src >= (long)n) {
                if (!looping) continue;
                src = (long)loop + (src - (long)n) % (long)(n - loop);
            }
            acc += hv * pcm[src];
        }
        y[k] = (float)acc;
    }
    free(h);
    *out_n = N;
    *out_loop = looping ? L : N;
    return y;
}

typedef struct {
    int32_t s1, s2;
} History;

static double code_block(const double *t, History *hist, int f, int sh, int look, int8_t *res) {
    int32_t s1 = hist->s1, s2 = hist->s2;
    int32_t a = sound_filters[f][0], b = sound_filters[f][1];
    double sse = 0;
    for (int i = 0; i < SOUND_BLOCK; i++) {
        int32_t pred = (a * s1 + b * s2) >> 8;
        int c = (int)floor((t[i] - pred) / (double)(1 << sh));
        double best = HUGE_VAL;
        int best_r = 0;
        int32_t best_s = 0;
        for (int r = c - 1; r <= c + 2; r++) {
            if (r < -8 || r > 7) continue;
            int32_t s = pred + r * (1 << sh);
            if (s < -32768 || s > 32767) continue;
            double e = (double)s - t[i], cost = e * e;
            if (look && i + 1 < SOUND_BLOCK && cost < best) {
                int32_t p2 = (a * s + b * s1) >> 8;
                int c2 = (int)floor((t[i + 1] - p2) / (double)(1 << sh));
                double follow = HUGE_VAL;
                for (int r2 = c2; r2 <= c2 + 1; r2++) {
                    int rr = r2 < -8 ? -8 : r2 > 7 ? 7 : r2;
                    int32_t q = p2 + rr * (1 << sh);
                    if (q < -32768 || q > 32767) continue;
                    double e2 = (double)q - t[i + 1];
                    if (e2 * e2 < follow) follow = e2 * e2;
                }
                cost += follow;
            }
            if (cost < best) {
                best = cost;
                best_r = r;
                best_s = s;
            }
        }
        if (best == HUGE_VAL) return HUGE_VAL;
        double e = (double)best_s - t[i];
        sse += e * e;
        if (res) res[i] = (int8_t)best_r;
        s2 = s1;
        s1 = best_s;
    }
    hist->s1 = s1;
    hist->s2 = s2;
    return sse;
}

double sound_encode(ByteBuffer *b, const float *x, size_t n, size_t loop) {
    if (n % SOUND_BLOCK || (loop < n && loop % SOUND_BLOCK)) die("sound: length / loop not block aligned");
    SoundBlob head;
    memset(&head, 0, sizeof head);
    head.length = (uint32_t)n;
    head.loop_start = (uint32_t)(loop < n ? loop : n);
    size_t at = b->len;
    buffer_append(b, &head, sizeof head);

    double *t = xmalloc(sizeof(double) * (n ? n : 1));
    for (size_t i = 0; i < n; i++) {
        double v = (double)x[i] * 256.0 + 128.0;
        t[i] = v < -32768 ? -32768 : v > 32767 ? 32767 : v;
    }
    History hist = { 0, 0 };
    double total = 0;
    int8_t res[SOUND_BLOCK];
    for (size_t k = 0; k < n; k += SOUND_BLOCK) {
        if (k == loop) {
            head.loop_s1 = (int16_t)hist.s1;
            head.loop_s2 = (int16_t)hist.s2;
        }

        double cand_e[2] = { HUGE_VAL, HUGE_VAL };
        int cand[2][2] = { { 0, 12 }, { 0, 12 } };
        for (int f = 0; f < 16; f++) {

            int32_t a = sound_filters[f][0], bb = sound_filters[f][1];
            double s1 = hist.s1, s2 = hist.s2, peak = 0;
            for (int i = 0; i < SOUND_BLOCK; i++) {
                double r = fabs(t[k + i] - (a * s1 + bb * s2) / 256.0);
                if (r > peak) peak = r;
                s2 = s1;
                s1 = t[k + i];
            }
            int est = 0;
            while (est < 12 && 7.5 * (double)(1 << est) < peak) est++;
            for (int sh = est - 2; sh <= est + 1; sh++) {
                if (sh < 0 || sh > 12) continue;
                History h = hist;
                double e = code_block(t + k, &h, f, sh, 0, NULL);
                if (e < cand_e[0]) {
                    cand_e[1] = cand_e[0];
                    cand[1][0] = cand[0][0];
                    cand[1][1] = cand[0][1];
                    cand_e[0] = e;
                    cand[0][0] = f;
                    cand[0][1] = sh;
                } else if (e < cand_e[1]) {
                    cand_e[1] = e;
                    cand[1][0] = f;
                    cand[1][1] = sh;
                }
            }
        }
        double best = HUGE_VAL;
        int best_c = 0;
        for (int c = 0; c < 2; c++) {
            History h = hist;
            double e = code_block(t + k, &h, cand[c][0], cand[c][1], 1, NULL);
            if (e < best) {
                best = e;
                best_c = c;
            }
        }
        int f = cand[best_c][0], sh = cand[best_c][1];
        if (best == HUGE_VAL) {
            f = 0;
            sh = 12;
        }
        best = code_block(t + k, &hist, f, sh, 1, res);
        if (best == HUGE_VAL) die("sound: block %zu cannot be coded", k / SOUND_BLOCK);
        uint8_t block[SOUND_BLOCK_BYTES];
        block[0] = (uint8_t)(f << 4 | sh);
        for (int i = 0; i < SOUND_BLOCK / 2; i++)
            block[1 + i] = (uint8_t)((res[2 * i] & 15) | (res[2 * i + 1] & 15) << 4);
        buffer_append(b, block, sizeof block);
    }
    memcpy(b->data + at, &head, sizeof head);
    free(t);

    int8_t *out = xmalloc(n ? n : 1);
    sound_decode(b->data + at, out);
    for (size_t i = 0; i < n; i++) {
        double d = (double)out[i] - (double)x[i];
        total += d * d;
    }
    free(out);
    return total;
}

void sound_decode(const uint8_t *data, int8_t *out) {
    const SoundBlob *blob = (const SoundBlob *)(const void *)data;
    int32_t s1 = 0, s2 = 0;
    const uint8_t *p = blob->blocks;
    for (uint32_t k = 0; k < blob->length; k += SOUND_BLOCK, p += SOUND_BLOCK_BYTES) {
        int32_t a = sound_filters[p[0] >> 4][0], b = sound_filters[p[0] >> 4][1];
        int sh = p[0] & 15;
        for (int i = 0; i < SOUND_BLOCK; i++) {
            int r = (int8_t)(uint8_t)((p[1 + i / 2] >> (4 * (i & 1))) << 4) >> 4;
            int32_t s = ((a * s1 + b * s2) >> 8) + r * (1 << sh);
            s2 = s1;
            s1 = s;
            out[k + i] = (int8_t)(s >> 8);
        }
    }
}
