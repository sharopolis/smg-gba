import struct

F32 = struct.Struct("<f")
F32_PACK = F32.pack
F32_UNPACK = F32.unpack


def f32(x):
    return F32_UNPACK(F32_PACK(x))[0]


INV255 = f32(1.0 / 255.0)
BYTE_F = [f32(i * INV255) for i in range(256)]
F255 = f32(255.0)
HALF = f32(0.5)
TOP = f32(1.0 - 0.5 / 255.0)


def to_byte(v):
    if v <= 0.0:
        return 0
    if v > TOP:
        return 255
    return int(f32(f32(F255 * v) + HALF))


def pairwise(xs, lo, n):
    if n < 8:
        r = 0.0
        for i in range(lo, lo + n):
            r = f32(r + xs[i])
        return r
    if n <= 128:
        r0, r1, r2, r3, r4, r5, r6, r7 = xs[lo:lo + 8]
        i = 8
        end = n - (n % 8)
        while i < end:
            k = lo + i
            r0 = f32(r0 + xs[k])
            r1 = f32(r1 + xs[k + 1])
            r2 = f32(r2 + xs[k + 2])
            r3 = f32(r3 + xs[k + 3])
            r4 = f32(r4 + xs[k + 4])
            r5 = f32(r5 + xs[k + 5])
            r6 = f32(r6 + xs[k + 6])
            r7 = f32(r7 + xs[k + 7])
            i += 8
        res = f32(f32(f32(r0 + r1) + f32(r2 + r3)) + f32(f32(r4 + r5) + f32(r6 + r7)))
        while i < n:
            res = f32(res + xs[lo + i])
            i += 1
        return res
    n2 = n // 2
    n2 -= n2 % 8
    return f32(pairwise(xs, lo, n2) + pairwise(xs, lo + n2, n - n2))


def flip_rows(px, w, h):
    out = bytearray(len(px))
    s = w * 4
    for y in range(h):
        out[y * s:(y + 1) * s] = px[(h - 1 - y) * s:(h - y) * s]
    return out


def box_save(px, w, h, tw, th, crop=None, alpha_mode=0):
    up = flip_rows(px, w, h)
    if crop:
        x0c, y0c, x1c, y1c = crop
        rows = []
        for y in range(y0c, y1c):
            rows.append(up[(y * w + x0c) * 4:(y * w + x1c) * 4])
        up = b"".join(rows)
        w, h = x1c - x0c, y1c - y0c
    fl = [BYTE_F[b] for b in up]
    out = [0.0] * (tw * th * 4)
    for j in range(th):
        y0 = j * h // th
        y1 = max((j + 1) * h // th, y0 + 1)
        for i in range(tw):
            x0 = i * w // tw
            x1 = max((i + 1) * w // tw, x0 + 1)
            idx = [(y * w + x) * 4 for y in range(y0, y1) for x in range(x0, x1)]
            n = len(idx)
            alist = [fl[k + 3] for k in idx]
            wsum = pairwise(alist, 0, n)
            o = (j * tw + i) * 4
            if wsum > 1e-6:
                for c in range(3):
                    k0 = idx[0]
                    acc = f32(fl[k0 + c] * fl[k0 + 3])
                    for k in idx[1:]:
                        acc = f32(acc + f32(fl[k + c] * fl[k + 3]))
                    out[o + c] = f32(acc / wsum)
            else:
                for c in range(3):
                    k0 = idx[0]
                    acc = fl[k0 + c]
                    for k in idx[1:]:
                        acc = f32(acc + fl[k + c])
                    out[o + c] = f32(acc / n)
            out[o + 3] = f32(wsum / n)
    if alpha_mode == 1:
        for p in range(3, len(out), 4):
            out[p] = 1.0
    elif alpha_mode == 2:
        for p in range(0, len(out), 4):
            out[p + 3] = 1.0 if max(out[p], out[p + 1], out[p + 2]) > 0.3 else 0.0
    res = bytearray(to_byte(v) for v in out)
    return flip_rows(res, tw, th)
