from common import u16, u32

BLOCK = {0: (8, 8, 32), 1: (8, 4, 32), 2: (8, 4, 32), 3: (4, 4, 32), 4: (4, 4, 32), 5: (4, 4, 32),
         6: (4, 4, 64), 8: (8, 8, 32), 9: (8, 4, 32), 10: (4, 4, 32), 14: (8, 8, 32)}


def byte_size(fmt, w, h):
    bw, bh, bs = BLOCK[fmt]
    return ((w + bw - 1) // bw) * ((h + bh - 1) // bh) * bs


def rgb565(v):
    return (((v >> 11) & 0x1F) * 255 // 31, ((v >> 5) & 0x3F) * 255 // 63, (v & 0x1F) * 255 // 31, 255)


def rgb5a3(v):
    if v & 0x8000:
        return (((v >> 10) & 0x1F) * 255 // 31, ((v >> 5) & 0x1F) * 255 // 31, (v & 0x1F) * 255 // 31, 255)
    return (((v >> 8) & 0xF) * 17, ((v >> 4) & 0xF) * 17, (v & 0xF) * 17, ((v >> 12) & 0x7) * 255 // 7)


def ia8(v):
    i = v & 0xFF
    return (i, i, i, (v >> 8) & 0xFF)


def decode_palette(pal_fmt, data):
    if pal_fmt == 0:
        conv = ia8
    elif pal_fmt == 1:
        conv = rgb565
    else:
        conv = rgb5a3
    return [conv(data[i] << 8 | data[i + 1]) for i in range(0, len(data) - 1, 2)]


def decode_cmpr(raw, p, img, pw, ox, oy):
    for sub in range(4):
        sx = ox + (sub & 1) * 4
        sy = oy + (sub >> 1) * 4
        c0 = raw[p] << 8 | raw[p + 1]
        c1 = raw[p + 2] << 8 | raw[p + 3]
        p0 = rgb565(c0)
        p1 = rgb565(c1)
        if c0 > c1:
            p2 = ((2 * p0[0] + p1[0]) // 3, (2 * p0[1] + p1[1]) // 3, (2 * p0[2] + p1[2]) // 3, 255)
            p3 = ((p0[0] + 2 * p1[0]) // 3, (p0[1] + 2 * p1[1]) // 3, (p0[2] + 2 * p1[2]) // 3, 255)
        else:
            p2 = ((p0[0] + p1[0]) // 2, (p0[1] + p1[1]) // 2, (p0[2] + p1[2]) // 2, 255)
            p3 = (0, 0, 0, 0)
        pal = (p0, p1, p2, p3)
        for r in range(4):
            bits = raw[p + 4 + r]
            row = (sy + r) * pw + sx
            img[row] = pal[bits >> 6]
            img[row + 1] = pal[(bits >> 4) & 3]
            img[row + 2] = pal[(bits >> 2) & 3]
            img[row + 3] = pal[bits & 3]
        p += 8
    return p


def decode(fmt, w, h, data, palette):
    bw, bh, bs = BLOCK[fmt]
    bx = (w + bw - 1) // bw
    by = (h + bh - 1) // bh
    n = bx * by * bs
    raw = bytes(data[:n]).ljust(n, b"\0")
    if fmt in (8, 9, 10):
        if not palette:
            palette = [(0, 0, 0, 0)]
        last = len(palette) - 1
    pw = bx * bw
    ph = by * bh
    img = [None] * (pw * ph)
    p = 0
    for byi in range(by):
        for bxi in range(bx):
            ox = bxi * bw
            oy = byi * bh
            if fmt == 14:
                p = decode_cmpr(raw, p, img, pw, ox, oy)
                continue
            if fmt == 6:
                for i in range(16):
                    y, x = divmod(i, 4)
                    a, r = raw[p + 2 * i], raw[p + 2 * i + 1]
                    g, b = raw[p + 32 + 2 * i], raw[p + 33 + 2 * i]
                    img[(oy + y) * pw + ox + x] = (r, g, b, a)
                p += 64
                continue
            for y in range(bh):
                row = (oy + y) * pw + ox
                if fmt in (0, 8):
                    for x in range(0, 8, 2):
                        b = raw[p]
                        p += 1
                        hi = b >> 4
                        lo = b & 15
                        if fmt == 0:
                            img[row + x] = (hi * 17, hi * 17, hi * 17, 255)
                            img[row + x + 1] = (lo * 17, lo * 17, lo * 17, 255)
                        else:
                            img[row + x] = palette[min(hi, last)]
                            img[row + x + 1] = palette[min(lo, last)]
                elif fmt in (1, 9):
                    for x in range(8):
                        b = raw[p]
                        p += 1
                        img[row + x] = (b, b, b, 255) if fmt == 1 else palette[min(b, last)]
                elif fmt == 2:
                    for x in range(8):
                        b = raw[p]
                        p += 1
                        i = (b & 15) * 17
                        img[row + x] = (i, i, i, (b >> 4) * 17)
                elif fmt == 3:
                    for x in range(4):
                        a = raw[p]
                        i = raw[p + 1]
                        p += 2
                        img[row + x] = (i, i, i, a)
                else:
                    for x in range(4):
                        v = raw[p] << 8 | raw[p + 1]
                        p += 2
                        if fmt == 4:
                            img[row + x] = rgb565(v)
                        elif fmt == 5:
                            img[row + x] = rgb5a3(v)
                        else:
                            img[row + x] = palette[min(v & 0x3FFF, last)]
    out = bytearray(w * h * 4)
    q = 0
    for y in range(h):
        base = y * pw
        for x in range(w):
            out[q:q + 4] = bytes(img[base + x])
            q += 4
    return out


def bti_levels(b, h, all_levels=True):
    fmt = b[h]
    w = u16(b, h + 2)
    hh = u16(b, h + 4)
    palette = None
    if fmt in (8, 9, 10):
        pal_off = h + u32(b, h + 0xC)
        palette = decode_palette(b[h + 9], b[pal_off:pal_off + u16(b, h + 0xA) * 2])
    levels = []
    off = h + u32(b, h + 0x1C)
    for _ in range(max(1, b[h + 0x18]) if all_levels else 1):
        if w < 1 or hh < 1:
            break
        sz = byte_size(fmt, w, hh)
        levels.append((w, hh, decode(fmt, w, hh, b[off:off + sz], palette)))
        off += sz
        w //= 2
        hh //= 2
    return levels


def j3d_texture_headers(b):
    if b[:4] != b"J3D2":
        return []
    p = 0x20
    for _ in range(u32(b, 0xC)):
        if p + 8 > len(b):
            break
        if b[p:p + 4] == b"TEX1":
            hdr = p + u32(b, p + 0xC)
            return [hdr + i * 0x20 for i in range(u16(b, p + 8))]
        p += u32(b, p + 4)
    return []
