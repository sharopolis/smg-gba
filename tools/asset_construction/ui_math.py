import math
import struct
from fractions import Fraction

PI = 3.141592653589793
DEG = PI / 180.0
PIO2_1 = 1.5707963267948966
PIO2_2 = 6.123233995736766e-17
PIO2_3 = -1.4973849048591698e-33
INV_PIO2 = 0.6366197723675814
LN2_HI, LN2_LO = 0.6931471805599453, 2.3190468138462996e-17


def two_sum(a, b):
    s = a + b
    bb = s - a
    return s, (a - (s - bb)) + (b - bb)


def split(a):
    c = 134217729.0 * a
    h = c - (c - a)
    return h, a - h


def two_prod(a, b):
    p = a * b
    ah, al = split(a)
    bh, bl = split(b)
    return p, ((ah * bh - p) + ah * bl + al * bh) + al * bl


def dd_add(ah, al, bh, bl):
    s, e = two_sum(ah, bh)
    e += al + bl
    return two_sum(s, e)


def dd_mul(ah, al, bh, bl):
    p, e = two_prod(ah, bh)
    e += ah * bl + al * bh
    return two_sum(p, e)


def dd_mul_d(ah, al, b):
    p, e = two_prod(ah, b)
    e += al * b
    return two_sum(p, e)


def dd_div(ah, al, bh, bl):
    q1 = ah / bh
    ph, pl = dd_mul(q1, 0.0, bh, bl)
    rh, rl = dd_add(ah, al, -ph, -pl)
    q2 = rh / bh
    ph, pl = dd_mul(q2, 0.0, bh, bl)
    rh, rl = dd_add(rh, rl, -ph, -pl)
    q3 = rh / bh
    h, l = two_sum(q1, q2)
    return dd_add(h, l, q3, 0.0)


def dd_div_d(ah, al, b):
    q = ah / b
    ph, pl = two_prod(q, b)
    rh, rl = dd_add(ah, al, -ph, -pl)
    q2 = (rh + rl) / b
    return two_sum(q, q2)


def inv_factorials():
    out = []
    num, den = 1, 1
    for k in range(1, 40):
        den *= k
        h = num / den
        out.append((h, float(Fraction(num, den) - Fraction(h))))
    return out


INV_FACT = inv_factorials()


def sincos_series(rh, rl):
    zh, zl = dd_mul(rh, rl, rh, rl)
    sh, sl = rh, rl
    th, tl = rh, rl
    ch, cl = 1.0, 0.0
    uh, ul = 1.0, 0.0
    sign = -1.0
    for k in range(1, 18):
        uh, ul = dd_mul(uh, ul, zh, zl)
        th, tl = dd_mul(th, tl, zh, zl)
        fc = INV_FACT[2 * k - 1]
        fs = INV_FACT[2 * k]
        ah, al = dd_mul(uh, ul, fc[0], fc[1])
        bh, bl = dd_mul(th, tl, fs[0], fs[1])
        ch, cl = dd_add(ch, cl, sign * ah, sign * al)
        sh, sl = dd_add(sh, sl, sign * bh, sign * bl)
        sign = -sign
    return (sh, sl), (ch, cl)


def reduce_pio2(x):
    n = round(x * INV_PIO2)
    if n == 0:
        return 0, x, 0.0
    fn = float(n)
    ph, pl = two_prod(fn, PIO2_1)
    rh, rl = dd_add(x, 0.0, -ph, -pl)
    qh, ql = two_prod(fn, PIO2_2)
    rh, rl = dd_add(rh, rl, -qh, -ql)
    rh, rl = dd_add(rh, rl, -fn * PIO2_3, 0.0)
    return n, rh, rl


def sincos_dd(x):
    n, rh, rl = reduce_pio2(x)
    s, c = sincos_series(rh, rl)
    n &= 3
    if n == 0:
        return s, c
    if n == 1:
        return c, (-s[0], -s[1])
    if n == 2:
        return (-s[0], -s[1]), (-c[0], -c[1])
    return (-c[0], -c[1]), s


def sincos(x):
    x = float(x)
    if x == 0.0:
        return x, 1.0
    s, c = sincos_dd(x)
    return s[0] + s[1], c[0] + c[1]


def sin(x):
    return sincos(x)[0]


def cos(x):
    return sincos(x)[1]


def tan(x):
    s, c = sincos_dd(float(x))
    h, l = dd_div(s[0], s[1], c[0], c[1])
    return h + l


def radians(a):
    return a * DEG


def fraction_fma(a, b, c):
    return float(Fraction(a) * Fraction(b) + Fraction(c))


fma = getattr(math, "fma", fraction_fma)


def ulp_shift(v, n):
    if not n:
        return v
    i = struct.unpack("<q", struct.pack("<d", v))[0]
    return struct.unpack("<d", struct.pack("<q", i + n))[0]


def cube(u):
    ph, pl = two_prod(u, u)
    h, l = dd_mul_d(ph, pl, u)
    return h + l


def ipow(x, n):
    x = float(x)
    if n == 0:
        return 1.0
    h, l = x, 0.0
    for _ in range(n - 1):
        h, l = dd_mul_d(h, l, x)
    return h + l


def log_dd(x):
    m, e = math.frexp(x)
    if m < 0.7071067811865476:
        m *= 2.0
        e -= 1
    nh, nl = two_sum(m, -1.0)
    dh, dl = two_sum(m, 1.0)
    q = nh / dh
    ph, pl = dd_mul_d(dh, dl, q)
    rh, rl = dd_add(nh, nl, -ph, -pl)
    q2 = (rh + rl) / dh
    th, tl = two_sum(q, q2)
    zh, zl = dd_mul(th, tl, th, tl)
    sh, sl = th, tl
    ph, pl = th, tl
    for k in range(3, 61, 2):
        ph, pl = dd_mul(ph, pl, zh, zl)
        ch, cl = dd_div_d(ph, pl, float(k))
        if abs(ch) < 1e-36:
            break
        sh, sl = dd_add(sh, sl, ch, cl)
    sh, sl = dd_add(sh, sl, sh, sl)
    eh, el = dd_mul_d(LN2_HI, LN2_LO, float(e))
    return dd_add(sh, sl, eh, el)


def exp_dd(xh, xl):
    k = round(xh / LN2_HI)
    eh, el = dd_mul_d(LN2_HI, LN2_LO, float(k))
    rh, rl = dd_add(xh, xl, -eh, -el)
    sh, sl = 1.0, 0.0
    th, tl = 1.0, 0.0
    for n in range(1, 40):
        th, tl = dd_mul(th, tl, rh, rl)
        th, tl = dd_div_d(th, tl, float(n))
        sh, sl = dd_add(sh, sl, th, tl)
        if abs(th) < 1e-36:
            break
    return math.ldexp(sh + sl, k)


def powf(x, y):
    if x == 0.0:
        return 0.0
    if x == 1.0:
        return 1.0
    lh, ll = log_dd(x)
    ph, pl = dd_mul_d(lh, ll, y)
    return exp_dd(ph, pl)


def rgb555(c, top=1):
    r, g, b = (int(round(min(max(x, 0), top) / top * 31)) for x in c[:3])
    return r | g << 5 | b << 10
