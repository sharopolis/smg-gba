#include <math.h>
#include <stdlib.h>


#define PI 3.141592653589793
#define PIO2_HI 1.5707963267341256e+00
#define PIO2_LO 6.0771005065061922e-11
#define PIO2_LO2 2.0222662487959506e-21
#define LN2 0.69314718055994531
#define NL 57

typedef struct {
    double p0[3], e1[3], e2[3];
    double uv[3][2], col[3][4];
    int mat;
} Tri;

typedef struct {
    int tex, kind, ws, wt, tex1;
    double ou, ov, su, sv;
    double k[24];
} Mat;

typedef struct {
    int w, h;
    const unsigned char *px;
} Tex;

typedef struct {
    double t, u, v;
    int tri;
} Hit;

static Tri *tris;
static int ntris;
static Mat *mats;
static Tex *texs;
static int gw;
static int *cell_start, *cell_list;
static double tol = 1e-6;
static int nearest_only;
static int max_layers;
static double *lut;
static unsigned char *lut_ok;

static const double EG2020[9] = {1.1375807621586465, 0.00835044265905122, -0.14593120481769767,
                                 -0.019087466007028417, 1.2153352181676491, -0.19624775216062096,
                                 -0.09731114895965517, -0.10087322981618292, 1.198184378775838};
static const double TIN[9] = {0.856627153315983, 0.0951212405381588, 0.0482516061458583,
                              0.137318972929847, 0.761241990602591, 0.101439036467562,
                              0.11189821299995, 0.0767994186031903, 0.811302368396859};
static const double MOUT[9] = {1.127100581814438, -0.11060664309660326, -0.016493938717834455,
                               -0.14132976349843826, 1.1578237022162725, -0.016493938717834247,
                               -0.1413297634984382, -0.110606643096603, 1.251936406595041};
static const double M2020_709[9] = {1.6604910021084343, -0.5876411387885495, -0.07284986331988486,
                                    -0.1245504745215906, 1.13289989712596, -0.00834942260436947,
                                    -0.01815076335490522, -0.10057889800800743, 1.118729661362913};
static const double TOEG[9] = {0.5593711333100773, 0.30478334662300893, 0.13584556048047705,
                               0.07622070548350629, 0.7879717701028465, 0.13580746617472153,
                               0.06552671177877019, 0.16454675460372414, 0.7699265016503745};
static const double CK[3] = {0.2658180370250449, 0.59846986045365, 0.1357121025213052};

static void sincos_kern(double x, double *s, double *c)
{
    double z = x * x;
    double ts = x, ss = x, tc = 1.0, sc = 1.0;
    int k;
    for (k = 1; k < 26; k += 2) {
        ts = -ts * z / ((k + 1) * (k + 2));
        ss += ts;
    }
    for (k = 0; k < 26; k += 2) {
        tc = -tc * z / ((k + 1) * (k + 2));
        sc += tc;
    }
    *s = ss;
    *c = sc;
}

static void sincos_d(double a, double *s, double *c)
{
    double q = floor(a / (PI / 2) + 0.5);
    double r = ((a - q * PIO2_HI) - q * PIO2_LO) - q * PIO2_LO2;
    double ss, cc;
    long n = (long)q;
    sincos_kern(r, &ss, &cc);
    switch (((n % 4) + 4) % 4) {
    case 0:
        *s = ss;
        *c = cc;
        break;
    case 1:
        *s = cc;
        *c = -ss;
        break;
    case 2:
        *s = -ss;
        *c = -cc;
        break;
    default:
        *s = -cc;
        *c = ss;
        break;
    }
}

static double ln_d(double x)
{
    double y = x, m, m2, t, s = 0.0;
    int e = 0, k;
    if (!(x > 0.0))
        return -1e300;
    while (y >= 1.4142135623730951) {
        y *= 0.5;
        e++;
    }
    while (y < 0.70710678118654752) {
        y *= 2.0;
        e--;
    }
    m = (y - 1.0) / (y + 1.0);
    m2 = m * m;
    t = m;
    for (k = 1; k < 60; k += 2) {
        s += t / k;
        t *= m2;
    }
    return 2.0 * s + e * LN2;
}

static double exp_d(double x)
{
    double n = floor(x / LN2 + 0.5), r = x - n * LN2, t = 1.0, s = 1.0;
    int k;
    if (x < -700.0)
        return 0.0;
    for (k = 1; k < 30; k++) {
        t = t * r / k;
        s += t;
    }
    while (n > 0) {
        s *= 2.0;
        n -= 1.0;
    }
    while (n < 0) {
        s *= 0.5;
        n += 1.0;
    }
    return s;
}

static double pow_d(double x, double p)
{
    if (x <= 0.0)
        return 0.0;
    return exp_d(p * ln_d(x));
}

static void dir_of(double lon, double lat, double d[3])
{
    double so, co, sa, ca;
    sincos_d(lon, &so, &co);
    sincos_d(lat, &sa, &ca);
    d[0] = -so * ca;
    d[1] = sa;
    d[2] = co * ca;
}

static double clamp01(double v)
{
    return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

static double srgb_to_lin(double c)
{
    double y = (c + 0.055) / 1.055, m, m2, t = 0.0, s = 0.0;
    int e = 0, k;
    if (c <= 0.04045)
        return c < 0.0 ? 0.0 : c / 12.92;
    while (y < 0.70710678118654752) {
        y *= 2.0;
        e--;
    }
    m = (y - 1.0) / (y + 1.0);
    m2 = m * m;
    t = m;
    for (k = 1; k < 40; k += 2) {
        s += t / k;
        t *= m2;
    }
    return exp_d((2.0 * s + e * LN2) * 2.4);
}

static double wrap1(double v)
{
    return v - floor(v);
}

static double texel(const Tex *t, int x, int y, int c)
{
    return t->px[(y * t->w + x) * 4 + c] / 255.0;
}

static int wrap_index(int i, int n)
{
    return ((i % n) + n) % n;
}

static int clamp_index(int i, int n)
{
    return i < 0 ? 0 : (i >= n ? n - 1 : i);
}

static void tex_linear(const Tex *t, int ws, int wt, double u, double v, double out[4])
{
    int rep = ws == 1 && wt == 1;
    double x, y, fx, fy;
    int x0, y0, x1, y1, c;
    if (!rep) {
        if (ws == 1)
            u = wrap1(u);
        if (wt == 1)
            v = wrap1(v);
    }
    x = u * t->w - 0.5;
    y = v * t->h - 0.5;
    fx = floor(x);
    fy = floor(y);
    x0 = (int)fx;
    y0 = (int)fy;
    fx = x - fx;
    fy = y - fy;
    x1 = x0 + 1;
    y1 = y0 + 1;
    if (rep) {
        x0 = wrap_index(x0, t->w);
        x1 = wrap_index(x1, t->w);
        y0 = wrap_index(y0, t->h);
        y1 = wrap_index(y1, t->h);
    } else {
        x0 = clamp_index(x0, t->w);
        x1 = clamp_index(x1, t->w);
        y0 = clamp_index(y0, t->h);
        y1 = clamp_index(y1, t->h);
    }
    for (c = 0; c < 4; c++) {
        double a = texel(t, x0, y0, c), b = texel(t, x1, y0, c);
        double d = texel(t, x0, y1, c), e = texel(t, x1, y1, c);
        double top = a + (b - a) * fx, bot = d + (e - d) * fx;
        out[c] = top + (bot - top) * fy;
    }
}

static void tex_nearest(const Tex *t, double u, double v, double out[3])
{
    long x = (long)floor(u * t->w), y = (long)floor(v * t->h);
    int c;
    x = ((x % t->w) + t->w) % t->w;
    y = ((y % t->h) + t->h) % t->h;
    for (c = 0; c < 3; c++)
        out[c] = t->px[(y * t->w + x) * 4 + c] / 255.0;
}

static int intersect(const Tri *tri, const double dir[3], double *tt, double *uu, double *vv)
{
    double pv[3], qv[3], tv[3], det, inv, u, v, t;
    const double *e1 = tri->e1, *e2 = tri->e2;
    pv[0] = dir[1] * e2[2] - dir[2] * e2[1];
    pv[1] = dir[2] * e2[0] - dir[0] * e2[2];
    pv[2] = dir[0] * e2[1] - dir[1] * e2[0];
    det = pv[0] * e1[0] + pv[1] * e1[1] + pv[2] * e1[2];
    if (!(fabs(det) > 1e-9))
        return 0;
    inv = 1.0 / det;
    tv[0] = -tri->p0[0];
    tv[1] = -tri->p0[1];
    tv[2] = -tri->p0[2];
    u = (pv[0] * tv[0] + pv[1] * tv[1] + pv[2] * tv[2]) * inv;
    qv[0] = tv[1] * e1[2] - tv[2] * e1[1];
    qv[1] = tv[2] * e1[0] - tv[0] * e1[2];
    qv[2] = tv[0] * e1[1] - tv[1] * e1[0];
    v = (dir[0] * qv[0] + dir[1] * qv[1] + dir[2] * qv[2]) * inv;
    t = (qv[0] * e2[0] + qv[1] * e2[1] + qv[2] * e2[2]) * inv;
    if (u >= -tol && v >= -tol && u + v <= 1 + tol && t > 0) {
        *tt = t;
        *uu = u;
        *vv = v;
        return 1;
    }
    return 0;
}

static int cell_of(const double dir[3])
{
    double ax = fabs(dir[0]), ay = fabs(dir[1]), az = fabs(dir[2]);
    int face, i, j;
    double a, b, m;
    if (ax >= ay && ax >= az) {
        face = dir[0] > 0 ? 0 : 1;
        m = ax;
        a = dir[1];
        b = dir[2];
    } else if (ay >= az) {
        face = dir[1] > 0 ? 2 : 3;
        m = ay;
        a = dir[0];
        b = dir[2];
    } else {
        face = dir[2] > 0 ? 4 : 5;
        m = az;
        a = dir[0];
        b = dir[1];
    }
    i = (int)floor((a / m + 1.0) * 0.5 * gw);
    j = (int)floor((b / m + 1.0) * 0.5 * gw);
    if (i < 0)
        i = 0;
    if (i >= gw)
        i = gw - 1;
    if (j < 0)
        j = 0;
    if (j >= gw)
        j = gw - 1;
    return (face * gw + j) * gw + i;
}

static int neighbour_cell(int face, int x, int y, int g)
{
    double e[3];
    double ua = ((x + 0.5) / g) * 2.0 - 1.0, vb = ((y + 0.5) / g) * 2.0 - 1.0;
    if (x >= 0 && y >= 0 && x < g && y < g)
        return (face * g + y) * g + x;
    if (face < 2) {
        e[0] = face == 0 ? 1 : -1;
        e[1] = ua;
        e[2] = vb;
    } else if (face < 4) {
        e[1] = face == 2 ? 1 : -1;
        e[0] = ua;
        e[2] = vb;
    } else {
        e[2] = face == 4 ? 1 : -1;
        e[0] = ua;
        e[1] = vb;
    }
    return cell_of(e);
}

static int tri_cells(const Tri *tri, int g, unsigned char *mark, int *seen)
{
    int nt = 0, a, b, k, steps = 48;
    for (a = 0; a <= steps; a++) {
        for (b = 0; b <= steps - a; b++) {
            double d[3], len;
            int c, di, dj, face, ci, cj;
            for (k = 0; k < 3; k++)
                d[k] = tri->p0[k] + tri->e1[k] * a / steps + tri->e2[k] * b / steps;
            len = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (len == 0.0)
                continue;
            c = cell_of(d);
            face = c / (g * g);
            cj = (c / g) % g;
            ci = c % g;
            for (dj = -2; dj <= 2; dj++) {
                for (di = -2; di <= 2; di++) {
                    int cc = neighbour_cell(face, ci + di, cj + dj, g);
                    if (!mark[cc]) {
                        mark[cc] = 1;
                        seen[nt++] = cc;
                    }
                }
            }
        }
    }
    return nt;
}

static void build_grid(int g)
{
    int ncell = 6 * g * g, i, n, pass;
    int *cnt = calloc(ncell + 1, sizeof(int));
    unsigned char *mark = calloc(ncell, 1);
    int *seen = malloc(sizeof(int) * ncell);
    gw = g;
    cell_start = calloc(ncell + 1, sizeof(int));
    cell_list = NULL;
    for (pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            int s = 0;
            for (i = 0; i < ncell; i++) {
                cell_start[i] = s;
                s += cnt[i];
                cnt[i] = 0;
            }
            cell_start[ncell] = s;
            cell_list = malloc(sizeof(int) * (s + 1));
        }
        for (i = 0; i < ntris; i++) {
            int nt = tri_cells(&tris[i], g, mark, seen);
            for (n = 0; n < nt; n++) {
                int cc = seen[n];
                mark[cc] = 0;
                if (pass == 1)
                    cell_list[cell_start[cc] + cnt[cc]] = i;
                cnt[cc]++;
            }
        }
    }
    free(cnt);
    free(mark);
    free(seen);
}

static void shade_mat(const Tri *tri, double u, double v, double col[3], double *alpha, int *additive)
{
    const Mat *mat = &mats[tri->mat];
    double w0 = 1.0 - u - v;
    double uv0 = w0 * tri->uv[0][0] + u * tri->uv[1][0] + v * tri->uv[2][0];
    double uv1 = w0 * tri->uv[0][1] + u * tri->uv[1][1] + v * tri->uv[2][1];
    double tc[4];
    int c;
    *additive = mat->kind == 1;
    *alpha = 1.0;
    if (mat->kind <= 1) {
        double su = mat->ou + mat->su * uv0, sv = mat->ov + mat->sv * uv1;
        tex_linear(&texs[mat->tex], mat->ws, mat->wt, su, sv, tc);
        for (c = 0; c < 3; c++)
            col[c] = srgb_to_lin(tc[c]);
        *alpha = tc[3];
    } else if (mat->kind == 2 || mat->kind == 3) {
        double t0[3], t1[3], ras[3];
        double u1 = (uv0 - 0.5) * mat->k[0] + 0.5, v1 = (uv1 - 0.5) * mat->k[1] + 0.5;
        tex_nearest(&texs[mat->tex], uv0, uv1, t0);
        tex_nearest(&texs[mat->tex1], u1, v1, t1);
        for (c = 0; c < 3; c++) {
            double prev;
            ras[c] = w0 * tri->col[0][c] + u * tri->col[1][c] + v * tri->col[2][c];
            prev = mat->kind == 2 ? clamp01(t0[c] * ras[c] * 0.5) : clamp01(t0[c] * ras[c] - 0.5);
            col[c] = clamp01(2 * (mat->k[2 + c] + prev * (1 - t1[c]) + mat->k[5 + c] * t1[c]));
        }
    } else if (mat->kind == 4) {
        const double *q = mat->k;
        int fl = (int)q[15];
        double t0[4], t1[4];
        tex_linear(&texs[mat->tex1], fl & 3, (fl >> 2) & 3, q[9] * uv0 + q[10] * uv1 + q[11],
                   q[12] * uv0 + q[13] * uv1 + q[14], t1);
        tex_linear(&texs[mat->tex], mat->ws, mat->wt, q[3] * uv0 + q[4] * uv1 + q[5],
                   q[6] * uv0 + q[7] * uv1 + q[8], t0);
        for (c = 0; c < 3; c++) {
            double ras = w0 * tri->col[0][c] + u * tri->col[1][c] + v * tri->col[2][c];
            double bg = srgb_to_lin(clamp01(clamp01(q[c]) + t1[c]));
            double st = srgb_to_lin(clamp01(clamp01(q[c] + t0[c] * ras) + t1[c]));
            col[c] = bg + q[20 + c] * (st - bg);
        }
    } else if (mat->kind == 5) {
        for (c = 0; c < 3; c++)
            col[c] = mat->k[c];
    } else {
        col[0] = col[1] = col[2] = 0.0;
    }
}

static void set_hit(Hit *h, double t, double u, double v, int tri)
{
    h->t = t;
    h->u = u;
    h->v = v;
    h->tri = tri;
}

static void trace(const double dir[3], double out[3])
{
    Hit hits[64];
    int nh = 0, i, k, c;
    int cell = cell_of(dir);
    double thr = 1.0;
    for (k = cell_start[cell]; k < cell_start[cell + 1]; k++) {
        double t, u, v;
        int ti = cell_list[k];
        if (!intersect(&tris[ti], dir, &t, &u, &v))
            continue;
        if (nearest_only) {
            if (nh == 0 || t < hits[0].t) {
                set_hit(&hits[0], t, u, v, ti);
                nh = 1;
            }
            continue;
        }
        if (t <= 1.0)
            continue;
        if (nh < 64) {
            set_hit(&hits[nh], t, u, v, ti);
            nh++;
        }
    }
    for (i = 1; i < nh; i++) {
        Hit h = hits[i];
        int j = i - 1;
        while (j >= 0 && (hits[j].t > h.t || (hits[j].t == h.t && hits[j].tri > h.tri))) {
            hits[j + 1] = hits[j];
            j--;
        }
        hits[j + 1] = h;
    }
    out[0] = out[1] = out[2] = 0.0;
    for (i = 0; i < nh && i < max_layers; i++) {
        double col[3], a;
        int add;
        shade_mat(&tris[hits[i].tri], hits[i].u, hits[i].v, col, &a, &add);
        if (nearest_only) {
            for (c = 0; c < 3; c++)
                out[c] = col[c];
            return;
        }
        if (add) {
            for (c = 0; c < 3; c++)
                out[c] += thr * col[c];
        } else {
            for (c = 0; c < 3; c++)
                out[c] += thr * a * col[c];
            thr *= 1.0 - a;
        }
        if (thr <= 0.0)
            break;
    }
}

static void load_scene(int ntri, const double *tv, const int *tm, int nmat, const int *mi, const double *md,
                       int ntex, const int *ti, const unsigned char *tpx)
{
    int i, k, a;
    long off = 0;
    tris = malloc(sizeof(Tri) * (ntri > 0 ? ntri : 1));
    ntris = ntri;
    for (i = 0; i < ntri; i++) {
        const double *q = tv + i * 27;
        Tri *tri = &tris[i];
        for (k = 0; k < 3; k++) {
            tri->p0[k] = q[k];
            tri->e1[k] = q[9 + k] - q[k];
            tri->e2[k] = q[18 + k] - q[k];
        }
        for (a = 0; a < 3; a++) {
            tri->uv[a][0] = q[a * 9 + 3];
            tri->uv[a][1] = q[a * 9 + 4];
            for (k = 0; k < 4; k++)
                tri->col[a][k] = q[a * 9 + 5 + k];
        }
        tri->mat = tm[i];
    }
    mats = malloc(sizeof(Mat) * (nmat > 0 ? nmat : 1));
    for (i = 0; i < nmat; i++) {
        Mat *mat = &mats[i];
        mat->tex = mi[i * 5];
        mat->kind = mi[i * 5 + 1];
        mat->ws = mi[i * 5 + 2];
        mat->wt = mi[i * 5 + 3];
        mat->tex1 = mi[i * 5 + 4];
        mat->ou = md[i * 28];
        mat->ov = md[i * 28 + 1];
        mat->su = md[i * 28 + 2];
        mat->sv = md[i * 28 + 3];
        for (k = 0; k < 24; k++)
            mat->k[k] = md[i * 28 + 4 + k];
    }
    texs = malloc(sizeof(Tex) * (ntex > 0 ? ntex : 1));
    for (i = 0; i < ntex; i++) {
        texs[i].w = ti[i * 2];
        texs[i].h = ti[i * 2 + 1];
        texs[i].px = tpx + off;
        off += (long)texs[i].w * texs[i].h * 4;
    }
}

static void render_nearest(int w, int h, int nsub, const double *offs, const double *wts, double *out)
{
    int x, y, a, b, k;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double acc[3] = {0, 0, 0}, wsum = 0.0;
            for (b = 0; b < nsub; b++) {
                for (a = 0; a < nsub; a++) {
                    double d[3], c[3], wt = wts[a] * wts[b];
                    double lon = ((x + offs[a]) / w) * 2 * PI;
                    double lat = PI / 2 - ((y + offs[b]) / h) * PI;
                    dir_of(lon, lat, d);
                    trace(d, c);
                    for (k = 0; k < 3; k++)
                        acc[k] += c[k] * wt;
                    wsum += wt;
                }
            }
            for (k = 0; k < 3; k++)
                out[(y * w + x) * 3 + k] = acc[k] / wsum;
        }
    }
}

static void render_filtered(int w, int h, int f, const double *wts, double *out)
{
    int fw = w * f, nrow = h * f + 2 * f, taps = 3 * f, r, x, y, a, k;
    double *row = malloc(sizeof(double) * fw * 3);
    double *hrow = malloc(sizeof(double) * (size_t)nrow * w * 3);
    double wsum = 0.0;
    for (a = 0; a < taps; a++)
        wsum += wts[a];
    for (r = 0; r < nrow; r++) {
        double yy = (r - f + 0.5) / f;
        double lat = PI / 2 - yy / h * PI;
        for (x = 0; x < fw; x++) {
            double d[3], lon = (x + 0.5) / f / w * (2 * PI);
            dir_of(lon, lat, d);
            trace(d, row + x * 3);
        }
        for (x = 0; x < w; x++) {
            double acc[3] = {0, 0, 0};
            for (a = 0; a < taps; a++) {
                int sx = x * f - f + a;
                sx = ((sx % fw) + fw) % fw;
                for (k = 0; k < 3; k++)
                    acc[k] += row[sx * 3 + k] * wts[a];
            }
            for (k = 0; k < 3; k++)
                hrow[((size_t)r * w + x) * 3 + k] = acc[k] / wsum;
        }
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double acc[3] = {0, 0, 0};
            for (a = 0; a < taps; a++) {
                int rr = y * f + a;
                for (k = 0; k < 3; k++)
                    acc[k] += hrow[((size_t)rr * w + x) * 3 + k] * wts[a];
            }
            for (k = 0; k < 3; k++)
                out[(y * w + x) * 3 + k] = acc[k] / wsum;
        }
    }
    free(row);
    free(hrow);
}

int sky_render(int ntri, const double *tv, const int *tm, int nmat, const int *mi, const double *md, int ntex,
               const int *ti, const unsigned char *tpx, int w, int h, int nsub, const double *offs,
               const double *wts, int mode, int layers, int grid, double *out)
{
    load_scene(ntri, tv, tm, nmat, mi, md, ntex, ti, tpx);
    nearest_only = mode == 1;
    tol = mode == 1 ? 1e-6 : 0.0;
    max_layers = layers;
    build_grid(grid);
    if (mode == 1)
        render_nearest(w, h, nsub, offs, wts, out);
    else
        render_filtered(w, h, nsub, wts, out);
    free(tris);
    free(mats);
    free(texs);
    free(cell_start);
    free(cell_list);
    return 0;
}

static void mat3(const double *m, const double *v, double *o)
{
    int i;
    for (i = 0; i < 3; i++)
        o[i] = m[i * 3] * v[0] + m[i * 3 + 1] * v[1] + m[i * 3 + 2] * v[2];
}

static void comp_low(double *v)
{
    double mn = v[0], off[3], a, b, mx, imx, k;
    int i;
    if (v[1] < mn)
        mn = v[1];
    if (v[2] < mn)
        mn = v[2];
    if (mn >= 0.0)
        return;
    for (i = 0; i < 3; i++)
        off[i] = v[i] - mn;
    mx = v[0] > v[1] ? (v[0] > v[2] ? v[0] : v[2]) : (v[1] > v[2] ? v[1] : v[2]);
    imx = mx - mn;
    a = imx - ((mx - v[0]) * CK[0] + (mx - v[1]) * CK[1] + (mx - v[2]) * CK[2])
        + (v[0] * CK[0] + v[1] * CK[1] + v[2] * CK[2]);
    b = imx - ((mx - v[0]) * CK[0] + (mx - v[1]) * CK[1] + (mx - v[2]) * CK[2])
        + (off[0] * CK[0] + off[1] * CK[1] + off[2] * CK[2]);
    k = b > 0.0 ? a / b : 0.0;
    for (i = 0; i < 3; i++)
        v[i] = off[i] * k;
}

static double sig(double x)
{
    double xp = 10.0 / 16.5, yp = pow_d(0.18, 1.0 / 2.4), slope = 2.4, p = 1.5, a, d, s, ax, t;
    if (x < 0.0)
        x = 0.0;
    if (x > 10.0)
        x = 10.0;
    if (x >= xp) {
        a = slope * (1.0 - xp);
        d = 1.0 - yp;
    } else {
        a = slope * xp;
        d = yp;
    }
    s = a / pow_d(pow_d(a / d, p) - 1.0, 1.0 / p);
    ax = slope * (x - xp);
    t = fabs(ax) / s;
    return yp + ax / pow_d(1.0 + pow_d(t, p), 1.0 / p);
}

static void rgb_hsv(const double *c, double *h, double *s, double *v)
{
    double mx = c[0], mn = c[0], d;
    int i;
    for (i = 1; i < 3; i++) {
        if (c[i] > mx)
            mx = c[i];
        if (c[i] < mn)
            mn = c[i];
    }
    d = mx - mn;
    *v = mx;
    *s = mx != 0.0 ? d / mx : 0.0;
    if (d == 0.0) {
        *h = 0.0;
        return;
    }
    if (mx == c[0]) {
        double q = (c[1] - c[2]) / d;
        *h = (q - 6.0 * floor(q / 6.0)) / 6.0;
    } else if (mx == c[1]) {
        *h = ((c[2] - c[0]) / d + 2.0) / 6.0;
    } else {
        *h = ((c[0] - c[1]) / d + 4.0) / 6.0;
    }
}

static void set3(double *o, double a, double b, double c)
{
    o[0] = a;
    o[1] = b;
    o[2] = c;
}

static void hsv_rgb(double h, double s, double v, double *o)
{
    double h6 = (h - floor(h)) * 6.0, f, p, q, t;
    int i = (int)floor(h6);
    f = h6 - floor(h6);
    i = ((i % 6) + 6) % 6;
    p = v * (1.0 - s);
    q = v * (1.0 - s * f);
    t = v * (1.0 - s * (1.0 - f));
    switch (i) {
    case 0:
        set3(o, v, t, p);
        break;
    case 1:
        set3(o, q, v, p);
        break;
    case 2:
        set3(o, p, v, t);
        break;
    case 3:
        set3(o, p, q, v);
        break;
    case 4:
        set3(o, t, p, v);
        break;
    default:
        set3(o, v, p, q);
        break;
    }
}

static void agx_model(int ir, int ig, int ib, double *out)
{
    double n[3] = {ir / 56.0, ig / 56.0, ib / 56.0}, eg[3], l20[3], c[3], y[3];
    double h0, s0, v0, h1, s1, v1, dh, o20[3], o7[3];
    int i;
    for (i = 0; i < 3; i++)
        eg[i] = exp_d((25.0 * n[i] - 12.47393) * LN2);
    mat3(EG2020, eg, l20);
    comp_low(l20);
    mat3(TIN, l20, c);
    for (i = 0; i < 3; i++) {
        double x;
        if (c[i] > 1e-30)
            x = (ln_d(c[i]) / LN2 + 2.473931188332412 + 10.0) / 16.5;
        else
            x = (ln_d(1e-30) / LN2 + 2.473931188332412 + 10.0) / 16.5;
        y[i] = pow_d(sig(x), 2.4);
    }
    rgb_hsv(c, &h0, &s0, &v0);
    rgb_hsv(y, &h1, &s1, &v1);
    dh = h0 - h1 + 0.5;
    dh = dh - floor(dh) - 0.5;
    hsv_rgb(h1 + dh * 0.6, s1, v1, y);
    mat3(MOUT, y, o20);
    mat3(M2020_709, o20, o7);
    comp_low(o7);
    for (i = 0; i < 3; i++) {
        double v = o7[i] < 0.0 ? 0.0 : o7[i];
        v = pow_d(v, 1.0 / 2.4);
        out[i] = v > 1.0 ? 1.0 : v;
    }
}

static const double *lut_node(int r, int g, int b)
{
    int k = (b * NL + g) * NL + r;
    if (!lut_ok[k]) {
        agx_model(r, g, b, lut + k * 3);
        lut_ok[k] = 1;
    }
    return lut + k * 3;
}

static void agx_pixel(const double *rgb, double *out)
{
    double eg[3], p[3], f[3], res[3];
    int i0[3], i, c;
    const double *c000, *c111, *ca, *cb;
    double wa, wb, wc, wd;
    mat3(TOEG, rgb, eg);
    for (i = 0; i < 3; i++) {
        double l = eg[i] > 1.17549435e-38 ? ln_d(eg[i]) / LN2 : ln_d(1.17549435e-38) / LN2;
        double nn = (l + 12.47393) / (12.5260688117 + 12.47393);
        if (nn < 0.0)
            nn = 0.0;
        if (nn > 1.0)
            nn = 1.0;
        p[i] = nn * (NL - 1);
        i0[i] = (int)floor(p[i]);
        if (i0[i] > NL - 2)
            i0[i] = NL - 2;
        f[i] = p[i] - i0[i];
    }
    c000 = lut_node(i0[0], i0[1], i0[2]);
    c111 = lut_node(i0[0] + 1, i0[1] + 1, i0[2] + 1);
    if (f[0] > f[1] && f[1] > f[2]) {
        ca = lut_node(i0[0] + 1, i0[1], i0[2]);
        cb = lut_node(i0[0] + 1, i0[1] + 1, i0[2]);
        wa = 1 - f[0];
        wb = f[0] - f[1];
        wc = f[1] - f[2];
        wd = f[2];
    } else if (f[0] > f[2] && f[2] >= f[1]) {
        ca = lut_node(i0[0] + 1, i0[1], i0[2]);
        cb = lut_node(i0[0] + 1, i0[1], i0[2] + 1);
        wa = 1 - f[0];
        wb = f[0] - f[2];
        wc = f[2] - f[1];
        wd = f[1];
    } else if (f[2] >= f[0] && f[0] > f[1]) {
        ca = lut_node(i0[0], i0[1], i0[2] + 1);
        cb = lut_node(i0[0] + 1, i0[1], i0[2] + 1);
        wa = 1 - f[2];
        wb = f[2] - f[0];
        wc = f[0] - f[1];
        wd = f[1];
    } else if (f[1] >= f[0] && f[0] > f[2]) {
        ca = lut_node(i0[0], i0[1] + 1, i0[2]);
        cb = lut_node(i0[0] + 1, i0[1] + 1, i0[2]);
        wa = 1 - f[1];
        wb = f[1] - f[0];
        wc = f[0] - f[2];
        wd = f[2];
    } else if (f[1] > f[2] && f[2] >= f[0]) {
        ca = lut_node(i0[0], i0[1] + 1, i0[2]);
        cb = lut_node(i0[0], i0[1] + 1, i0[2] + 1);
        wa = 1 - f[1];
        wb = f[1] - f[2];
        wc = f[2] - f[0];
        wd = f[0];
    } else {
        ca = lut_node(i0[0], i0[1], i0[2] + 1);
        cb = lut_node(i0[0], i0[1] + 1, i0[2] + 1);
        wa = 1 - f[2];
        wb = f[2] - f[1];
        wc = f[1] - f[0];
        wd = f[0];
    }
    for (c = 0; c < 3; c++)
        res[c] = wa * c000[c] + wb * ca[c] + wc * cb[c] + wd * c111[c];
    for (c = 0; c < 3; c++)
        out[c] = pow_d(res[c] < 0.0 ? 0.0 : res[c], 2.4);
}

static double srgb_oetf(double v)
{
    if (v <= 0.0031308)
        return v < 0.0 ? 0.0 : v * 12.92;
    return 1.055 * pow_d(v, 1.0 / 2.4) - 0.055;
}

static float dither_at(unsigned x, unsigned y)
{
    unsigned mul = 1103515245u, a, b, h;
    float s, t;
    a = (y ^ (x >> 1)) * mul;
    b = (x ^ (y >> 1)) * mul;
    h = (a ^ (b >> 3)) * mul;
    s = (float)h * (1.0f / 4294967296.0f);
    s = s + s;
    s = s - 1.0f;
    t = 1.0f - (float)sqrt((double)(1.0f - (s < 0.0f ? -s : s)));
    s = s < 0.0f ? -t : t;
    return s * 0.0033f;
}

static unsigned char quant(float v, float d)
{
    float x = d + v;
    if (x <= 0.0f)
        return 0;
    if (x > 0.99803925f)
        return 255;
    return (unsigned char)(int)(x * 255.0f + 0.5f);
}

static void halve_width(const unsigned char *img, int w, int h, unsigned char *out)
{
    int nw = w / 2, x, y, c;
    float *row = malloc(sizeof(float) * (w + 2) * 3);
    float add = ((float)w - 0.01f) / (float)nw, inv = 1.0f / add;
    for (y = 0; y < h; y++) {
        float val[3] = {0, 0, 0}, nval[3], sample = 0.0f;
        int p = 0;
        for (x = 0; x < w * 3; x++)
            row[x] = img[(size_t)y * w * 3 + x];
        for (x = w * 3; x < (w + 2) * 3; x++)
            row[x] = 0.0f;
        for (x = 0; x < nw; x++) {
            for (c = 0; c < 3; c++)
                nval[c] = -val[c] * sample;
            sample += add;
            while (sample >= 1.0f) {
                sample -= 1.0f;
                for (c = 0; c < 3; c++)
                    nval[c] += row[p * 3 + c];
                p++;
            }
            for (c = 0; c < 3; c++) {
                float r;
                val[c] = row[p * 3 + c];
                nval[c] += sample * val[c];
                r = nval[c] * inv + 0.5f;
                out[((size_t)y * nw + x) * 3 + c] = (unsigned char)(int)r;
            }
        }
    }
    free(row);
}

int sky_finish(const double *lin, int w, int h, int view, int rot, int ncorr, const int *cidx,
               const int *cval, unsigned char *out)
{
    int x, y, c, i;
    unsigned char *img = malloc((size_t)w * h * 3);
    lut = calloc((size_t)NL * NL * NL * 3, sizeof(double));
    lut_ok = calloc((size_t)NL * NL * NL, 1);
    for (i = 0; i < ncorr; i++) {
        int k = cidx[i];
        agx_model(k % NL, (k / NL) % NL, k / (NL * NL), lut + k * 3);
        for (c = 0; c < 3; c++)
            lut[k * 3 + c] += cval[i * 3 + c] * 1e-6;
        lut_ok[k] = 1;
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double d[3];
            const double *px = lin + ((size_t)y * w + x) * 3;
            float dv = dither_at((unsigned)x, (unsigned)(h - 1 - y));
            if (view == 0) {
                agx_pixel(px, d);
            } else {
                for (c = 0; c < 3; c++)
                    d[c] = px[c];
            }
            for (c = 0; c < 3; c++)
                img[((size_t)y * w + x) * 3 + c] = quant((float)srgb_oetf(d[c]), dv);
        }
    }
    if (rot) {
        for (i = 0; i < w * h / 2; i++) {
            int j = w * h - 1 - i;
            for (c = 0; c < 3; c++) {
                unsigned char t = img[i * 3 + c];
                img[i * 3 + c] = img[j * 3 + c];
                img[j * 3 + c] = t;
            }
        }
    }
    halve_width(img, w, h, out);
    free(img);
    free(lut);
    free(lut_ok);
    return 0;
}
