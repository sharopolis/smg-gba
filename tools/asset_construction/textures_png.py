import struct
import zlib

from common import fsum
from textures_deflate import deflate_fast

LIBPNG_FILTERS = (0, 1, 2, 3, 4)
COST = [v if v < 128 else 256 - v for v in range(256)]


def paeth(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def filter_candidates(row, prev, bpp):
    n = len(row)
    sub = bytearray(n)
    up = bytearray(n)
    avg = bytearray(n)
    pae = bytearray(n)
    for i in range(n):
        x = row[i]
        b = prev[i]
        if i >= bpp:
            a = row[i - bpp]
            c = prev[i - bpp]
        else:
            a = 0
            c = 0
        sub[i] = (x - a) & 255
        up[i] = (x - b) & 255
        avg[i] = (x - ((a + b) >> 1)) & 255
        pae[i] = (x - paeth(a, b, c)) & 255
    return (bytes(row), bytes(sub), bytes(up), bytes(avg), bytes(pae))


def filter_rows(px, w, h, bpp, order):
    stride = w * bpp
    prev = bytes(stride)
    rows = []
    for y in range(h):
        row = px[y * stride:(y + 1) * stride]
        cands = filter_candidates(row, prev, bpp)
        best = None
        best_cost = None
        for ft in order:
            cost = fsum(COST[v] for v in cands[ft])
            if best_cost is None or cost < best_cost:
                best = ft
                best_cost = cost
        rows.append(bytes((best,)) + cands[best])
        prev = row
    return rows


def chunk(kind, data):
    crc = zlib.crc32(kind + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", crc)


def window_bits(size):
    wbits = 15
    if size <= 16384:
        half = 1 << 14
        while size + 262 <= half:
            half >>= 1
            wbits -= 1
    return wbits


def png(w, h, ctype, chunks):
    out = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, ctype, 0, 0, 0))
    for kind, data in chunks:
        out += chunk(kind, data)
    return out + chunk(b"IEND", b"")


def idat_chunks(z, split):
    return [(b"IDAT", z[i:i + split]) for i in range(0, len(z), split)]


def blender_png(data, w, h, ctype, pre_chunks):
    raw = b"".join(filter_rows(data, w, h, 4 if ctype == 6 else 3, LIBPNG_FILTERS))
    return png(w, h, ctype, pre_chunks + idat_chunks(deflate_fast(raw, window_bits(len(raw))), 8192))


def pil_png(data, w, h, ctype):
    raw = b"".join(filter_rows(data, w, h, 4 if ctype == 6 else 3, LIBPNG_FILTERS))
    return png(w, h, ctype, idat_chunks(deflate_fast(raw, 15), 65536))
