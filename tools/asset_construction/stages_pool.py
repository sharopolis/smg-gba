import math
import struct

from common import f32_at, name_table, u16, u32, yaz0_decompress
from stages_galaxy import Arc
from stages_math import PI, cos, radians, rot_matrix, sin

OBJ_SCALE = 1000.0


def euler_matrix(rx, ry, rz):
    sx, cx = sin(rx), cos(rx)
    sy, cy = sin(ry), cos(ry)
    sz, cz = sin(rz), cos(rz)
    return [[cz * cy, sx * cz * sy - cx * sz, cx * cz * sy + sx * sz],
            [sz * cy, sx * sz * sy + cx * cz, cx * sz * sy - sx * cz],
            [-sy, cy * sx, cy * cx]]


def mat4_mul(a, b):
    return [[a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j] for j in range(4)]
            for i in range(4)]


def nearest(stack, typ):
    for s in reversed(stack):
        if s is not None and s[0] == typ:
            return s[1]
    return None


class Model:
    def __init__(self, data):
        self.data = data
        self.sec = {}
        o = 0x20
        for _ in range(u32(data, 0x0C)):
            size = u32(data, o + 4)
            self.sec[data[o:o + 4].decode("ascii", "replace")] = (o, size)
            o += size
        self.read_inf1()
        self.read_jnt1()
        self.read_evp1()
        self.read_drw1()
        self.read_vtx1()
        self.read_tex1()
        self.read_mat3()
        self.read_shp1()
        self.bind_world = self.bind_matrices()
        self.vertex_matrices = {}

    def read_inf1(self):
        b = self.data
        o, _ = self.sec["INF1"]
        self.mtx_calc = u16(b, o + 8) & 0xF
        h = o + u32(b, o + 0x14)
        self.shape_material = {}
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
                    self.joint_parent[v] = nearest(stack, 0x10)
                elif t == 0x12:
                    mat = nearest(stack, 0x11)
                    if mat is None and last is not None and last[0] == 0x11:
                        mat = last[1]
                    self.shape_material[v] = mat
                last = (t, v)

    def read_jnt1(self):
        b = self.data
        o, _ = self.sec["JNT1"]
        n = u16(b, o + 8)
        data_off = o + u32(b, o + 0x0C)
        remap_off = o + u32(b, o + 0x10)
        self.joints = []
        for i in range(n):
            p = data_off + u16(b, remap_off + i * 2) * 0x40
            rot = tuple(r * PI / 32768.0 for r in struct.unpack_from(">hhh", b, p + 0x10))
            self.joints.append({"ssc": b[p + 2], "scale": struct.unpack_from(">fff", b, p + 4), "rot": rot,
                                "trans": struct.unpack_from(">fff", b, p + 0x18), "parent": -1,
                                "children": []})
        for j, p in sorted(self.joint_parent.items()):
            if p is not None and j < n and p < n:
                self.joints[j]["parent"] = p
                self.joints[p]["children"].append(j)

    def read_evp1(self):
        self.envelopes = []
        self.ibm = {}
        if "EVP1" not in self.sec:
            return
        b = self.data
        o, size = self.sec["EVP1"]
        n = u16(b, o + 8)
        cnt_off = o + u32(b, o + 0x0C)
        idx_off = o + u32(b, o + 0x10)
        w_off = o + u32(b, o + 0x14)
        m_rel = u32(b, o + 0x18)
        ii = 0
        maxj = -1
        for e in range(n):
            c = b[cnt_off + e]
            env = []
            for k in range(c):
                j = u16(b, idx_off + (ii + k) * 2)
                env.append((j, f32_at(b, w_off + (ii + k) * 4)))
                maxj = max(maxj, j)
            ii += c
            self.envelopes.append(env)
        if m_rel:
            for j in range(maxj + 1):
                p = o + m_rel + j * 48
                if p + 48 > o + size:
                    break
                v = struct.unpack_from(">12f", b, p)
                self.ibm[j] = [list(v[0:4]), list(v[4:8]), list(v[8:12]), [0.0, 0.0, 0.0, 1.0]]

    def read_drw1(self):
        b = self.data
        o, _ = self.sec["DRW1"]
        n = u16(b, o + 8)
        wo = o + u32(b, o + 0x0C)
        do = o + u32(b, o + 0x10)
        self.drw = [(bool(b[wo + i]), u16(b, do + i * 2)) for i in range(n)]

    def read_vtx1(self):
        b = self.data
        o, size = self.sec["VTX1"]
        fmt_off = o + u32(b, o + 8)
        offs = [u32(b, o + 0x0C + i * 4) for i in range(13)]
        formats = {}
        p = fmt_off
        while True:
            attr, cc, ct, frac = struct.unpack_from(">IIIB", b, p)
            p += 16
            if attr == 0xFF:
                break
            formats[attr] = (cc, ct, frac)
        self.arrays = {}
        sorted_offs = sorted(x for x in offs if x)
        for attr, (cc, ct, frac) in formats.items():
            if attr == 9:
                slot = 0
            elif 13 <= attr <= 20:
                slot = attr - 8
            else:
                continue
            start = offs[slot]
            if not start:
                continue
            nxt = [x for x in sorted_offs if x > start]
            end = nxt[0] if nxt else size
            raw = b[o + start:o + end]
            if attr == 9:
                ncomp = 3 if cc == 1 else 2
            else:
                ncomp = 2 if cc == 1 else 1
            code, isz = {0: ("B", 1), 1: ("b", 1), 2: ("H", 2), 3: ("h", 2), 4: ("f", 4)}[ct]
            cnt = len(raw) // (isz * ncomp)
            vals = struct.unpack_from(">%d%s" % (cnt * ncomp, code), raw, 0)
            if ct != 4:
                sc = float(1 << frac)
                vals = [v / sc for v in vals]
            else:
                vals = [float(v) for v in vals]
            rows = [tuple(vals[i * ncomp:(i + 1) * ncomp]) for i in range(cnt)]
            if ncomp == 2 and attr == 9:
                rows = [r + (0.0,) for r in rows]
            if ncomp == 1:
                rows = [r + (0.0,) for r in rows]
            self.arrays[attr] = rows

    def read_tex1(self):
        self.textures = []
        if "TEX1" not in self.sec:
            return
        b = self.data
        o, _ = self.sec["TEX1"]
        if u32(b, o + 0x10):
            self.textures = name_table(b, o + u32(b, o + 0x10))

    def read_mat3(self):
        self.materials = []
        sec = self.sec.get("MAT3") or self.sec.get("MAT2")
        if not sec:
            return
        b = self.data
        o, _ = sec
        n = u16(b, o + 8)
        offs = [u32(b, o + 0x0C + i * 4) for i in range(30)]
        init = o + offs[0]
        remap = o + offs[1]
        names = name_table(b, o + offs[2]) if offs[2] else []
        texremap = o + offs[15] if offs[15] else None
        tevorder = o + offs[16] if offs[16] else None
        texgen = o + offs[11] if offs[11] else None
        for i in range(n):
            p = init + u16(b, remap + i * 2) * 0x14C
            tex = []
            for k in range(8):
                t = u16(b, p + 0x84 + k * 2)
                tex.append(None if t == 0xFFFF or texremap is None else u16(b, texremap + t * 2))
            used = {}
            if tevorder is not None:
                for k in range(16):
                    to = u16(b, p + 0xBC + k * 2)
                    if to == 0xFFFF:
                        continue
                    tc, tm = b[tevorder + to * 4], b[tevorder + to * 4 + 1]
                    if tm != 0xFF and tm < 8 and tex[tm] is not None and tm not in used:
                        used[tm] = tc
            first = None
            coord = None
            if used:
                first = min(used)
                coord = used[first]
            else:
                for k, t in enumerate(tex):
                    if t is not None:
                        first = coord = k
                        break
            uv_src = 0
            if coord is not None and coord < 8 and texgen is not None:
                tg = u16(b, p + 0x28 + coord * 2)
                if tg != 0xFFFF:
                    src = b[texgen + tg * 4 + 1]
                    uv_src = src - 4 if 4 <= src <= 11 else -1
            texname = None
            if first is not None and tex[first] is not None and tex[first] < len(self.textures):
                texname = self.textures[tex[first]]
            name = names[i] if i < len(names) else "mat%d" % i
            self.materials.append({"name": name, "texture": texname, "uv_src": uv_src})

    def read_shp1(self):
        b = self.data
        o, _ = self.sec["SHP1"]
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
            desc_off = u16(b, p + 4)
            first_mtx = u16(b, p + 6)
            first_draw = u16(b, p + 8)
            desc = []
            q = desc_base + desc_off
            while True:
                attr, typ = struct.unpack_from(">II", b, q)
                q += 8
                if attr == 0xFF:
                    break
                desc.append((attr, typ))
            verts = []
            tris = []
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
            self.shapes.append({"material": self.shape_material.get(si), "verts": verts, "tris": tris})

    def read_display_list(self, start, size, desc, slots, default, verts, tris):
        b = self.data
        p = start
        end = start + size
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
                    elif attr >= 9:
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

    def local_matrix(self, j):
        jt = self.joints[j]
        s = jt["scale"]
        r = euler_matrix(*jt["rot"])
        m = [[r[i][k] * s[k] for k in range(3)] + [jt["trans"][i]] for i in range(3)]
        if self.mtx_calc == 2 and jt["ssc"] == 1 and jt["parent"] >= 0:
            ps = self.joints[jt["parent"]]["scale"]
            for i in range(3):
                inv = 1.0 / ps[i] if ps[i] != 0 else 0.0
                for k in range(3):
                    m[i][k] *= inv
        m.append([0.0, 0.0, 0.0, 1.0])
        return m

    def bind_matrices(self):
        n = len(self.joints)
        order = []
        stack = list(reversed([j for j in range(n) if self.joints[j]["parent"] < 0]))
        while stack:
            j = stack.pop()
            order.append(j)
            stack.extend(reversed(self.joints[j]["children"]))
        seen = set(order)
        order += [j for j in range(n) if j not in seen]
        world = [None] * n
        for j in order:
            lm = self.local_matrix(j)
            p = self.joints[j]["parent"]
            world[j] = lm if p < 0 or world[p] is None else mat4_mul(world[p], lm)
        return world

    def vertex_matrix(self, drw_index):
        m = self.vertex_matrices.get(drw_index)
        if m is not None:
            return m
        weighted, idx = self.drw[drw_index]
        if not weighted:
            m = self.bind_world[idx]
        else:
            m = [[0.0] * 4 for _ in range(4)]
            for j, w in self.envelopes[idx]:
                ibm = self.ibm.get(j)
                if ibm is None:
                    continue
                bm = mat4_mul(self.bind_world[j], ibm)
                for r in range(4):
                    for c in range(4):
                        m[r][c] += w * bm[r][c]
        self.vertex_matrices[drw_index] = m
        return m

    def position(self, drw, pi):
        m = self.vertex_matrix(drw)
        x, y, z = self.arrays[9][pi]
        return (m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3],
                m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3],
                m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3])


def kcl_triangles(data):
    data = yaz0_decompress(data)
    pos_off, nrm_off, prism_off, oct_off = struct.unpack_from(">IIII", data, 0)
    n_prisms = (oct_off - (prism_off + 0x10)) // 0x10
    prisms = [struct.unpack_from(">fHHHHHH", data, prism_off + 0x10 + i * 0x10) for i in range(n_prisms)]
    if not prisms:
        return []
    n_pos = (nrm_off - pos_off) // 12 if nrm_off > pos_off else 0
    n_pos = max(n_pos, max(p[1] for p in prisms) + 1)
    n_nrm = (prism_off + 0x10 - nrm_off) // 12 if prism_off > nrm_off else 0
    n_nrm = max(n_nrm, max(max(p[2], p[3], p[4], p[5]) for p in prisms) + 1)
    pos = [struct.unpack_from(">3f", data, pos_off + i * 12) for i in range(n_pos)]
    nrm = [struct.unpack_from(">3f", data, nrm_off + i * 12) for i in range(n_nrm)]
    out = []
    for h, pi, fi, e0i, e1i, e2i, _ in prisms:
        a = pos[pi]
        fn, e0, e1, e2 = nrm[fi], nrm[e0i], nrm[e1i], nrm[e2i]
        c1 = (fn[1] * e1[2] - fn[2] * e1[1], fn[2] * e1[0] - fn[0] * e1[2], fn[0] * e1[1] - fn[1] * e1[0])
        c2 = (e0[1] * fn[2] - e0[2] * fn[1], e0[2] * fn[0] - e0[0] * fn[2], e0[0] * fn[1] - e0[1] * fn[0])
        d1 = c1[0] * e2[0] + c1[1] * e2[1] + c1[2] * e2[2]
        d2 = c2[0] * e2[0] + c2[1] * e2[1] + c2[2] * e2[2]
        if d1 == 0.0 or d2 == 0.0:
            continue
        f1 = h / d1
        f2 = h / d2
        bp = (a[0] + c1[0] * f1, a[1] + c1[1] * f1, a[2] + c1[2] * f1)
        cp = (a[0] + c2[0] * f2, a[1] + c2[1] * f2, a[2] + c2[2] * f2)
        if not all(math.isfinite(v) for v in bp + cp):
            continue
        out.append((tuple(a), bp, cp))
    return out


class ArchiveCache:
    def __init__(self, dump):
        self.dump = dump
        self.cache = {}

    def parts(self, name):
        if name in self.cache:
            return self.cache[name]
        out = []
        self.cache[name] = out
        try:
            data = self.dump.read("ObjectData/%s.arc" % name)
        except OSError:
            return out
        arc = Arc(data)
        for path in arc.files:
            fn = path.rsplit("/", 1)[-1].lower()
            if fn.endswith((".bdl", ".bmd")):
                out.append(("mdl", Model(arc.get(path))))
            elif fn.endswith(".kcl"):
                out.append(("kcl", kcl_triangles(arc.get(path))))
        return out


def material_of(model, sh):
    mi = sh["material"]
    if mi is None or mi >= len(model.materials):
        return None
    return model.materials[mi]


def model_points(model):
    seen = {}
    uvs = []
    pts = []
    for sh in model.shapes:
        mat = material_of(model, sh)
        src = mat["uv_src"] if mat else 0
        has_src = src is not None and src >= 0
        tex = model.arrays.get(13 + src) if has_src else None
        for drw, att in sh["verts"]:
            pi = att.get(9)
            if pi is None:
                continue
            key = (drw, pi)
            k = seen.get(key)
            if k is None:
                k = seen[key] = len(pts)
                pts.append(model.position(drw, pi))
                uvs.append([])
            ti = att.get(13 + src) if has_src else None
            if tex is not None and ti is not None and ti < len(tex):
                uv = (tex[ti][0], 1.0 - tex[ti][1])
                if uv not in uvs[k]:
                    uvs[k].append(uv)
    return pts, uvs


def kcl_points(tris):
    seen = set()
    pts = []
    for t in tris:
        for p in t:
            q = (float("%.3f" % p[0]), float("%.3f" % p[1]), float("%.3f" % p[2]))
            if q not in seen:
                seen.add(q)
                pts.append(q)
    return pts


class Pool:
    def __init__(self):
        self.pts = []
        self.uvs = []

    def add(self, pts, uvs, world):
        r = rot_matrix(world["rot"])
        s = world["scale"]
        t = world["pos"]
        m = [[r[i][k] * s[k] for k in range(3)] for i in range(3)]
        for i, p in enumerate(pts):
            x = (m[0][0] * p[0] + m[0][1] * p[1] + m[0][2] * p[2] + t[0]) / OBJ_SCALE
            y = (m[1][0] * p[0] + m[1][1] * p[1] + m[1][2] * p[2] + t[1]) / OBJ_SCALE
            z = (m[2][0] * p[0] + m[2][1] * p[1] + m[2][2] * p[2] + t[2]) / OBJ_SCALE
            self.pts.append((x, y, z))
            self.uvs.append(uvs[i] if uvs else [])


def mat3_mul(a, b):
    out = []
    for row in a:
        out_row = []
        for j in range(len(b[0])):
            s = 0.0
            for k in range(len(row)):
                s += row[k] * b[k][j]
            out_row.append(0.0 + s)
        out.append(out_row)
    return out


def placement_matrix(raw):
    pos, rot, scl = raw
    x, y, z = (radians(a) for a in rot)
    rx = [[1.0, 0.0, 0.0], [0.0, cos(x), -sin(x)], [0.0, sin(x), cos(x)]]
    ry = [[cos(y), 0.0, sin(y)], [0.0, 1.0, 0.0], [-sin(y), 0.0, cos(y)]]
    rz = [[cos(z), -sin(z), 0.0], [sin(z), cos(z), 0.0], [0.0, 0.0, 1.0]]
    r = mat3_mul(mat3_mul(mat3_mul(rz, ry), rx), [[scl[0], 0.0, 0.0], [0.0, scl[1], 0.0], [0.0, 0.0, scl[2]]])
    return [r[0] + [pos[0]], r[1] + [pos[1]], r[2] + [pos[2]], [0.0, 0.0, 0.0, 1.0]]


def placement_world(obj, placements):
    chain = [placement_matrix(obj["raw"])]
    zp = obj["zone_placement"]
    while zp is not None and zp >= 0:
        chain.append(placement_matrix(placements[zp]["raw"]))
        zp = placements[zp]["parent"]
    m = chain[-1]
    for c in reversed(chain[:-1]):
        m = mat3_mul(m, c)
    return m


def build_pool(arcs, objects, sources, placements):
    pool = Pool()
    for arcname, obj_index, kinds in sources:
        world = objects[obj_index]["world"]
        for kind, data in arcs.parts(arcname):
            if kind == "kcl" and "kclx" in kinds:
                m = placement_world(objects[obj_index], placements)
                for t in data:
                    for p in t:
                        q = [(0.0 + p[0] * m[i][0] + p[1] * m[i][1] + p[2] * m[i][2] + m[i][3]) / OBJ_SCALE
                             for i in range(3)]
                        pool.pts.append(tuple(q))
                        pool.uvs.append([])
            if kind not in kinds:
                continue
            if kind == "mdl":
                pts, uvs = model_points(data)
            else:
                pts, uvs = kcl_points(data), None
            pool.add(pts, uvs, world)
    return pool


def name_candidates(arcs, sources, objects, zone_list):
    names = set()
    for arcname, _, _ in sources:
        names.add(arcname)
        for kind, data in arcs.parts(arcname):
            if kind == "mdl":
                names.update(data.textures)
                names.update(m["name"] for m in data.materials)
    for o in objects:
        names.add(o["name"])
    names.update(zone_list)
    out = []
    for n in sorted(n for n in names if n):
        out += ["newmtl " + n, "\tmap_Kd " + n + ".png", "usemtl " + n, "o " + n, "o " + n.lower()]
    return out


def texture_repeats(arcs, obj):
    w = obj["world"]
    r = rot_matrix(w["rot"])
    s = w["scale"]
    m = [[r[i][k] * s[k] / 256.0 for k in range(3)] for i in range(3)]
    acc = {}
    for kind, model in arcs.parts(obj["name"]):
        if kind != "mdl":
            continue
        for sh in model.shapes:
            mat = material_of(model, sh)
            if mat is None:
                continue
            src = mat["uv_src"]
            if mat["texture"] is None or src is None or src < 0 or 13 + src not in model.arrays:
                continue
            tex = model.arrays[13 + src]
            a = acc.setdefault(mat["texture"], [0.0, 0.0])
            for tri in sh["tris"]:
                if any(9 not in sh["verts"][vi][1] or 13 + src not in sh["verts"][vi][1] for vi in tri):
                    continue
                ps = []
                us = []
                for vi in tri:
                    drw, att = sh["verts"][vi]
                    p = model.position(drw, att[9])
                    ps.append([m[i][0] * p[0] + m[i][1] * p[1] + m[i][2] * p[2] for i in range(3)])
                    us.append(tex[att[13 + src]])
                e1 = [ps[1][k] - ps[0][k] for k in range(3)]
                e2 = [ps[2][k] - ps[0][k] for k in range(3)]
                cx = e1[1] * e2[2] - e1[2] * e2[1]
                cy = e1[2] * e2[0] - e1[0] * e2[2]
                cz = e1[0] * e2[1] - e1[1] * e2[0]
                du1 = us[1][0] - us[0][0]
                dv1 = us[1][1] - us[0][1]
                du2 = us[2][0] - us[0][0]
                dv2 = us[2][1] - us[0][1]
                ua = abs(du1 * dv2 - dv1 * du2) / 2
                if ua > 1e-9:
                    a[0] += math.sqrt(cx * cx + cy * cy + cz * cz) / 2
                    a[1] += ua
    out = {}
    for t, (area, uva) in acc.items():
        if uva > 0:
            out[t] = max(0.25, math.sqrt(area / uva))
    return out
