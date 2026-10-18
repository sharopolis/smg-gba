import base64
import ctypes
import lzma
import struct

import textures_gx
import textures_tev
from common import Dump, IntReader, f32_at, load_lib, s16, u16

SKIES = {
    "astrodome": ("AstroDomeSkyA", "dome"),
    "beachbowl": ("SummerSky", "agx"),
    "buoybase": ("GalaxySky", "universe"),
    "deepdark": ("VROrbit", "agx"),
    "freezeflame": ("AuroraSky", "agx"),
    "goodegg": ("VROrbit", "agx"),
    "gustygarden": ("GreenPlanetOrbitSky", "rot180"),
}
STARS = {
    "buoybase": {"UniverseMat_v": (0.899, 1.123, 1.289)},
    "freezeflame": {"UniMat_v": (0.564, 0.967, 1.453)},
    "gustygarden": {"UniMat_v": (0.822, 0.943, 1.202)},
}
W, H = 512, 256
SIZE = 256
PI = 3.141592653589793
FINE = 12
LAYERS = 8
GRID = 48
STAR_BOX = 32
AGX_CORRECTIONS = base64.b64decode(
    "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"
)


class Sky(textures_tev.Model):
    def __init__(self, data):
        super().__init__(data)
        self.wraps = [(data[h + 6], data[h + 7]) for h in textures_gx.j3d_texture_headers(data)]
        self.arrays = {}
        self.read_vertices()
        self.shape_list = self.shapes()
        self.read_texgens()

    def read_vertices(self):
        for attr, slot in ((9, 0), (11, 3), (13, 5)):
            arr = self.vertex_array(attr, slot)
            if arr is None:
                continue
            cc, ct, frac, raw = arr
            if attr == 11:
                self.arrays[11] = textures_tev.vertex_colors(ct, raw)
                continue
            if attr == 9:
                ncomp = 3 if cc == 1 else 2
                width = 3
            else:
                ncomp = 2 if cc == 1 else 1
                width = 2
            code, isz = {0: ("B", 1), 1: ("b", 1), 2: ("H", 2), 3: ("h", 2), 4: ("f", 4)}[ct]
            cnt = len(raw) // (isz * ncomp)
            vals = struct.unpack_from(">%d%s" % (cnt * ncomp, code), raw, 0)
            div = float(1 << frac)
            arr = []
            for i in range(cnt):
                v = [float(x) if ct == 4 else x / div for x in vals[i * ncomp:(i + 1) * ncomp]]
                while len(v) < width:
                    v.append(0.0)
                arr.append(tuple(v))
            self.arrays[attr] = arr

    def read_texgens(self):
        b = self.data
        gen_tbl = self.mat_tables[11]
        mtx_tbl = self.mat_tables[13]
        for m in self.materials:
            p = m.offset
            m.texgen = []
            for k in range(8):
                tg = u16(b, p + 0x28 + k * 2)
                if tg == 0xFFFF or not gen_tbl:
                    m.texgen.append(None)
                else:
                    m.texgen.append(tuple(b[gen_tbl + tg * 4:gen_tbl + tg * 4 + 3]))
            m.texmtx = []
            for k in range(10):
                ti = u16(b, p + 0x48 + k * 2)
                if ti == 0xFFFF or not mtx_tbl:
                    m.texmtx.append(None)
                    continue
                q = mtx_tbl + ti * 0x64
                m.texmtx.append((f32_at(b, q + 4), f32_at(b, q + 8), f32_at(b, q + 0x10), f32_at(b, q + 0x14),
                                 s16(b, q + 0x18), f32_at(b, q + 0x1C), f32_at(b, q + 0x20)))

    def coord_srt(self, mi, coord):
        m = self.materials[mi]
        g = m.texgen[coord] if coord < 8 else None
        if g is None or not 30 <= g[2] < 60:
            return None
        return m.texmtx[(g[2] - 30) // 3]

    def base_coord(self, mi):
        m = self.materials[mi]
        for st in m.stages:
            o = m.order[st["idx"]]
            if o and o[1] == m.first_texmap:
                return o[0]
        return m.first_texmap

    def triangles(self):
        pos = self.arrays[9]
        col = self.arrays.get(11)
        uv = self.arrays.get(13)
        for mi, verts, tris in self.shape_list:
            for t in tris:
                vs = []
                for k in t:
                    a = verts[k]
                    p = pos[min(a.get(9, 0), len(pos) - 1)]
                    c = col[min(a[11], len(col) - 1)] if col and 11 in a else (255, 255, 255, 255)
                    w = uv[min(a[13], len(uv) - 1)] if uv and 13 in a else (0.0, 0.0)
                    vs.append((p, w, c))
                yield mi, vs


def sky_lib(bin_dir):
    lib = load_lib("textures_sky_render", bin_dir)
    p = ctypes.c_void_p
    i = ctypes.c_int
    lib.sky_render.argtypes = [i, p, p, i, p, p, i, p, p, i, i, i, p, p, i, i, i, p]
    lib.sky_finish.argtypes = [p, i, i, i, i, i, p, p, p]
    return lib


def doubles(vals):
    return (ctypes.c_double * max(1, len(vals)))(*vals)


def ints(vals):
    return (ctypes.c_int * max(1, len(vals)))(*vals)


def sincos(a):
    q = (a / (PI / 2) + 0.5) // 1
    r = a - q * (PI / 2)
    z = r * r
    ts = sv = r
    tc = cv = 1.0
    for k in range(1, 30, 2):
        ts = -ts * z / ((k + 1) * (k + 2))
        sv += ts
    for k in range(0, 30, 2):
        tc = -tc * z / ((k + 1) * (k + 2))
        cv += tc
    return [(sv, cv), (cv, -sv), (-sv, -cv), (-cv, sv)][int(q) % 4]


def filter_taps(f):
    offs = []
    wts = []
    for i in range(3 * f):
        x = -1.5 + (i + 0.5) / f
        a = 2 * PI * (x / 3.0 + 0.5)
        offs.append(x)
        wts.append(0.35875 - 0.48829 * sincos(a)[1] + 0.14128 * sincos(2 * a)[1] - 0.01168 * sincos(3 * a)[1])
    return offs, wts


def srt_matrix(srt):
    cu, cv, ss, st, rot, ts, tt = srt
    s, c = sincos(rot * PI / 32768)
    a = ss * c
    b = -ss * s
    d = st * s
    e = st * c
    return [a, b, ts + cu - (a * cu + b * cv), d, e, tt + cv - (d * cu + e * cv)]


def ln(x):
    e = 0
    while x >= 1.4142135623730951:
        x *= 0.5
        e += 1
    while x < 0.7071067811865476:
        x *= 2.0
        e -= 1
    m = (x - 1.0) / (x + 1.0)
    z = m * m
    t = m
    s = 0.0
    for k in range(1, 60, 2):
        s += t / k
        t *= z
    return 2.0 * s + e * 0.6931471805599453


def exp(x):
    n = (x / 0.6931471805599453 + 0.5) // 1
    r = x - n * 0.6931471805599453
    t = s = 1.0
    for k in range(1, 30):
        t = t * r / k
        s += t
    n = int(n)
    while n > 0:
        s *= 2.0
        n -= 1
    while n < 0:
        s *= 0.5
        n += 1
    return s


def srgb_to_linear(c):
    if c <= 0.04045:
        return c / 12.92
    return exp(2.4 * ln((c + 0.055) / 1.055))


S2L = [srgb_to_linear(i / 255) for i in range(256)]


def box_down(tex, f):
    w, h, px = tex
    nw = w // f
    nh = h // f
    out = bytearray(nw * nh * 4)
    for y in range(nh):
        for x in range(nw):
            for c in range(4):
                s = 0
                for yy in range(y * f, y * f + f):
                    o = (yy * w + x * f) * 4 + c
                    for v in px[o:o + f * 4:4]:
                        s += v
                out[(y * nw + x) * 4 + c] = (2 * s + f * f) // (2 * f * f)
    return nw, nh, bytes(out)


def universe_materials(sky, texs, mats, stars):
    for mi, m in enumerate(sky.materials):
        t0, t1 = m.tex[0], m.tex[1]
        texs[t0] = box_down(texs[t0], STAR_BOX)
        gain = stars.get(m.name, (1.0, 1.0, 1.0))
        k = list(m.konst[0][:3]) + srt_matrix(sky.coord_srt(mi, 0)) + srt_matrix(sky.coord_srt(mi, 1))
        ws1, wt1 = sky.wraps[t1]
        k.append(float(ws1 | (wt1 << 2)))
        k += [0.0] * 4 + list(gain) + [0.0]
        ws0, wt0 = sky.wraps[t0]
        mats.append(((t0, 4, ws0, wt0, t1), [0.0, 0.0, 1.0, 1.0] + k))


def dome_materials(sky, mats):
    for mi, m in enumerate(sky.materials):
        kind = {"UniverseMat_v": 2, "UnderUniverseMat_v": 3}.get(m.name)
        if kind is None:
            mats.append(None)
            continue
        srt = sky.coord_srt(mi, 1)
        k = [round(srt[2], 4), round(srt[3], 4)] + list(m.regs[0][:3]) + list(m.konst[0][:3]) + [0.0] * 16
        mats.append(((m.tex[0], kind, 1, 1, m.tex[1]), [0.0, 0.0, 1.0, 1.0] + k))


def star_colour(tex, beta):
    w, h, px = tex
    acc = [0.0, 0.0, 0.0]
    count = {}
    for i in range(w * h):
        key = bytes(px[i * 4:i * 4 + 3])
        count[key] = count.get(key, 0) + 1
        for c in range(3):
            acc[c] += S2L[px[i * 4 + c]]
    bg = max(sorted(count), key=count.get)
    return [S2L[bg[c]] + beta[c] * (acc[c] / (w * h) - S2L[bg[c]]) for c in range(3)]


def baked_materials(sky, texs, mats, stars):
    for mi, m in enumerate(sky.materials):
        if m.first_texmap is None or m.tex[m.first_texmap] is None:
            mats.append(None)
            continue
        w, h, px = textures_tev.effective_rgba(sky, mi)
        ws, wt = sky.wraps[m.tex[m.first_texmap]]
        srt = sky.coord_srt(mi, sky.base_coord(mi))
        ou, ov, su, sv = 0.0, 0.0, 1.0, 1.0
        if srt is not None:
            cu, cv, ss, st, rot, ts, tt = srt
            if (ss, st) != (1.0, 1.0) or ts or tt or rot:
                ou, ov, su, sv = cu * (1 - ss) + ts, cv * (1 - st) + tt, ss, st
        kind = 1 if ("Sun" in m.name or "CityLight" in m.name) else 0
        tex = (w, h, bytes(px))
        if m.name in stars:
            mats.append(((0, 5, 0, 0, 0), [0.0] * 4 + star_colour(tex, stars[m.name]) + [0.0] * 21))
            continue
        mats.append(((len(texs), kind, ws, wt, 0), [ou, ov, su, sv] + [0.0] * 24))
        texs.append(tex)


def render_linear(orig_root, stage, bin_dir):
    model, mode = SKIES[stage]
    low = model.lower()
    sky = Sky(Dump(orig_root).load("ObjectData/%s.arc:%s/%s.bdl" % (model, low, low)))
    texs = []
    mats = []
    if mode == "dome":
        texs = [(w, h, bytes(px)) for w, h, px in sky.textures]
        dome_materials(sky, mats)
    elif mode == "universe":
        texs = [(w, h, bytes(px)) for w, h, px in sky.textures]
        universe_materials(sky, texs, mats, STARS.get(stage, {}))
    else:
        baked_materials(sky, texs, mats, STARS.get(stage, {}))
    tv = []
    tm = []
    for mi, vs in sky.triangles():
        if mats[mi] is None:
            continue
        for p, uv, c in vs:
            tv.extend(p)
            tv.extend(uv)
            tv.extend(x / 255 for x in c)
        tm.append(mi)
    mi_flat = []
    md_flat = []
    for m in mats:
        a, b = m if m is not None else ((0, 9, 0, 0, 0), [0.0] * 28)
        mi_flat.extend(a)
        md_flat.extend(b)
    tpx = b"".join(t[2] for t in texs)
    ti = []
    for t in texs:
        ti.extend(t[:2])
    if mode == "dome":
        offs = [(o + 0.5) / 4 for o in range(4)]
        wts = [1.0] * 4
        n = 4
        rmode = 1
    else:
        offs, wts = filter_taps(FINE)
        n = FINE
        rmode = 0
    out = (ctypes.c_double * (W * H * 3))()
    tbuf = ctypes.create_string_buffer(tpx, max(1, len(tpx)))
    sky_lib(bin_dir).sky_render(len(tm), doubles(tv), ints(tm), len(mats), ints(mi_flat), doubles(md_flat),
                                len(texs), ints(ti), tbuf, W, H, n, doubles(offs), doubles(wts), rmode,
                                LAYERS, GRID, out)
    return out, mode


def agx_corrections():
    r = IntReader(lzma.decompress(AGX_CORRECTIONS))
    idx = []
    val = []
    k = 0
    for _ in range(r.uvarint()):
        k += r.uvarint()
        idx.append(k)
        val.extend(r.ints(3))
    return idx, val


def finish(lin, mode, bin_dir):
    out = bytearray(SIZE * SIZE * 4)
    if mode == "dome":
        img = [min(255, int(v * 255 + 0.5)) for v in lin]
        for y in range(H):
            for x in range(SIZE):
                o = (y * W + 2 * x) * 3
                for c in range(3):
                    out[(y * SIZE + x) * 4 + c] = (img[o + c] + img[o + 3 + c] + 1) >> 1
                out[(y * SIZE + x) * 4 + 3] = 255
        return out
    idx, val = agx_corrections()
    rgb = ctypes.create_string_buffer(SIZE * SIZE * 3)
    view = 1 if mode == "universe" else 0
    rot = 1 if mode == "rot180" else 0
    sky_lib(bin_dir).sky_finish(lin, W, H, view, rot, len(idx), ints(idx), ints(val), rgb)
    raw = rgb.raw
    for i in range(SIZE * SIZE):
        out[i * 4:i * 4 + 3] = raw[i * 3:i * 3 + 3]
        out[i * 4 + 3] = 255
    return out


def predict_sky(orig_root, stage, bin_dir):
    lin, mode = render_linear(orig_root, stage, bin_dir)
    return finish(lin, mode, bin_dir)
