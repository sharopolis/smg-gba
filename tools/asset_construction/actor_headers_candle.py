import struct
from operator import add

from actor_headers_lib import BCSV
from common import f32, s16, u16, u32

ARC = "ParticleData/Effect.arc"
GROUP = "CandlestandIceVolcano"
BLOCK = {0: (8, 8, 32), 1: (8, 4, 32), 2: (8, 4, 32), 3: (4, 4, 32), 4: (4, 4, 32), 5: (4, 4, 32)}


class Resource:
    def __init__(self, jpc, off):
        self.id, nblocks = struct.unpack_from(">HH", jpc, off)
        self.blocks = {}
        o = off + 8
        for _ in range(nblocks):
            size = u32(jpc, o + 4)
            self.blocks.setdefault(jpc[o:o + 4].decode("ascii"), jpc[o:o + size])
            o += size
        self.size = o - off
        self.raw = jpc[off:o]


class JPC:
    def __init__(self, jpc):
        if jpc[:8] != b"JPAC2-10":
            raise ValueError("not a JPAC2-10 resource")
        nres, ntex, tex_off = struct.unpack_from(">HHI", jpc, 8)
        self.resources = {}
        o = 0x10
        for _ in range(nres):
            r = Resource(jpc, o)
            self.resources[r.id] = r
            o += r.size
        self.textures = []
        o = tex_off
        for _ in range(ntex):
            size = u32(jpc, o + 4)
            self.textures.append(jpc[o + 0x20:o + size])
            o += size


def floats(b, o, n):
    return struct.unpack_from(">%df" % n, b, o)


def dynamics(b):
    return {
        "translation": floats(b, 0x1C, 3),
        "max_frame": s16(b, 0x6E),
        "lifetime": s16(b, 0x72),
    }


def color_keys(b, off, n):
    return [(s16(b, off + i * 6), tuple(b[off + i * 6 + 2:off + i * 6 + 6])) for i in range(n)]


def base_shape(b):
    tex_anm_num = b[0x1F]
    s = {
        "base_size": floats(b, 0x10, 2),
        "blend_destination": (u16(b, 0x18) >> 6) & 0xF,
        "texture_index": b[0x20],
        "color_flags": b[0x21],
        "color_frame_max": s16(b, 0x24),
        "prm_color": tuple(b[0x26:0x2A]),
        "env_color": tuple(b[0x2A:0x2E]),
        "prm_keys": color_keys(b, u16(b, 0x0C), b[0x22]),
        "env_keys": color_keys(b, u16(b, 0x0E), b[0x23]),
        "texture_order": [],
    }
    if b[0x1E] & 1 and tex_anm_num:
        o = 0x34 + (0x28 if u32(b, 0x08) & 0x1000000 else 0)
        s["texture_order"] = list(b[o:o + tex_anm_num])
    return s


def extra_shape(b):
    flags = u32(b, 0x08)
    v = floats(b, 0x0C, 6)
    a = floats(b, 0x2C, 5)
    return {
        "scale_animation": bool(flags & 1),
        "scale_in_timing": v[0],
        "scale_out_timing": v[1],
        "scale_in_values": (v[2], v[4]),
        "scale_out_values": (v[3], v[5]),
        "alpha_animation": bool(flags & 0x10000),
        "alpha_in_timing": a[0],
        "alpha_out_timing": a[1],
        "alpha_in_value": a[2],
        "alpha_base_value": a[3],
        "alpha_out_value": a[4],
    }


def texture_db(b):
    n = (u32(b, 4) - 8) // 2
    return [u16(b, 8 + i * 2) for i in range(n)]


def rgb5a3(v):
    if v & 0x8000:
        return (((v >> 10) & 0x1F) * 255 // 31, ((v >> 5) & 0x1F) * 255 // 31, (v & 0x1F) * 255 // 31, 255)
    return (((v >> 8) & 0xF) * 17, ((v >> 4) & 0xF) * 17, (v & 0xF) * 17, ((v >> 12) & 7) * 255 // 7)


def texel(fmt, data, p, k):
    if fmt == 0:
        i = ((data[p + k // 2] >> (4 - 4 * (k & 1))) & 0xF) * 17
        return (i, i, i, 255)
    if fmt == 1:
        i = data[p + k]
        return (i, i, i, 255)
    if fmt == 2:
        c = data[p + k]
        i = (c & 0xF) * 17
        return (i, i, i, (c >> 4) * 17)
    v = (data[p + 2 * k] << 8) | data[p + 2 * k + 1]
    if fmt == 3:
        i = v & 0xFF
        return (i, i, i, v >> 8)
    if fmt == 4:
        return (((v >> 11) & 0x1F) * 255 // 31, ((v >> 5) & 0x3F) * 255 // 63, (v & 0x1F) * 255 // 31, 255)
    return rgb5a3(v)


def decode_bti(bti):
    fmt = bti[0]
    w, h = u16(bti, 2), u16(bti, 4)
    data = bti[u32(bti, 0x1C):]
    bw, bh, bs = BLOCK[fmt]
    bx = (w + bw - 1) // bw
    out = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            p = ((y // bh) * bx + x // bw) * bs
            k = (y % bh) * bw + x % bw
            q = (y * w + x) * 4
            out[q:q + 4] = bytes(texel(fmt, data, p, k))
    return w, h, bytes(out)


def effect_resources(ctx):
    names = BCSV(ctx.arc_file(ARC, "particlenames.bcsv"))
    ids = {names.get(i, "name"): names.get(i, "id") for i in range(names.n)}
    auto = BCSV(ctx.arc_file(ARC, "autoeffectlist.bcsv"))
    out = {}
    for i in range(auto.n):
        if auto.get(i, "GroupName") != GROUP:
            continue
        yoff = auto.get(i, "OffsetY")
        for token in auto.get(i, "EffectName").split():
            if token in ids:
                out[ids[token]] = yoff
                continue
            k = 0
            while "%s%02d" % (token, k) in ids:
                out[ids["%s%02d" % (token, k)]] = yoff
                k += 1
    return sorted(out.items())


def color_table(keys, base, maxframe):
    if not keys:
        return [list(base)] * (maxframe + 1)
    result = []
    last = [float(c) for c in keys[0][1]]
    step = [0.0] * 4
    k = 0
    for at in range(maxframe + 1):
        if k < len(keys) and at == keys[k][0]:
            last = [float(c) for c in keys[k][1]]
            result.append([int(c) for c in last])
            k += 1
            if k < len(keys):
                inv = f32(1 / (keys[k][0] - keys[k - 1][0]))
                step = [f32(inv * f32(keys[k][1][c] - last[c])) for c in range(4)]
            else:
                step = [0.0] * 4
        else:
            last = [f32(last[c] + step[c]) for c in range(4)]
            result.append([max(0, min(255, int(x))) for x in last])
    return result


def anm(t, inside, outside, first, base, last):
    if t < inside:
        if not inside:
            return base
        return f32(first + f32(f32((base - first) / inside) * t))
    if t > outside:
        if outside == 1:
            return base
        return f32(base + f32(f32((last - base) / (1 - outside)) * f32(t - outside)))
    return base


def color_index(ctype, age, t, maxcolor):
    if ctype == 3:
        return int(f32(t * f32(maxcolor + 1))) % (maxcolor + 1)
    if ctype == 1:
        return age % (maxcolor + 1)
    if ctype == 2:
        ci = age % maxcolor if maxcolor else 0
        if maxcolor and (age // maxcolor) & 1:
            return maxcolor - ci
        return ci
    return min(age, maxcolor)


def frames(res, texpos):
    b = base_shape(res.blocks["BSP1"])
    e = extra_shape(res.blocks["ESP1"])
    d = dynamics(res.blocks["BEM1"])
    tdb = texture_db(res.blocks["TDB1"])
    life = d["lifetime"]
    maxcolor = b["color_frame_max"]
    prm = color_table(b["prm_keys"], b["prm_color"], maxcolor)
    env = color_table(b["env_keys"], b["env_color"], maxcolor)
    ctype = (b["color_flags"] >> 4) & 7
    order = b["texture_order"]
    out = []
    for age in range(life):
        t = f32(age / life)
        ci = color_index(ctype, age, t, maxcolor)
        scale = [1.0, 1.0]
        if e["scale_animation"]:
            scale = [anm(t, e["scale_in_timing"], e["scale_out_timing"], e["scale_in_values"][k], 1,
                         e["scale_out_values"][k]) for k in range(2)]
        alpha = 1.0
        if e["alpha_animation"]:
            alpha = anm(t, e["alpha_in_timing"], e["alpha_out_timing"], e["alpha_in_value"],
                        e["alpha_base_value"], e["alpha_out_value"])
        tex = b["texture_index"]
        if order:
            tex = order[int(f32(t * len(order))) % len(order)]
        out += [max(0, round(b["base_size"][k] * 25 * scale[k] * 256)) for k in range(2)]
        out += prm[ci] + env[ci]
        out.append(max(0, min(255, int(f32(alpha * 255)))))
        out.append(texpos[tdb[tex]])
    return out, d, b


def rgb5_lut(p5):
    bterm = [[(b - c[2]) * (b - c[2]) * 256 for c in p5] for b in range(32)]
    lut = []
    for r in range(32):
        rterm = [(r - c[0]) * (r - c[0]) for c in p5]
        for g in range(32):
            keys = [(rterm[i] + (g - c[1]) * (g - c[1])) * 256 + i for i, c in enumerate(p5)]
            for b in range(32):
                lut.append(min(map(add, keys, bterm[b])) & 255)
    return lut


def candle(ctx, spec):
    jpc = JPC(ctx.arc_file(ARC, "particles.jpc"))
    layers = effect_resources(ctx)
    used = sorted(set(g for rid, _ in layers for g in texture_db(jpc.resources[rid].blocks["TDB1"])))
    texpos = {g: i for i, g in enumerate(used)}
    out = []
    dims = []
    for g in used:
        bti = jpc.textures[g]
        w, h, rgba = decode_bti(bti)
        out += list(bti) + list(rgba)
        dims += [w, h, len(bti)]
    out += dims
    tail = []
    for rid, yoff in layers:
        res = jpc.resources[rid]
        vals, d, b = frames(res, texpos)
        out += [rid] + list(res.raw) + [rid] + vals
        offsets = [round((x + (yoff if k == 1 else 0)) * 256) for k, x in enumerate(d["translation"])]
        tail += [rid, rid] + offsets + [rid, d["lifetime"], d["max_frame"],
                                        int(b["blend_destination"] == 1), int(d["max_frame"] == 0)]
    out += tail
    pal = ctx.built_file("assets/palette.rgba")
    p5 = [tuple(pal[i * 4 + c] >> 3 for c in range(3)) for i in range(256)]
    out += [(v * 255 + 15) // 31 for c in p5 for v in c]
    out += rgb5_lut(p5)
    return out


PREDICTORS = {
    "src/actors/candle_effect_assets_gen.h": candle,
}
