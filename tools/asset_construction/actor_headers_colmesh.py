import math

from actor_headers_lib import BCSV, KCL, cross, dot, sub
from common import IntReader, f32

MAX_GROUP = 24
DECIMATE_ABOVE = 128
COL_WATER, COL_NO_CAMERA, COL_HONEY, COL_NOT_GRAB = 1 << 3, 1 << 4, 1 << 5, 1 << 6
WALL_NOT_GRAB, WALL_FUR = 3, 7
FLOOR_WATER_BOTTOM = (20, 21, 22)
OPTIMIZE_EPS = 1e-8

MAIN = [
    ("BeeFlowerHover", ("beeflowerhover.kcl",)),
    ("LotusLeaf", ("lotusleaf.kcl",)),
    ("JumpStand", ("jumpstand.kcl",)),
    ("HipDropRock", ("hipdroprock.kcl",)),
    ("HipDropSwitch", ("hipdropswitch.kcl", "move.kcl")),
    ("ForestAppearStepA", ("forestappearstepa.kcl",)),
    ("ForestObstacleWoodA", ("forestobstaclewooda.kcl",)),
    ("ForestWoodCover", ("forestwoodcover.kcl",)),
    ("StrongBlock", ("strongblock.kcl",)),
    ("WatchTowerRotateStep", ("watchtowerrotatestep.kcl",)),
    ("WatchTowerRotateStepLift", ("watchtowerrotatesteplift.kcl",)),
    ("ForestBarricadeRockA", ("forestbarricaderocka.kcl",)),
    ("ForestHomeGate", ("foresthomegate.kcl",)),
    ("PicketSwitch", ("picketswitch.kcl",)),
    ("Pole", ("pole.kcl",)),
    ("CoinBlock", ("coinblock.kcl",)),
    ("EarthenPipe", ("earthenpipe.kcl",)),
    ("StinkBugParent", ("switch.kcl",)),
    ("BossStinkBug", ("body.kcl",)),
    ("HoneyQueen", ("centerfur.kcl",)),
    ("HoneyQueen", ("center.kcl",)),
    ("HoneyQueen", ("face.kcl",)),
    ("HoneyQueen", ("larm01.kcl",)),
    ("HoneyQueen", ("larm02.kcl",)),
    ("HoneyQueen", ("lfoot001.kcl",)),
    ("HoneyQueen", ("rarm01.kcl",)),
    ("HoneyQueen", ("rarm02.kcl",)),
    ("HoneyQueen", ("rfoot001.kcl",)),
    ("HoneyQueen", ("tactile.kcl",)),
]
BOSS = [
    ("BossStinkBug", ("body.kcl",), "wall"),
    ("BossStinkBug", ("bodyground.kcl",), "wall"),
    ("BossStinkBug", ("bodyhipdrop.kcl",), "floor"),
]


def attr_bits(pa, i, not_grab):
    if pa is None or not 0 <= i < pa.n:
        return 0
    a = 0
    wall = pa.get(i, "Wall_code", 0)
    if wall == WALL_FUR:
        a |= COL_HONEY
    if not_grab and wall == WALL_NOT_GRAB:
        a |= COL_NOT_GRAB
    if pa.get(i, "Floor_code", 0) in FLOOR_WATER_BOTTOM:
        a |= COL_WATER
    if pa.get(i, "Camera_through") == 1:
        a |= COL_NO_CAMERA
    return a


def pa_table(ctx, rel, name):
    try:
        return BCSV(ctx.arc_file(rel, name[:-4] + ".pa"))
    except KeyError:
        return None


def load(ctx, archive, names, not_grab):
    rel = "ObjectData/%s.arc" % archive
    tris, attrs, codes = [], [], []
    for name in names:
        k = KCL(ctx.arc_file(rel, name))
        pa = pa_table(ctx, rel, name)
        for t, n, a in zip(k.triangles, k.normals, k.attributes):
            if dot(cross(sub(t[1], t[0]), sub(t[2], t[0])), n) < 0:
                t = (t[0], t[2], t[1])
            tris.append(t)
            attrs.append(attr_bits(pa, a, not_grab))
            ok = pa is not None and 0 <= a < pa.n
            codes.append((pa.get(a, "Wall_code", 0), pa.get(a, "Floor_code", 0)) if ok else (0, 0))
    return tris, attrs, codes


def weld(tris):
    key = {}
    verts = []
    faces = []
    for t in tris:
        f = []
        for p in t:
            k = (round(p[0]), round(p[1]), round(p[2]))
            if k not in key:
                key[k] = len(verts)
                verts.append(p)
            f.append(key[k])
        faces.append(f)
    parent = list(range(len(verts)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for f in faces:
        for a in f[1:]:
            parent[find(a)] = find(f[0])
    comps = {}
    for fi, f in enumerate(faces):
        if len(set(f)) < 3:
            continue
        comps.setdefault(find(f[0]), []).append(fi)
    return verts, faces, list(comps.values())


def most_common(values):
    best = None
    for v in values:
        if best is None or values.count(v) > values.count(best):
            best = v
    return best


def plane_quadric(a, b, c):
    a, b, c = [tuple(f32(x) for x in p) for p in (a, b, c)]
    n1 = [f32(a[k] - b[k]) for k in range(3)]
    n2 = [f32(b[k] - c[k]) for k in range(3)]
    n = [f32(f32(n1[1] * n2[2]) - f32(n1[2] * n2[1])),
         f32(f32(n1[2] * n2[0]) - f32(n1[0] * n2[2])),
         f32(f32(n1[0] * n2[1]) - f32(n1[1] * n2[0]))]
    d = f32(f32(f32(n[0] * n[0]) + f32(n[1] * n[1])) + f32(n[2] * n[2]))
    if d > 1e-35:
        inv = f32(1.0 / f32(math.sqrt(d)))
        n = [f32(x * inv) for x in n]
    third = f32(1.0 / 3.0)
    cen = [f32(f32(f32(f32(a[k] + b[k]) + c[k])) * third) for k in range(3)]
    p = (n[0], n[1], n[2], -(n[0] * cen[0] + n[1] * cen[1] + n[2] * cen[2]))
    return [p[i] * p[j] for i in range(4) for j in range(4)]


def optimize(q):
    m = [[q[0], q[1], q[2]], [q[4], q[5], q[6]], [q[8], q[9], q[10]]]
    v = (q[3], q[7], q[11])
    c00 = m[1][1] * m[2][2] - m[1][2] * m[2][1]
    c01 = m[1][2] * m[2][0] - m[1][0] * m[2][2]
    c02 = m[1][0] * m[2][1] - m[1][1] * m[2][0]
    det = m[0][0] * c00 + m[0][1] * c01 + m[0][2] * c02
    if abs(det) <= OPTIMIZE_EPS:
        return None
    inv = [[c00, m[0][2] * m[2][1] - m[0][1] * m[2][2], m[0][1] * m[1][2] - m[0][2] * m[1][1]],
           [c01, m[0][0] * m[2][2] - m[0][2] * m[2][0], m[0][2] * m[1][0] - m[0][0] * m[1][2]],
           [c02, m[0][1] * m[2][0] - m[0][0] * m[2][1], m[0][0] * m[1][1] - m[0][1] * m[1][0]]]
    return tuple(-(inv[i][0] * v[0] + inv[i][1] * v[1] + inv[i][2] * v[2]) / det for i in range(3))


def decimated_hull(points, rd):
    labels = [rd.int() for _ in points]
    pre = [i for i, label in enumerate(labels) if label]
    faces = [[pre[rd.int()] for _ in range(3)] for _ in range(rd.int())]
    survivors = [i for i, label in enumerate(labels) if label == 2]
    cluster = {}
    for i, label in enumerate(labels):
        if label == 2:
            cluster[i] = survivors.index(i)
        elif label > 2:
            cluster[i] = label - 3
    quad = [[0.0] * 16 for _ in survivors]
    for f in faces:
        q = None
        for i in f:
            if i in cluster:
                q = q or plane_quadric(*[points[j] for j in f])
                acc = quad[cluster[i]]
                for k in range(16):
                    acc[k] += q[k]
    slot = {}
    out = []
    for i, label in enumerate(labels):
        if label == 1:
            slot[i] = len(out)
            out.append(tuple(f32(x) for x in points[i]))
        elif label == 2:
            slot[i] = len(out)
            out.append(tuple(f32(x) for x in optimize(quad[cluster[i]]) or points[i]))
    for i, c in cluster.items():
        slot[i] = slot[survivors[c]]
    tris = []
    for f in faces:
        g = [slot[i] for i in f]
        if g[0] != g[1] and g[1] != g[2] and g[0] != g[2]:
            tris.append(g)
    sx = sy = sz = 0.0
    for p in out:
        sx += p[0]
        sy += p[1]
        sz += p[2]
    c = (sx / len(out), sy / len(out), sz / len(out))
    for g in tris:
        a, b, d = out[g[0]], out[g[1]], out[g[2]]
        mid = tuple((a[k] + b[k] + d[k]) / 3 for k in range(3))
        if dot(cross(sub(b, a), sub(d, a)), sub(mid, c)) < 0:
            g[1], g[2] = g[2], g[1]
    return out, tris


def keep_role(out_v, t, role):
    a, b, c = out_v[t[0]], out_v[t[1]], out_v[t[2]]
    n = cross(sub(b, a), sub(c, a))
    ny = n[1] / max(math.sqrt(dot(n, n)), 1e-9)
    return ny > 0.5 if role == "floor" else ny <= 0.5


def build_mesh(ctx, archive, names, rd, role=None, not_grab=False):
    tris, attrs, codes = load(ctx, archive, names, not_grab)
    verts, faces, comps = weld(tris)
    out_v, out_t, groups, out_codes = [], [], [], []
    for fis in comps:
        if len(fis) > DECIMATE_ABOVE:
            idx = sorted({i for fi in fis for i in faces[fi]})
            hv, hf = decimated_hull([verts[i] for i in idx], rd)
            a = most_common([attrs[fi] for fi in fis])
            base = len(out_v)
            out_v += hv
            tri_list = [(base + f[0], base + f[1], base + f[2], a) for f in hf]
            code_list = [(0, 0)] * len(tri_list)
        else:
            remap = {}
            tri_list = []
            code_list = []
            for fi in fis:
                g = []
                for i in faces[fi]:
                    if i not in remap:
                        remap[i] = len(out_v)
                        out_v.append(verts[i])
                    g.append(remap[i])
                tri_list.append((g[0], g[1], g[2], attrs[fi]))
                code_list.append(codes[fi])
        if role:
            keep = [k for k, t in enumerate(tri_list) if keep_role(out_v, t, role)]
            tri_list = [tri_list[k] for k in keep]
            code_list = [code_list[k] for k in keep]
        if len(tri_list) > MAX_GROUP:
            cen = [tuple((out_v[t[0]][k] + out_v[t[1]][k] + out_v[t[2]][k]) / 3 for k in range(3))
                   for t in tri_list]
            ext = [max(c[k] for c in cen) - min(c[k] for c in cen) for k in range(3)]
            axis = ext.index(max(ext))
            order = sorted(range(len(tri_list)), key=lambda i: cen[i][axis])
            tri_list = [tri_list[i] for i in order]
            code_list = [code_list[i] for i in order]
        for k in range(0, len(tri_list), MAX_GROUP):
            chunk = tri_list[k:k + MAX_GROUP]
            pts = [out_v[i] for t in chunk for i in t[:3]]
            lo = [math.floor(min(p[c] for p in pts)) for c in range(3)]
            hi = [math.ceil(max(p[c] for p in pts)) for c in range(3)]
            groups.append(lo + hi + [len(out_t), len(chunk)])
            out_t += chunk
            out_codes += code_list[k:k + MAX_GROUP]
    vals = [len(out_v)]
    for p in out_v:
        vals += [round(c) for c in p]
    vals.append(len(out_t))
    for t in out_t:
        vals += list(t)
    vals.append(len(groups))
    for g in groups:
        vals += g
    return vals, len(groups), out_codes


def mesh_table(ctx, meshes, spec):
    rd = IntReader(spec)
    vals = []
    counts = []
    for m in meshes:
        v, ng, _ = build_mesh(ctx, m[0], m[1], rd, m[2] if len(m) > 2 else None)
        vals += v
        counts.append(ng)
    return vals + counts


def honey(ctx, spec):
    return mesh_table(ctx, MAIN, spec)


def honey_boss(ctx, spec):
    return mesh_table(ctx, BOSS, spec)


def mandibug_child(ctx, spec):
    rd = IntReader(spec)
    vals, ng, codes = build_mesh(ctx, "StinkBugParent", ("switchchildj.kcl",), rd, None, True)
    _, _, base = build_mesh(ctx, "StinkBugParent", ("switch.kcl",), rd, None, True)
    vals.append(ng)
    for cl in (codes, base):
        vals.append(len(cl))
        for w, f in cl:
            vals += [w, f]
    return vals


PREDICTORS = {
    "src/actors/honey_colmesh_gen.h": honey,
    "src/actors/honey_boss_colmesh_gen.h": honey_boss,
    "src/actors/honey_mandibug_child_colmesh_gen.h": mandibug_child,
}
