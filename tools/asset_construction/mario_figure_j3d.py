import math
import struct

from common import f32_at, name_table, u16, u32

PI = math.pi
PIO2_HI = 1.5707963267341256
PIO2_LO = 6.077100506506192e-11


def sin_poly(x):
    z = x * x
    s = 0.0
    t = x
    k = 1
    while k < 23:
        s += t
        t = -t * z / ((k + 1) * (k + 2))
        k += 2
    return s


def cos_poly(x):
    z = x * x
    s = 0.0
    t = 1.0
    k = 0
    while k < 24:
        s += t
        t = -t * z / ((k + 1) * (k + 2))
        k += 2
    return s


def sincos(x):
    k = math.floor(x / (PI * 0.5) + 0.5)
    r = (x - k * PIO2_HI) - k * PIO2_LO
    s = sin_poly(r)
    c = cos_poly(r)
    q = int(k) & 3
    if q == 0:
        return s, c
    if q == 1:
        return c, -s
    if q == 2:
        return -s, -c
    return -c, s


def mmul(a, b):
    return (a[0] * b[0] + a[1] * b[4] + a[2] * b[8],
            a[0] * b[1] + a[1] * b[5] + a[2] * b[9],
            a[0] * b[2] + a[1] * b[6] + a[2] * b[10],
            a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + a[3],
            a[4] * b[0] + a[5] * b[4] + a[6] * b[8],
            a[4] * b[1] + a[5] * b[5] + a[6] * b[9],
            a[4] * b[2] + a[5] * b[6] + a[6] * b[10],
            a[4] * b[3] + a[5] * b[7] + a[6] * b[11] + a[7],
            a[8] * b[0] + a[9] * b[4] + a[10] * b[8],
            a[8] * b[1] + a[9] * b[5] + a[10] * b[9],
            a[8] * b[2] + a[9] * b[6] + a[10] * b[10],
            a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11])


def minv(a):
    c00 = a[5] * a[10] - a[6] * a[9]
    c01 = a[2] * a[9] - a[1] * a[10]
    c02 = a[1] * a[6] - a[2] * a[5]
    c10 = a[6] * a[8] - a[4] * a[10]
    c11 = a[0] * a[10] - a[2] * a[8]
    c12 = a[2] * a[4] - a[0] * a[6]
    c20 = a[4] * a[9] - a[5] * a[8]
    c21 = a[1] * a[8] - a[0] * a[9]
    c22 = a[0] * a[5] - a[1] * a[4]
    d = a[0] * c00 + a[1] * c10 + a[2] * c20
    r = (c00 / d, c01 / d, c02 / d, c10 / d, c11 / d, c12 / d, c20 / d, c21 / d, c22 / d)
    t0, t1, t2 = a[3], a[7], a[11]
    return (r[0], r[1], r[2], -(r[0] * t0 + r[1] * t1 + r[2] * t2),
            r[3], r[4], r[5], -(r[3] * t0 + r[4] * t1 + r[5] * t2),
            r[6], r[7], r[8], -(r[6] * t0 + r[7] * t1 + r[8] * t2))


def mapply(a, p):
    return (a[0] * p[0] + a[1] * p[1] + a[2] * p[2] + a[3],
            a[4] * p[0] + a[5] * p[1] + a[6] * p[2] + a[7],
            a[8] * p[0] + a[9] * p[1] + a[10] * p[2] + a[11])


def euler_matrix(rx, ry, rz):
    sx, cx = sincos(rx)
    sy, cy = sincos(ry)
    sz, cz = sincos(rz)
    return (cz * cy, sx * cz * sy - cx * sz, cx * cz * sy + sx * sz,
            sz * cy, sx * sz * sy + cx * cz, cx * sz * sy - sx * cz,
            -sy, cy * sx, cy * cx)


def euler_quat(rx, ry, rz):
    sx, cx = sincos(rx * 0.5)
    sy, cy = sincos(ry * 0.5)
    sz, cz = sincos(rz * 0.5)
    w = cz * cy * cx + sz * sy * sx
    x = cz * cy * sx - sz * sy * cx
    y = cz * sy * cx + sz * cy * sx
    z = sz * cy * cx - cz * sy * sx
    return x, y, z, w


def trs_quat(t, q, s):
    x, y, z, w = q
    r = ((1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)),
         (2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)),
         (2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)))
    return (r[0][0] * s[0], r[0][1] * s[1], r[0][2] * s[2], t[0],
            r[1][0] * s[0], r[1][1] * s[1], r[1][2] * s[2], t[1],
            r[2][0] * s[0], r[2][1] * s[1], r[2][2] * s[2], t[2])


def trs_euler(t, rot, s, inv_ps=None):
    e = euler_matrix(*rot)
    m = [e[0] * s[0], e[1] * s[1], e[2] * s[2], t[0],
         e[3] * s[0], e[4] * s[1], e[5] * s[2], t[1],
         e[6] * s[0], e[7] * s[1], e[8] * s[2], t[2]]
    if inv_ps is not None:
        for r in range(3):
            for c in range(3):
                m[4 * r + c] = m[4 * r + c] * inv_ps[r]
    return tuple(m)


def polar_rs(m):
    a = [m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10]]
    x = list(a)
    for _ in range(60):
        d = (x[0] * (x[4] * x[8] - x[5] * x[7]) - x[1] * (x[3] * x[8] - x[5] * x[6])
             + x[2] * (x[3] * x[7] - x[4] * x[6]))
        it = [(x[4] * x[8] - x[5] * x[7]) / d, (x[5] * x[6] - x[3] * x[8]) / d,
              (x[3] * x[7] - x[4] * x[6]) / d, (x[2] * x[7] - x[1] * x[8]) / d,
              (x[0] * x[8] - x[2] * x[6]) / d, (x[1] * x[6] - x[0] * x[7]) / d,
              (x[1] * x[5] - x[2] * x[4]) / d, (x[2] * x[3] - x[0] * x[5]) / d,
              (x[0] * x[4] - x[1] * x[3]) / d]
        nx = [(x[i] + it[i]) * 0.5 for i in range(9)]
        diff = 0.0
        for i in range(9):
            e = nx[i] - x[i]
            diff += e * e
        x = nx
        if diff < 1e-30:
            break
    sc = [x[0] * a[0] + x[3] * a[3] + x[6] * a[6],
          x[1] * a[1] + x[4] * a[4] + x[7] * a[7],
          x[2] * a[2] + x[5] * a[5] + x[8] * a[8]]
    return x, sc


def close(a, b):
    for i in range(12):
        if abs(a[i] - b[i]) > 1e-5 + 1e-5 * abs(b[i]):
            return False
    return True


class Model:
    def __init__(self, data):
        self.data = data
        nsec = u32(data, 0x0C)
        self.sections = {}
        o = 0x20
        for _ in range(nsec):
            self.sections[data[o:o + 4].decode("ascii")] = (o, u32(data, o + 4))
            o += u32(data, o + 4)
        self.read_inf1()
        self.read_jnt1()
        self.read_drw1()
        self.read_evp1()
        self.read_vtx1()
        self.order = self.joint_order()
        self.bind_j3d = self.bind_matrices_j3d()
        self.bind = self.bind_matrices_gltf()
        self.bind_inv = [minv(m) for m in self.bind]
        self.rest_local = []
        for j in range(len(self.joints)):
            p = self.parent[j]
            self.rest_local.append(self.bind[j] if p < 0 else mmul(self.bind_inv[p], self.bind[j]))

    def read_inf1(self):
        b = self.data
        o, _ = self.sections["INF1"]
        self.mtx_calc = u16(b, o + 8) & 0xF
        h = o + u32(b, o + 0x14)
        stack = []
        last = None
        self.joint_parent = {}
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
                    par = None
                    for s in reversed(stack):
                        if s is not None and s[0] == 0x10:
                            par = s[1]
                            break
                    self.joint_parent[v] = par
                last = (t, v)

    def read_jnt1(self):
        b = self.data
        o, _ = self.sections["JNT1"]
        n = u16(b, o + 8)
        data_off = o + u32(b, o + 0x0C)
        remap_off = o + u32(b, o + 0x10)
        self.names = name_table(b, o + u32(b, o + 0x14))
        self.joints = []
        for i in range(n):
            p = data_off + u16(b, remap_off + i * 2) * 0x40
            ssc = b[p + 2]
            scale = struct.unpack_from(">fff", b, p + 4)
            rr = struct.unpack_from(">hhh", b, p + 0x10)
            rot = tuple(r * PI / 32768.0 for r in rr)
            trans = struct.unpack_from(">fff", b, p + 0x18)
            self.joints.append((ssc, scale, rot, trans))
        self.parent = [-1] * n
        self.children = [[] for _ in range(n)]
        for j in sorted(self.joint_parent):
            p = self.joint_parent[j]
            if p is not None and j < n and p < n:
                self.parent[j] = p
                self.children[p].append(j)

    def joint_order(self):
        order = []
        roots = [j for j in range(len(self.joints)) if self.parent[j] < 0]
        stack = list(reversed(roots))
        while stack:
            j = stack.pop()
            order.append(j)
            stack.extend(reversed(self.children[j]))
        seen = set(order)
        order += [j for j in range(len(self.joints)) if j not in seen]
        return order

    def read_drw1(self):
        b = self.data
        if "DRW1" not in self.sections:
            self.drw = []
            return
        o, _ = self.sections["DRW1"]
        n = u16(b, o + 8)
        wo = o + u32(b, o + 0x0C)
        do = o + u32(b, o + 0x10)
        self.drw = [(b[wo + i] != 0, u16(b, do + i * 2)) for i in range(n)]

    def read_evp1(self):
        self.envelopes = []
        self.evp_ibm = {}
        if "EVP1" not in self.sections:
            return
        b = self.data
        o, size = self.sections["EVP1"]
        n = u16(b, o + 8)
        cnt_off = o + u32(b, o + 0x0C)
        idx_off = o + u32(b, o + 0x10)
        w_off = o + u32(b, o + 0x14)
        m_rel = u32(b, o + 0x18)
        ii = 0
        maxj = -1
        for e in range(n):
            env = []
            for k in range(b[cnt_off + e]):
                j = u16(b, idx_off + (ii + k) * 2)
                env.append((j, f32_at(b, w_off + (ii + k) * 4)))
                maxj = max(maxj, j)
            ii += b[cnt_off + e]
            self.envelopes.append(env)
        if m_rel:
            for j in range(maxj + 1):
                p = o + m_rel + j * 48
                if p + 48 > o + size:
                    break
                self.evp_ibm[j] = struct.unpack_from(">12f", b, p)

    def read_vtx1(self):
        b = self.data
        o, size = self.sections["VTX1"]
        fmt_off = o + u32(b, o + 8)
        offs = [u32(b, o + 0x0C + i * 4) for i in range(13)]
        slot_of = {9: 0, 13: 5}
        fmts = {}
        p = fmt_off
        while True:
            attr, cc, ct, frac = struct.unpack_from(">IIIB", b, p)
            p += 16
            if attr == 0xFF:
                break
            fmts[attr] = (cc, ct, frac)
        sorted_offs = sorted(x for x in offs if x)
        self.arrays = {}
        for attr in (9, 13):
            if attr not in fmts or not offs[slot_of[attr]]:
                continue
            cc, ct, frac = fmts[attr]
            start = offs[slot_of[attr]]
            nxt = [x for x in sorted_offs if x > start]
            end = nxt[0] if nxt else size
            raw = b[o + start:o + end]
            ncomp = (3 if cc == 1 else 2) if attr == 9 else (2 if cc == 1 else 1)
            code, isz = {0: ("B", 1), 1: ("b", 1), 2: ("H", 2), 3: ("h", 2), 4: ("f", 4)}[ct]
            cnt = len(raw) // (isz * ncomp)
            vals = struct.unpack_from(">" + code * (cnt * ncomp), raw, 0)
            div = 1.0 if ct == 4 else float(1 << frac)
            arr = []
            for i in range(cnt):
                e = [vals[i * ncomp + k] / div for k in range(ncomp)]
                while len(e) < (3 if attr == 9 else 2):
                    e.append(0.0)
                arr.append(tuple(e))
            self.arrays[attr] = arr

    def local_j3d(self, j, s, r, t, ps):
        inv_ps = None
        if self.mtx_calc == 2 and self.joints[j][0] == 1 and ps is not None:
            inv_ps = tuple(1.0 / x if x != 0 else 0.0 for x in ps)
        return trs_euler(t, r, s, inv_ps)

    def bind_matrices_j3d(self):
        w = [None] * len(self.joints)
        for j in self.order:
            p = self.parent[j]
            ps = self.joints[p][1] if p >= 0 else None
            _, s, r, t = self.joints[j]
            lm = self.local_j3d(j, s, r, t, ps)
            w[j] = lm if p < 0 else mmul(w[p], lm)
        return w

    def bind_matrices_gltf(self):
        w = [None] * len(self.joints)
        for j in self.order:
            _, s, r, t = self.joints[j]
            s = tuple(x if abs(x) > 1e-6 else 1.0 for x in s)
            lm = trs_quat(t, euler_quat(*r), s)
            p = self.parent[j]
            w[j] = lm if p < 0 else mmul(w[p], lm)
        return w

    def drw_matrix(self, d):
        weighted, idx = self.drw[d]
        if not weighted:
            return self.bind_j3d[idx]
        acc = [0.0] * 12
        for j, wt in self.envelopes[idx]:
            ibm = self.evp_ibm.get(j)
            if ibm is None:
                ibm = minv(self.bind_j3d[j])
            m = mmul(self.bind_j3d[j], ibm)
            for k in range(12):
                acc[k] += wt * m[k]
        return tuple(acc)

    def anchor_point(self, pos_index, drw_index):
        return mapply(self.drw_matrix(drw_index), self.arrays[9][pos_index])

    def pose(self, raw):
        n = len(self.joints)
        maya = self.mtx_calc == 2
        wj = [None] * n
        wg = [None] * n
        lg = [None] * n
        for j in self.order:
            ssc = self.joints[j][0]
            s, r, t = raw[j]
            p = self.parent[j]
            ps = raw[p][0] if p >= 0 else None
            lj = self.local_j3d(j, s, r, t, ps)
            wj[j] = lj if p < 0 else mmul(wj[p], lj)
            scaled = ps is not None and (abs(ps[0] - 1.0) > 1e-6 or abs(ps[1] - 1.0) > 1e-6
                                         or abs(ps[2] - 1.0) > 1e-6)
            needs = maya and ssc == 1 and scaled
            clean = p < 0 or close(wg[p], wj[p])
            if not needs and clean:
                loc = trs_quat(t, euler_quat(*r), s)
            else:
                lm = mmul(minv(wg[p]), wj[j])
                rot, sc = polar_rs(lm)
                loc = (rot[0] * sc[0], rot[1] * sc[1], rot[2] * sc[2], lm[3],
                       rot[3] * sc[0], rot[4] * sc[1], rot[5] * sc[2], lm[7],
                       rot[6] * sc[0], rot[7] * sc[1], rot[8] * sc[2], lm[11])
            lg[j] = loc
            wg[j] = loc if p < 0 else mmul(wg[p], loc)
        return lg

    def rest_raw(self):
        return [(js[1], js[2], js[3]) for js in self.joints]

    def deform(self, lg, corr):
        n = len(self.joints)
        pw = [None] * n
        out = [None] * n
        for j in self.order:
            loc = lg[j]
            if j in corr:
                loc = mmul(corr[j][0], loc)
            p = self.parent[j]
            if p >= 0 and p in corr:
                loc = mmul(corr[p][1], loc)
            pw[j] = loc if p < 0 else mmul(pw[p], loc)
        for j in range(n):
            if j in corr:
                out[j] = mmul(pw[j], corr[j][2])
            else:
                out[j] = mmul(pw[j], self.bind_inv[j])
        return out

    def correction(self, j, e):
        einv = minv(e)
        rest = self.rest_local[j]
        return mmul(mmul(rest, e), minv(rest)), einv, mmul(einv, self.bind_inv[j])


class Track:
    __slots__ = ("keys", "const")

    def __init__(self, keys, const):
        self.keys = keys
        self.const = const

    def sample(self, f):
        if self.keys is None:
            return self.const
        k = self.keys
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
        s2 = s * s
        s3 = s * s * s
        return (v0 * (2 * s3 - 3 * s2 + 1) + v1 * (-2 * s3 + 3 * s2)
                + o0 * d * (s3 - 2 * s2 + s) + i1 * d * (s3 - s2))


class BCK:
    def __init__(self, data):
        o = 0x20
        while data[o:o + 4] != b"ANK1":
            o += u32(data, o + 4)
        b = data
        self.loop_mode = b[o + 8]
        rot_shift = b[o + 9]
        self.frame_count = u16(b, o + 0x0A)
        nj = u16(b, o + 0x0C)
        ns, nr, nt = u16(b, o + 0x0E), u16(b, o + 0x10), u16(b, o + 0x12)
        joff = o + u32(b, o + 0x14)
        soff = o + u32(b, o + 0x18)
        roff = o + u32(b, o + 0x1C)
        toff = o + u32(b, o + 0x20)
        sdat = struct.unpack_from(">%df" % ns, b, soff)
        rdat = struct.unpack_from(">%dh" % nr, b, roff)
        tdat = struct.unpack_from(">%df" % nt, b, toff)
        rscale = float(1 << rot_shift) * PI / 32768.0
        self.joint_count = nj
        self.tracks = []
        for j in range(nj):
            comp = {}
            for k, (key, dat, default) in enumerate((("s", sdat, 1.0), ("r", rdat, 0.0), ("t", tdat, 0.0))):
                comp[key] = []
                for axis in range(3):
                    cnt, idx, tmode = struct.unpack_from(">HHH", b, joff + j * 54 + axis * 18 + k * 6)
                    vs = rscale if key == "r" else 1.0
                    if cnt == 0:
                        tr = Track(None, default)
                    elif cnt == 1:
                        tr = Track(None, dat[idx] * vs)
                    else:
                        stride = 4 if tmode == 1 else 3
                        keys = []
                        for q in range(cnt):
                            e = dat[idx + q * stride:idx + q * stride + stride]
                            out_tan = e[3] if stride == 4 else e[2]
                            keys.append((float(e[0]), e[1] * vs, e[2] * vs, out_tan * vs))
                        tr = Track(keys, None)
                    comp[key].append(tr)
            self.tracks.append(comp)

    def raw(self, model, frame):
        out = []
        for j in range(len(model.joints)):
            if j < self.joint_count:
                c = self.tracks[j]
                out.append((tuple(float(t.sample(frame)) for t in c["s"]),
                            tuple(float(t.sample(frame)) for t in c["r"]),
                            tuple(float(t.sample(frame)) for t in c["t"])))
            else:
                js = model.joints[j]
                out.append((js[1], js[2], js[3]))
        return out


def bva_visibility(data):
    o = 0x20
    cnt = u16(data, o + 0xC)
    so, vo = u32(data, o + 0x10), u32(data, o + 0x14)
    shapes = []
    for s in range(cnt):
        c, f = struct.unpack_from(">HH", data, o + so + 4 * s)
        shapes.append(list(data[o + vo + f:o + vo + f + c]))
    return shapes


def quat_of(m):
    c = []
    for k in range(3):
        x, y, z = m[k], m[4 + k], m[8 + k]
        ln = math.sqrt(x * x + y * y + z * z)
        c.append([x / ln, y / ln, z / ln] if ln > 0 else [0.0, 0.0, 0.0])
    det = (c[0][0] * (c[1][1] * c[2][2] - c[1][2] * c[2][1])
           - c[1][0] * (c[0][1] * c[2][2] - c[0][2] * c[2][1])
           + c[2][0] * (c[0][1] * c[1][2] - c[0][2] * c[1][1]))
    if det < 0:
        c = [[-v for v in col] for col in c]
    if c[2][2] < 0.0:
        if c[0][0] > c[1][1]:
            s = 2.0 * math.sqrt(1.0 + c[0][0] - c[1][1] - c[2][2])
            if c[1][2] < c[2][1]:
                s = -s
            q1 = 0.25 * s
            s = 1.0 / s
            q0 = (c[1][2] - c[2][1]) * s
            q2 = (c[0][1] + c[1][0]) * s
            q3 = (c[2][0] + c[0][2]) * s
        else:
            s = 2.0 * math.sqrt(1.0 - c[0][0] + c[1][1] - c[2][2])
            if c[2][0] < c[0][2]:
                s = -s
            q2 = 0.25 * s
            s = 1.0 / s
            q0 = (c[2][0] - c[0][2]) * s
            q1 = (c[0][1] + c[1][0]) * s
            q3 = (c[1][2] + c[2][1]) * s
    else:
        if c[0][0] < -c[1][1]:
            s = 2.0 * math.sqrt(1.0 - c[0][0] - c[1][1] + c[2][2])
            if c[0][1] < c[1][0]:
                s = -s
            q3 = 0.25 * s
            s = 1.0 / s
            q0 = (c[0][1] - c[1][0]) * s
            q1 = (c[2][0] + c[0][2]) * s
            q2 = (c[1][2] + c[2][1]) * s
        else:
            s = 2.0 * math.sqrt(1.0 + c[0][0] + c[1][1] + c[2][2])
            q0 = 0.25 * s
            s = 1.0 / s
            q1 = (c[1][2] - c[2][1]) * s
            q2 = (c[2][0] - c[0][2]) * s
            q3 = (c[0][1] - c[1][0]) * s
    l2 = q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3
    if abs(l2 - 1.0) >= 0.0006:
        ln = math.sqrt(l2)
        return q1 / ln, q2 / ln, q3 / ln, q0 / ln
    return q1, q2, q3, q0


def rigid_of(m):
    return trs_quat((m[3], m[7], m[11]), quat_of(m), (1.0, 1.0, 1.0))
