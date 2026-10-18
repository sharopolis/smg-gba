import math
import os
import struct

from common import Dump, cross, dot, f32_at, jhash, name_table, u16, u32, yaz0_decompress

PI = 3.141592653589793
PIO2_1 = 1.57079632673412561417e+00
PIO2_1T = 6.07710050650619224932e-11
INVPIO2 = 6.36619772367581382433e-01

S1 = -1.66666666666666324348e-01
S2 = 8.33333333332248946124e-03
S3 = -1.98412698298579493134e-04
S4 = 2.75573137070700676789e-06
S5 = -2.50507602534068634195e-08
S6 = 1.58969099521155010221e-10
C1 = 4.16666666666666019037e-02
C2 = -1.38888888888741095749e-03
C3 = 2.48015872894767294178e-05
C4 = -2.75573143513906633035e-07
C5 = 2.08757232129817482790e-09
C6 = -1.13596475577881948265e-11


def kernel_sin(x):
    z = x * x
    r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)))
    return x + z * x * (S1 + z * r)


def kernel_cos(x):
    z = x * x
    r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))))
    hz = 0.5 * z
    w = 1.0 - hz
    return w + (((1.0 - w) - hz) + z * r)


def reduce_angle(x):
    if -0.7853981633974483 <= x <= 0.7853981633974483:
        return 0, x
    k = int(round(x * INVPIO2))
    r = x - k * PIO2_1
    r = r - k * PIO2_1T
    return k & 3, r


def sin(x):
    q, r = reduce_angle(x)
    if q == 0:
        return kernel_sin(r)
    if q == 1:
        return kernel_cos(r)
    if q == 2:
        return -kernel_sin(r)
    return -kernel_cos(r)


def cos(x):
    q, r = reduce_angle(x)
    if q == 0:
        return kernel_cos(r)
    if q == 1:
        return -kernel_sin(r)
    if q == 2:
        return -kernel_cos(r)
    return kernel_sin(r)


def radians(deg):
    return deg * (PI / 180.0)


def rint(x):
    return int(round(x))


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def wound(t, n):
    a, b, c = t
    if dot(cross(sub(b, a), sub(c, a)), n) < 0:
        return (a, c, b)
    return (a, b, c)


def gba(v):
    return [-v[0], v[2], v[1]]


def mtx_tr(t, r):
    rx, ry, rz = (radians(a) for a in r)
    sx, sy, sz = sin(rx), sin(ry), sin(rz)
    cx, cy, cz = cos(rx), cos(ry), cos(rz)
    return [[cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx, float(t[0])],
            [sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx, float(t[1])],
            [-sy, cy * sx, cy * cx, float(t[2])]]


def mtx_mul(a, b):
    out = []
    for i in range(3):
        row = [a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] for j in range(3)]
        row.append(a[i][0] * b[0][3] + a[i][1] * b[1][3] + a[i][2] * b[2][3] + a[i][3])
        out.append(row)
    return out


def mtx_apply(m, p):
    return tuple(m[i][0] * p[0] + m[i][1] * p[1] + m[i][2] * p[2] + m[i][3] for i in range(3))


def mtx_col(m, c):
    return (m[0][c], m[1][c], m[2][c])


def r3(v):
    return [round(float(x), 3) for x in v]


def spec_lines(spec):
    return [line.split() for line in spec.decode().splitlines() if line.strip()]


class Ctx(Dump):
    def __init__(self, orig, out_root):
        Dump.__init__(self, orig)
        self.out_root = out_root
        self.cache = {}

    def exists(self, rel):
        return os.path.exists(self.path(rel))

    def arc_file(self, rel, name):
        return self.arc(rel).find(name)

    def arc_names(self, rel):
        return [p for p, _ in self.arc(rel).files]

    def built_file(self, rel):
        with open(os.path.join(self.out_root, rel), "rb") as f:
            return f.read()

    def memo(self, key, fn):
        v = self.cache.get(key)
        if v is None:
            v = self.cache[key] = fn()
        return v


class BCSV:
    def __init__(self, data):
        data = yaz0_decompress(data)
        self.data = data
        n, nf, off, size = struct.unpack_from(">iiiI", data, 0)
        self.n = n
        self.data_off = off
        self.entry_size = size
        self.fields = {}
        for i in range(nf):
            h, mask, fo, shift, typ = struct.unpack_from(">IIHBB", data, 16 + i * 12)
            self.fields[h] = (mask, fo, shift, typ)
        self.str_table = off + n * size

    def has(self, name):
        return jhash(name) in self.fields

    def get(self, i, name, default=None):
        f = self.fields.get(jhash(name) if isinstance(name, str) else name)
        if f is None:
            return default
        mask, fo, shift, typ = f
        d = self.data
        p = self.data_off + i * self.entry_size + fo
        if typ in (0, 3):
            v = (u32(d, p) & mask) >> shift
            if mask == 0xFFFFFFFF and shift == 0 and v >= 0x80000000:
                v -= 0x100000000
            return v
        if typ == 4:
            v = (u16(d, p) & mask) >> shift
            if mask == 0xFFFF and shift == 0 and v >= 0x8000:
                v -= 0x10000
            return v
        if typ == 5:
            v = (d[p] & mask) >> shift
            if mask == 0xFF and shift == 0 and v >= 0x80:
                v -= 0x100
            return v
        if typ == 2:
            return f32_at(d, p)
        if typ == 1:
            return d[p:p + 32].split(b"\0", 1)[0].decode("shift_jis", "replace")
        if typ == 6:
            s = self.str_table + u32(d, p)
            return d[s:d.index(b"\0", s)].decode("shift_jis", "replace")
        return None


class KCL:
    def __init__(self, data):
        data = yaz0_decompress(data)
        pos_off, nrm_off, prism_off, oct_off = struct.unpack_from(">IIII", data, 0)
        n = (oct_off - (prism_off + 0x10)) // 0x10
        self.triangles = []
        self.normals = []
        self.attributes = []
        for i in range(n):
            h, pi, fi, e0i, e1i, e2i, attr = struct.unpack_from(">fHHHHHH", data, prism_off + 0x10 + i * 0x10)
            a = struct.unpack_from(">fff", data, pos_off + pi * 12)
            fn = struct.unpack_from(">fff", data, nrm_off + fi * 12)
            e0 = struct.unpack_from(">fff", data, nrm_off + e0i * 12)
            e1 = struct.unpack_from(">fff", data, nrm_off + e1i * 12)
            e2 = struct.unpack_from(">fff", data, nrm_off + e2i * 12)
            c1 = cross(fn, e1)
            c2 = cross(e0, fn)
            d1 = dot(c1, e2)
            d2 = dot(c2, e2)
            if d1 == 0 or d2 == 0:
                continue
            q1 = h / d1
            q2 = h / d2
            b = tuple(a[k] + c1[k] * q1 for k in range(3))
            c = tuple(a[k] + c2[k] * q2 for k in range(3))
            if not all(math.isfinite(v) for v in b + c):
                continue
            self.triangles.append((a, b, c))
            self.normals.append(fn)
            self.attributes.append(attr)


class Joint:
    def __init__(self, index, name, ssc, scale, rot, trans):
        self.index = index
        self.name = name
        self.ssc = ssc
        self.scale = scale
        self.rot = rot
        self.trans = trans
        self.parent = -1
        self.children = []


def euler_matrix(rx, ry, rz):
    sx, cx = sin(rx), cos(rx)
    sy, cy = sin(ry), cos(ry)
    sz, cz = sin(rz), cos(rz)
    return [[cz * cy, sx * cz * sy - cx * sz, cx * cz * sy + sx * sz],
            [sz * cy, sx * sz * sy + cx * cz, cx * sz * sy - sx * cz],
            [-sy, cy * sx, cy * cx]]


class Model:
    def __init__(self, data):
        data = yaz0_decompress(data)
        self.data = data
        self.sections = {}
        o = 0x20
        for _ in range(u32(data, 0x0C)):
            self.sections[data[o:o + 4].decode("ascii", "replace")] = o
            o += u32(data, o + 4)
        self.read_inf1()
        self.read_jnt1()

    def read_inf1(self):
        b = self.data
        o = self.sections["INF1"]
        self.mtx_calc = u16(b, o + 8) & 0xF
        h = o + u32(b, o + 0x14)
        self.joint_parent = {}
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
        o = self.sections["JNT1"]
        n = u16(b, o + 8)
        data_off = o + u32(b, o + 0x0C)
        remap = o + u32(b, o + 0x10)
        names = name_table(b, o + u32(b, o + 0x14))
        self.joints = []
        for i in range(n):
            p = data_off + u16(b, remap + i * 2) * 0x40
            rot = tuple(r * PI / 32768.0 for r in struct.unpack_from(">hhh", b, p + 0x10))
            self.joints.append(Joint(i, names[i], b[p + 2], struct.unpack_from(">fff", b, p + 4), rot,
                                     struct.unpack_from(">fff", b, p + 0x18)))
        for j in sorted(self.joint_parent):
            p = self.joint_parent[j]
            if p is not None and j < n and p < n:
                self.joints[j].parent = p
                self.joints[p].children.append(j)

    def joint(self, name):
        for j in self.joints:
            if j.name == name:
                return j.index
        raise KeyError(name)

    def joint_order(self):
        order = []
        stack = list(reversed([j.index for j in self.joints if j.parent < 0]))
        while stack:
            j = stack.pop()
            order.append(j)
            stack.extend(reversed(self.joints[j].children))
        seen = set(order)
        order += [j.index for j in self.joints if j.index not in seen]
        return order

    def chain(self, targets):
        need = set()
        for j in targets:
            while j >= 0 and j not in need:
                need.add(j)
                j = self.joints[j].parent
        return [j for j in self.joint_order() if j in need]

    def local_matrix(self, j, s, r, t, parent_scale):
        m = euler_matrix(*r)
        inv = None
        if self.mtx_calc == 2 and self.joints[j].ssc == 1 and parent_scale is not None:
            inv = tuple(1.0 / x if x != 0 else 0.0 for x in parent_scale)
        out = []
        for i in range(3):
            row = [m[i][k] * s[k] for k in range(3)]
            if inv is not None:
                row = [v * inv[i] for v in row]
            out.append(row + [float(t[i])])
        return out

    def bind_sample(self, j):
        jt = self.joints[j]
        return jt.scale, jt.rot, jt.trans

    def world(self, sample, order=None, parent_scale=True):
        if order is None:
            order = self.joint_order()
        raw = [None] * len(self.joints)
        out = [None] * len(self.joints)
        for j in order:
            s, r, t = raw[j] = sample(j)
            p = self.joints[j].parent
            local = self.local_matrix(j, s, r, t, raw[p][0] if parent_scale and p >= 0 else None)
            out[j] = local if p < 0 else mtx_mul(out[p], local)
        return out

    def bind_world(self):
        return self.world(self.bind_sample)

    def pose(self, bck, frame, order=None, parent_scale=True):
        def sample(j):
            if j < bck.joint_count:
                return bck.sample(j, frame)
            return self.bind_sample(j)
        return self.world(sample, order, parent_scale)


class Track:
    def __init__(self, keys=None, const=None):
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
        lo, hi = 0, len(k)
        while lo < hi:
            mid = (lo + hi) // 2
            if f < k[mid][0]:
                hi = mid
            else:
                lo = mid + 1
        i = lo - 1
        t0, v0, _, o0 = k[i]
        t1, v1, i1, _ = k[i + 1]
        d = t1 - t0
        if d <= 0:
            return v1
        s = (f - t0) / d
        s2 = s * s
        s3 = s2 * s
        return (v0 * (2 * s3 - 3 * s2 + 1) + v1 * (-2 * s3 + 3 * s2)
                + o0 * d * (s3 - 2 * s2 + s) + i1 * d * (s3 - s2))


def bck_track(cnt, idx, tmode, dat, default, vscale):
    if cnt == 0:
        return Track(const=default)
    if cnt == 1:
        return Track(const=float(dat[idx]) * vscale)
    stride = 4 if tmode == 1 else 3
    keys = []
    for i in range(cnt):
        r = dat[idx + i * stride:idx + i * stride + stride]
        keys.append((float(r[0]), float(r[1]) * vscale, float(r[2]) * vscale,
                     float(r[3] if stride == 4 else r[2]) * vscale))
    return Track(keys=keys)


class BCK:
    def __init__(self, data):
        b = yaz0_decompress(data)
        if b[:8] != b"J3D1bck1":
            raise ValueError("not a BCK")
        o = 0x20
        for _ in range(u32(b, 0x0C)):
            if b[o:o + 4] == b"ANK1":
                break
            o += u32(b, o + 4)
        self.loop_mode = b[o + 8]
        self.frame_count = u16(b, o + 0x0A)
        self.joint_count = u16(b, o + 0x0C)
        ns, nr, nt = u16(b, o + 0x0E), u16(b, o + 0x10), u16(b, o + 0x12)
        joff = o + u32(b, o + 0x14)
        sdat = struct.unpack_from(">%df" % ns, b, o + u32(b, o + 0x18))
        rdat = struct.unpack_from(">%dh" % nr, b, o + u32(b, o + 0x1C))
        tdat = struct.unpack_from(">%df" % nt, b, o + u32(b, o + 0x20))
        rot_scale = float(1 << b[o + 9]) * PI / 32768.0
        kinds = ((sdat, 1.0, 1.0), (rdat, 0.0, rot_scale), (tdat, 0.0, 1.0))
        self.tracks = []
        for j in range(self.joint_count):
            comp = [[None] * 3 for _ in kinds]
            for axis in range(3):
                base = joff + j * 54 + axis * 18
                for k, (dat, default, vscale) in enumerate(kinds):
                    cnt, idx, tmode = struct.unpack_from(">HHH", b, base + k * 6)
                    comp[k][axis] = bck_track(cnt, idx, tmode, dat, default, vscale)
            self.tracks.append(comp)

    def sample(self, joint, frame):
        return tuple(tuple(float(t.sample(frame)) for t in c) for c in self.tracks[joint])


class CameraAnim:
    def __init__(self, data):
        data = yaz0_decompress(data)
        self.kind = data[4:8]
        self.frames, voff = struct.unpack_from(">II", data, 0x18)
        pool_at = 0x20 + voff
        n = u32(data, pool_at) // 4
        pool = struct.unpack_from(">%df" % n, data, pool_at + 4)
        self.tracks = []
        for c in range(8):
            if self.kind == b"CKAN":
                count, first, typ = struct.unpack_from(">III", data, 0x20 + c * 12)
                stride = 3 if typ == 0 else 4
                if count <= 1:
                    self.tracks.append([(0.0, pool[first], 0.0, 0.0)])
                    continue
                keys = []
                for k in range(count):
                    v = pool[first + k * stride:first + k * stride + stride]
                    keys.append((v[0], v[1], v[2], v[2] if stride == 3 else v[3]))
                self.tracks.append(keys)
            else:
                count, first = struct.unpack_from(">II", data, 0x20 + c * 8)
                self.tracks.append([(float(k), pool[first + k], None, None) for k in range(count)])

    def value(self, c, t):
        keys = self.tracks[c]
        if len(keys) == 1:
            return keys[0][1]
        if self.kind != b"CKAN":
            i = int(t)
            r = min(max(t - i, 0.0), 1.0)
            if i < len(keys) - 1:
                return keys[i][1] + (keys[i + 1][1] - keys[i][1]) * r
            return keys[-1][1]
        if t <= keys[0][0]:
            return keys[0][1]
        if t >= keys[-1][0]:
            return keys[-1][1]
        i = max(k for k in range(len(keys)) if keys[k][0] <= t)
        t0, v0, _, m0 = keys[i]
        t1, v1, m1, _ = keys[i + 1]
        d = t1 - t0
        u = (t - t0) / d
        u2 = u * u
        u3 = u2 * u
        m0, m1 = m0 / 30.0, m1 / 30.0
        return ((2 * u3 - 3 * u2 + 1) * v0 + (u3 - 2 * u2 + u) * d * m0
                + (-2 * u3 + 3 * u2) * v1 + (u3 - u2) * d * m1)

    def sample(self, t):
        return [self.value(c, t) for c in range(8)]
