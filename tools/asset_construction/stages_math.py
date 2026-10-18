import math
import struct

PI = 3.141592653589793
DEG = PI / 180.0
RAD = 180.0 / PI

S1 = -1.66666666666666324348e-01
S2 = 8.33333333332248946124e-03
S3 = -1.98412698298579493134e-04
S4 = 2.75573137070700676789e-06
S5 = -2.50507602534068634195e-08
S6 = 1.58969099521155010221e-10
C1 = 4.16666666666666019037e-02
C2 = -1.38888888888741095749e-03
C3 = 2.48015872894767294178e-05
C4 = -2.75573143513906633035e-07
C5 = 2.08757232129817482790e-09
C6 = -1.13596475577881948265e-11
PIO2_1 = 1.57079632673412561417e+00
PIO2_1T = 6.07710050650619224932e-11
PIO2_2 = 6.07710050630396597660e-11
PIO2_2T = 2.02226624879595063154e-21
PIO2_3 = 2.02226624871116645580e-21
PIO2_3T = 8.47842766036889956997e-32
INVPIO2 = 6.36619772367581382433e-01


def high_word(x):
    return struct.unpack("<q", struct.pack("<d", x))[0] >> 32


def with_high_word(hi):
    return struct.unpack("<d", struct.pack("<q", (hi & 0xFFFFFFFF) << 32))[0]


def ksin(x, y, iy):
    if (high_word(x) & 0x7FFFFFFF) < 0x3E400000:
        return x
    z = x * x
    v = z * x
    r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)))
    if iy == 0:
        return x + v * (S1 + z * r)
    return x - ((z * (0.5 * y - v * r) - y) - v * S1)


def kcos(x, y):
    ix = high_word(x) & 0x7FFFFFFF
    if ix < 0x3E400000:
        return 1.0
    z = x * x
    r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))))
    if ix < 0x3FD33333:
        return 1.0 - (0.5 * z - (z * r - x * y))
    if ix > 0x3FE90000:
        qx = 0.28125
    else:
        qx = with_high_word(ix - 0x00200000)
    hz = 0.5 * z - qx
    a = 1.0 - qx
    return a - (hz - (z * r - x * y))


def rem_pio2(x):
    hx = high_word(x)
    ix = hx & 0x7FFFFFFF
    if ix <= 0x3FE921FB:
        return 0, x, 0.0
    if ix < 0x4002D97C:
        if hx > 0:
            z = x - PIO2_1
            if ix != 0x3FF921FB:
                y0 = z - PIO2_1T
                return 1, y0, (z - y0) - PIO2_1T
            z -= PIO2_2
            y0 = z - PIO2_2T
            return 1, y0, (z - y0) - PIO2_2T
        z = x + PIO2_1
        if ix != 0x3FF921FB:
            y0 = z + PIO2_1T
            return -1, y0, (z - y0) + PIO2_1T
        z += PIO2_2
        y0 = z + PIO2_2T
        return -1, y0, (z - y0) + PIO2_2T
    if ix > 0x413921FB:
        raise ValueError("argument too large")
    t = abs(x)
    n = int(t * INVPIO2 + 0.5)
    fn = float(n)
    r = t - fn * PIO2_1
    w = fn * PIO2_1T
    j = ix >> 20
    y0 = r - w
    i = j - ((high_word(y0) >> 20) & 0x7FF)
    if i > 16:
        t = r
        w = fn * PIO2_2
        r = t - w
        w = fn * PIO2_2T - ((t - r) - w)
        y0 = r - w
        i = j - ((high_word(y0) >> 20) & 0x7FF)
        if i > 49:
            t = r
            w = fn * PIO2_3
            r = t - w
            w = fn * PIO2_3T - ((t - r) - w)
            y0 = r - w
    y1 = (r - y0) - w
    if hx < 0:
        return -n, -y0, -y1
    return n, y0, y1


def sin(x):
    ix = high_word(x) & 0x7FFFFFFF
    if ix <= 0x3FE921FB:
        return ksin(x, 0.0, 0)
    n, y0, y1 = rem_pio2(x)
    n &= 3
    if n == 0:
        return ksin(y0, y1, 1)
    if n == 1:
        return kcos(y0, y1)
    if n == 2:
        return -ksin(y0, y1, 1)
    return -kcos(y0, y1)


def cos(x):
    ix = high_word(x) & 0x7FFFFFFF
    if ix <= 0x3FE921FB:
        return kcos(x, 0.0)
    n, y0, y1 = rem_pio2(x)
    n &= 3
    if n == 0:
        return kcos(y0, y1)
    if n == 1:
        return -ksin(y0, y1, 1)
    if n == 2:
        return -kcos(y0, y1)
    return ksin(y0, y1, 1)


ATANHI = (4.63647609000806093515e-01, 7.85398163397448278999e-01, 9.82793723247329054082e-01,
           1.57079632679489655800e+00)
ATANLO = (2.26987774529616870924e-17, 3.06161699786838301793e-17, 1.39033110312309984516e-17,
           6.12323399573676603587e-17)
AT = (3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01,
       -1.11111104054623557880e-01, 9.09088713343650656196e-02, -7.69187620504482999495e-02,
       6.66107313738753120669e-02, -5.83357013379057348645e-02, 4.97687799461593236017e-02,
       -3.65315727442169155270e-02, 1.62858201153657823623e-02)


def atan(x):
    hx = high_word(x)
    ix = hx & 0x7FFFFFFF
    if ix >= 0x44100000:
        return ATANHI[3] + ATANLO[3] if hx > 0 else -ATANHI[3] - ATANLO[3]
    if ix < 0x3FDC0000:
        if ix < 0x3E400000:
            return x
        idx = -1
    else:
        x = abs(x)
        if ix < 0x3FF30000:
            if ix < 0x3FE60000:
                idx = 0
                x = (2.0 * x - 1.0) / (2.0 + x)
            else:
                idx = 1
                x = (x - 1.0) / (x + 1.0)
        elif ix < 0x40038000:
            idx = 2
            x = (x - 1.5) / (1.0 + 1.5 * x)
        else:
            idx = 3
            x = -1.0 / x
    z = x * x
    w = z * z
    a = AT
    s1 = z * (a[0] + w * (a[2] + w * (a[4] + w * (a[6] + w * (a[8] + w * a[10])))))
    s2 = w * (a[1] + w * (a[3] + w * (a[5] + w * (a[7] + w * a[9]))))
    if idx < 0:
        return x - x * (s1 + s2)
    z = ATANHI[idx] - ((x * (s1 + s2) - ATANLO[idx]) - x)
    return -z if hx < 0 else z


PI_LO = 1.2246467991473531772e-16


def atan2(y, x):
    if x == 1.0:
        return atan(y)
    ys = math.copysign(1.0, y) < 0
    xs = math.copysign(1.0, x) < 0
    if y == 0.0:
        if not xs:
            return y
        return -PI if ys else PI
    if x == 0.0:
        return -PI / 2 if ys else PI / 2
    k = (((high_word(y) >> 20) & 0x7FF) - ((high_word(x) >> 20) & 0x7FF))
    if k > 60:
        z = PI / 2 + 0.5 * PI_LO
    elif xs and k < -60:
        z = 0.0
    else:
        z = atan(abs(y / x))
    if not xs and not ys:
        return z
    if not xs and ys:
        return -z
    if xs and not ys:
        return PI - (z - PI_LO)
    return (z - PI_LO) - PI


PIO2_HI = 1.57079632679489655800e+00
PIO2_LO = 6.12323399573676603587e-17
PIO4_HI = 7.85398163397448278999e-01
PS0 = 1.66666666666666657415e-01
PS1 = -3.25565818622400915405e-01
PS2 = 2.01212532134862925881e-01
PS3 = -4.00555345006794114027e-02
PS4 = 7.91534994289814532176e-04
PS5 = 3.47933107596021167570e-05
QS1 = -2.40339491173441421878e+00
QS2 = 2.02094576023350569471e+00
QS3 = -6.88283971605453293030e-01
QS4 = 7.70381505559019352791e-02


def asin(x):
    hx = high_word(x)
    ix = hx & 0x7FFFFFFF
    if ix >= 0x3FF00000:
        if x == 1.0 or x == -1.0:
            return x * PIO2_HI + x * PIO2_LO
        raise ValueError("math domain error")
    if ix < 0x3FE00000:
        if ix < 0x3E400000:
            return x
        t = x * x
        p = t * (PS0 + t * (PS1 + t * (PS2 + t * (PS3 + t * (PS4 + t * PS5)))))
        q = 1.0 + t * (QS1 + t * (QS2 + t * (QS3 + t * QS4)))
        return x + x * (p / q)
    w = 1.0 - abs(x)
    t = w * 0.5
    p = t * (PS0 + t * (PS1 + t * (PS2 + t * (PS3 + t * (PS4 + t * PS5)))))
    q = 1.0 + t * (QS1 + t * (QS2 + t * (QS3 + t * QS4)))
    s = math.sqrt(t)
    if ix >= 0x3FEF3333:
        w = p / q
        t = PIO2_HI - (2.0 * (s + s * w) - PIO2_LO)
    else:
        w = with_high_word(high_word(s))
        c = (t - w * w) / (s + w)
        r = p / q
        p = 2.0 * s * r - (PIO2_LO - 2.0 * c)
        q = PIO4_HI - 2.0 * w
        t = PIO4_HI - (p - q)
    return t if hx > 0 else -t


def radians(x):
    return x * DEG


def degrees(x):
    return x * RAD


def rot_matrix(r):
    rx, ry, rz = (radians(a) for a in r)
    sx, sy, sz = sin(rx), sin(ry), sin(rz)
    cx, cy, cz = cos(rx), cos(ry), cos(rz)
    return [[cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx],
            [sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx],
            [-sy, cy * sx, cy * cx]]


def mtx_tr(t, r):
    m = rot_matrix(r)
    return [m[0] + [t[0]], m[1] + [t[1]], m[2] + [t[2]]]


def mtx_mul(a, b):
    out = [[0.0] * 4 for _ in range(3)]
    for i in range(3):
        for j in range(3):
            out[i][j] = 0.0 + a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j]
        out[i][3] = 0.0 + a[i][0] * b[0][3] + a[i][1] * b[1][3] + a[i][2] * b[2][3] + a[i][3]
    return out


def xform_point(m, p):
    return [0.0 + m[i][0] * p[0] + m[i][1] * p[1] + m[i][2] * p[2] + m[i][3] for i in range(3)]


def mtx_to_euler(m):
    r20 = max(-1.0, min(1.0, m[2][0]))
    ry = asin(-r20)
    if abs(r20) < 0.99999:
        rx = atan2(m[2][1], m[2][2])
        rz = atan2(m[1][0], m[0][0])
    else:
        rz = 0.0
        rx = atan2(-m[1][2], m[1][1])
    return [round(degrees(a), 4) for a in (rx, ry, rz)]
