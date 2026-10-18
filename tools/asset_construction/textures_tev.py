import struct

import textures_gx
from common import name_table, s16, u16, u32

KONST_FRACTIONS = {0: 1.0, 1: 7 / 8, 2: 6 / 8, 3: 5 / 8, 4: 4 / 8, 5: 3 / 8, 6: 2 / 8, 7: 1 / 8}
BIAS = {0: 0.0, 1: 0.5, 2: -0.5, 3: 0.0}
SCALE = {0: 1.0, 1: 2.0, 2: 4.0, 3: 0.5}
PRIMITIVES = (0x80, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8)


class Material:
    pass


def vertex_colors(ct, raw):
    out = []
    if ct == 0:
        for i in range(len(raw) // 2):
            v = u16(raw, i * 2)
            r = (v >> 11) & 31
            g = (v >> 5) & 63
            b = v & 31
            out.append(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), 255))
    elif ct == 1:
        for i in range(len(raw) // 3):
            out.append((raw[i * 3], raw[i * 3 + 1], raw[i * 3 + 2], 255))
    elif ct == 2:
        for i in range(len(raw) // 4):
            out.append((raw[i * 4], raw[i * 4 + 1], raw[i * 4 + 2], 255))
    elif ct == 3:
        for i in range(len(raw) // 2):
            v = u16(raw, i * 2)
            out.append((((v >> 12) & 15) * 17, ((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17))
    elif ct == 4:
        for i in range(len(raw) // 3):
            v = (raw[i * 3] << 16) | (raw[i * 3 + 1] << 8) | raw[i * 3 + 2]
            out.append(tuple(((v >> s) & 63) * 255 // 63 for s in (18, 12, 6, 0)))
    else:
        for i in range(len(raw) // 4):
            out.append(tuple(raw[i * 4:i * 4 + 4]))
    return out


class Model:
    def __init__(self, data):
        self.data = data
        self.sections = {}
        o = 0x20
        for _ in range(u32(data, 0x0C)):
            self.sections[data[o:o + 4].decode("ascii", "replace")] = (o, u32(data, o + 4))
            o += u32(data, o + 4)
        self.textures = []
        for h in textures_gx.j3d_texture_headers(data):
            self.textures.append(textures_gx.bti_levels(data, h, False)[0])
        self.read_inf1()
        self.read_mat3()
        self.colors = None
        self.shape_colors = None

    def read_inf1(self):
        b = self.data
        o, _ = self.sections["INF1"]
        h = o + u32(b, o + 0x14)
        self.shape_material = {}
        stack = []
        last = None
        while True:
            t, v = struct.unpack_from(">HH", b, h)
            h += 4
            if t == 0:
                break
            if t == 1:
                stack.append(last)
            elif t == 2:
                if stack:
                    stack.pop()
            elif t in (0x10, 0x11, 0x12):
                if t == 0x12:
                    mat = None
                    for s in reversed(stack):
                        if s is not None and s[0] == 0x11:
                            mat = s[1]
                            break
                    if mat is None and last is not None and last[0] == 0x11:
                        mat = last[1]
                    self.shape_material[v] = mat
                last = (t, v)

    def read_mat3(self):
        b = self.data
        sec = self.sections.get("MAT3") or self.sections.get("MAT2")
        self.materials = []
        if not sec:
            return
        o, _ = sec
        tbl = []
        for i in range(30):
            x = u32(b, o + 0x0C + i * 4)
            tbl.append(o + x if x else None)
        self.mat_tables = tbl
        names = name_table(b, tbl[2]) if tbl[2] else []
        for i in range(u16(b, o + 8)):
            p = tbl[0] + u16(b, tbl[1] + i * 2) * 0x14C
            self.materials.append(self.read_material(p, names[i] if i < len(names) else "mat%d" % i))

    def table_rgba(self, table, index):
        b = self.data
        tbl = self.mat_tables[table]
        if index == 0xFFFF or not tbl:
            return [1.0] * 4
        return [x / 255 for x in b[tbl + index * 4:tbl + index * 4 + 4]]

    def read_material(self, p, name):
        b = self.data
        tbl = self.mat_tables
        m = Material()
        m.name = name
        m.offset = p
        m.mat_color = self.table_rgba(5, u16(b, p + 8))
        ch = u16(b, p + 0x0C)
        m.ras_from_vertex = bool(tbl[7] and ch != 0xFFFF and b[tbl[7] + ch * 8 + 1] == 1)
        m.konst = [self.table_rgba(18, u16(b, p + 0x94 + k * 2)) for k in range(4)]
        m.kcsel = [b[p + 0x9C + k] for k in range(16)]
        m.kasel = [b[p + 0xAC + k] for k in range(16)]
        m.order = []
        for k in range(16):
            ti = u16(b, p + 0xBC + k * 2)
            if ti == 0xFFFF or not tbl[16]:
                m.order.append(None)
            else:
                q = tbl[16] + ti * 4
                m.order.append((b[q], b[q + 1], b[q + 2]))
        m.regs = []
        for k in range(4):
            ci = u16(b, p + 0xDC + k * 2)
            if ci != 0xFFFF and tbl[17]:
                q = tbl[17] + ci * 8
                m.regs.append([s16(b, q + 2 * c) / 255 for c in range(4)])
            else:
                m.regs.append([1.0] * 4)
        ns = b[tbl[19] + b[p + 4]] if tbl[19] else 1
        m.stages = []
        for k in range(ns):
            si = u16(b, p + 0xE4 + k * 2)
            if si == 0xFFFF or not tbl[20]:
                continue
            q = tbl[20] + si * 20
            m.stages.append(dict(ci=[b[q + 1 + j] for j in range(4)], cop=b[q + 5], cbias=b[q + 6],
                                 cscale=b[q + 7], cclamp=b[q + 8], creg=b[q + 9],
                                 ai=[b[q + 10 + j] for j in range(4)], aop=b[q + 14], abias=b[q + 15],
                                 ascale=b[q + 16], aclamp=b[q + 17], areg=b[q + 18], idx=k))
        m.swap_tables = []
        for k in range(4):
            ti = u16(b, p + 0x124 + k * 2)
            if ti == 0xFFFF or not tbl[22]:
                m.swap_tables.append((0, 1, 2, 3))
            else:
                m.swap_tables.append(tuple(b[tbl[22] + ti * 4 + c] for c in range(4)))
        m.swap = []
        for k in range(16):
            si = u16(b, p + 0x104 + k * 2)
            if si == 0xFFFF or not tbl[21]:
                m.swap.append((0, 0))
            else:
                m.swap.append((b[tbl[21] + si * 4], b[tbl[21] + si * 4 + 1]))
        m.tex = []
        for k in range(8):
            t = u16(b, p + 0x84 + k * 2)
            m.tex.append(None if t == 0xFFFF or not tbl[15] else u16(b, tbl[15] + t * 2))
        used = {}
        for k in range(16):
            to = u16(b, p + 0xBC + k * 2)
            if to == 0xFFFF or not tbl[16]:
                continue
            q = tbl[16] + to * 4
            tm = b[q + 1]
            if tm != 0xFF and tm < 8 and m.tex[tm] is not None and tm not in used:
                used[tm] = b[q]
        if used:
            m.first_texmap = min(used)
            coord = used[m.first_texmap]
        else:
            m.first_texmap = next((k for k, t in enumerate(m.tex) if t is not None), None)
            coord = m.first_texmap
        m.uv_src = 0
        if coord is not None and coord < 8 and tbl[11]:
            tg = u16(b, p + 0x28 + coord * 2)
            if tg != 0xFFFF:
                src = b[tbl[11] + tg * 4 + 1]
                m.uv_src = src - 4 if 4 <= src <= 11 else -1
        return m

    def vertex_array(self, attr, slot):
        b = self.data
        o, size = self.sections["VTX1"]
        fmt = o + u32(b, o + 8)
        offs = [u32(b, o + 0x0C + i * 4) for i in range(13)]
        formats = {}
        while True:
            a, cc, ct, frac = struct.unpack_from(">IIIB", b, fmt)
            fmt += 16
            if a == 0xFF:
                break
            formats[a] = (cc, ct, frac)
        start = offs[slot]
        if attr not in formats or not start:
            return None
        ends = sorted(x for x in offs if x > start)
        return formats[attr] + (b[o + start:o + (ends[0] if ends else size)],)

    def shapes(self):
        b = self.data
        o, _ = self.sections["SHP1"]
        init = o + u32(b, o + 0x0C)
        remap = o + u32(b, o + 0x10)
        desc_base = o + u32(b, o + 0x18)
        dl_base = o + u32(b, o + 0x20)
        drawinit = o + u32(b, o + 0x28)
        out = []
        for si in range(u16(b, o + 8)):
            p = init + u16(b, remap + si * 2) * 0x28
            desc = []
            q = desc_base + u16(b, p + 4)
            while True:
                attr, typ = struct.unpack_from(">II", b, q)
                q += 8
                if attr == 0xFF:
                    break
                desc.append((attr, typ))
            verts = []
            tris = []
            first_draw = u16(b, p + 8)
            for pk in range(u16(b, p + 2)):
                dl_size, dl_off = struct.unpack_from(">II", b, drawinit + (first_draw + pk) * 8)
                self.display_list(dl_base + dl_off, dl_size, desc, verts, tris)
            out.append((self.shape_material.get(si), verts, tris))
        return out

    def display_list(self, start, size, desc, verts, tris):
        b = self.data
        p = start
        end = start + size
        while p < end:
            op = b[p]
            p += 1
            if op == 0:
                continue
            prim = op & 0xF8
            if prim not in PRIMITIVES:
                break
            cnt = u16(b, p)
            p += 2
            idxs = []
            for _ in range(cnt):
                att = {}
                for attr, typ in desc:
                    if typ in (1, 2):
                        val = b[p]
                        p += 1
                    elif typ == 3:
                        val = u16(b, p)
                        p += 2
                    else:
                        continue
                    if attr > 8:
                        att[attr] = val
                idxs.append(len(verts))
                verts.append(att)
            if prim == 0x90:
                for i in range(0, cnt - 2, 3):
                    tris.append((idxs[i], idxs[i + 1], idxs[i + 2]))
            elif prim == 0x98:
                for i in range(cnt - 2):
                    if i & 1:
                        tris.append((idxs[i + 1], idxs[i], idxs[i + 2]))
                    else:
                        tris.append((idxs[i], idxs[i + 1], idxs[i + 2]))
            elif prim == 0xA0:
                for i in range(1, cnt - 1):
                    tris.append((idxs[0], idxs[i], idxs[i + 1]))
            elif prim == 0x80:
                for i in range(0, cnt - 3, 4):
                    tris.append((idxs[i], idxs[i + 1], idxs[i + 2]))
                    tris.append((idxs[i], idxs[i + 2], idxs[i + 3]))

    def load_colors(self):
        arr = self.vertex_array(11, 3)
        if arr is None:
            return
        cols = vertex_colors(arr[1], arr[3])
        mx = max(max(c) for c in cols) if cols else 0
        if mx > 1:
            self.colors = [[float(x) / 255.0 for x in c] for c in cols]
        else:
            self.colors = [[float(x) for x in c] for c in cols]
        self.shape_colors = []
        for mat, verts, _ in self.shapes():
            self.shape_colors.append((mat, [v[11] for v in verts if 11 in v]))

    def material_colors(self, mi):
        if self.colors is None:
            self.load_colors()
        if self.colors is None:
            return []
        idx = []
        for mat, vals in self.shape_colors:
            if mat == mi:
                idx.extend(vals)
        return [self.colors[k] for k in idx]

    def vertex_color_mean(self, mi):
        cols = self.material_colors(mi)
        if not cols:
            return None
        acc = list(cols[0])
        for c in cols[1:]:
            for j in range(len(acc)):
                acc[j] += c[j]
        n = len(cols)
        return [a / n for a in acc]


def _konst(m, sel, alpha):
    if sel in KONST_FRACTIONS:
        return [KONST_FRACTIONS[sel]] * 4
    if 0x0C <= sel <= 0x0F and not alpha:
        return list(m.konst[sel - 0x0C])
    if sel >= 0x10:
        k = (sel - 0x10) % 4
        comp = (sel - 0x10) // 4
        return [m.konst[k][comp]] * 4
    return [1.0] * 4


def clip(v, lo, hi):
    if v < lo:
        return lo
    if v > hi:
        return hi
    return v


def combine(a, b, c, d, op, bias, scale, clamp):
    a = clip(a, 0.0, 1.0)
    b = clip(b, 0.0, 1.0)
    c = clip(c, 0.0, 1.0)
    v = (1 - c) * a + c * b
    v = d - v if op == 1 else d + v
    v = v + BIAS.get(bias, 0.0)
    v = v * SCALE.get(scale, 1.0)
    if clamp:
        return clip(v, 0.0, 1.0)
    return clip(v, -4.0, 4.0)


def swizzle(v, table):
    return [v[table[0]], v[table[1]], v[table[2]], v[table[3]]]


def stage_setup(m, st):
    ci = [x if x < 15 else 15 for x in st["ci"]]
    ai = [x if x < 7 else 7 for x in st["ai"]]
    rs, ts = m.swap[st["idx"]]
    kc = _konst(m, m.kcsel[st["idx"]], False)
    ka = _konst(m, m.kasel[st["idx"]], True)
    cop = st["cop"] if st["cop"] < 2 else 0
    aop = st["aop"] if st["aop"] < 2 else 0
    return ci, ai, m.swap_tables[rs % 4], m.swap_tables[ts % 4], kc, ka, cop, aop


def run_stage(st, setup, pv, r0, r1, r2, t, ras):
    ci, ai, _, _, kc, ka, cop, aop = setup
    col = []
    for c in range(3):
        src = (pv[c], pv[3], r0[c], r0[3], r1[c], r1[3], r2[c], r2[3], t[c], t[3], ras[c], ras[3], 1.0, 0.5,
               kc[c], 0.0)
        col.append(combine(src[ci[0]], src[ci[1]], src[ci[2]], src[ci[3]], cop, st["cbias"], st["cscale"],
                           st["cclamp"]))
    src = (pv[3], r0[3], r1[3], r2[3], t[3], ras[3], ka[3], 0.0)
    return col, combine(src[ai[0]], src[ai[1]], src[ai[2]], src[ai[3]], aop, st["abias"], st["ascale"],
                        st["aclamp"])


def sample_plane(t, hh, ww):
    if t is None:
        return None
    tw, th, px = t
    out = []
    for y in range(hh):
        sy = y * th // hh
        for x in range(ww):
            k = (sy * tw + x * tw // ww) * 4
            out.append([px[k] / 255, px[k + 1] / 255, px[k + 2] / 255, px[k + 3] / 255])
    return out


def uses_texture(st):
    return 8 in st["ci"] or 9 in st["ci"]


def effective(model, mi, max_stages="auto", regs_override=None):
    m = model.materials[mi]
    regs = [list(r) for r in m.regs]
    if regs_override:
        for k, v in regs_override.items():
            regs[k] = [x / 255 for x in v]
    texs = {}
    base = None
    for st in m.stages:
        o = m.order[st["idx"]]
        if o and o[1] != 0xFF and o[1] < 8 and m.tex[o[1]] is not None and m.tex[o[1]] < len(model.textures):
            t = model.textures[m.tex[o[1]]]
            if base is None:
                base = (t[1], t[0])
            texs[o[1]] = t
    hh, ww = base or (1, 1)
    npx = hh * ww
    base_col = list(m.mat_color)
    if m.ras_from_vertex:
        vc = model.vertex_color_mean(mi)
        if vc is not None:
            base_col = [1.0] * 4
            base_col[:len(vc)] = vc[:4]
    first_tex = next((k for k, st in enumerate(m.stages) if uses_texture(st)), None)
    if first_tex is None or first_tex > 0:
        if texs:
            return hh, ww, sample_plane(texs[min(texs)], hh, ww)
        return hh, ww, [list(m.mat_color) for _ in range(npx)]
    if max_stages == "auto":
        max_stages = 1
        if len(m.stages) > 1:
            s1 = m.stages[1]
            if (s1["ci"][0] == 15 and s1["cop"] == 0 and s1["cscale"] in (1, 2) and not uses_texture(s1)
                    and 0 in s1["ci"][1:]):
                max_stages = 2
    prev = [list(regs[3]) for _ in range(npx)]
    reg = [[list(regs[i]) for _ in range(npx)] for i in range(3)]
    ones = [1.0, 1.0, 1.0, 1.0]
    for st in m.stages[:max(max_stages, first_tex + 1)]:
        o = m.order[st["idx"]]
        tex = sample_plane(texs.get(o[1]), hh, ww) if o and o[1] != 0xFF else None
        chan = o[2] if o else 0xFF
        setup = stage_setup(m, st)
        ras = swizzle(base_col if chan in (0, 2, 4) else [0.0, 0.0, 0.0, 0.0], setup[2])
        tsw = setup[3]
        newc = []
        newa = []
        for p in range(npx):
            t = swizzle(tex[p] if tex is not None else ones, tsw)
            col, al = run_stage(st, setup, prev[p], reg[0][p], reg[1][p], reg[2][p], t, ras)
            newc.append(col)
            newa.append(al)
        dstc = prev if st["creg"] == 0 else reg[st["creg"] - 1]
        dsta = prev if st["areg"] == 0 else reg[st["areg"] - 1]
        for p in range(npx):
            dstc[p][0], dstc[p][1], dstc[p][2] = newc[p]
        for p in range(npx):
            dsta[p][3] = newa[p]
    return hh, ww, [[clip(v, 0.0, 1.0) for v in px] for px in prev]


def effective_rgba(model, mi, max_stages="auto", regs_override=None):
    h, w, img = effective(model, mi, max_stages, regs_override)
    out = bytearray(w * h * 4)
    for p, px in enumerate(img):
        for c in range(4):
            out[p * 4 + c] = int(round(px[c] * 255))
    return w, h, out


def flat_mean(model, mi):
    m = model.materials[mi]
    ras = list(m.mat_color[:3])
    regs = [list(r[:3]) for r in m.regs]
    texs = {}
    for st in m.stages:
        o = m.order[st["idx"]]
        if o and o[1] < 8 and m.tex[o[1]] is not None and m.tex[o[1]] < len(model.textures):
            texs[st["idx"]] = model.textures[m.tex[o[1]]]
    hh, ww = max(((t[1], t[0]) for t in texs.values()), default=(1, 1))
    n = hh * ww
    c = {0: [list(regs[3]) for _ in range(n)], 2: [regs[0]] * n, 4: [regs[1]] * n, 6: [regs[2]] * n}
    for st in m.stages:
        tex = sample_plane(texs.get(st["idx"]), hh, ww)
        k = m.kcsel[st["idx"]]
        if k < 8:
            konst = [KONST_FRACTIONS[k]] * 3
        elif 12 <= k <= 15:
            konst = list(m.konst[k - 12][:3])
        else:
            konst = [1.0] * 3
        bias = BIAS[st["cbias"]]
        scale = SCALE[st["cscale"]]
        out = []
        for p in range(n):
            ins = []
            for x in st["ci"]:
                if x in (0, 2, 4, 6):
                    ins.append(c[x][p])
                elif x == 8:
                    ins.append(tex[p] if tex is not None else [1.0, 1.0, 1.0])
                elif x == 10:
                    ins.append(ras)
                elif x == 12:
                    ins.append([1.0, 1.0, 1.0])
                elif x == 13:
                    ins.append([0.5, 0.5, 0.5])
                elif x == 14:
                    ins.append(konst)
                else:
                    ins.append([0.0, 0.0, 0.0])
            a, b, cc, d = ins
            px = []
            for ch in range(3):
                v = (d[ch] + a[ch] * (1 - cc[ch]) + b[ch] * cc[ch] + bias) * scale
                px.append(0.0 if v < 0.0 else 1.0 if v > 1.0 else v)
            out.append(px)
        c[2 * st["creg"]] = out
    acc = list(c[0][0])
    for px in c[0][1:]:
        for ch in range(3):
            acc[ch] += px[ch]
    return [int(round(acc[ch] / n * 255)) for ch in range(3)]


def texture_mean(tw, th, px, disc):
    acc = [0.0, 0.0, 0.0, 0.0]
    if disc:
        n = 0
        for y in range(th):
            dy = (y + 0.5 - th / 2) / (th / 2)
            for x in range(tw):
                dx = (x + 0.5 - tw / 2) / (tw / 2)
                if dx * dx + dy * dy <= 1.0:
                    i = (y * tw + x) * 4
                    for c in range(4):
                        acc[c] += px[i + c] / 255
                    n += 1
        return [acc[c] / n for c in range(4)]
    for i in range(0, len(px), 4):
        for c in range(4):
            acc[c] += float(px[i + c])
    n = tw * th
    return [acc[c] / n / 255 for c in range(4)]


def env_colour(model, mi, ras):
    m = model.materials[mi]
    texval = {}
    for k, ti in enumerate(m.tex):
        if ti is not None and ti < len(model.textures):
            tw, th, px = model.textures[ti]
            texval[k] = texture_mean(tw, th, px, m.uv_src < 0)
    prev = list(m.regs[3])
    reg = [list(r) for r in m.regs[:3]]
    for st in m.stages:
        o = m.order[st["idx"]]
        tex = texval.get(o[1], [1.0] * 4) if o and o[1] != 0xFF else [1.0] * 4
        chan = o[2] if o else 0xFF
        setup = stage_setup(m, st)
        r = swizzle(ras if chan in (0, 2, 4) else [0.0] * 4, setup[2])
        col, al = run_stage(st, setup, prev, reg[0], reg[1], reg[2], swizzle(tex, setup[3]), r)
        dst = prev if st["creg"] == 0 else reg[st["creg"] - 1]
        dst[0], dst[1], dst[2] = col
        dsta = prev if st["areg"] == 0 else reg[st["areg"] - 1]
        dsta[3] = al
    return [clip(v, 0.0, 1.0) for v in prev]


def vertex_hue(model, mi):
    rows = []
    for col in model.material_colors(mi):
        c = col[:3]
        mx = max(c)
        if mx < 1e-6:
            mx = 1e-6
        rows.append([c[0] / mx, c[1] / mx, c[2] / mx])
    n = len(rows)
    hue = []
    for ch in range(3):
        s = sorted(r[ch] for r in rows)
        hue.append(s[n // 2] if n % 2 else (s[n // 2 - 1] + s[n // 2]) / 2)
    return hue


def env_flat(model, mi):
    m = model.materials[mi]
    ras = list(m.mat_color)
    if m.ras_from_vertex:
        vc = model.vertex_color_mean(mi)
        if vc is not None:
            ras = [1.0] * 4
            ras[:len(vc)] = vc[:4]
    return [int(round(v * 255)) for v in env_colour(model, mi, ras)[:3]]
