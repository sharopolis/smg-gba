import math

from actor_headers_lib import BCK, BCSV, KCL, PI, Model, cos, cross, dot, gba, mtx_tr, rint, sin, sub, wound
from common import fsum

MAPPARTS = [
    ("BroadBeanMoveStepA", "broadbeanmovestepa.kcl", True),
    ("BroadBeanMoveStepB", "broadbeanmovestepb.kcl", True),
    ("PhantomShipBridge", "phantomshipbridge.kcl", True),
    ("PhantomShipBridge", "move.kcl", True),
    ("RGTowerPressGroundA", "rgtowerpressgrounda.kcl", True),
    ("FlipPanel", "flippanel.kcl", False),
    ("SideSpikeMoveStepA", "sidespikemovestepa.kcl", True),
    ("CollapsePlane", "move.kcl", True),
]
FF_NAMES = ["IceMountainFloaterA", "IceMountainSeesawNoSlipA", "IceMountainSeesawSlipA",
            "IceMountainSeesawSlipB", "IceMountainSeesawSlipC", "IceMountainSeesawSlipD"]
BUOY_PARTS = ["OceanFloaterTypeU", "OceanHexagonFloater", "OceanFloaterTowerRotateStepA",
              "OceanFloaterTowerRotateStepB", "OceanFloaterTowerRotateStepD", "BreakableFixation",
              "BreakableCage", "ScrewSwitch", "ScrewSwitchReverse", "ValveSwitch", "AirBubbleGenerator",
              "LargeChain", "LargeChainFixPoint", "Dossun"]


def quant(t):
    return tuple(tuple(rint(v) for v in p) for p in t)


def source(ctx, arc, kcl, pa=None):
    k = KCL(ctx.arc_file(arc, kcl))
    rows = BCSV(ctx.arc_file(arc, pa)) if pa else None
    return rows, list(zip(k.triangles, k.normals, k.attributes))


def field(rows, a, name):
    if rows is None:
        return 0
    v = rows.get(a, name)
    return 0 if v is None else v


class Mesh:
    def __init__(self):
        self.verts = []
        self.ids = {}
        self.faces = []

    def add(self, q):
        f = []
        for p in q:
            i = self.ids.get(p)
            if i is None:
                i = self.ids[p] = len(self.verts)
                self.verts.append(p)
            f.append(i)
        self.faces.append(f)
        return f


def bounds(points):
    lo = [min(p[k] for p in points) for k in range(3)]
    hi = [max(p[k] for p in points) for k in range(3)]
    return lo, hi


def flat(rows):
    return [v for r in rows for v in r]


def floor_bits(rows, a):
    fl = field(rows, a, "Floor_code")
    wl = field(rows, a, "Wall_code")
    bits = 128 if fl in (5, 33) else 4 if fl == 2 else 0
    bits |= 64 if wl in (3, 8) else 0
    bits |= 16 if field(rows, a, "Camera_through") else 0
    return bits, fl | wl << 8


def chunk_groups(mesh):
    out = []
    for first in range(0, len(mesh.faces), 12):
        fs = mesh.faces[first:first + 12]
        lo, hi = bounds([mesh.verts[i] for f in fs for i in f[:3]])
        out.append(lo + hi + [first, len(fs)])
    return out


def honey_crystal(ctx, spec):
    _, tris = source(ctx, "ObjectData/CrystalCageS.arc", "crystalcages.kcl")
    m = Mesh()
    for t, n, _ in tris:
        m.add(quant(wound(t, n)))
    lo, hi = bounds(m.verts)
    return flat(m.verts) + flat(f[:3] for f in m.faces) + lo + hi + [len(m.faces)]


def egg_mesh(ctx):
    rows, tris = source(ctx, "ObjectData/CrystalCageM.arc", "crystalcagem.kcl", "crystalcagem.pa")
    m = Mesh()
    codes = []
    for t, n, a in tris:
        m.add(quant(wound(t, n)))
        codes.append(field(rows, a, "Floor_code") | field(rows, a, "Wall_code") << 8)
    return m, codes


def crystal_cage(ctx, spec):
    m, codes = egg_mesh(ctx)
    lo, hi = bounds(m.verts)
    return flat(m.verts) + flat(m.faces) + codes + lo + hi + [len(m.faces)]


def wrap32(n):
    n &= 0xFFFFFFFF
    return n - 0x100000000 if n & 0x80000000 else n


def direction14(a):
    m = max(abs(x) for x in a)
    down = up = 0
    while m >= 32768:
        m >>= 1
        down += 1
    while m < 16384:
        m <<= 1
        up += 1
    a = [wrap32((x >> down) << up) for x in a]
    length = math.isqrt(fsum(x * x for x in a) & 0xFFFFFFFF)
    rem, recip = 1 << 30, 0
    for bit in range(16, -1, -1):
        d = (length << bit) & 0xFFFFFFFF
        if rem >= d:
            rem -= d
            recip |= 1 << bit
    return [wrap32(x * recip) >> 16 for x in a]


def crystal_cage_normals(ctx, spec):
    m, _ = egg_mesh(ctx)
    out = [len(m.faces)]
    for f in m.faces:
        a, b, c = [[-m.verts[i][0], m.verts[i][2], m.verts[i][1]] for i in f]
        b = [wrap32(x - y) for x, y in zip(b, a)]
        c = [wrap32(x - y) for x, y in zip(c, a)]
        n = []
        for j, k in ((1, 2), (2, 0), (0, 1)):
            n.append(wrap32((wrap32(b[j] * c[k]) >> 8) - (wrap32(b[k] * c[j]) >> 8)))
        out += direction14(n)
    return out


def icestep(ctx, spec):
    rows, tris = source(ctx, "ObjectData/IceStep.arc", "icestep.kcl", "icestep.pa")
    m = Mesh()
    codes = []
    boxes = []
    for t, n, a in tris:
        q = quant(wound(t, n))
        m.add(q)
        codes.append(field(rows, a, "Floor_code") | field(rows, a, "Wall_code") << 8)
        lo, hi = bounds(q)
        boxes += lo + hi
    return [len(tris)] + flat(m.verts) + flat(m.faces) + codes + boxes


def bits_mesh(ctx, arc, kcl, pa, order=None):
    rows, tris = source(ctx, arc, kcl, pa)
    if order is not None:
        tris = order(tris)
    m = Mesh()
    codes = []
    for t, n, a in tris:
        f = m.add(quant(wound(t, n)))
        bits, code = floor_bits(rows, a)
        f.append(bits)
        codes.append(code)
    groups = chunk_groups(m)
    return flat(m.verts) + flat(m.faces) + codes + flat(groups), len(groups)


def zone_bcsvs(ctx, zone_rel, folder):
    out = []
    for path, blob in ctx.arc(zone_rel).files:
        parts = path.split("/")
        low = [p.lower() for p in parts]
        if "jmp" in low:
            k = low.index("jmp")
            if k + 1 < len(parts) and low[k + 1] == folder.lower():
                out.append(("/".join(parts[k + 2:]), BCSV(blob)))
    out.sort(key=lambda x: x[0].lower())
    return out


def placement_rows(ctx, zone, folder, names):
    out = []
    for _, b in zone_bcsvs(ctx, zone, folder):
        for i in range(b.n):
            if b.get(i, "name") in names:
                out.append((b, i))
    return out


def leak_specs(ctx, zone):
    out = []
    for b, i in placement_rows(ctx, zone, "Placement", ("WaterLeakPipe",)):
        h = b.get(i, "Obj_arg0")
        h = h if h >= 0 else 500
        out += [h, h]
    return out


def waterleak(ctx, spec):
    vals, _ = bits_mesh(ctx, "ObjectData/IceStepNoSlip.arc", "icestepnoslip.kcl", "icestepnoslip.pa")
    return vals + leak_specs(ctx, "StageData/IceMountainZone.arc")


def ff_order(tris):
    def key(i):
        t = tris[i][0]
        return tuple(rint((t[0][k] + t[1][k] + t[2][k]) / 3 / 200) for k in range(3))
    return [tris[i] for i in sorted(range(len(tris)), key=key)]


def freezeflame_mountain(ctx, spec):
    out = []
    counts = []
    for name in FF_NAMES:
        low = name.lower()
        vals, n = bits_mesh(ctx, "ObjectData/" + name + ".arc", low + ".kcl", low + ".pa", ff_order)
        out += vals
        counts.append(n)
    out += counts
    for b, i in placement_rows(ctx, "StageData/IceMountainZone.arc", "MapParts", FF_NAMES):
        out += [b.get(i, f) for f in ("MoveConditionType", "RotateSpeed", "RotateAngle", "RotateAccelType",
                                       "RotateStopTime")]
        out += [FF_NAMES.index(b.get(i, "name")), b.get(i, "RotateAxis")]
    return out


def partition(ids, cents):
    if len(ids) <= 12:
        return [ids]
    span = [max(cents[i][k] for i in ids) - min(cents[i][k] for i in ids) for k in range(3)]
    axis = span.index(max(span))
    ids = sorted(ids, key=lambda i: cents[i][axis])
    n = len(ids) // 2
    return partition(ids[:n], cents) + partition(ids[n:], cents)


def buoy_role(ctx, arc, kcl):
    pa = kcl[:-4] + ".pa"
    has_pa = any(p.lower().endswith("/" + pa) for p in ctx.arc_names(arc))
    rows, tris = source(ctx, arc, kcl, pa if has_pa else None)
    qs = []
    attrs = []
    codes = []
    for t, n, a in tris:
        qs.append(quant(wound(t, n)))
        fl = field(rows, a, "Floor_code")
        wl = field(rows, a, "Wall_code")
        attrs.append((16 if field(rows, a, "Camera_through") else 0) | (8 if fl in (20, 21, 22, 23) else 0)
                     | (512 if wl == 1 else 0))
        codes.append(fl | wl << 8)
    cents = [tuple((q[0][k] + q[1][k] + q[2][k]) / 3 for k in range(3)) for q in qs]
    groups = partition(list(range(len(qs))), cents)
    m = Mesh()
    sorted_codes = []
    rows_g = []
    for g in groups:
        lo, hi = bounds([p for i in g for p in qs[i]])
        rows_g.append((lo, hi, len(m.faces), len(g)))
        for i in g:
            m.add(qs[i]).append(attrs[i])
            sorted_codes.append(codes[i])
    out = flat(m.verts) + flat(m.faces) + sorted_codes
    for lo, hi, first, count in rows_g:
        out += lo + hi + [first, count]
    out.append(len(groups))
    out.append(fsum(max(max(abs(lo[k]), abs(hi[k])) for lo, hi, _, _ in rows_g) for k in range(3)) + 1)
    return out


def buoybase(ctx, spec):
    out = []
    for name in BUOY_PARTS:
        arc = "ObjectData/" + name + ".arc"
        for p in ctx.arc_names(arc):
            base = p.rsplit("/", 1)[-1]
            if base.endswith(".kcl") and base != "movelimit.kcl":
                out += buoy_role(ctx, arc, base)
    return out


def kcl_points(ctx, arc, name):
    return [p for t in KCL(ctx.arc_file(arc, name)).triangles for p in t]


def lathe(points, sides, levels, rmax=None):
    y0 = min(p[1] for p in points)
    y1 = max(p[1] for p in points)
    r = [math.sqrt(p[0] * p[0] + p[2] * p[2]) for p in points]
    if rmax is not None:
        keep = [i for i in range(len(points)) if r[i] <= rmax]
        points = [points[i] for i in keep]
        r = [r[i] for i in keep]
    h = y1 - y0
    rings, verts, tris = [], [], []
    for t in levels:
        y = y0 + h * t
        near = [r[i] for i in range(len(points)) if abs(points[i][1] - y) < h * 0.08 + 1]
        rad = max(near) if near else max(r)
        ring = []
        for s in range(sides):
            a = 2 * PI * (s + 0.5) / sides
            ring.append(len(verts))
            verts.append((rad * sin(a), y, rad * cos(a)))
        rings.append(ring)
    for ra, rb in zip(rings, rings[1:]):
        for s in range(sides):
            a0, a1, b0, b1 = ra[s], ra[(s + 1) % sides], rb[s], rb[(s + 1) % sides]
            tris += [(a0, b1, a1), (a0, b0, b1)]
    c = len(verts)
    verts.append((0.0, y1, 0.0))
    for s in range(sides):
        tris.append((c, rings[-1][(s + 1) % sides], rings[-1][s]))
    return [tuple(verts[i] for i in t) for t in tris]


def lift(tris, dy):
    return [tuple((p[0], p[1] + dy, p[2]) for p in t) for t in tris]


def facing_out(tris):
    ys = [p[1] for t in tris for p in t]
    ymid = fsum(ys) / len(ys)
    out = []
    for t in tris:
        n = cross(sub(t[1], t[0]), sub(t[2], t[0]))
        c = [(t[0][k] + t[1][k] + t[2][k]) / 3 for k in range(3)]
        if abs(n[1]) > 0.9 * math.sqrt(dot(n, n)):
            want = (0.0, 1.0 if c[1] >= ymid else -1.0, 0.0)
        else:
            want = (c[0], 0.0, c[2])
        out.append((t[0], t[2], t[1]) if dot(n, want) < 0 else t)
    return out


def rows9(tris):
    return [rint(v) for t in tris for p in t for v in p]


def deep_water_bazooka(ctx, spec):
    base = KCL(ctx.arc_file("ObjectData/WaterBazooka.arc", "allroot.kcl")).triangles
    cannon = kcl_points(ctx, "ObjectData/WaterBazooka.arc", "cannon1.kcl")
    capsule = kcl_points(ctx, "ObjectData/WaterBazookaCapsule.arc", "waterbazookacapsule.kcl")
    stone = kcl_points(ctx, "ObjectData/ItemAppearStoneSpinDriver.arc", "itemappearstonespindriver.kcl")
    hull_cannon = lift(lathe(cannon, 8, [0.0, 1.0], 330), 370)
    hull_capsule = lift(lathe(capsule, 8, [0.0, 0.6, 1.0]), 492)
    bazooka = list(base) + facing_out(hull_cannon) + facing_out(hull_capsule)
    hull_stone = facing_out(lathe(stone, 8, [0.0, 1.0]))
    out = rows9(bazooka) + [len(bazooka), len(base) + len(hull_cannon)]
    out += rows9(hull_stone) + [len(hull_stone)]
    return out + leak_specs(ctx, "StageData/GhostShipCaveZone.arc")


def round_dec(x, digits):
    n, d = x.as_integer_ratio()
    n *= 10 ** digits
    q, r = divmod(n, d)
    if 2 * r > d or (2 * r == d and q & 1):
        q += 1
    return q / 10 ** digits


def atan(x):
    if x < 0:
        return -atan(-x)
    if x > 1:
        return PI / 2 - atan(1 / x)
    k = 1
    for _ in range(2):
        x = x / (1 + math.sqrt(1 + x * x))
        k *= 2
    z = x * x
    s = 0.0
    for n in range(14, -1, -1):
        s = (1 if n % 2 == 0 else -1) / (2 * n + 1) + z * s
    return k * x * s


def atan2(y, x):
    if x > 0:
        return atan(y / x)
    if x < 0:
        return atan(y / x) + (PI if y >= 0 else -PI)
    return PI / 2 if y > 0 else -PI / 2 if y < 0 else 0.0


def asin(x):
    return atan2(x, math.sqrt((1 - x) * (1 + x)))


def fsum_mtx_mul(a, b):
    out = []
    for i in range(3):
        row = [fsum(a[i][k] * b[k][j] for k in range(3)) + (a[i][3] if j == 3 else 0.0) for j in range(4)]
        out.append(row)
    return out


def to_euler(m):
    r20 = max(-1.0, min(1.0, m[2][0]))
    ry = asin(-r20)
    if abs(r20) < 0.99999:
        rx = atan2(m[2][1], m[2][2])
        rz = atan2(m[1][0], m[0][0])
    else:
        rz = 0.0
        rx = atan2(-m[1][2], m[1][1])
    return [round_dec(a * (180.0 / PI), 4) for a in (rx, ry, rz)]


def rotation(deg):
    x, y, z = (a * (PI / 180.0) for a in deg)
    cx, sx, cy, sy, cz, sz = cos(x), sin(x), cos(y), sin(y), cos(z), sin(z)
    rz = [[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]]
    ry = [[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]
    rx = [[1, 0, 0], [0, cx, -sx], [0, sx, cx]]

    def mm(a, b):
        return [[fsum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
    return mm(mm(rz, ry), rx)


def row_vec(b, i, prefix, default):
    out = []
    for a in "xyz":
        v = b.get(i, prefix + a)
        out.append(default if v is None else v)
    return out


def zone_matrix(ctx, galaxy, zone):
    for b, i in placement_rows(ctx, galaxy, "Placement", (zone,)):
        m = mtx_tr(row_vec(b, i, "pos_", 0.0), row_vec(b, i, "dir_", 0.0))
        return [[round_dec(v, 3) for v in r] for r in m]
    raise KeyError(zone)


def apply3(m, p):
    return [m[i][0] * p[0] + m[i][1] * p[1] + m[i][2] * p[2] for i in range(3)]


def iceking_move_limit(ctx, spec):
    zm = zone_matrix(ctx, "StageData/IceVolcanoGalaxy.arc", "IceMountainZone")
    b, i = placement_rows(ctx, "StageData/IceMountainZone.arc", "Placement", ("IceMountainPlanet",))[0]
    wm = fsum_mtx_mul(zm, mtx_tr(row_vec(b, i, "pos_", 0.0), row_vec(b, i, "dir_", 0.0)))
    pos = [round_dec(wm[k][3], 3) for k in range(3)]
    scale = [round_dec(v, 3) for v in row_vec(b, i, "scale_", 1.0)]
    rot = rotation(to_euler(wm))
    arc = "ObjectData/IceMountainPlanet.arc"
    k = KCL(ctx.arc_file(arc, "movelimit.kcl"))
    pa = BCSV(ctx.arc_file(arc, "movelimit.pa"))
    out = [len(k.triangles)]
    for t, n, a in zip(k.triangles, k.normals, k.attributes):
        verts = []
        for p in t:
            w = apply3(rot, [p[0] * scale[0], p[1] * scale[1], p[2] * scale[2]])
            verts.append(tuple(rint(v) for v in gba([w[0] + pos[0], w[1] + pos[1], w[2] + pos[2]])))
        nn = tuple(rint(v * 16384) for v in gba(apply3(rot, n)))
        for v in verts:
            out += v
        out += nn
        out += [pa.get(a, "Floor_code"), pa.get(a, "Wall_code")]
        out.append(fsum(x * y for x, y in zip(nn, verts[0])) >> 14)
    return out


def kcl_ints(ctx, arc, name):
    k = KCL(ctx.arc_file(arc, name))
    return [rint(v) for t in k.triangles for p in t for v in p], len(k.triangles)


def part_tris(ctx, arc, name):
    k = KCL(ctx.arc_file(arc, name))
    out = []
    for t, n in zip(k.triangles, k.normals):
        q = quant(wound(t, n))
        if cross(sub(q[1], q[0]), sub(q[2], q[0])) != (0, 0, 0):
            out.append(q)
    return out


def part_block(ctx, arc, name, count=True):
    tris = part_tris(ctx, arc, name)
    vals = [v for t in tris for p in t for v in p]
    return vals + [len(tris)] if count else vals


def pole_square(ctx, spec):
    vals, n = kcl_ints(ctx, "ObjectData/PoleSquare.arc", "polesquare.kcl")
    return [n] + vals


def deepdark_kcl(ctx, spec):
    return part_block(ctx, "ObjectData/AirBubbleGenerator.arc", "airbubblegenerator.kcl")


def joint_turns(ctx, arc, model, joint, bcks):
    m = Model(ctx.arc_file(arc, model))
    j = m.joint(joint)
    out = [rint(v) for v in m.joints[j].trans]
    for name in bcks:
        b = BCK(ctx.arc_file(arc, name))
        for f in range(b.frame_count + 1):
            rz = b.sample(j, f)[1][2]
            out += [rint(cos(rz) * 16384), rint(sin(rz) * 16384)]
        out.append(b.frame_count + 1)
    return out


def mapparts(ctx, spec):
    out = []
    for arc, name, count in MAPPARTS:
        out += part_block(ctx, "ObjectData/" + arc + ".arc", name, count)
    out += joint_turns(ctx, "ObjectData/PhantomShipBridge.arc", "phantomshipbridge.bdl", "Move",
                       ["movea.bck", "moveb.bck"])
    return out


def earthen_pipes(ctx, spec):
    out = []
    for name in ("earthenpipe.kcl", "earthenpipes.kcl"):
        out += part_block(ctx, "ObjectData/EarthenPipe.arc", name)
    return out


def phantom_floater(ctx, spec):
    names = ["PhantomShipBoxFloater" + v for v in "ABC"]
    out = []
    for b, i in placement_rows(ctx, "StageData/GhostShipCaveZone.arc", "MapParts", names):
        out += [b.get(i, f) for f in ("MoveConditionType", "RotateSpeed", "RotateAngle", "RotateAxis")]
        out.append(names.index(b.get(i, "name")))
    counts = []
    for name in names:
        tris = part_tris(ctx, "ObjectData/" + name + ".arc", name.lower() + ".kcl")
        out += [x for t in tris for p in t for x in p]
        counts.append(len(tris))
    return out + counts


PREDICTORS = {
    "src/actors/pole_square_gen.h": pole_square,
    "src/actors/deepdark_kcl_gen.h": deepdark_kcl,
    "src/actors/mapparts_gen.h": mapparts,
    "src/actors/earthen_pipes_gen.h": earthen_pipes,
    "src/actors/phantom_floater_gen.h": phantom_floater,
    "src/actors/buoybase_collision_gen.h": buoybase,
    "src/actors/deep_water_bazooka_gen.h": deep_water_bazooka,
    "src/world/icestep_colmesh_gen.h": icestep,
    "src/actors/waterleak_gen.h": waterleak,
    "src/actors/freezeflame_mountain_gen.h": freezeflame_mountain,
    "src/actors/iceking_move_limit_gen.h": iceking_move_limit,
    "src/actors/crystal_cage_colmesh_gen.h": crystal_cage,
    "src/actors/crystal_cage_normals_gen.h": crystal_cage_normals,
    "src/actors/honey_crystal_colmesh_gen.h": honey_crystal,
}
