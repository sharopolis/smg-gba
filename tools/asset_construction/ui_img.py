import ctypes

import ui_lyt
from common import fsum
from ui_lyt import Img, dbuf, ibuf, ptr
from ui_math import PI, sin

BOX, BILINEAR, LANCZOS = 0, 1, 2
ONE = 1 << 22
COEFFS = {}


def box_filter(x):
    return 1.0 if -0.5 < x <= 0.5 else 0.0


def bilinear_filter(x):
    x = abs(x)
    return 1.0 - x if x < 1.0 else 0.0


def sinc(x):
    if x == 0.0:
        return 1.0
    x = x * PI
    return sin(x) / x


def lanczos_filter(x):
    if -3.0 <= x < 3.0:
        return sinc(x) * sinc(x / 3)
    return 0.0


FILTERS = {BOX: (box_filter, 0.5), BILINEAR: (bilinear_filter, 1.0), LANCZOS: (lanczos_filter, 3.0)}


def make_coeffs(in_size, out_size, filt):
    fn, support = FILTERS[filt]
    scale = float(in_size) / out_size
    filterscale = max(scale, 1.0)
    support = support * filterscale
    ksize = int(-(-support // 1)) * 2 + 1
    bounds, kk = [], []
    ss = 1.0 / filterscale
    for xx in range(out_size):
        center = 0.0 + (xx + 0.5) * scale
        xmin = max(int(center - support + 0.5), 0)
        xmax = min(int(center + support + 0.5), in_size) - xmin
        k = []
        ww = 0.0
        for x in range(xmax):
            w = fn(((x + xmin) - center + 0.5) * ss)
            k.append(w)
            ww += w
        if ww != 0.0:
            k = [w / ww for w in k]
        kk.extend(k + [0.0] * (ksize - len(k)))
        bounds += [xmin, xmax]
    return bounds, kk, ksize


def coeffs(in_size, out_size, filt):
    key = (in_size, out_size, filt)
    if key not in COEFFS:
        COEFFS[key] = make_coeffs(in_size, out_size, filt)
    b, k, n = COEFFS[key]
    return list(b), k, n


def fixed(kk):
    return [int(-0.5 + v * ONE) if v < 0 else int(0.5 + v * ONE) for v in kk]


def first_rows(bv, h2):
    yfirst = bv[0]
    ylast = bv[h2 * 2 - 2] + bv[h2 * 2 - 1]
    for i in range(h2):
        bv[i * 2] -= yfirst
    return yfirst, ylast - yfirst


def resize_f32(src, w, h, w2, h2):
    if (w2, h2) == (w, h):
        out = (ctypes.c_float * (w * h))()
        ctypes.memmove(out, src, 4 * w * h)
        return out
    bh, kh, ksh = coeffs(w, w2, BOX)
    bv, kv, ksv = coeffs(h, h2, BOX)
    if w2 != w:
        yfirst, rows = first_rows(bv, h2)
        tmp = (ctypes.c_float * (w2 * max(rows, 1)))()
        ui_lyt.LIB.hpass_f32(src, w, rows, yfirst, tmp, w2, ibuf(bh), dbuf(kh), ksh)
        if h2 == h:
            return tmp
        src = tmp
    out = (ctypes.c_float * (w2 * h2))()
    ui_lyt.LIB.vpass_f32(src, w2, h2, out, ibuf(bv), dbuf(kv), ksv)
    return out


def resize_u8(data, w, h, bands, w2, h2, filt):
    src = bytearray(data)
    if (w2, h2) == (w, h):
        return src
    bh, kh, ksh = coeffs(w, w2, filt)
    bv, kv, ksv = coeffs(h, h2, filt)
    if w2 != w:
        yfirst, rows = first_rows(bv, h2)
        tmp = bytearray(w2 * max(rows, 1) * bands)
        ui_lyt.LIB.hpass_u8(ptr(src), w, rows, bands, yfirst, ptr(tmp), w2, ibuf(bh), ibuf(fixed(kh)), ksh)
        if h2 == h:
            return tmp
        src = tmp
    out = bytearray(w2 * h2 * bands)
    ui_lyt.LIB.vpass_u8(ptr(src), w2, h2, bands, ptr(out), ibuf(bv), ibuf(fixed(kv)), ksv)
    return out


def resize_rgba(data, w, h, w2, h2, filt):
    pm = bytearray(data)
    ui_lyt.LIB.premul_u8(ptr(pm), w * h)
    out = resize_u8(pm, w, h, 4, w2, h2, filt)
    ui_lyt.LIB.unpremul_u8(ptr(out), w2 * h2)
    return out


def shrink_planes(canvas, box, w2, h2):
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    n = w * h
    planes = (ctypes.c_float * (4 * n))()
    ui_lyt.LIB.shrink_pm(canvas.d, canvas.w, canvas.h, x0, y0, x1, y1, planes)
    out = (ctypes.c_float * (4 * w2 * h2))()
    for k in range(4):
        r = resize_f32(ctypes.byref(planes, 4 * k * n), w, h, w2, h2)
        ctypes.memmove(ctypes.byref(out, 4 * k * w2 * h2), r, 4 * w2 * h2)
    return out


def shrink(canvas, box, scale):
    w2 = max(1, round((box[2] - box[0]) * scale))
    h2 = max(1, round((box[3] - box[1]) * scale))
    img = Img(w2, h2)
    ui_lyt.LIB.unpremul(shrink_planes(canvas, box, w2, h2), w2 * h2, img.d, 0)
    return img


def bbox(img, thr=0.02):
    out = (ctypes.c_int * 4)()
    if not ui_lyt.LIB.bbox(img.d, img.w, img.h, thr, out):
        raise ValueError("empty canvas")
    return out[0], out[1], out[2], out[3]


def nearest_i(px, pal):
    n = len(px) // 3
    out = (ctypes.c_int * max(n, 1))()
    ui_lyt.LIB.nearest_i(ibuf(px), n, ibuf([v for c in pal for v in c]), len(pal), out)
    return list(out[:n])


def index_rgba(d, n, cols):
    out = (ctypes.c_int * n)()
    ui_lyt.LIB.index_rgba(d, n, dbuf([v for c in cols for v in c]), len(cols), out)
    return list(out)


class CutBox:
    __slots__ = ("items", "count", "volume", "l", "r")

    def __init__(self, items, count):
        self.items, self.count, self.volume, self.l, self.r = items, count, -1, None, None

    def vol(self):
        if self.volume < 0:
            if not self.items:
                self.volume = 0
            else:
                v = 1
                for ch in range(3):
                    vals = [c[0][ch] for c in self.items]
                    v *= max(vals) - min(vals) + 1
                self.volume = v
        return self.volume

    def split(self):
        f = []
        for ch, wt in enumerate((77, 150, 29)):
            vals = [c[0][ch] for c in self.items]
            f.append((max(vals) - min(vals)) * wt)
        best, axis = f[0], 0
        for i in (1, 2):
            if best < f[i]:
                best, axis = f[i], i
        lst = sorted(self.items, key=lambda c: (-c[0][axis], c[0]))
        n = len(lst)
        left = n0 = 0
        k = 0
        while k < n:
            left += lst[k][1]
            n0 += lst[k][1]
            k += 1
            if left * 2 > self.count:
                break
        if k < n:
            sv = lst[k - 1][0][axis]
            while k < n and lst[k][0][axis] == sv:
                n0 += lst[k][1]
                k += 1
        cut = k
        if cut == n:
            sv = lst[-1][0][axis]
            while cut > 0 and lst[cut - 1][0][axis] == sv:
                cut -= 1
                n0 -= lst[cut][1]
        n1 = fsum(c[1] for c in lst[cut:])
        self.l, self.r = CutBox(lst[:cut], n0), CutBox(lst[cut:], n1)


class Heap:
    def __init__(self):
        self.a = [None]

    def add(self, v):
        self.a.append(None)
        k = len(self.a) - 1
        while k != 1:
            if v.count - self.a[k // 2].count <= 0:
                break
            self.a[k] = self.a[k // 2]
            k >>= 1
        self.a[k] = v

    def remove(self):
        if len(self.a) <= 1:
            return None
        r = self.a[1]
        v = self.a.pop()
        n = len(self.a) - 1
        if n == 0:
            return r
        k = 1
        while k * 2 <= n:
            l = k * 2
            if l < n and self.a[l].count - self.a[l + 1].count < 0:
                l += 1
            if v.count - self.a[l].count > 0:
                break
            self.a[k] = self.a[l]
            k = l
        self.a[k] = v
        return r


def split_boxes(root, colors):
    heap = Heap()
    heap.add(root)
    n = colors
    while True:
        n -= 1
        if not n:
            break
        node = heap.remove()
        while node is not None and node.vol() == 1:
            node = heap.remove()
        if node is None:
            break
        node.split()
        heap.add(node.l)
        heap.add(node.r)


def leaves(b, out):
    if b.l and b.r:
        leaves(b.l, out)
        leaves(b.r, out)
    else:
        out.append(b)
    return out


def median_cut(pixels, colors):
    scale = 0
    while True:
        counts = {}
        for p in pixels:
            key = (p[0] >> scale, p[1] >> scale, p[2] >> scale)
            counts[key] = counts.get(key, 0) + 1
        if len(counts) <= 65536:
            break
        scale += 1
    root = CutBox(sorted(counts.items()), len(pixels))
    split_boxes(root, colors)
    leaf_of = {}
    used = []
    for b in leaves(root, []):
        for key, _ in b.items:
            leaf_of[key] = len(used)
        if b.items:
            used.append(b)
    sums = [[0, 0, 0, 0] for _ in used]
    for p in pixels:
        s = sums[leaf_of[(p[0] >> scale, p[1] >> scale, p[2] >> scale)]]
        s[0] += p[0]
        s[1] += p[1]
        s[2] += p[2]
        s[3] += 1
    return [tuple(int(0.5 + s[k] / s[3]) for k in range(3)) for s in sums]
