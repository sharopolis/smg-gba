import struct

from figures_j3d import PI, sincos

PB = 22


def load(data):
    count, nt, texoff = struct.unpack_from(">HHI", data, 8)
    res = {}
    p = 16
    for _ in range(count):
        rid, n = struct.unpack_from(">HH", data, p)
        p += 8
        blocks = {}
        for _ in range(n):
            tag, size = struct.unpack_from(">4sI", data, p)
            blocks[tag] = data[p:p + size]
            p += size
        b = blocks.get(b"BSP1")
        if b is not None and b"TDB1" in blocks:
            tdb = blocks[b"TDB1"]
            res[rid] = dict(prm=list(b[38:42]), env=list(b[42:46]),
                            tex=list(struct.unpack_from(">%dH" % ((len(tdb) - 8) // 2), tdb, 8)))
    texs = []
    p = texoff
    for _ in range(nt):
        size = struct.unpack_from(">I", data, p + 4)[0]
        texs.append(data[p + 32:p + size])
        p += size
    return res, texs


def sinc(x):
    if x == 0.0:
        return 1.0
    x = x * PI
    return sincos(x)[0] / x


def lanczos(x):
    if -3.0 <= x < 3.0:
        return sinc(x) * sinc(x / 3.0)
    return 0.0


def box_filter(x):
    return 1.0 if -0.5 < x <= 0.5 else 0.0


def coeffs(insize, outsize, filt=lanczos, fsupport=3.0):
    scale = insize / outsize
    fscale = scale if scale > 1.0 else 1.0
    support = fsupport * fscale
    out = []
    for xx in range(outsize):
        center = (xx + 0.5) * scale
        ss = 1.0 / fscale
        xmin = int(center - support + 0.5)
        if xmin < 0:
            xmin = 0
        xmax = int(center + support + 0.5)
        if xmax > insize:
            xmax = insize
        xmax -= xmin
        k = [filt((x + xmin - center + 0.5) * ss) for x in range(xmax)]
        ww = 0.0
        for w in k:
            ww += w
        if ww != 0.0:
            k = [w / ww for w in k]
        ki = [int(-0.5 + w * (1 << PB)) if w < 0 else int(0.5 + w * (1 << PB)) for w in k]
        out.append((xmin, ki))
    return out


def clip8(v):
    v >>= PB
    return 0 if v < 0 else 255 if v > 255 else v


def resize_rgba(px, w, h, tw, th):
    if (w, h) == (tw, th):
        return bytearray(px)
    pre = bytearray(len(px))
    for i in range(0, len(px), 4):
        a = px[i + 3]
        for c in range(3):
            t = px[i + c] * a + 128
            pre[i + c] = ((t >> 8) + t) >> 8
        pre[i + 3] = a
    ch = coeffs(w, tw)
    cv = coeffs(h, th)
    ybox_first = cv[0][0]
    ybox_last = cv[-1][0] + len(cv[-1][1])
    rows = ybox_last - ybox_first
    tmp = bytearray(tw * rows * 4)
    half = 1 << (PB - 1)
    for y in range(rows):
        sy = y + ybox_first
        for xx in range(tw):
            xmin, k = ch[xx]
            for c in range(4):
                ss = half
                for x, kk in enumerate(k):
                    ss += pre[(sy * w + xmin + x) * 4 + c] * kk
                tmp[(y * tw + xx) * 4 + c] = clip8(ss)
    out = bytearray(tw * th * 4)
    for yy in range(th):
        ymin, k = cv[yy]
        ymin -= ybox_first
        for xx in range(tw):
            for c in range(4):
                ss = half
                for y, kk in enumerate(k):
                    ss += tmp[((ymin + y) * tw + xx) * 4 + c] * kk
                out[(yy * tw + xx) * 4 + c] = clip8(ss)
    for i in range(0, len(out), 4):
        a = out[i + 3]
        if a != 0 and a != 255:
            for c in range(3):
                v = (255 * out[i + c]) // a
                out[i + c] = 255 if v > 255 else v
    return out


def box_resize(chan, w, h, tw, th):
    ch = coeffs(w, tw, box_filter, 0.5)
    cv = coeffs(h, th, box_filter, 0.5)
    half = 1 << (PB - 1)
    tmp = []
    for y in range(h):
        row = []
        for xx in range(tw):
            xmin, k = ch[xx]
            ss = half
            for x, kk in enumerate(k):
                ss += chan[y * w + xmin + x] * kk
            row.append(clip8(ss))
        tmp.append(row)
    out = []
    for yy in range(th):
        ymin, k = cv[yy]
        for xx in range(tw):
            ss = half
            for y, kk in enumerate(k):
                ss += tmp[ymin + y][xx] * kk
            out.append(clip8(ss))
    return out
