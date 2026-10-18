#include <math.h>
#include <stdint.h>

#define PRECISION_BITS (32 - 8 - 2)

static double clip01(double v)
{
    return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

static long wrap_coord(long c, long n, int mode)
{
    if (mode == 1) {
        long m = c % n;
        return m < 0 ? m + n : m;
    }
    if (mode == 2) {
        long m = c % (2 * n);
        if (m < 0) {
            m += 2 * n;
        }
        return m >= n ? 2 * n - 1 - m : m;
    }
    return c < 0 ? 0 : (c > n - 1 ? n - 1 : c);
}

static long wrap_index(long a, long n)
{
    long m = a % n;
    return m < 0 ? m + n : m;
}

static double bilerp(const double *q, int stride, int k, double s, double t)
{
    return (1.0 - s) * (1.0 - t) * q[0 * stride + k] + s * (1.0 - t) * q[1 * stride + k] +
           (1.0 - s) * t * q[2 * stride + k] + s * t * q[3 * stride + k];
}

void draw_quad(double *canvas, int W, int xmin, int ymin, int xmax, int ymax, const double *o,
               const double *ai, const uint8_t *tex, int tw, int th, const double *uvs, const double *vcols,
               int ws, int wt, const double *ca, const double *cb, const uint8_t *mask, int mw, int mh,
               const double *muvs, int mws, int mwt, int additive, int mask_invert)
{
    for (int y = ymin; y < ymax; y++) {
        for (int x = xmin; x < xmax; x++) {
            double px = ((double)x + 0.5) - o[0];
            double py = ((double)y + 0.5) - o[1];
            double s = ai[0] * px + ai[1] * py;
            double t = ai[2] * px + ai[3] * py;
            if (!(s >= 0.0 && s < 1.0 && t >= 0.0 && t < 1.0)) {
                continue;
            }
            double col[4];
            for (int k = 0; k < 4; k++) {
                col[k] = bilerp(vcols, 4, k, s, t) / 255.0;
            }
            if (tex) {
                double u0 = bilerp(uvs, 2, 0, s, t), v0 = bilerp(uvs, 2, 1, s, t);
                long u = wrap_coord((long)floor(u0 * tw), tw, ws);
                long v = wrap_coord((long)floor(v0 * th), th, wt);
                const uint8_t *p = tex + (v * tw + u) * 4;
                double tc[4];
                for (int k = 0; k < 4; k++) {
                    tc[k] = p[k] / 255.0;
                    if (ca) {
                        tc[k] = ca[k] + (cb[k] - ca[k]) * tc[k];
                    }
                    col[k] = tc[k] * col[k];
                }
                if (mask) {
                    double mu0 = bilerp(muvs, 2, 0, s, t), mv0 = bilerp(muvs, 2, 1, s, t);
                    long mu = wrap_coord((long)floor(mu0 * mw), mw, mws);
                    long mv = wrap_coord((long)floor(mv0 * mh), mh, mwt);
                    double mval = mask[(mv * mw + mu) * 4 + 3] / 255.0;
                    col[3] *= mask_invert ? (1.0 - mval) : mval;
                }
            }
            for (int k = 0; k < 4; k++) {
                col[k] = clip01(col[k]);
            }
            double *d = canvas + ((long)y * W + x) * 4;
            if (additive) {
                double add[3], rgb[3], mx;
                for (int k = 0; k < 3; k++) {
                    add[k] = col[k] * col[3];
                    rgb[k] = d[k] * d[3] + add[k];
                }
                mx = add[0];
                if (add[1] > mx) {
                    mx = add[1];
                }
                if (add[2] > mx) {
                    mx = add[2];
                }
                mx = clip01(mx);
                double oa = d[3] > mx ? d[3] : mx;
                double den = oa > 1e-6 ? oa : 1e-6;
                for (int k = 0; k < 3; k++) {
                    d[k] = clip01(rgb[k] / den);
                }
                d[3] = oa;
                continue;
            }
            double a = col[3];
            double oa = a + d[3] * (1.0 - a);
            double den = oa > 1e-6 ? oa : 1e-6;
            for (int k = 0; k < 3; k++) {
                d[k] = oa > 0.0 ? (col[k] * a + d[k] * d[3] * (1.0 - a)) / den : 0.0;
            }
            d[3] = oa;
        }
    }
}

void blit_glyph(double *img, int iw, int ih, const uint8_t *cell, int cell_stride, int gh_px, int gw_px,
                const int *yi, const int *xi, const double *ca, const double *cb, const double *top,
                const double *bot, int x0, int y0)
{
    int xa = x0 > 0 ? x0 : 0, ya = y0 > 0 ? y0 : 0;
    int xb = x0 + gw_px < iw ? x0 + gw_px : iw, yb = y0 + gh_px < ih ? y0 + gh_px : ih;
    double den_r = (double)(gh_px - 1 > 1 ? gh_px - 1 : 1);
    for (int y = ya; y < yb; y++) {
        int gy = y - y0;
        double r = (double)gy / den_r;
        double grad[4];
        for (int k = 0; k < 4; k++) {
            grad[k] = top[k] + (bot[k] - top[k]) * r;
        }
        for (int x = xa; x < xb; x++) {
            int gx = x - x0;
            const uint8_t *p = cell + (yi[gy] * cell_stride + xi[gx]) * 4;
            double src[4];
            for (int k = 0; k < 4; k++) {
                double g = p[k] / 255.0;
                if (ca) {
                    g = ca[k] + (cb[k] - ca[k]) * g;
                }
                src[k] = g * grad[k];
            }
            double *d = img + ((long)y * iw + x) * 4;
            double a = src[3];
            double oa = a + d[3] * (1.0 - a);
            double den = oa > 1e-6 ? oa : 1e-6;
            for (int k = 0; k < 3; k++) {
                d[k] = oa > 0.0 ? (src[k] * a + d[k] * d[3] * (1.0 - a)) / den : 0.0;
            }
            d[3] = oa;
        }
    }
}

void over(double *dst, int W, int H, const double *src, int w, int h, int x, int y)
{
    int xa = x > 0 ? x : 0, ya = y > 0 ? y : 0;
    int xb = x + w < W ? x + w : W, yb = y + h < H ? y + h : H;
    for (int yy = ya; yy < yb; yy++) {
        for (int xx = xa; xx < xb; xx++) {
            const double *s = src + ((long)(yy - y) * w + (xx - x)) * 4;
            double *d = dst + ((long)yy * W + xx) * 4;
            double a = s[3];
            double oa = a + d[3] * (1.0 - a);
            double den = oa > 1e-6 ? oa : 1e-6;
            for (int k = 0; k < 3; k++) {
                d[k] = oa > 0.0 ? (s[k] * a + d[k] * d[3] * (1.0 - a)) / den : 0.0;
            }
            d[3] = oa;
        }
    }
}

void to_u8(const double *in, uint8_t *out, long n)
{
    for (long i = 0; i < n; i++) {
        out[i] = (uint8_t)(long)(in[i] * 255.0);
    }
}

int bbox(const double *c, int W, int H, double thr, int *out)
{
    int x0 = W, y0 = H, x1 = -1, y1 = -1;
    for (int y = 0; y < H; y++) {
        const double *row = c + (long)y * W * 4;
        for (int x = 0; x < W; x++) {
            if (row[x * 4 + 3] > thr) {
                if (x < x0) {
                    x0 = x;
                }
                if (x > x1) {
                    x1 = x;
                }
                if (y < y0) {
                    y0 = y;
                }
                y1 = y;
            }
        }
    }
    out[0] = x0;
    out[1] = y0;
    out[2] = x1 + 1;
    out[3] = y1 + 1;
    return x1 >= 0;
}

static double dot_f32(const float *p, const double *k, int n)
{
    double t = 0.0, s[8];
    int i = 0;
    if (n >= 8) {
        for (int l = 0; l < 8; l++) {
            s[l] = 0.0;
        }
        for (; i + 8 <= n; i += 8) {
            for (int l = 0; l < 8; l++) {
                s[l] = fma((double)p[i + l], k[i + l], s[l]);
            }
        }
        t = s[0];
        for (int l = 1; l < 8; l++) {
            t = t + s[l];
        }
    }
    if (n - i >= 4) {
        s[0] = t;
        s[1] = s[2] = s[3] = 0.0;
        for (; i + 4 <= n; i += 4) {
            for (int l = 0; l < 4; l++) {
                s[l] = fma((double)p[i + l], k[i + l], s[l]);
            }
        }
        t = ((s[0] + s[1]) + s[2]) + s[3];
    }
    for (; i < n; i++) {
        t = fma((double)p[i], k[i], t);
    }
    return t;
}

void hpass_f32(const float *in, int w, int rows, int yoff, float *out, int w2, const int *bounds,
               const double *kk, int ksize)
{
    for (int yy = 0; yy < rows; yy++) {
        for (int xx = 0; xx < w2; xx++) {
            int xmin = bounds[xx * 2], xmax = bounds[xx * 2 + 1];
            const float *p = in + (long)(yy + yoff) * w + xmin;
            out[(long)yy * w2 + xx] = (float)dot_f32(p, kk + (long)xx * ksize, xmax);
        }
    }
}

void vpass_f32(const float *in, int w, int h2, float *out, const int *bounds, const double *kk, int ksize)
{
    for (int yy = 0; yy < h2; yy++) {
        int ymin = bounds[yy * 2], ymax = bounds[yy * 2 + 1];
        const double *k = kk + (long)yy * ksize;
        for (int xx = 0; xx < w; xx++) {
            double ss = 0.0;
            for (int y = 0; y < ymax; y++) {
                ss = fma((double)in[(long)(y + ymin) * w + xx], k[y], ss);
            }
            out[(long)yy * w + xx] = (float)ss;
        }
    }
}

static uint8_t clip8(int v)
{
    if (v >= (1 << PRECISION_BITS << 8)) {
        return 255;
    }
    if (v <= 0) {
        return 0;
    }
    return (uint8_t)(v >> PRECISION_BITS);
}

void hpass_u8(const uint8_t *in, int w, int rows, int bands, int yoff, uint8_t *out, int w2,
              const int *bounds, const int32_t *kk, int ksize)
{
    for (int yy = 0; yy < rows; yy++) {
        for (int xx = 0; xx < w2; xx++) {
            int xmin = bounds[xx * 2], xmax = bounds[xx * 2 + 1];
            const int32_t *k = kk + (long)xx * ksize;
            for (int c = 0; c < bands; c++) {
                int ss = 1 << (PRECISION_BITS - 1);
                for (int x = 0; x < xmax; x++) {
                    ss += in[((long)(yy + yoff) * w + x + xmin) * bands + c] * k[x];
                }
                out[((long)yy * w2 + xx) * bands + c] = clip8(ss);
            }
        }
    }
}

void vpass_u8(const uint8_t *in, int w, int h2, int bands, uint8_t *out, const int *bounds, const int32_t *kk,
              int ksize)
{
    for (int yy = 0; yy < h2; yy++) {
        int ymin = bounds[yy * 2], ymax = bounds[yy * 2 + 1];
        const int32_t *k = kk + (long)yy * ksize;
        for (int xx = 0; xx < w; xx++) {
            for (int c = 0; c < bands; c++) {
                int ss = 1 << (PRECISION_BITS - 1);
                for (int y = 0; y < ymax; y++) {
                    ss += in[((long)(y + ymin) * w + xx) * bands + c] * k[y];
                }
                out[((long)yy * w + xx) * bands + c] = clip8(ss);
            }
        }
    }
}

void premul_u8(uint8_t *p, long n)
{
    for (long i = 0; i < n; i++, p += 4) {
        unsigned a = p[3];
        for (int k = 0; k < 3; k++) {
            unsigned t = p[k] * a + 128;
            p[k] = (uint8_t)(((t >> 8) + t) >> 8);
        }
    }
}

void unpremul_u8(uint8_t *p, long n)
{
    for (long i = 0; i < n; i++, p += 4) {
        unsigned a = p[3];
        if (a == 255 || a == 0) {
            continue;
        }
        for (int k = 0; k < 3; k++) {
            unsigned v = (255u * p[k]) / a;
            p[k] = (uint8_t)(v > 255 ? 255 : v);
        }
    }
}

void shrink_pm(const double *canvas, int W, int H, int x0, int y0, int x1, int y1, float *planes)
{
    int w = x1 - x0, h = y1 - y0;
    long n = (long)w * h;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            long i = (long)y * w + x;
            int sx = x + x0, sy = y + y0;
            if (sx < 0 || sy < 0 || sx >= W || sy >= H) {
                planes[i] = planes[n + i] = planes[2 * n + i] = planes[3 * n + i] = 0.0f;
                continue;
            }
            const double *p = canvas + ((long)sy * W + sx) * 4;
            planes[i] = (float)(p[0] * p[3]);
            planes[n + i] = (float)(p[1] * p[3]);
            planes[2 * n + i] = (float)(p[2] * p[3]);
            planes[3 * n + i] = (float)p[3];
        }
    }
}

void unpremul(const float *planes, long n, double *out, int clip_alpha)
{
    for (long i = 0; i < n; i++) {
        float a = planes[3 * n + i];
        float den = a > 1e-4f ? a : 1e-4f;
        for (int k = 0; k < 3; k++) {
            float v = a > 1e-4f ? planes[k * n + i] / den : 0.0f;
            out[i * 4 + k] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        }
        out[i * 4 + 3] = clip_alpha ? (a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a)) : a;
    }
}

void nearest_i(const int *px, long n, const int *pal, int k, int *out)
{
    for (long i = 0; i < n; i++) {
        const int *p = px + i * 3;
        long best = 0;
        int bi = 0;
        for (int j = 0; j < k; j++) {
            long d0 = p[0] - pal[j * 3], d1 = p[1] - pal[j * 3 + 1], d2 = p[2] - pal[j * 3 + 2];
            long d = d0 * d0 + d1 * d1 + d2 * d2;
            if (j == 0 || d < best) {
                best = d;
                bi = j;
            }
        }
        out[i] = bi;
    }
}

void index_rgba(const double *img, long n, const double *cols, int ncols, int *out)
{
    for (long i = 0; i < n; i++) {
        const double *p = img + i * 4;
        double best = 0.0;
        int bi = 0;
        for (int k = 0; k < ncols; k++) {
            double d0 = p[0] - cols[k * 3], d1 = p[1] - cols[k * 3 + 1], d2 = p[2] - cols[k * 3 + 2];
            double d = (d0 * d0 + d1 * d1) + d2 * d2;
            if (k == 0 || d < best) {
                best = d;
                bi = k;
            }
        }
        out[i] = p[3] < 0.5 ? 0 : bi + 1;
    }
}

void nearest_pal(const uint8_t *rgb, long n, const uint8_t *pal, int npal, const int *wt, uint8_t *out)
{
    for (long i = 0; i < n; i++) {
        long best = -1;
        int bi = 0;
        for (int k = 0; k < npal; k++) {
            long d0 = (long)rgb[i * 3] - pal[k * 4], d1 = (long)rgb[i * 3 + 1] - pal[k * 4 + 1],
                 d2 = (long)rgb[i * 3 + 2] - pal[k * 4 + 2];
            long d = d0 * d0 * wt[0] + d1 * d1 * wt[1] + d2 * d2 * wt[2];
            if (best < 0 || d < best) {
                best = d;
                bi = k;
            }
        }
        out[i] = (uint8_t)bi;
    }
}

void composite_bg(const double *c, long n, const double *bg, uint8_t *out)
{
    for (long i = 0; i < n; i++) {
        double a = c[i * 4 + 3];
        for (int k = 0; k < 3; k++) {
            double v = c[i * 4 + k] * a + bg[k] * (1.0 - a);
            v = v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
            out[i * 3 + k] = (uint8_t)(long)(v * 255.0);
        }
    }
}

void window_img(double *img, int w, int h, const uint8_t *ftex, int fw, int fh, const double *f0,
                const double *f1, const double *col, double ascale)
{
    for (int y = fh; y < h - fh; y++) {
        for (int x = fw; x < w - fw; x++) {
            for (int k = 0; k < 4; k++) {
                img[((long)y * w + x) * 4 + k] = col[k];
            }
        }
    }
    for (int q = 0; q < 4; q++) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int in, u, v;
                if (q == 0) {
                    in = x < w - fw && y < fh;
                    u = x;
                    v = y;
                } else if (q == 1) {
                    in = x >= w - fw && y < h - fh;
                    u = fw - 1 - (x - (w - fw));
                    v = y;
                } else if (q == 2) {
                    in = x >= fw && y >= h - fh;
                    u = w - 1 - x;
                    v = h - 1 - y;
                } else {
                    in = x < fw && y >= fh;
                    u = x;
                    v = h - 1 - y;
                }
                if (!in) {
                    continue;
                }
                u = u < 0 ? 0 : (u > fw - 1 ? fw - 1 : u);
                v = v < 0 ? 0 : (v > fh - 1 ? fh - 1 : v);
                const uint8_t *t = ftex + ((long)v * fw + u) * 4;
                for (int k = 0; k < 4; k++) {
                    img[((long)y * w + x) * 4 + k] = f0[k] + (f1[k] - f0[k]) * (t[k] / 255.0);
                }
            }
        }
    }
    for (long i = 0; i < (long)w * h; i++) {
        img[i * 4 + 3] *= ascale;
    }
}

void sky(uint8_t *out, int W, int H, const double *p, const uint8_t *t0, int t0w, int t0h, const uint8_t *t1,
         int t1w, int t1h, const double *pal, const double *bayer)
{
    double pix = p[0], mx = p[1], my = p[2];
    const double *A = p + 3, *C = p + 9;
    const double k[3] = {150.0 / 255, 110.0 / 255, 75.0 / 255}, c0[3] = {4.0 / 255, 12.0 / 255, 16.0 / 255};
    for (int yy = 0; yy < H; yy++) {
        for (int xx = 0; xx < W; xx++) {
            double X = (((double)xx + 0.5) - 120) - mx;
            double Y = ((double)(yy * 2) + 1.0 - 80) - my;
            double wx = X * pix, wy = -Y * pix;
            double acc[3] = {0.0, 0.0, 0.0};
            for (int oy = 0; oy < 4; oy++) {
                for (int ox = 0; ox < 4; ox++) {
                    double x = (wx + ((ox + 0.5) / 4) * pix) - pix / 2;
                    double y = (wy + ((oy + 0.5) / 4) * pix) - pix / 2;
                    double uv0 = fma(y, A[2], x * A[0]) + A[4];
                    double uv1 = fma(y, A[3], x * A[1]) + A[5];
                    double ras[3];
                    for (int j = 0; j < 3; j++) {
                        ras[j] = clip01(fma(y, C[3 + j], x * C[j]) + C[6 + j]);
                    }
                    double u0 = (((uv0 - 0.5) * 1.0 + 0.5) - 0.611) - 0.0;
                    double v0 = uv1;
                    double u1 = (((uv0 - 0.5) * 0.16 + 0.5) - 0.5896) - 0.0;
                    double v1 = (uv1 - 0.5) * 0.25 + 0.5;
                    long tx0 = wrap_index((long)floor(u0 * t0w), t0w);
                    long ty0 = wrap_index((long)floor(v0 * t0h), t0h);
                    long tx1 = wrap_index((long)floor(u1 * t1w), t1w);
                    long ty1 = (long)floor(v1 * t1h);
                    if (ty1 < 0) {
                        ty1 = 0;
                    }
                    if (ty1 > t1h - 1) {
                        ty1 = t1h - 1;
                    }
                    const uint8_t *a0 = t0 + (ty0 * t0w + tx0) * 4, *a1 = t1 + (ty1 * t1w + tx1) * 4;
                    for (int j = 0; j < 3; j++) {
                        double tv0 = a0[j] / 255.0, tv1 = a1[j] / 255.0;
                        double prev = clip01((tv0 * ras[j]) * 0.5);
                        double col = clip01(2 * ((c0[j] + prev * (1 - tv1)) + k[j] * tv1));
                        acc[j] = (oy == 0 && ox == 0) ? col : acc[j] + col;
                    }
                }
            }
            double v[3];
            double nz = bayer[(yy % 4) * 4 + xx % 4] * 0.08;
            for (int j = 0; j < 3; j++) {
                v[j] = clip01(acc[j] / 16 + nz);
            }
            double best = 0.0;
            int bi = 0;
            for (int i = 0; i < 255; i++) {
                double d0 = v[0] - pal[i * 3], d1 = v[1] - pal[i * 3 + 1], d2 = v[2] - pal[i * 3 + 2];
                double d = (d0 * d0 + d1 * d1) + d2 * d2;
                if (i == 0 || d < best) {
                    best = d;
                    bi = i;
                }
            }
            out[(long)yy * W + xx] = (uint8_t)(bi + 1);
        }
    }
}
