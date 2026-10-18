import ctypes
import json
import math
import os
import struct

from common import load_lib, u32, yaz0_decompress
from ui_math import fma, radians, sincos, ulp_shift

GX_BLOCK = {0: (8, 8, 32), 1: (8, 4, 32), 2: (8, 4, 32), 3: (4, 4, 32), 4: (4, 4, 32), 5: (4, 4, 32),
            6: (4, 4, 64), 8: (8, 8, 32), 9: (8, 4, 32), 10: (4, 4, 32), 14: (8, 8, 32)}
PANE_KINDS = {b"pan1": "pane", b"pic1": "picture", b"txt1": "textbox", b"wnd1": "window", b"bnd1": "bounding"}
ANIM_TARGETS = {
    "RLPA": ["trans_x", "trans_y", "trans_z", "rot_x", "rot_y", "rot_z", "scale_x", "scale_y", "size_w",
             "size_h"],
    "RLVC": ["lt_r", "lt_g", "lt_b", "lt_a", "rt_r", "rt_g", "rt_b", "rt_a",
             "lb_r", "lb_g", "lb_b", "lb_a", "rb_r", "rb_g", "rb_b", "rb_a", "alpha"],
    "RLMC": (["mat_r", "mat_g", "mat_b", "mat_a", "c0_r", "c0_g", "c0_b", "c0_a",
              "c1_r", "c1_g", "c1_b", "c1_a", "c2_r", "c2_g", "c2_b", "c2_a"] +
             ["k%d_%s" % (i, c) for i in range(4) for c in "rgba"]),
    "RLTS": ["trans_s", "trans_t", "rotate", "scale_s", "scale_t"],
    "RLVI": ["visible"],
    "RLTP": ["texture"],
    "RLIM": ["ind_trans_s", "ind_trans_t", "ind_rotate", "ind_scale_s", "ind_scale_t"],
}
LANGS = ["JpJa", "UsEn", "UsSp", "UsFr", "EuEn", "EuSp", "EuFr", "EuGe", "EuIt", "EuDu", "CnSi", "KrKo"]
EYE = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]]
UNIT_UVS = [[0, 0], [1, 0], [0, 1], [1, 1]]
PICTURE_CHAR_BASE = 0xE000
MASK_INVERT = {"PicLogoGalaxy"}
LIB = None


class Trig:
    def __init__(self):
        self.seen = {}
        self.fix = {}

    def sincos(self, a):
        if a == 0.0:
            return 0.0 * a, 1.0
        k = self.seen.get(a)
        if k is None:
            s, c = sincos(a)
            d = self.fix.get(len(self.seen))
            if d:
                s, c = ulp_shift(s, d[0]), ulp_shift(c, d[1])
            k = self.seen[a] = (s, c)
        return k


TRIG = Trig()


def open_lib(bin_dir):
    global LIB
    lib = load_lib("ui_px", bin_dir)
    P, I, D, U = ctypes.c_void_p, ctypes.c_int, ctypes.c_double, ctypes.c_long
    lib.draw_quad.argtypes = [P, I, I, I, I, I, P, P, P, I, I, P, P, I, I, P, P, P, I, I, P, I, I, I, I]
    lib.blit_glyph.argtypes = [P, I, I, P, I, I, I, P, P, P, P, P, P, I, I]
    lib.over.argtypes = [P, I, I, P, I, I, I, I]
    lib.to_u8.argtypes = [P, P, U]
    lib.bbox.argtypes = [P, I, I, D, P]
    lib.hpass_f32.argtypes = [P, I, I, I, P, I, P, P, I]
    lib.vpass_f32.argtypes = [P, I, I, P, P, P, I]
    lib.hpass_u8.argtypes = [P, I, I, I, I, P, I, P, P, I]
    lib.vpass_u8.argtypes = [P, I, I, I, P, P, P, I]
    lib.premul_u8.argtypes = [P, U]
    lib.unpremul_u8.argtypes = [P, U]
    lib.shrink_pm.argtypes = [P, I, I, I, I, I, I, P]
    lib.unpremul.argtypes = [P, U, P, I]
    lib.nearest_i.argtypes = [P, U, P, I, P]
    lib.index_rgba.argtypes = [P, U, P, I, P]
    lib.nearest_pal.argtypes = [P, U, P, I, P, P]
    lib.composite_bg.argtypes = [P, U, P, P]
    lib.window_img.argtypes = [P, I, I, P, I, I, P, P, P, D]
    lib.sky.argtypes = [P, I, I, P, P, I, I, P, I, I, P, P]
    LIB = lib


def ptr(buf, offset=0):
    if buf is None:
        return None
    return ctypes.addressof(ctypes.c_char.from_buffer(buf, offset))


def dbuf(values):
    return (ctypes.c_double * len(values))(*values)


def ibuf(values):
    return (ctypes.c_int * len(values))(*values)


class Img:
    __slots__ = ("w", "h", "d")

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.d = (ctypes.c_double * (w * h * 4))()


class Tex:
    __slots__ = ("w", "h", "rgba", "fmt")

    def __init__(self, w, h, rgba, fmt=0):
        self.w, self.h, self.rgba, self.fmt = w, h, rgba, fmt


def tex_size(fmt, w, h):
    bw, bh, bs = GX_BLOCK[fmt]
    return ((w + bw - 1) // bw) * ((h + bh - 1) // bh) * bs


def rgb565(v):
    return (((v >> 11) & 0x1F) * 255 // 31, ((v >> 5) & 0x3F) * 255 // 63, (v & 0x1F) * 255 // 31, 255)


def rgb5a3(v):
    if v & 0x8000:
        return (((v >> 10) & 0x1F) * 255 // 31, ((v >> 5) & 0x1F) * 255 // 31, (v & 0x1F) * 255 // 31, 255)
    return (((v >> 8) & 0xF) * 17, ((v >> 4) & 0xF) * 17, (v & 0xF) * 17, ((v >> 12) & 0x7) * 255 // 7)


def ia8(v):
    i, a = v & 0xFF, (v >> 8) & 0xFF
    return (i, i, i, a)


def decode_palette(fmt, data):
    vals = struct.unpack(">%dH" % (len(data) // 2), data)
    if fmt == 0:
        return [ia8(v) for v in vals]
    if fmt == 1:
        return [rgb565(v) for v in vals]
    return [rgb5a3(v) for v in vals]


def decode_cmpr(blk):
    pix = [None] * 64
    for sb in range(4):
        s = blk[sb * 8:sb * 8 + 8]
        c0 = s[0] << 8 | s[1]
        c1 = s[2] << 8 | s[3]
        p0, p1 = rgb565(c0), rgb565(c1)
        if c0 > c1:
            p2 = tuple((2 * p0[k] + p1[k]) // 3 for k in range(3)) + (255,)
            p3 = tuple((p0[k] + 2 * p1[k]) // 3 for k in range(3)) + (255,)
        else:
            p2 = tuple((p0[k] + p1[k]) // 2 for k in range(3)) + (255,)
            p3 = (0, 0, 0, 0)
        cols = (p0, p1, p2, p3)
        sy, sx = sb // 2, sb % 2
        for r in range(4):
            bits = s[4 + r]
            for c in range(4):
                pix[(sy * 4 + r) * 8 + sx * 4 + c] = cols[(bits >> (6 - 2 * c)) & 3]
    return pix


def decode_block(fmt, blk, pal):
    if fmt in (0, 8):
        idx = []
        for b in blk:
            idx += (b >> 4, b & 0xF)
        if fmt == 0:
            return [(i * 17, i * 17, i * 17, 255) for i in idx]
        return [pal[min(i, len(pal) - 1)] for i in idx]
    if fmt == 1:
        return [(i, i, i, 255) for i in blk]
    if fmt == 9:
        return [pal[min(i, len(pal) - 1)] for i in blk]
    if fmt == 2:
        return [((b & 0xF) * 17, (b & 0xF) * 17, (b & 0xF) * 17, (b >> 4) * 17) for b in blk]
    if fmt == 3:
        return [(blk[k + 1], blk[k + 1], blk[k + 1], blk[k]) for k in range(0, 32, 2)]
    if fmt == 4:
        return [rgb565(v) for v in struct.unpack(">16H", blk)]
    if fmt == 5:
        return [rgb5a3(v) for v in struct.unpack(">16H", blk)]
    if fmt == 10:
        return [pal[min(v & 0x3FFF, len(pal) - 1)] for v in struct.unpack(">16H", blk)]
    if fmt == 6:
        return [(blk[2 * k + 1], blk[32 + 2 * k], blk[32 + 2 * k + 1], blk[2 * k]) for k in range(16)]
    return decode_cmpr(blk)


def decode_texture(fmt, w, h, data, pal=None):
    bw, bh, bs = GX_BLOCK[fmt]
    bx, by = (w + bw - 1) // bw, (h + bh - 1) // bh
    n = bx * by * bs
    raw = bytes(data[:n]).ljust(n, b"\0")
    W, H = bx * bw, by * bh
    out = bytearray(W * H * 4)
    pos = 0
    for byi in range(by):
        for bxi in range(bx):
            pix = decode_block(fmt, raw[pos:pos + bs], pal)
            pos += bs
            for i, p in enumerate(pix):
                yy, xx = byi * bh + i // bw, bxi * bw + i % bw
                o = (yy * W + xx) * 4
                out[o:o + 4] = bytes(p)
    if W != w or H != h:
        cut = bytearray(w * h * 4)
        for y in range(h):
            cut[y * w * 4:(y + 1) * w * 4] = out[y * W * 4:y * W * 4 + w * 4]
        out = cut
    if fmt in (0, 1):
        for i in range(0, len(out), 4):
            out[i + 3] = out[i]
    return out


def parse_tpl(data):
    tbl = struct.unpack_from(">I", data, 8)[0]
    ih, ph = struct.unpack_from(">II", data, tbl)
    h, w, fmt, doff = struct.unpack_from(">HHII", data, ih)
    pal = None
    if fmt in (8, 9, 10):
        cnt, _, _, pfmt, poff = struct.unpack_from(">HBBII", data, ph)
        pal = decode_palette(pfmt, data[poff:poff + cnt * 2])
    return Tex(w, h, decode_texture(fmt, w, h, data[doff:doff + tex_size(fmt, w, h)], pal), fmt)


def latin_str(b, o, n=None):
    e = b.index(b"\0", o) if n is None else o + n
    s = b[o:e]
    z = s.find(b"\0")
    if z >= 0:
        s = s[:z]
    return s.decode("latin-1")


def rgba_at(b, o):
    return list(b[o:o + 4])


def material_name(mats, i):
    return mats[i]["name"] if i < len(mats) else i


def texcoords(b, o, n):
    return [[list(struct.unpack_from(">2f", b, o + k * 32 + c * 8)) for c in range(4)] for k in range(n)]


def parse_material(b, m, textures):
    bits = struct.unpack_from(">I", b, m + 60)[0]
    q = m + 64
    texmaps = []
    for _ in range(bits & 0xF):
        idx, ws, wt = struct.unpack_from(">HBB", b, q)
        tex = textures[idx] if idx < len(textures) else idx
        texmaps.append({"texture": tex, "wrap_s": ws & 3, "wrap_t": wt & 3})
        q += 4
    srts = []
    for _ in range((bits >> 4) & 0xF):
        srts.append(list(struct.unpack_from(">5f", b, q)))
        q += 20
    tcg = []
    for _ in range((bits >> 8) & 0xF):
        tcg.append({"mtx": b[q + 2]})
        q += 4
    chan = None
    if (bits >> 25) & 1:
        chan = {"color_src": "material" if b[q] == 0 else "vertex",
                "alpha_src": "material" if b[q + 1] == 0 else "vertex"}
        q += 4
    matcol = None
    if (bits >> 27) & 1:
        matcol = rgba_at(b, q)
        q += 4
    q += 4 if (bits >> 12) & 1 else 0
    q += 20 * ((bits >> 13) & 3)
    q += 4 * ((bits >> 15) & 7)
    q += 16 * ((bits >> 18) & 0x1F)
    q += 4 if (bits >> 23) & 1 else 0
    tev = [list(struct.unpack_from(">4h", b, m + 20 + i * 8)) for i in range(3)]
    return {"blend_mode": list(b[q:q + 4]) if (bits >> 24) & 1 else None, "name": latin_str(b, m, 20),
            "tev_color0_black": tev[0], "tev_color1_white": tev[1], "tev_color2": tev[2],
            "kcolors": [rgba_at(b, m + 44 + i * 4) for i in range(4)], "texmaps": texmaps, "tex_srt": srts,
            "texcoord_gen": tcg, "chan_ctrl": chan, "mat_color": matcol}


def parse_pane(b, p, kind, mats, fonts):
    flag = b[p + 8]
    d = {"kind": PANE_KINDS[kind], "name": latin_str(b, p + 12, 16), "visible": bool(flag & 1),
         "influenced_alpha": bool(flag & 2), "base_position": b[p + 9], "alpha": b[p + 10]}
    tx, ty, tz, rx, ry, rz, sx, sy, w, h = struct.unpack_from(">10f", b, p + 36)
    d["translate"] = [tx, ty, tz]
    d["rotate"] = [rx, ry, rz]
    d["scale"] = [sx, sy]
    d["size"] = [w, h]
    q = p + 0x4C
    if kind == b"pic1":
        d["vertex_colors"] = [rgba_at(b, q + i * 4) for i in range(4)]
        mi, ntc = struct.unpack_from(">HB", b, q + 16)
        d["material"] = material_name(mats, mi)
        d["material_index"] = mi
        d["texcoords"] = texcoords(b, q + 20, ntc)
    elif kind == b"txt1":
        strb, mi, fi, tpos, talign = struct.unpack_from(">HHHBB", b, q + 2)
        soff = struct.unpack_from(">I", b, q + 12)[0]
        d["material"] = material_name(mats, mi)
        d["material_index"] = mi
        d["font"] = fonts[fi] if fi < len(fonts) else fi
        d["text_position_raw"] = tpos
        d["text_alignment"] = ["synchronous", "left", "center", "right"][talign & 3]
        d["text_colors_top_bottom"] = [rgba_at(b, q + 16), rgba_at(b, q + 20)]
        fw, fh, cs, ls = struct.unpack_from(">4f", b, q + 24)
        d["font_size"] = [fw, fh]
        d["char_space"] = cs
        d["line_space"] = ls
        d["text"] = b[p + soff:p + soff + strb].decode("utf-16-be", "replace").rstrip("\0") if strb else ""
    elif kind == b"wnd1":
        d["inflation_lrtb"] = list(struct.unpack_from(">4f", b, q))
        nfr = b[q + 16]
        coff, ftoff = struct.unpack_from(">II", b, q + 20)
        c = p + coff
        cmi, ntc = struct.unpack_from(">HB", b, c + 16)
        d["content"] = {"vertex_colors": [rgba_at(b, c + i * 4) for i in range(4)],
                        "material": material_name(mats, cmi), "material_index": cmi,
                        "texcoords": texcoords(b, c + 20, ntc)}
        frames = []
        for i in range(nfr):
            fo = struct.unpack_from(">I", b, p + ftoff + i * 4)[0]
            fmi, flip = struct.unpack_from(">HB", b, p + fo)
            frames.append({"material": material_name(mats, fmi), "material_index": fmi, "flip": flip})
        d["frames"] = frames
    d["children"] = []
    return d


def parse_brlyt(b):
    hs, nsec = struct.unpack_from(">HH", b, 12)
    out = {"textures": [], "fonts": [], "materials": []}
    p = hs
    stack, last, root = [], None, None
    for _ in range(nsec):
        if p + 8 > len(b):
            break
        kind, size = struct.unpack_from(">4sI", b, p)
        if kind == b"lyt1":
            out["width"], out["height"] = struct.unpack_from(">2f", b, p + 12)
        elif kind in (b"txl1", b"fnl1"):
            n = struct.unpack_from(">H", b, p + 8)[0]
            base = p + 12
            names = [latin_str(b, base + struct.unpack_from(">I", b, base + i * 8)[0]) for i in range(n)]
            out["textures" if kind == b"txl1" else "fonts"] = names
        elif kind == b"mat1":
            n = struct.unpack_from(">H", b, p + 8)[0]
            for i in range(n):
                mo = struct.unpack_from(">I", b, p + 12 + i * 4)[0]
                out["materials"].append(parse_material(b, p + mo, out["textures"]))
        elif kind in PANE_KINDS:
            pane = parse_pane(b, p, kind, out["materials"], out["fonts"])
            if stack:
                stack[-1]["children"].append(pane)
            elif root is None:
                root = pane
            last = pane
        elif kind == b"pas1":
            stack.append(last)
        elif kind == b"pae1":
            stack.pop()
        p += size
    out["root"] = root
    return out


def parse_track(b, ta, kind):
    tid, tgt, curve, _, nkey, _, koff = struct.unpack_from(">BBBBHHI", b, ta)
    keys = []
    for kk in range(nkey):
        if curve == 2:
            keys.append(list(struct.unpack_from(">3f", b, ta + koff + kk * 12)))
        else:
            keys.append(list(struct.unpack_from(">fH", b, ta + koff + kk * 8)))
    names = ANIM_TARGETS.get(kind, [])
    return {"kind": kind, "index": tid, "target": names[tgt] if tgt < len(names) else tgt, "target_id": tgt,
            "curve": "hermite" if curve == 2 else "step", "keys": keys}


def parse_content(b, c):
    cont = {"name": latin_str(b, c, 20), "type": "material" if b[c + 21] else "pane", "tracks": []}
    for j in range(b[c + 20]):
        ia = c + u32(b, c + 24 + j * 4)
        k = b[ia:ia + 4].decode("latin-1")
        for t in range(b[ia + 4]):
            cont["tracks"].append(parse_track(b, ia + u32(b, ia + 8 + t * 4), k))
    return cont


def parse_brlan(b):
    hs, nsec = struct.unpack_from(">HH", b, 12)
    out = {"frame_size": 0, "files": [], "contents": []}
    p = hs
    for _ in range(nsec):
        kind, size = struct.unpack_from(">4sI", b, p)
        if kind == b"pai1":
            fsz, _, _, nfile, ncont, coff = struct.unpack_from(">HBBHHI", b, p + 8)
            out["frame_size"] = fsz
            fb = p + 0x14
            out["files"] = [latin_str(b, fb + u32(b, fb + i * 4)) for i in range(nfile)]
            for i in range(ncont):
                out["contents"].append(parse_content(b, p + u32(b, p + coff + i * 4)))
        p += size
    return out


def sample_track(tr, f):
    keys = tr["keys"]
    if not keys:
        return None
    if tr["curve"] == "step":
        v = keys[0][1]
        for k in keys:
            if k[0] <= f:
                v = k[1]
        return v
    if f <= keys[0][0]:
        return keys[0][1]
    if f >= keys[-1][0]:
        return keys[-1][1]
    for a, c in zip(keys, keys[1:]):
        if a[0] <= f <= c[0]:
            d = c[0] - a[0]
            if d <= 0:
                return c[1]
            t = (f - a[0]) / d
            t2, t3 = t * t, t * t * t
            h1 = 2 * t3 - 3 * t2 + 1
            h2 = -2 * t3 + 3 * t2
            h3 = t3 - 2 * t2 + t
            h4 = t3 - t2
            return h1 * a[1] + h2 * c[1] + (h3 * a[2] + h4 * c[2]) * d
    return keys[-1][1]


class Font:
    def __init__(self, b):
        hs, nsec = struct.unpack_from(">HH", b, 12)
        self.widths = {}
        self.cmap = {}
        self.sheet_raw = []
        self.sheets = {}
        p = hs
        for _ in range(nsec):
            if p + 8 > len(b):
                break
            kind, size = struct.unpack_from(">4sI", b, p)
            if kind == b"FINF":
                self.read_finf(b, p)
            elif kind == b"TGLP":
                self.read_tglp(b, p)
            elif kind == b"CWDH":
                first, last = struct.unpack_from(">HH", b, p + 8)
                for i in range(last - first + 1):
                    self.widths[first + i] = struct.unpack_from(">bBb", b, p + 16 + i * 3)
            elif kind == b"CMAP":
                self.read_cmap(b, p)
            p += size

    def read_finf(self, b, p):
        fields = struct.unpack_from(">BbHbBbBIIIBBB", b, p + 8)
        self.alter_char_index = fields[2]
        self.default_width = fields[3:6]
        self.height, self.width, self.ascent = fields[10:13]

    def read_tglp(self, b, p):
        fields = struct.unpack_from(">BBbBIHHHHHHI", b, p + 8)
        self.cell_width, self.cell_height = fields[:2]
        ssize, snum = fields[4:6]
        self.fmt = fields[6] & 0x7FFF
        self.sheet_row, self.sheet_line, self.sheet_width, self.sheet_height = fields[7:11]
        simg = fields[11]
        self.sheet_raw = [b[simg + i * ssize:simg + (i + 1) * ssize] for i in range(snum)]

    def read_cmap(self, b, p):
        cb, ce, meth = struct.unpack_from(">HHH", b, p + 8)
        q = p + 20
        if meth == 0:
            off = struct.unpack_from(">H", b, q)[0]
            for c in range(cb, ce + 1):
                self.cmap[c] = c - cb + off
        elif meth == 1:
            for c in range(cb, ce + 1):
                gi = struct.unpack_from(">H", b, q + (c - cb) * 2)[0]
                if gi != 0xFFFF:
                    self.cmap[c] = gi
        else:
            for i in range(struct.unpack_from(">H", b, q)[0]):
                c, gi = struct.unpack_from(">HH", b, q + 2 + i * 4)
                self.cmap[c] = gi

    def fh(self):
        return self.height or self.cell_height

    def fw(self):
        return self.width or self.cell_width

    def sheet(self, s):
        if s not in self.sheets:
            self.sheets[s] = decode_texture(self.fmt, self.sheet_width, self.sheet_height, self.sheet_raw[s])
        return self.sheets[s]

    def char_index(self, ch):
        gi = self.cmap.get(ord(ch))
        return self.alter_char_index if gi is None else gi

    def width_of(self, gi):
        return self.widths.get(gi, self.default_width)

    def glyph(self, gi):
        s, r = divmod(gi, self.sheet_row * self.sheet_line)
        ln, col = divmod(r, self.sheet_row)
        if s >= len(self.sheet_raw):
            return None
        return s, col * (self.cell_width + 1), ln * (self.cell_height + 1)


def arc_find_suffix(arc, suffix):
    suffix = suffix.lower()
    return [(p.rsplit("/", 1)[1], yaz0_decompress(b)) for p, b in arc.files if p.lower().endswith(suffix)]


def load_fonts(arc):
    return {name.lower(): Font(data) for name, data in arc_find_suffix(arc, ".brfnt")}


def load_layout(arc):
    textures = {name.lower(): parse_tpl(data) for name, data in arc_find_suffix(arc, ".tpl")}
    anims = {}
    for name, data in arc_find_suffix(arc, ".brlan"):
        anims[os.path.splitext(name)[0].lower()] = parse_brlan(data)
    return {"layout": parse_brlyt(arc_find_suffix(arc, ".brlyt")[0][1]), "textures": textures, "anims": anims}


def iter_panes(pane):
    yield pane
    for c in pane["children"]:
        yield from iter_panes(c)


def find_pane(lay, name):
    for p in iter_panes(lay["root"]):
        if p["name"] == name:
            return p
    return None


def set_text_recursive(lay, pane_name, text):
    p = find_pane(lay, pane_name)
    if p is None:
        return
    for q in iter_panes(p):
        if q["kind"] == "textbox":
            q["text"] = text


def deep(x):
    return json.loads(json.dumps(x))


def language_pane(name):
    return len(name) >= 4 and name[-4:] in LANGS


def apply_language(lay, lang):
    lay = deep(lay)

    def rec(p):
        kids = p["children"]
        cur = [c for c in kids if len(c["name"]) >= 4 and c["name"][-4:] == lang]
        if cur:
            for c in cur:
                c["name"] = c["name"][:-4]
            p["children"] = cur
        else:
            p["children"] = [c for c in kids if not language_pane(c["name"])]
        for c in p["children"]:
            rec(c)

    rec(lay["root"])
    return lay


def anim_scope(pane):
    scope = set()
    for q in iter_panes(pane):
        scope.add(q["name"])
        if "material" in q:
            scope.add(q["material"])
        if "content" in q:
            scope.add(q["content"]["material"])
    return scope


def animate_pane(p, k, tg, v):
    if k == "RLPA":
        if tg < 3:
            p["translate"][tg] = v
        elif tg < 6:
            p["rotate"][tg - 3] = v
        elif tg < 8:
            p["scale"][tg - 6] = v
        elif tg < 10:
            p["size"][tg - 8] = v
    elif k == "RLVC":
        vv = int(max(0, min(255, round(v))))
        if tg == 16:
            p["alpha"] = vv
        elif "vertex_colors" in p:
            p["vertex_colors"][tg // 4][tg % 4] = vv
        elif "content" in p:
            p["content"]["vertex_colors"][tg // 4][tg % 4] = vv
    elif k == "RLVI":
        p["visible"] = bool(v)


def animate_material(m, tr, v, files):
    k, tg = tr["kind"], tr["target_id"]
    if k == "RLMC":
        vv = int(round(v))
        if tg < 4:
            if m["mat_color"] is None:
                m["mat_color"] = [255, 255, 255, 255]
            m["mat_color"][tg] = max(0, min(255, vv))
        elif tg < 16:
            key = ["tev_color0_black", "tev_color1_white", "tev_color2"][(tg - 4) // 4]
            m[key][(tg - 4) % 4] = vv
        else:
            m["kcolors"][(tg - 16) // 4][(tg - 16) % 4] = max(0, min(255, vv))
    elif k == "RLTS":
        if tr["index"] < len(m["tex_srt"]) and tg < 5:
            m["tex_srt"][tr["index"]][tg] = v
    elif k == "RLTP":
        fi = int(v)
        if fi < len(files) and tr["index"] < len(m["texmaps"]):
            m["texmaps"][tr["index"]]["texture"] = files[fi]


def apply_anims(lay, states):
    lay = deep(lay)
    panes = {p["name"]: p for p in iter_panes(lay["root"])}
    mats = {m["name"]: m for m in lay["materials"]}
    for st in states:
        anim, frame = st[0], st[1]
        scope = None
        if len(st) > 2 and st[2] in panes:
            scope = anim_scope(panes[st[2]])
        for c in anim["contents"]:
            if scope is not None and c["name"] not in scope:
                continue
            for tr in c["tracks"]:
                v = sample_track(tr, frame)
                if v is None:
                    continue
                if c["type"] == "pane" and c["name"] in panes:
                    animate_pane(panes[c["name"]], tr["kind"], tr["target_id"], v)
                elif c["type"] == "material" and c["name"] in mats:
                    animate_material(mats[c["name"]], tr, v, anim["files"])
    return lay


def mat_mul(a, b):
    out = []
    for i in range(4):
        row = []
        for j in range(4):
            row.append(fma(a[i][2], b[2][j], a[i][0] * b[0][j]) + fma(a[i][3], b[3][j], a[i][1] * b[1][j]))
        out.append(row)
    return out


def mat_vec(m, v):
    return [(m[i][0] * v[0] + m[i][2] * v[2]) + (m[i][1] * v[1] + m[i][3] * v[3]) for i in range(4)]


def rotation(axis, a):
    s, c = TRIG.sincos(a)
    m = [row[:] for row in EYE]
    if axis == "x":
        m[1][1], m[1][2], m[2][1], m[2][2] = c, -s, s, c
    elif axis == "y":
        m[0][0], m[0][2], m[2][0], m[2][2] = c, s, -s, c
    else:
        m[0][0], m[0][1], m[1][0], m[1][1] = c, -s, s, c
    return m


def translation(x, y, z=0.0):
    m = [row[:] for row in EYE]
    m[0][3], m[1][3], m[2][3] = x, y, z
    return m


def mat4(p):
    rx, ry, rz = [radians(a) for a in p["rotate"]]
    sx, sy = p["scale"]
    scale = [[sx, 0.0, 0.0, 0.0], [0.0, sy, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]]
    m = mat_mul(translation(*p["translate"]), rotation("z", rz))
    m = mat_mul(mat_mul(m, rotation("y", ry)), rotation("x", rx))
    return mat_mul(m, scale)


def local_rect(p):
    w, h = p["size"]
    bp = p["base_position"]
    x0 = [0.0, -w / 2, -w][bp % 3]
    y0 = [0.0, h / 2, h][bp // 3 % 3]
    return x0, y0, w, h


def corners(p, M, W, H):
    x0, y0, w, h = local_rect(p)
    pts = []
    for x, y in ((x0, y0), (x0 + w, y0), (x0, y0 - h), (x0 + w, y0 - h)):
        v = mat_vec(M, [x, y, 0.0, 1.0])
        pts.append((W / 2 + v[0], H / 2 - v[1]))
    return pts


def inv2(a, b, c, d):
    swap = abs(c) > abs(a)
    if swap:
        a, b, c, d = c, d, a, b
    l = c * (1.0 / a)
    u22 = d - l * b
    cols = []
    for e in ((1.0, 0.0), (0.0, 1.0)):
        y0, y1 = (e[1], e[0]) if swap else e
        y1 = y1 - y0 * l
        x1 = y1 * (1.0 / u22)
        y0 = y0 - x1 * b
        x0 = y0 * (1.0 / a)
        cols.append((x0, x1))
    return [cols[0][0], cols[1][0], cols[0][1], cols[1][1]]


def unit_colors(c):
    return dbuf([min(max(v / 255.0, 0.0), 1.0) for v in c])


def flat(rows):
    return dbuf([float(v) for row in rows for v in row])


def draw_quad(canvas, cs, tex, uvs, vcols, wrap=(0, 0), c0=None, c1=None, mask=None, mask_uvs=None,
              mask_wrap=(0, 0), additive=False, mask_invert=False):
    W, H = canvas.w, canvas.h
    xs = [c[0] for c in cs]
    ys = [c[1] for c in cs]
    xmin, ymin = max(math.floor(min(xs)), 0), max(math.floor(min(ys)), 0)
    xmax, ymax = min(math.ceil(max(xs)), W), min(math.ceil(max(ys)), H)
    if xmax <= xmin or ymax <= ymin:
        return
    ox, oy = cs[0]
    a, b = cs[1][0] - ox, cs[2][0] - ox
    c, d = cs[1][1] - oy, cs[2][1] - oy
    if abs(a * d - b * c) < 1e-9:
        return
    tb = uvb = ca = cb = mb = muvb = None
    tw = th = mw = mh = 0
    if tex is not None:
        tb, tw, th = tex.rgba, tex.w, tex.h
        uvb = flat(uvs)
        if c0 is not None:
            ca, cb = unit_colors(c0), unit_colors(c1)
        if mask is not None:
            mb, mw, mh = mask.rgba, mask.w, mask.h
            muvb = flat(mask_uvs if mask_uvs is not None else uvs)
    LIB.draw_quad(canvas.d, W, xmin, ymin, xmax, ymax, dbuf([ox, oy]), dbuf(inv2(a, b, c, d)), ptr(tb), tw,
                  th, uvb, flat(vcols), wrap[0], wrap[1], ca, cb, ptr(mb), mw, mh, muvb, mask_wrap[0],
                  mask_wrap[1], int(bool(additive)), int(bool(mask_invert)))


def material_texture(mat, textures, slot=0):
    if not mat or len(mat["texmaps"]) <= slot:
        return None, (0, 0)
    tm = mat["texmaps"][slot]
    t = textures.get(str(tm["texture"]).lower())
    if t is None:
        return None, (0, 0)
    return t, (tm["wrap_s"], tm["wrap_t"])


def uv_xform(mat, uvs, slot):
    if not mat or slot >= len(mat.get("texcoord_gen", [])):
        return uvs
    m = mat["texcoord_gen"][slot]["mtx"]
    if m == 60 or m < 30:
        return uvs
    k = (m - 30) // 3
    if k >= len(mat["tex_srt"]):
        return uvs
    tx, ty, rot, sx, sy = mat["tex_srt"][k]
    sr, cr = TRIG.sincos(radians(rot))
    a00, a01 = cr * sx, -sr * sy
    a10, a11 = sr * sx, cr * sy
    b0 = tx + 0.5 - a00 * 0.5 - a01 * 0.5
    b1 = ty + 0.5 - a10 * 0.5 - a11 * 0.5
    return [[a00 * u + a01 * v + b0, a10 * u + a11 * v + b1] for u, v in uvs]


def ras_colors(mat, vcols, alpha):
    vc = [list(c) for c in vcols]
    if mat and mat.get("chan_ctrl"):
        mc = mat.get("mat_color") or [255, 255, 255, 255]
        if mat["chan_ctrl"]["color_src"] == "material":
            for c in vc:
                c[:3] = mc[:3]
        if mat["chan_ctrl"]["alpha_src"] == "material":
            for c in vc:
                c[3] = mc[3]
    for c in vc:
        c[3] = c[3] * alpha / 255.0
    return vc


class TextStyle:
    def __init__(self, p, font, pfont):
        self.p, self.font, self.pfont = p, font, pfont
        self.sx = p["font_size"][0] / font.fw()
        self.sy = p["font_size"][1] / font.fh()

    def glyph(self, ch):
        o = ord(ch)
        if PICTURE_CHAR_BASE <= o < PICTURE_CHAR_BASE + 0x100 and self.pfont is not None:
            s = self.p["font_size"][1] / self.pfont.fh()
            return self.pfont, self.pfont.char_index(chr(0x30 + o - PICTURE_CHAR_BASE)), s, s, -2.0
        return self.font, self.font.char_index(ch), self.sx, self.sy, 0.0

    def line_width(self, line):
        x = 0.0
        for i, ch in enumerate(line):
            font, gi, sx, _, _ = self.glyph(ch)
            x += font.width_of(gi)[2] * sx + (self.p.get("char_space", 0) if i < len(line) - 1 else 0)
        return x


def blit_char(img, font, gi, sx, sy, x0, y0, colors):
    g = font.glyph(gi)
    gw = font.width_of(gi)[1]
    if g is None or gw <= 0:
        return
    s, gx0, gy0 = g
    gcw = min(max(gw, 1), font.cell_width, font.sheet_width - gx0)
    gch = min(font.cell_height, font.sheet_height - gy0)
    gh_px = max(int(round(gch * sy)), 1)
    gw_px = max(int(round(gcw * sx)), 1)
    yi = ibuf([min(max(int(i / sy), 0), gch - 1) for i in range(gh_px)])
    xi = ibuf([min(max(int(i / sx), 0), gcw - 1) for i in range(gw_px)])
    sheet = font.sheet(s)
    cell = ptr(sheet, (gy0 * font.sheet_width + gx0) * 4)
    LIB.blit_glyph(img.d, img.w, img.h, cell, font.sheet_width, gh_px, gw_px, yi, xi, *colors, x0, y0)


def render_text(p, font, pad, pfont, c0, c1):
    w, h = max(int(round(p["size"][0])), 1), max(int(round(p["size"][1])), 1)
    img = Img(w + 2 * pad, h + 2 * pad)
    text = "".join(ch for ch in p.get("text", "") if ch == "\n" or ord(ch) >= 0x20)
    if not text or font is None or not font.sheet_raw:
        return img
    style = TextStyle(p, font, pfont)
    line_h = font.fh() * style.sy + p.get("line_space", 0)
    lines = text.split("\n")
    widths = [style.line_width(ln) for ln in lines]
    block_w, block_h = max(widths), line_h * len(lines)
    tpos = p.get("text_position_raw", 4)
    hpos, vpos = tpos % 3, tpos // 3 % 3
    bx = [0, (w - block_w) / 2, w - block_w][hpos]
    by = [0, (h - block_h) / 2, h - block_h][vpos]
    amode = {"left": 0, "center": 1, "right": 2}.get(p.get("text_alignment", "synchronous"), hpos)
    top = dbuf([v / 255.0 for v in p["text_colors_top_bottom"][0]])
    bot = dbuf([v / 255.0 for v in p["text_colors_top_bottom"][1]])
    ca = cb = None
    if c0 is not None:
        ca, cb = unit_colors(c0), unit_colors(c1)
    for li, line in enumerate(lines):
        x = bx + [0, (block_w - widths[li]) / 2, block_w - widths[li]][amode]
        y = by + li * line_h
        for ch in line:
            f, gi, sx, sy, oy = style.glyph(ch)
            style.sx, style.sy = sx, sy
            left, _, cw = f.width_of(gi)
            colors = (ca, cb, top, bot) if f is font else (None, None, top, bot)
            blit_char(img, f, gi, sx, sy, int(round(x + left * sx)) + pad, int(round(y + oy)) + pad, colors)
            x += cw * sx + p.get("char_space", 0)
    return img


def tex_of_img(img):
    n = img.w * img.h * 4
    out = bytearray(n)
    LIB.to_u8(img.d, ptr(out), n)
    return Tex(img.w, img.h, out)


def offset_quad(p, M, x, y, w, h):
    q = dict(p, base_position=0, translate=[0, 0, 0], rotate=[0, 0, 0], scale=[1, 1], size=[w, h])
    return q, mat_mul(M, translation(x, y))


class Renderer:
    def __init__(self, lay, textures, fonts):
        self.textures, self.fonts = textures, fonts
        self.mats = lay["materials"]
        self.canvas = Img(int(lay.get("width", 608)), int(lay.get("height", 456)))

    def mat_at(self, i):
        return self.mats[i] if isinstance(i, int) and i < len(self.mats) else None

    def quad(self, p, M, *args, **kw):
        draw_quad(self.canvas, corners(p, M, self.canvas.w, self.canvas.h), *args, **kw)

    def picture(self, p, M, alpha):
        mat = self.mat_at(p["material_index"])
        tex, wrap = material_texture(mat, self.textures)
        raw = p["texcoords"][0] if p["texcoords"] else UNIT_UVS
        uvs = uv_xform(mat, raw, 0)
        c0 = mat["tev_color0_black"] if mat else None
        c1 = mat["tev_color1_white"] if mat else None
        mask, mwrap = material_texture(mat, self.textures, 1)
        muvs = uv_xform(mat, p["texcoords"][1] if len(p["texcoords"]) > 1 else raw, 1)
        add = bool(mat and mat.get("blend_mode") and mat["blend_mode"][2] == 1)
        invert = bool(mat) and mat["name"] in MASK_INVERT
        vcols = ras_colors(mat, p["vertex_colors"], alpha)
        self.quad(p, M, tex, uvs, vcols, wrap, c0, c1, mask, muvs, mwrap, add, invert)

    def window(self, p, M, alpha):
        cmat = self.mat_at(p["content"]["material_index"])
        l, r, t, b = p["inflation_lrtb"]
        x0, y0, w, h = local_rect(p)
        q, Mq = offset_quad(p, M, x0 - l, y0 + t, w + l + r, h + t + b)
        tex, wrap = material_texture(cmat, self.textures)
        uvs = p["content"]["texcoords"][0] if p["content"]["texcoords"] else UNIT_UVS
        self.quad(q, Mq, tex, uvs, ras_colors(cmat, p["content"]["vertex_colors"], alpha), wrap,
                  cmat["tev_color0_black"] if cmat else None, cmat["tev_color1_white"] if cmat else None)
        frames = p["frames"]
        if not frames:
            return
        for fi, (cx, cy) in enumerate(((0, 0), (1, 0), (0, 1), (1, 1))):
            fmat = self.mat_at(frames[fi if len(frames) == 4 else 0]["material_index"])
            ftex, _ = material_texture(fmat, self.textures)
            if ftex is None:
                continue
            qx = x0 + (w - ftex.w if cx else 0)
            qy = y0 - (h - ftex.h if cy else 0)
            fq, Mf = offset_quad(p, M, qx, qy, ftex.w, ftex.h)
            if len(frames) == 1:
                u0, u1 = (1, 0) if cx else (0, 1)
                v0, v1 = (1, 0) if cy else (0, 1)
            else:
                u0, u1, v0, v1 = 0, 1, 0, 1
            self.quad(fq, Mf, ftex, [[u0, v0], [u1, v0], [u0, v1], [u1, v1]],
                      ras_colors(fmat, [[255] * 4] * 4, alpha), (0, 0), fmat["tev_color0_black"],
                      fmat["tev_color1_white"])

    def textbox(self, p, M, alpha):
        pad = 48
        mat = self.mat_at(p["material_index"])
        font = self.fonts.get(str(p.get("font", "")).lower())
        timg = render_text(p, font, pad, self.fonts.get("picturefont.brfnt"),
                           mat["tev_color0_black"] if mat else None, mat["tev_color1_white"] if mat else None)
        x0, y0, w, h = local_rect(p)
        w, h = max(int(round(w)), 1), max(int(round(h)), 1)
        q, Mq = offset_quad(p, M, x0 - pad, y0 + pad, w + 2 * pad, h + 2 * pad)
        self.quad(q, Mq, tex_of_img(timg), UNIT_UVS, [[255, 255, 255, alpha]] * 4, (0, 0))

    def walk(self, p, parent_m, parent_alpha, influenced):
        M = mat_mul(parent_m, mat4(p))
        alpha = p["alpha"] * parent_alpha / 255.0 if influenced else float(p["alpha"])
        if not p["visible"]:
            return
        if p["kind"] == "picture":
            self.picture(p, M, alpha)
        elif p["kind"] == "window":
            self.window(p, M, alpha)
        elif p["kind"] == "textbox":
            self.textbox(p, M, alpha)
        for c in p["children"]:
            self.walk(c, M, alpha, p["influenced_alpha"])


def render(bundle, fonts, anim_states=(), lang="UsEn", layout=None):
    lay = layout or bundle["layout"]
    if lang:
        lay = apply_language(lay, lang)
    if anim_states:
        lay = apply_anims(lay, anim_states)
    r = Renderer(lay, bundle["textures"], fonts)
    r.walk(lay["root"], EYE, 255.0, False)
    return r.canvas
