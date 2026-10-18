import struct

from common import name_table, s16, u16, u32

PI = 3.141592653589793
HALF_PI = 1.5707963267948966


def sin_poly(x):
    x2 = x * x
    t = x
    s = x
    k = 1
    while k < 23:
        t = -t * x2 / ((k + 1) * (k + 2))
        s += t
        k += 2
    return s


def cos_poly(x):
    x2 = x * x
    t = 1.0
    s = 1.0
    k = 0
    while k < 22:
        t = -t * x2 / ((k + 1) * (k + 2))
        s += t
        k += 2
    return s


def sincos(a):
    q = int((a / HALF_PI + 0.5) // 1)
    r = a - q * HALF_PI
    s, c = sin_poly(r), cos_poly(r)
    q &= 3
    if q == 0:
        return s, c
    if q == 1:
        return c, -s
    if q == 2:
        return -s, -c
    return -c, s


def round_half_even(x):
    f = int(x // 1)
    r = x - f
    if r > 0.5 or (r == 0.5 and f % 2 == 1):
        f += 1
    return f


def mat_mul(a, b):
    return [[a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + (a[i][3] if j == 3 else 0.0)
             for j in range(4)]
            for i in range(3)]


def mat_inv(m):
    a, b, c = m[0][0], m[0][1], m[0][2]
    d, e, f = m[1][0], m[1][1], m[1][2]
    g, h, i = m[2][0], m[2][1], m[2][2]
    ca, cb, cc = e * i - f * h, f * g - d * i, d * h - e * g
    det = a * ca + b * cb + c * cc
    if det == 0:
        det = 1.0
    r = [[ca / det, (c * h - b * i) / det, (b * f - c * e) / det],
         [cb / det, (a * i - c * g) / det, (c * d - a * f) / det],
         [cc / det, (b * g - a * h) / det, (a * e - b * d) / det]]
    t = [m[0][3], m[1][3], m[2][3]]
    return [[r[k][0], r[k][1], r[k][2], -(r[k][0] * t[0] + r[k][1] * t[1] + r[k][2] * t[2])]
            for k in range(3)]


def det3(m):
    return (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
            - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
            + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]))


def mat_apply(m, p):
    x, y, z = p
    return (m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3],
            m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3],
            m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3])


def polar(m):
    x = [row[:] for row in m]
    for _ in range(40):
        a, b, c = x[0]
        d, e, f = x[1]
        g, h, i = x[2]
        ca, cb, cc = e * i - f * h, f * g - d * i, d * h - e * g
        det = a * ca + b * cb + c * cc
        if det == 0:
            return [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]
        it = [[ca / det, cb / det, cc / det],
              [(c * h - b * i) / det, (a * i - c * g) / det, (b * g - a * h) / det],
              [(b * f - c * e) / det, (c * d - a * f) / det, (a * e - b * d) / det]]
        x = [[0.5 * (x[r][k] + it[r][k]) for k in range(3)] for r in range(3)]
    return x


def trs(t, r, s, inv_ps=None):
    sx, cx = sincos(r[0])
    sy, cy = sincos(r[1])
    sz, cz = sincos(r[2])
    rot = [[cz * cy, sx * cz * sy - cx * sz, cx * cz * sy + sx * sz],
           [sz * cy, sx * sz * sy + cx * cz, cx * sz * sy - sx * cz],
           [-sy, cy * sx, cy * cx]]
    m = [[rot[i][j] * s[j] for j in range(3)] for i in range(3)]
    if inv_ps is not None:
        m = [[m[i][j] * inv_ps[i] for j in range(3)] for i in range(3)]
    return [m[0] + [t[0]], m[1] + [t[1]], m[2] + [t[2]]]


def section(data, tag):
    o = 0x20
    for _ in range(u32(data, 0x0C)):
        if data[o:o + 4] == tag:
            return o
        o += u32(data, o + 4)
    return None


def colours(raw, ct):
    out = []
    if ct == 0:
        for i in range(0, len(raw) // 2 * 2, 2):
            v = raw[i] << 8 | raw[i + 1]
            out.append((((v >> 11) & 0x1F) * 255 // 31, ((v >> 5) & 0x3F) * 255 // 63, (v & 0x1F) * 255 // 31,
                        255))
    elif ct == 1:
        for i in range(0, len(raw) // 3 * 3, 3):
            out.append((raw[i], raw[i + 1], raw[i + 2], 255))
    elif ct == 2:
        for i in range(0, len(raw) // 4 * 4, 4):
            out.append((raw[i], raw[i + 1], raw[i + 2], 255))
    elif ct == 3:
        for i in range(0, len(raw) // 2 * 2, 2):
            v = raw[i] << 8 | raw[i + 1]
            out.append((((v >> 12) & 15) * 17, ((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17))
    elif ct == 4:
        for i in range(0, len(raw) // 3 * 3, 3):
            v = raw[i] << 16 | raw[i + 1] << 8 | raw[i + 2]
            out.append(tuple(((v >> s) & 63) * 255 // 63 for s in (18, 12, 6, 0)))
    else:
        for i in range(0, len(raw) // 4 * 4, 4):
            out.append((raw[i], raw[i + 1], raw[i + 2], raw[i + 3]))
    return out


class Joint:
    def __init__(self, b, p):
        self.ssc = b[p + 2]
        self.scale = struct.unpack_from(">fff", b, p + 4)
        self.rot = tuple(r * PI / 32768.0 for r in struct.unpack_from(">hhh", b, p + 0x10))
        self.trans = struct.unpack_from(">fff", b, p + 0x18)
        self.parent = -1
        self.children = []


class Model:
    def __init__(self, data):
        self.data = data
        if data[:4] != b"J3D2":
            raise ValueError("not a J3D2 model")
        self.sections = {}
        o = 0x20
        for _ in range(u32(data, 0x0C)):
            self.sections[data[o:o + 4].decode("ascii", "replace")] = o
            o += u32(data, o + 4)
        self.read_inf1()
        self.read_jnt1()
        self.read_evp1()
        self.read_drw1()
        self.read_vtx1()
        self.texture_names = []
        if "TEX1" in self.sections:
            o = self.sections["TEX1"]
            if u32(data, o + 0x10):
                self.texture_names = name_table(data, o + u32(data, o + 0x10))
        self.read_shp1()
        self.bind = self.world_matrices(None)

    def read_inf1(self):
        b = self.data
        o = self.sections["INF1"]
        self.mtx_calc = u16(b, o + 8) & 0xF
        h = o + u32(b, o + 0x14)
        self.shape_material = {}
        self.joint_parent = {}
        stack = []
        last = None

        def nearest(typ):
            for s in reversed(stack):
                if s is not None and s[0] == typ:
                    return s[1]
            return None

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
                if t == 0x10:
                    self.joint_parent[v] = nearest(0x10)
                elif t == 0x12:
                    mat = nearest(0x11)
                    if mat is None and last is not None and last[0] == 0x11:
                        mat = last[1]
                    self.shape_material[v] = mat
                last = (t, v)

    def read_jnt1(self):
        b = self.data
        o = self.sections["JNT1"]
        n = u16(b, o + 8)
        data_off = o + u32(b, o + 0x0C)
        remap = o + u32(b, o + 0x10)
        self.joints = [Joint(b, data_off + u16(b, remap + i * 2) * 0x40) for i in range(n)]
        for j in sorted(self.joint_parent):
            p = self.joint_parent[j]
            if p is not None and j < n and p < n:
                self.joints[j].parent = p
                self.joints[p].children.append(j)
        order = []
        stack = [j for j in reversed(range(n)) if self.joints[j].parent < 0]
        while stack:
            j = stack.pop()
            order.append(j)
            stack.extend(reversed(self.joints[j].children))
        seen = set(order)
        self.order = order + [j for j in range(n) if j not in seen]

    def read_evp1(self):
        self.envelopes = []
        self.evp_ibm = {}
        if "EVP1" not in self.sections:
            return
        b = self.data
        o = self.sections["EVP1"]
        size = u32(b, o + 4)
        n = u16(b, o + 8)
        cnt_off, idx_off, w_off = (o + u32(b, o + k) for k in (0x0C, 0x10, 0x14))
        m_rel = u32(b, o + 0x18)
        ii = 0
        maxj = -1
        for _ in range(n):
            env = []
            for k in range(b[cnt_off + len(self.envelopes)]):
                j = u16(b, idx_off + (ii + k) * 2)
                env.append((j, struct.unpack_from(">f", b, w_off + (ii + k) * 4)[0]))
                maxj = max(maxj, j)
            ii += len(env)
            self.envelopes.append(env)
        if m_rel:
            for j in range(maxj + 1):
                p = o + m_rel + j * 48
                if p + 48 > o + size:
                    break
                v = struct.unpack_from(">12f", b, p)
                self.evp_ibm[j] = [list(v[0:4]), list(v[4:8]), list(v[8:12])]

    def read_drw1(self):
        b = self.data
        o = self.sections["DRW1"]
        n = u16(b, o + 8)
        wo, do = o + u32(b, o + 0x0C), o + u32(b, o + 0x10)
        self.drw = [(bool(b[wo + i]), u16(b, do + i * 2)) for i in range(n)]

    def read_vtx1(self):
        b = self.data
        o = self.sections["VTX1"]
        size = u32(b, o + 4)
        fmt = o + u32(b, o + 8)
        offs = [u32(b, o + 0x0C + i * 4) for i in range(13)]
        slot_of = {9: 0, 10: 1, 25: 2, 11: 3, 12: 4}
        for k in range(8):
            slot_of[13 + k] = 5 + k
        formats = {}
        while True:
            attr, cc, ct, frac = struct.unpack_from(">IIIB", b, fmt)
            fmt += 16
            if attr == 0xFF:
                break
            formats[attr] = (cc, ct, frac)
        srt = sorted(x for x in offs if x)
        self.arrays = {}
        for attr in sorted(formats):
            cc, ct, frac = formats[attr]
            if attr not in (9, 10, 11, 25) and not 13 <= attr <= 20:
                continue
            slot = slot_of.get(attr)
            if slot is None or not offs[slot]:
                continue
            start = offs[slot]
            nxt = [x for x in srt if x > start]
            end = nxt[0] if nxt else size
            raw = b[o + start:o + end]
            if attr == 11:
                self.arrays[attr] = colours(raw, ct)
                continue
            if attr in (10, 25):
                ncomp = 9 if (cc == 1 or attr == 25) else 3
            elif attr == 9:
                ncomp = 3 if cc == 1 else 2
            else:
                ncomp = 2 if cc == 1 else 1
            code, isz = {0: ("B", 1), 1: ("b", 1), 2: ("H", 2), 3: ("h", 2), 4: ("f", 4)}[ct]
            cnt = len(raw) // (isz * ncomp)
            vals = struct.unpack_from(">%d%s" % (cnt * ncomp, code), raw, 0)
            div = float(1 << frac)
            arr = []
            for i in range(cnt):
                v = vals[i * ncomp:(i + 1) * ncomp]
                v = [float(x) for x in v] if ct == 4 else [x / div for x in v]
                if attr in (10, 25):
                    v = v[:3]
                while len(v) < (3 if attr == 9 else 2):
                    v.append(0.0)
                arr.append(tuple(v))
            self.arrays[attr] = arr

    def read_shp1(self):
        b = self.data
        o = self.sections["SHP1"]
        n = u16(b, o + 8)
        init = o + u32(b, o + 0x0C)
        remap = o + u32(b, o + 0x10)
        desc_base = o + u32(b, o + 0x18)
        mtxtbl = o + u32(b, o + 0x1C)
        dl_base = o + u32(b, o + 0x20)
        mtxinit = o + u32(b, o + 0x24)
        drawinit = o + u32(b, o + 0x28)
        self.shapes = []
        for si in range(n):
            p = init + u16(b, remap + si * 2) * 0x28
            npk = u16(b, p + 2)
            desc_off, first_mtx, first_draw = u16(b, p + 4), u16(b, p + 6), u16(b, p + 8)
            desc = []
            q = desc_base + desc_off
            while True:
                attr, typ = struct.unpack_from(">II", b, q)
                q += 8
                if attr == 0xFF:
                    break
                desc.append((attr, typ))
            verts, tris = [], []
            slots = [0xFFFF] * 10
            for pk in range(npk):
                use_mtx, mcount, mfirst = struct.unpack_from(">HHI", b, mtxinit + (first_mtx + pk) * 8)
                for k in range(mcount):
                    v = u16(b, mtxtbl + (mfirst + k) * 2)
                    if v != 0xFFFF:
                        slots[k] = v
                default = slots[0] if slots[0] != 0xFFFF else use_mtx
                dl_size, dl_off = struct.unpack_from(">II", b, drawinit + (first_draw + pk) * 8)
                self.read_display_list(dl_base + dl_off, dl_size, desc, slots, default, verts, tris)
            self.shapes.append((self.shape_material.get(si), verts, tris))

    def read_display_list(self, start, size, desc, slots, default, verts, tris):
        b = self.data
        p, end = start, start + size
        while p < end:
            op = b[p]
            p += 1
            if op == 0:
                continue
            prim = op & 0xF8
            if prim not in (0x80, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8):
                break
            cnt = u16(b, p)
            p += 2
            idxs = []
            for _ in range(cnt):
                drw = default
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
                    if attr == 0:
                        s = val // 3
                        drw = slots[s] if s < 10 and slots[s] != 0xFFFF else default
                    elif attr > 8:
                        att[attr] = val
                idxs.append(len(verts))
                verts.append((drw, att))
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

    def local(self, j, s, r, t, ps, ssc=True):
        jt = self.joints[j]
        inv = None
        if ssc and self.mtx_calc == 2 and jt.ssc == 1 and ps is not None:
            inv = tuple(1.0 / x if x != 0 else 0.0 for x in ps)
        return trs(t, r, s, inv)

    def world_matrices(self, pose, ssc=True):
        w = [None] * len(self.joints)
        for j in self.order:
            jt = self.joints[j]
            if pose is None:
                s, r, t = jt.scale, jt.rot, jt.trans
                ps = self.joints[jt.parent].scale if jt.parent >= 0 else None
            else:
                s, r, t = pose[j]
                ps = pose[jt.parent][0] if jt.parent >= 0 else None
            lm = self.local(j, s, r, t, ps, ssc)
            w[j] = lm if jt.parent < 0 else mat_mul(w[jt.parent], lm)
        return w

    def fit_world(self, pose, over=None):
        n = len(self.joints)
        rest = None
        if over:
            rest = self.world_matrices(None)
            over = dict((o[0], o[1:]) for o in over)
        wj = [None] * n
        wg = [None] * n
        maya = self.mtx_calc == 2
        for j in self.order:
            jt = self.joints[j]
            s, r, t = pose[j]
            p = jt.parent
            ps = pose[p][0] if p >= 0 else None
            lj = self.local(j, s, r, t, ps)
            wj[j] = lj if p < 0 else mat_mul(wj[p], lj)
            needs = maya and jt.ssc == 1 and ps is not None and any(abs(x - 1.0) > 1e-6 for x in ps)
            clean = p < 0 or all(abs(wg[p][a][c] - wj[p][a][c]) <= 1e-5 + 1e-5 * abs(wj[p][a][c])
                                 for a in range(3) for c in range(4))
            if not needs and clean:
                wg[j] = wj[j] if p < 0 else mat_mul(wg[p], lj)
            else:
                loc = mat_mul(mat_inv(wg[p]), wj[j])
                rot = polar([row[:3] for row in loc])
                sc = [rot[0][k] * loc[0][k] + rot[1][k] * loc[1][k] + rot[2][k] * loc[2][k] for k in range(3)]
                lg = [[rot[a][k] * sc[k] for k in range(3)] + [loc[a][3]] for a in range(3)]
                wg[j] = mat_mul(wg[p], lg)
            if over and j in over:
                kind, dx, dy, dz = over[j]
                base = rest[j] if kind else wg[j]
                wg[j] = [base[0][:3] + [base[0][3] + dx],
                         base[1][:3] + [base[1][3] + dy],
                         base[2][:3] + [base[2][3] + dz]]
                wj[j] = wg[j]
        return wg

    def influences(self, d):
        weighted, idx = self.drw[d]
        return self.envelopes[idx] if weighted else [(idx, 1.0)]

    def vertex_matrix(self, d, pose=None):
        bind = self.bind if pose is None else pose
        weighted, idx = self.drw[d]
        if not weighted:
            return bind[idx]
        m = [[0.0] * 4 for _ in range(3)]
        for j, wt in self.envelopes[idx]:
            ibm = self.evp_ibm.get(j) or mat_inv(self.bind[j])
            jm = mat_mul(bind[j], ibm)
            for a in range(3):
                for c in range(4):
                    m[a][c] += wt * jm[a][c]
        return m


class Track:
    def __init__(self, keys=None, const=0.0):
        self.keys = keys
        self.const = const

    def sample(self, f):
        k = self.keys
        if k is None:
            return self.const
        if f <= k[0][0]:
            return k[0][1]
        if f >= k[-1][0]:
            return k[-1][1]
        i = 0
        while i + 1 < len(k) and k[i + 1][0] <= f:
            i += 1
        t0, v0, _, o0 = k[i]
        t1, v1, i1, _ = k[i + 1]
        d = t1 - t0
        if d <= 0:
            return v1
        s = (f - t0) / d
        s2, s3 = s * s, s * s * s
        return (v0 * (2 * s3 - 3 * s2 + 1) + v1 * (-2 * s3 + 3 * s2)
                + o0 * d * (s3 - 2 * s2 + s) + i1 * d * (s3 - s2))


class BCK:
    def __init__(self, data):
        if data[:8] != b"J3D1bck1":
            raise ValueError("not a BCK")
        b = data
        o = section(b, b"ANK1")
        rot_shift = b[o + 9]
        nj = u16(b, o + 0x0C)
        ns, nr, nt = u16(b, o + 0x0E), u16(b, o + 0x10), u16(b, o + 0x12)
        joff, soff, roff, toff = (o + u32(b, o + k) for k in (0x14, 0x18, 0x1C, 0x20))
        sdat = struct.unpack_from(">%df" % ns, b, soff)
        rdat = struct.unpack_from(">%dh" % nr, b, roff)
        tdat = struct.unpack_from(">%df" % nt, b, toff)
        rscale = float(1 << rot_shift) * PI / 32768.0
        self.joint_count = nj
        self.tracks = []
        for j in range(nj):
            comp = []
            for k, (dat, default, vs) in enumerate(((sdat, 1.0, 1.0), (rdat, 0.0, rscale), (tdat, 0.0, 1.0))):
                axes = []
                for axis in range(3):
                    cnt, idx, tmode = struct.unpack_from(">HHH", b, joff + j * 54 + axis * 18 + k * 6)
                    if cnt == 0:
                        axes.append(Track(const=default))
                    elif cnt == 1:
                        axes.append(Track(const=dat[idx] * vs))
                    else:
                        st = 4 if tmode == 1 else 3
                        keys = []
                        for q in range(cnt):
                            r = dat[idx + q * st:idx + q * st + st]
                            keys.append((float(r[0]), r[1] * vs, r[2] * vs, (r[3] if st == 4 else r[2]) * vs))
                        axes.append(Track(keys=keys))
                comp.append(axes)
            self.tracks.append(comp)

    def sample(self, j, f):
        c = self.tracks[j]
        return tuple(tuple(float(t.sample(f)) for t in c[k]) for k in range(3))


def bind_geometry(model):
    out = []
    pos = model.arrays.get(9, [])
    cache = {}
    for _, verts, _ in model.shapes:
        pv = []
        for drw, att in verts:
            m = cache.get(drw)
            if m is None:
                m = cache[drw] = model.vertex_matrix(drw)
            p = pos[min(att.get(9, 0), len(pos) - 1)]
            pv.append(mat_apply(m, p))
        out.append(pv)
    return out


def material_info(data):
    b = data
    o = section(b, b"MAT3") or section(b, b"MAT2")
    out = []
    if o is None:
        return out
    n = u16(b, o + 8)
    offs = [u32(b, o + 0x0C + i * 4) for i in range(30)]

    def table(i):
        return o + offs[i] if offs[i] else None

    init, remap = o + offs[0], o + offs[1]
    texgen_t, texmtx_t, texremap_t = table(11), table(13), table(15)
    tevorder_t, alphacmp_t, blend_t = table(16), table(24), table(25)
    for i in range(n):
        p = init + u16(b, remap + i * 2) * 0x14C
        texgens = []
        for k in range(8):
            tg = u16(b, p + 0x28 + k * 2)
            if tg == 0xFFFF or texgen_t is None:
                texgens.append(None)
            else:
                texgens.append(tuple(b[texgen_t + tg * 4:texgen_t + tg * 4 + 3]))
        texmtx = []
        for k in range(10):
            ti = u16(b, p + 0x48 + k * 2)
            if ti == 0xFFFF or texmtx_t is None:
                texmtx.append(None)
            else:
                q = texmtx_t + ti * 0x64
                cen = struct.unpack_from(">fff", b, q + 4)
                ss, st = struct.unpack_from(">ff", b, q + 0x10)
                ts, tt = struct.unpack_from(">ff", b, q + 0x1C)
                texmtx.append((cen, ss, st, s16(b, q + 0x18), ts, tt))
        slots = []
        for k in range(8):
            t = u16(b, p + 0x84 + k * 2)
            slots.append(None if t == 0xFFFF or texremap_t is None else u16(b, texremap_t + t * 2))
        used = {}
        if tevorder_t is not None:
            for k in range(16):
                to = u16(b, p + 0xBC + k * 2)
                if to == 0xFFFF:
                    continue
                q = tevorder_t + to * 4
                tc, tm = b[q], b[q + 1]
                if tm != 0xFF and tm < 8 and slots[tm] is not None and tm not in used:
                    used[tm] = tc
        first, coord = None, None
        if used:
            first = min(used)
            coord = used[first]
        if first is None:
            for k, t in enumerate(slots):
                if t is not None:
                    first, coord = k, k
                    break
        uv_src, srt = 0, None
        if coord is not None and coord < 8 and texgens[coord] is not None:
            _, src, mtx = texgens[coord]
            uv_src = src - 4 if 4 <= src <= 11 else -1
            if 30 <= mtx < 60:
                tmi = (mtx - 30) // 3
                if tmi < len(texmtx) and texmtx[tmi] is not None:
                    srt = texmtx[tmi]
        mode, ref = "OPAQUE", 0.5
        ac = u16(b, p + 0x146)
        bl = u16(b, p + 0x148)
        if ac != 0xFFFF and alphacmp_t is not None:
            q = alphacmp_t + ac * 8
            comp0, ref0, op, comp1, ref1 = b[q:q + 5]
            pass0 = comp0 == 7 or (comp0 in (3, 1) and ref0 == 255) or (comp0 == 6 and ref0 == 0)
            pass1 = comp1 == 7 or (comp1 in (3, 1) and ref1 == 255) or (comp1 == 6 and ref1 == 0)
            if not ((op == 0 and pass0 and pass1) or (op == 1 and (pass0 or pass1))):
                lows = [r for c, r in ((comp0, ref0), (comp1, ref1)) if c in (4, 6) and r > 0]
                mode = "MASK"
                if lows:
                    ref = (max(lows) if op == 0 else min(lows)) / 255.0
        if bl != 0xFFFF and blend_t is not None:
            q = blend_t + bl * 4
            if b[q] == 1 and mode == "OPAQUE" and b[q + 1] == 4 and b[q + 2] == 5:
                mode = "BLEND"
        out.append({"first": first, "uv_src": uv_src, "srt": srt, "alpha_mode": mode, "alpha_ref": ref})
    return out


def btp_textures(data, frame):
    o = section(data, b"TPT1")
    n = u16(data, o + 0xC)
    ent, idx = o + u32(data, o + 0x10), o + u32(data, o + 0x14)
    names = name_table(data, o + u32(data, o + 0x1C))
    out = []
    for i in range(n):
        count, first = u16(data, ent + i * 8), u16(data, ent + i * 8 + 2)
        slot = data[ent + i * 8 + 4]
        out.append((names[i], slot, u16(data, idx + (first + min(frame, count - 1)) * 2)))
    return out


def hermite(vals, count, index, tangent, frame):
    if count == 1:
        return vals[index]
    stride = 3 if tangent == 0 else 4
    keys = [vals[index + i * stride:index + i * stride + stride] for i in range(count)]
    if frame <= keys[0][0]:
        return keys[0][1]
    for i in range(count - 1):
        k0, k1 = keys[i], keys[i + 1]
        if k0[0] <= frame <= k1[0]:
            d = k1[0] - k0[0]
            u = (frame - k0[0]) / d if d else 0.0
            u2, u3 = u * u, u * u * u
            out_t = k0[3] if stride == 4 else k0[2]
            return ((2 * u3 - 3 * u2 + 1) * k0[1] + (u3 - 2 * u2 + u) * d * out_t + (-2 * u3 + 3 * u2) * k1[1]
                    + (u3 - u2) * d * k1[2])
    return keys[-1][1]


def colour_track(data, p, vals, frame):
    out = []
    for k in range(4):
        count, index, tangent = u16(data, p + k * 6), u16(data, p + k * 6 + 2), u16(data, p + k * 6 + 4)
        out.append(max(-1024, min(1023, round_half_even(hermite(vals[k], count, index, tangent, frame)))))
    return tuple(out)


def brk_regs(data, frame):
    o = section(data, b"TRK1")
    nreg = u16(data, o + 0x0C)
    off = [u32(data, o + 0x20 + i * 4) for i in range(14)]
    counts = [u16(data, o + 0x10 + i * 2) for i in range(8)]
    vals = [[s16(data, o + off[6 + c] + i * 2) for i in range(counts[c])] for c in range(4)]
    names = name_table(data, o + off[4]) if nreg else []
    out = {}
    for i in range(nreg):
        p = o + off[0] + i * 28
        rgba = colour_track(data, p, vals, frame)
        out.setdefault(names[i] if i < len(names) else str(i), {})[data[p + 24]] = rgba
    return out


def bva_visible(data, frame):
    o = section(data, b"VAF1")
    count = u16(data, o + 12)
    entries, values = u32(data, o + 16), u32(data, o + 20)
    out = []
    for i in range(count):
        num, start = u16(data, o + entries + 4 * i), u16(data, o + entries + 4 * i + 2)
        if num and data[o + values + start + min(max(frame, 0), num - 1)]:
            out.append(i)
    return out


def bpk_colours(data, frame):
    o = section(data, b"PAK1")
    n = u16(data, o + 0x0E)
    counts = [u16(data, o + 0x10 + i * 2) for i in range(4)]
    off = [u32(data, o + 0x18 + i * 4) for i in range(7)]
    vals = [[s16(data, o + off[3 + c] + i * 2) for i in range(counts[c])] for c in range(4)]
    names = name_table(data, o + off[2]) if n else []
    out = {}
    for i in range(n):
        out[names[i] if i < len(names) else str(i)] = colour_track(data, o + off[0] + i * 24, vals, frame)
    return out
