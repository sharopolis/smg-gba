#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define PHASES 16384
#define OUT_RATE 18157

static const double invpio2 = 6.36619772367581382433e-01;
static const double pio2_1 = 1.57079632673412561417e+00;
static const double pio2_1t = 6.07710050650619224932e-11;
static const double pio2_2 = 6.07710050630396597660e-11;
static const double pio2_2t = 2.02226624879595063154e-21;
static const double pio2_3 = 2.02226624871116645580e-21;
static const double pio2_3t = 8.47842766036889956997e-32;

static double ksin(double x, double y, int iy)
{
    const double s1 = -1.66666666666666324348e-01, s2 = 8.33333333332248946124e-03;
    const double s3 = -1.98412698298579493134e-04, s4 = 2.75573137070700676789e-06;
    const double s5 = -2.50507602534068634195e-08, s6 = 1.58969099521155010221e-10;
    double z = x * x, v = z * x, r = s2 + z * (s3 + z * (s4 + z * (s5 + z * s6)));
    if (!iy)
        return x + v * (s1 + z * r);
    return x - ((z * (0.5 * y - v * r) - y) - v * s1);
}

static double kcos(double x, double y)
{
    const double c1 = 4.16666666666666019037e-02, c2 = -1.38888888888741095749e-03;
    const double c3 = 2.48015872894767294178e-05, c4 = -2.75573143513906633035e-07;
    const double c5 = 2.08757232129817482790e-09, c6 = -1.13596475577881948265e-11;
    double z = x * x, r = z * (c1 + z * (c2 + z * (c3 + z * (c4 + z * (c5 + z * c6)))));
    double hz = 0.5 * z, w = 1.0 - hz;
    return w + (((1.0 - w) - hz) + (z * r - x * y));
}

static double xsin(double x)
{
    double ax = x < 0 ? -x : x, r;
    if (ax <= 7.85398163397448278999e-01) {
        r = ksin(ax, 0.0, 0);
    } else {
        double fn = floor(ax * invpio2 + 0.5);
        int n = (int)fn;
        double rr = ax - fn * pio2_1, w, t;
        t = rr;
        w = fn * pio2_2;
        rr = t - w;
        w = fn * pio2_2t - ((t - rr) - w);
        t = rr;
        w = fn * pio2_3;
        rr = t - w;
        w = fn * pio2_3t - ((t - rr) - w);
        double y0 = rr - w, y1 = (rr - y0) - w;
        switch (n & 3) {
        case 0:
            r = ksin(y0, y1, 1);
            break;
        case 1:
            r = kcos(y0, y1);
            break;
        case 2:
            r = -ksin(y0, y1, 1);
            break;
        default:
            r = -kcos(y0, y1);
            break;
        }
    }
    return x < 0 ? -r : r;
}

static double bessel_i0(double x)
{
    double t = x * x / 4, v = 1, term = 1;
    for (int k = 1; k < 200; k++) {
        term *= t / ((double)k * k);
        double nv = v + term;
        if (nv == v)
            break;
        v = nv;
    }
    return v;
}

typedef struct {
    int rate, len, alloc;
    float *bank;
} Bank;

static Bank banks[32];
static int nbanks;

static const double PI = 3.14159265358979323846;

static Bank *get_bank(int rate)
{
    for (int b = 0; b < nbanks; b++)
        if (banks[b].rate == rate)
            return &banks[b];
    Bank *bk = &banks[nbanks++];
    double factor = (double)OUT_RATE * 0.94 / rate;
    if (factor > 1.0)
        factor = 1.0;
    int len = (int)ceil(64 / factor);
    if (len < 1)
        len = 1;
    if (len > 1)
        len = (len + 1) & ~1;
    int alloc = (len + 7) & ~7;
    bk->rate = rate;
    bk->len = len;
    bk->alloc = alloc;
    bk->bank = calloc((size_t)(PHASES + 1) * alloc, sizeof(float));
    int center = (len - 1) / 2;
    double *tab = malloc(sizeof(double) * len);
    double norm = 0;
    for (int ph = 0; ph <= PHASES / 2; ph++) {
        double s = 0;
        if (factor == 1.0)
            s = xsin(PI * ph / PHASES) * ((center & 1) ? 1 : -1);
        for (int i = 0; i < len; i++) {
            double x = PI * ((double)(i - center) - (double)ph / PHASES) * factor;
            double y;
            if (x == 0)
                y = 1.0;
            else if (factor == 1.0)
                y = s / x;
            else
                y = xsin(x) / x;
            double w = 2.0 * x / (factor * len * PI);
            double a = 1 - w * w;
            if (a < 0)
                a = 0;
            y *= bessel_i0(9 * sqrt(a));
            tab[i] = y;
            s = -s;
            if (!ph)
                norm += y;
        }
        for (int i = 0; i < len; i++)
            bk->bank[(size_t)ph * alloc + i] = (float)(tab[i] * 1 / norm);
        for (int i = 0; i < len; i++)
            bk->bank[(size_t)(PHASES - ph) * alloc + (len - 1 - i)] = bk->bank[(size_t)ph * alloc + i];
    }
    free(tab);
    float *last = bk->bank + (size_t)PHASES * alloc;
    for (int i = alloc - 2; i >= 0; i--)
        last[i + 1] = bk->bank[i];
    last[0] = bk->bank[alloc - 1];
    return bk;
}

static long out_pos(long j, long inc)
{
    return (j * inc / OUT_RATE) / PHASES;
}

static long reflect_len(const Bank *bk, long n)
{
    long len = bk->len, inc = (long)bk->rate * PHASES, avail = n + (len - 1) / 2, j = 0;
    while (out_pos(j, inc) + len <= avail)
        j++;
    long cnt = avail - out_pos(j, inc);
    return ((cnt < len ? cnt : len) + 1) / 2;
}

long rs_count(long n, int rate)
{
    Bank *bk = get_bank(rate);
    long len = bk->len, inc = (long)rate * PHASES, avail = n + (len - 1) / 2 + reflect_len(bk, n), j = 0;
    while (out_pos(j, inc) + len <= avail)
        j++;
    return j;
}

static void rs_channel(const short *in, int stride, long n, Bank *bk, long nout, float *out)
{
    long len = bk->len, alloc = bk->alloc, refl = reflect_len(bk, n);
    long tot = len + n + refl;
    float *buf = calloc((size_t)tot + len + 8, sizeof(float));
    for (long i = 0; i < n; i++)
        buf[len + i] = in[i * stride] / 32768.0f;
    for (long k = 1; k <= len; k++)
        buf[len - k] = k < n ? buf[len + k] : 0;
    for (long j = 0; j < refl; j++)
        buf[len + n + j] = buf[len + n - 1 - j];
    long sidx = len, index = -(long)PHASES * ((len - 1) / 2);
    while (index < 0) {
        sidx--;
        index += PHASES;
    }
    long frac = 0;
    const long dinc = (long)bk->rate * PHASES, ddiv = dinc / OUT_RATE, dmod = dinc % OUT_RATE;
    const double inv = 1.0 / OUT_RATE;
    for (long o = 0; o < nout; o++) {
        const float *fl = bk->bank + (size_t)index * alloc;
        const float *src = buf + sidx;
        float val = 0, v2 = 0;
        for (long i = 0; i < len; i++) {
            val = fmaf(src[i], fl[i], val);
            v2 = fmaf(src[i], fl[i + alloc], v2);
        }
        val += (v2 - val) * inv * frac;
        out[o] = val;
        frac += dmod;
        index += ddiv;
        if (frac >= OUT_RATE) {
            frac -= OUT_RATE;
            index++;
        }
        while (index >= PHASES) {
            sidx++;
            index -= PHASES;
        }
    }
    free(buf);
}

long rs_run(const short *in, long n, int nch, int rate, float *out)
{
    Bank *bk = get_bank(rate);
    long nout = rs_count(n, rate);
    if (nch == 1) {
        rs_channel(in, 1, n, bk, nout, out);
        return nout;
    }
    float *ch = malloc(sizeof(float) * (size_t)nout * nch);
    for (int c = 0; c < nch; c++)
        rs_channel(in + c, nch, n, bk, nout, ch + (size_t)c * nout);
    const float h = (float)0.70710678118654752440;
    if (nch == 2) {
        for (long o = 0; o < nout; o++)
            out[o] = fmaf(ch[o], h, ch[nout + o] * h);
    } else {
        const float coef[4] = {h, h, 1.0f, 0.5f};
        for (long o = 0; o < nout; o++) {
            float v = 0;
            for (int c = 0; c < nch; c++)
                v = fmaf(ch[(size_t)c * nout + o], coef[c], v);
            out[o] = v;
        }
    }
    free(ch);
    return nout;
}

static double interp_src(const short *src, long nsrc, double x)
{
    if (x > (double)(nsrc - 1))
        return src[nsrc - 1];
    long j = (long)floor(x);
    if (j >= nsrc - 1)
        return src[nsrc - 1];
    if ((double)j == x)
        return src[j];
    double slope = ((double)src[j + 1] - (double)src[j]) / 1.0;
    return fma(slope, x - (double)j, (double)src[j]);
}

static double env_x(long k, int g32)
{
    if (g32)
        return (double)((float)k / 400.0f);
    return (double)k / 400.0;
}

static double interp_env(const double *gain, long ngain, double x, int g32)
{
    if (ngain == 1)
        return gain[0];
    double last = env_x(ngain - 1, g32);
    if (x >= last)
        return gain[ngain - 1];
    long j = (long)floor(x * 400.0);
    if (j > ngain - 2)
        j = ngain - 2;
    if (j < 0)
        j = 0;
    while (j + 1 < ngain - 1 && env_x(j + 1, g32) <= x)
        j++;
    while (j > 0 && env_x(j, g32) > x)
        j--;
    double xj = env_x(j, g32), xk = env_x(j + 1, g32);
    if (xj == x)
        return gain[j];
    double slope = (gain[j + 1] - gain[j]) / (xk - xj);
    return fma(slope, x - xj, gain[j]);
}

void render_part(const short *src, long nsrc, double step, int loop, long loop_start, long loop_end,
                 long out_len, const double *gain, long ngain, double rate, double vol, double *mix,
                 long total, int g32)
{
    double ll = (double)(loop_end - loop_start);
    for (long i = 0; i < out_len && i < total; i++) {
        double x = (double)i * step;
        if (loop && x >= (double)loop_end)
            x = (double)loop_start + fmod(x - (double)loop_start, ll);
        double y = interp_src(src, nsrc, x);
        double t = g32 ? (double)((float)i / (float)rate) : (double)i / rate;
        double g = interp_env(gain, ngain, t, g32);
        mix[i] += y * g * vol;
    }
}

long render_finish(double *mix, long n, int trim, short *out)
{
    double peak = 0;
    for (long i = 0; i < n; i++) {
        double a = fabs(mix[i]);
        if (a > peak)
            peak = a;
    }
    if (peak > 32767) {
        double s = 32767.0 / peak;
        for (long i = 0; i < n; i++)
            mix[i] *= s;
    }
    long a = 0, b = n;
    if (trim) {
        long first = -1, lastnz = -1;
        for (long i = 0; i < n; i++)
            if (fabs(mix[i]) > 40) {
                if (first < 0)
                    first = i;
                lastnz = i;
            }
        if (first >= 0) {
            a = first - 16 > 0 ? first - 16 : 0;
            b = lastnz + 1;
        }
    }
    for (long i = a; i < b; i++)
        out[i - a] = (short)mix[i];
    return b - a;
}

double peak_abs(const double *x, long n)
{
    double p = 0;
    for (long i = 0; i < n; i++) {
        double a = fabs(x[i]);
        if (a > p)
            p = a;
    }
    return p;
}

void scale(double *x, long n, double g)
{
    for (long i = 0; i < n; i++)
        x[i] *= g;
}

void add_at(double *dst, const double *src, long n)
{
    for (long i = 0; i < n; i++)
        dst[i] += src[i];
}

void f32_to_f64(const float *src, double *dst, long n)
{
    for (long i = 0; i < n; i++)
        dst[i] = src[i];
}

void fade_linspace(double *x, long f)
{
    if (f <= 1)
        return;
    double step = (0.0 - 1.0) / (double)(f - 1);
    for (long i = 0; i < f - 1; i++)
        x[i] *= (double)i * step + 1.0;
    x[f - 1] *= 0.0;
}

static unsigned __int128 pcg_state, pcg_inc;

static uint64_t pcg_next(void)
{
    const unsigned __int128 mult = ((unsigned __int128)2549297995355413924ULL << 64) | 4865540595714422341ULL;
    pcg_state = pcg_state * mult + pcg_inc;
    uint64_t hi = (uint64_t)(pcg_state >> 64), lo = (uint64_t)pcg_state;
    unsigned rot = (unsigned)(pcg_state >> 122);
    uint64_t v = hi ^ lo;
    return (v >> rot) | (v << ((64 - rot) & 63));
}

void pcg_seed(uint64_t s_hi, uint64_t s_lo, uint64_t i_hi, uint64_t i_lo)
{
    pcg_state = ((unsigned __int128)s_hi << 64) | s_lo;
    pcg_inc = ((unsigned __int128)i_hi << 64) | i_lo;
}

void quantise(const double *x, long n, signed char *q, long guide, const long *rpos, const int *rval,
              long nres)
{
    double *u = malloc(sizeof(double) * (size_t)(2 * n + 1));
    for (long k = 0; k < 2 * n; k++)
        u[k] = (double)(pcg_next() >> 11) * (1.0 / 9007199254740992.0);
    double e1 = 0, e2 = 0;
    long ri = 0;
    for (long i = 0; i < n; i++) {
        double d = (u[i] - u[n + i]) * 0.5;
        double want = x[i] * 127.0 - (1.5 * e1 - 0.6 * e2);
        double f = floor(want + d + 0.5);
        int v = f < -128 ? -128 : f > 127 ? 127 : (int)f;
        if (i < guide) {
            while (ri < nres && rpos[ri] < i)
                ri++;
            if (ri < nres && rpos[ri] == i)
                v += rval[ri];
        }
        e2 = e1;
        e1 = v - want;
        q[i] = (signed char)v;
    }
    free(u);
}
