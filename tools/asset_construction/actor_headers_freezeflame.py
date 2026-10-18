import json
import math
import struct

from actor_headers_lib import BCK, BCSV, KCL, Model, gba, mtx_apply, mtx_col, mtx_tr, r3, rint
from common import fsum, u16, u32

POS = ("pos_x", "pos_y", "pos_z")
ROT = ("dir_x", "dir_y", "dir_z")
ROT_ONE = 16384
TRANS_ONE = 256
GROUP = 4
PRIMS = (0x80, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8)
SPEC_FIELDS = ("MoveConditionType", "RotateSpeed", "RotateAngle", "RotateAxis", "RotateStopTime",
               "DemoGroupId", "CastId")


def positions(b, o):
    size = u32(b, o + 4)
    offs = [u32(b, o + 0x0C + i * 4) for i in range(13)]
    p = o + u32(b, o + 8)
    cc = ct = frac = None
    while True:
        attr, c, t, f = struct.unpack_from(">IIIB", b, p)
        p += 16
        if attr == 0xFF:
            break
        if attr == 9:
            cc, ct, frac = c, t, f
    start = offs[0]
    nxt = sorted(x for x in offs if x > start)
    end = nxt[0] if nxt else size
    n = 3 if cc == 1 else 2
    code, isz = {0: ("B", 1), 1: ("b", 1), 2: ("H", 2), 3: ("h", 2), 4: ("f", 4)}[ct]
    cnt = (end - start) // (isz * n)
    raw = struct.unpack_from(">%d%s" % (cnt * n, code), b, o + start)
    div = 1.0 if ct == 4 else float(1 << frac)
    out = []
    for i in range(cnt):
        v = [raw[i * n + k] / div for k in range(n)]
        out.append(tuple(v + [0.0] * (3 - n)))
    return out


def draw_joints(b, o):
    n = u16(b, o + 8)
    wo = o + u32(b, o + 0x0C)
    do = o + u32(b, o + 0x10)
    return [(b[wo + i], u16(b, do + i * 2)) for i in range(n)]


def display_list(b, p, size, desc, slots, default, verts):
    end = p + size
    while p < end:
        op = b[p]
        p += 1
        if op == 0:
            continue
        if op & 0xF8 not in PRIMS:
            break
        cnt = u16(b, p)
        p += 2
        for _ in range(cnt):
            d = default
            pos = None
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
                    d = slots[s] if s < 10 and slots[s] != 0xFFFF else default
                elif attr == 9:
                    pos = val
            verts.append((d, pos))


def shapes(b, o):
    n = u16(b, o + 8)
    init = o + u32(b, o + 0x0C)
    remap = o + u32(b, o + 0x10)
    desc_base = o + u32(b, o + 0x18)
    mtx_table = o + u32(b, o + 0x1C)
    dl_base = o + u32(b, o + 0x20)
    mtx_init = o + u32(b, o + 0x24)
    draw_init = o + u32(b, o + 0x28)
    out = []
    for si in range(n):
        p = init + u16(b, remap + si * 2) * 0x28
        npk = u16(b, p + 2)
        q = desc_base + u16(b, p + 4)
        first_mtx = u16(b, p + 6)
        first_draw = u16(b, p + 8)
        desc = []
        while True:
            attr, typ = struct.unpack_from(">II", b, q)
            q += 8
            if attr == 0xFF:
                break
            desc.append((attr, typ))
        verts = []
        slots = [0xFFFF] * 10
        for pk in range(npk):
            use_mtx, mcount, mfirst = struct.unpack_from(">HHI", b, mtx_init + (first_mtx + pk) * 8)
            for k in range(mcount):
                v = u16(b, mtx_table + (mfirst + k) * 2)
                if v != 0xFFFF:
                    slots[k] = v
            default = slots[0] if slots[0] != 0xFFFF else use_mtx
            size, off = struct.unpack_from(">II", b, draw_init + (first_draw + pk) * 8)
            display_list(b, dl_base + off, size, desc, slots, default, verts)
        out.append(verts)
    return out


class Geometry:
    def __init__(self, model):
        b = model.data
        pos = positions(b, model.sections["VTX1"])
        table = draw_joints(b, model.sections["DRW1"])
        self.shape_joint = []
        used = {}
        for verts in shapes(b, model.sections["SHP1"]):
            self.shape_joint.append(table[verts[0][0]][1])
            for d, i in verts:
                used.setdefault(table[d][1], set()).add(i)
        self.points = {j: [pos[i] for i in sorted(s)] for j, s in sorted(used.items())}


def table(ctx, zone, path):
    return ctx.memo(("freezeflame", zone, path), lambda: BCSV(ctx.arc("StageData/%s.arc" % zone).find(path)))


def field(ctx, ref, name):
    zone, path, i = ref
    v = table(ctx, zone, path).get(i, name)
    return -1 if v is None else v


def placed(ctx, ref):
    zone, path, i = ref
    b = table(ctx, zone, path)
    return [r3(r) for r in mtx_tr([b.get(i, n) for n in POS], [b.get(i, n) for n in ROT])]


def rail_points(ctx, zone, place, index):
    m = placed(ctx, place)
    b = table(ctx, zone, "stage/jmp/path/commonpathpointinfo.%d" % index)
    out = []
    for i in range(b.n):
        p = [b.get(i, "pnt0_" + a) for a in "xyz"]
        out.append((r3(mtx_apply(m, p)), [b.get(i, "point_arg%d" % k, -1) for k in range(8)]))
    return out


def rail_index(ctx, zone, l_id):
    b = table(ctx, zone, "stage/jmp/path/commonpathinfo")
    return [i for i in range(b.n) if b.get(i, "l_id") == l_id][0]


def object_rail(ctx, e):
    zone = e["obj"][0]
    return rail_points(ctx, zone, e["place"], rail_index(ctx, zone, field(ctx, e["obj"], "CommonPath_ID")))


def dist(a, b):
    return math.sqrt(fsum((a[k] - b[k]) * (a[k] - b[k]) for k in range(3)))


def files(ctx, arc, suffix):
    return [(p, b) for p, b in ctx.arc("ObjectData/%s.arc" % arc).files if p.endswith(suffix)]


def one(ctx, arc, suffix):
    return files(ctx, arc, suffix)[0][1]


def quantize(m):
    return tuple(rint(m[i][c] * (ROT_ONE if c < 3 else TRANS_ONE)) for i in range(3) for c in range(4))


def mesh(ctx, arc):
    k = KCL(one(ctx, arc, ".kcl"))
    pa = BCSV(one(ctx, arc, ".pa"))
    verts = []
    ids = {}
    faces = []
    codes = []
    for tri, attr in zip(k.triangles, k.attributes):
        f = []
        for v in tri:
            q = tuple(rint(x) for x in v)
            if q not in ids:
                ids[q] = len(verts)
                verts.append(q)
            f.append(ids[q])
        floor, wall = pa.get(attr, "Floor_code"), pa.get(attr, "Wall_code")
        bits = 16 if pa.get(attr, "Camera_through") else 0
        bits |= (64 if wall == 8 else 0) | (256 if floor == 10 else 0)
        faces.append(f + [bits])
        codes.append(floor | wall << 8)
    groups = []
    for first in range(0, len(faces), GROUP):
        chunk = faces[first:first + GROUP]
        pts = [verts[i] for f in chunk for i in f[:3]]
        groups += [min(p[a] for p in pts) for a in range(3)] + [max(p[a] for p in pts) for a in range(3)]
        groups += [first, len(chunk)]
    out = [x for v in verts for x in v] + [x for f in faces for x in f] + codes + groups
    return out + [len(groups) // 8]


def bind_corner(model, geo):
    world = model.bind_world()
    pts = [mtx_apply(world[j], p) for j, ps in geo.points.items() for p in ps]
    pts += [(m[0][3], m[1][3], m[2][3]) for m in world]
    s = 0.0
    for a in range(3):
        e = max(max(p[a] for p in pts), -min(p[a] for p in pts))
        s += e * e
    return math.sqrt(s)


def anim_reach(model, geo, bcks, joint=None):
    best = 0.0
    for b in bcks:
        for f in range(b.frame_count + 1):
            world = model.pose(b, f)
            c = (0.0, 0.0, 0.0) if joint is None else mtx_col(world[joint], 3)
            for j, ps in geo.points.items():
                for p in ps:
                    v = mtx_apply(world[j], p)
                    d = [v[k] - c[k] for k in range(3)]
                    best = max(best, d[0] * d[0] + d[1] * d[1] + d[2] * d[2])
    return math.sqrt(best)


def chunk_bounds(ctx, name):
    text = ctx.built_file("assets/figures/%s/%s.obj" % (name, name)).decode()
    pts = []
    for line in text.splitlines():
        if line.startswith("v "):
            x, y, z = (float(t) for t in line.split()[1:4])
            pts.append((-1000 * x, 1000 * z, 1000 * y))
    lo = [math.floor(min(p[a] for p in pts)) - 1 for a in range(3)]
    return lo + [math.ceil(max(p[a] for p in pts)) + 1 for a in range(3)]


def resources(ctx, spec):
    spec = json.loads(spec)
    dictionary = []
    index = {}
    sections = []
    out_table = []
    for m in spec["models"]:
        arc = m["arc"]
        model = Model(one(ctx, arc, ".bdl"))
        geo = Geometry(model)
        part_vals = []
        for i, shape in enumerate(m["parts"]):
            part_vals.append(geo.shape_joint[shape])
            if "chunks" in m:
                part_vals += chunk_bounds(ctx, "%s_c%02d" % (m["chunks"], i))
        clip_vals = []
        bcks = []
        for clip in m["clips"]:
            b = BCK([d for p, d in files(ctx, arc, ".bck") if p.endswith("/" + clip + ".bck")][0])
            bcks.append(b)
            rows = []
            for f in range(b.frame_count + 1):
                world = model.pose(b, f)
                for j in range(len(model.joints)):
                    key = quantize(world[j])
                    if key not in index:
                        index[key] = len(dictionary)
                        dictionary.append(key)
                    rows.append(index[key])
            clip_vals.append(rows + [b.frame_count, b.joint_count])
        sections.append(part_vals)
        sections += clip_vals
        if m["mesh"]:
            sections.append(mesh(ctx, arc))
        rest = [index.get(quantize(w), 0) for w in model.bind_world()]
        radius = bind_corner(model, geo)
        if spec["reach"] and bcks:
            radius = max(radius, anim_reach(model, geo, bcks))
        out_table += (rest + [0, 0, 0])[:3] + [len(model.joints), math.ceil(radius)]
    out = [x for key in dictionary for x in key]
    for s in sections:
        out += s
    return out + out_table


def valve_bounds(ctx, spec):
    out = []
    for e in json.loads(spec)["bounds"]:
        model = Model(one(ctx, e["arc"], ".bdl"))
        geo = Geometry(model)
        if e["rail"]:
            pts = [gba(p) for p, _ in object_rail(ctx, e)]
            r = math.ceil(bind_corner(model, geo))
            half = [abs(pts[1][k] - pts[0][k]) / 2 for k in range(3)]
            radius = math.sqrt(fsum((r + h) * (r + h) for h in half))
            centre = [rint((pts[0][k] + pts[1][k]) / 2) for k in range(3)]
        else:
            pos = gba(r3(mtx_apply(placed(ctx, e["place"]), [field(ctx, e["obj"], n) for n in POS])))
            bcks = [BCK(d) for _, d in files(ctx, e["arc"], ".bck")]
            radius = anim_reach(model, geo, bcks, e["joint"])
            centre = [int(v) for v in pos]
        out += [math.ceil(radius)] + centre
    return out


def float_bits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def valve_rails(ctx, spec):
    out = []
    for r in json.loads(spec)["rails"]:
        pts = object_rail(ctx, r)
        out.append(float_bits(dist(pts[0][0], pts[1][0])))
        for p, _ in pts:
            out += [float_bits(v) for v in (-p[0], p[2], p[1])]
        for _, args in pts:
            out += args
    return out


def decimal(x):
    s = format(x, ".9g")
    if "." not in s and "e" not in s:
        s += ".0"
    whole, frac = s.split(".")
    return [int(whole), int(frac.lstrip("0") or "0")]


def sheet(ctx, name, kind):
    key = name.lower()
    return BCSV(ctx.arc_file("ObjectData/DemoSheet.arc", "demosheet/demo%s/demo%s%s.bcsv" % (key, key, kind)))


def demo(ctx, d):
    ref = d["row"]
    name = field(ctx, ref, "TimeSheetName")
    time = sheet(ctx, name, "time")
    action = sheet(ctx, name, "action")
    start = {}
    phases = []
    at = 0
    for i in range(time.n):
        start[time.get(i, "PartName")] = at
        phases.append(at)
        at += time.get(i, "TotalStep")
    actions = []
    for i in range(action.n):
        actions += [start[action.get(i, "PartName")], action.get(i, "ActionType")]
    head = [field(ctx, ref, "SW_APPEAR") + d["base"], at, len(phases), action.n]
    return phases + actions, head


def demo_rail(ctx, r):
    pts = rail_points(ctx, r["zone"], r["place"], r["row"])
    coords = [0.0]
    for (a, _), (b, _) in zip(pts, pts[1:]):
        coords.append(coords[-1] + dist(a, b))
    out = []
    for c in coords:
        out += decimal(c)
    for p, _ in pts:
        for v in (-p[0], p[2], p[1]):
            out += decimal(v)
    for _, args in pts:
        out += decimal(float(args[0]))
    return out + [args[1] for _, args in pts]


def volcano_specs(ctx, spec):
    spec = json.loads(spec)
    out = []
    for ref in spec["specs"]:
        out += [field(ctx, ref, n) for n in SPEC_FIELDS]
    heads = []
    for d in spec["demos"]:
        body, head = demo(ctx, d)
        out += body
        heads += head
    out += heads
    for r in spec["rails"]:
        out += demo_rail(ctx, r)
    return out


PREDICTORS = {
    "src/actors/freezeflame_volcano_gen.h": resources,
    "src/actors/freezeflame_valve_gen.h": resources,
    "src/actors/freezeflame_volcano_specs_gen.h": volcano_specs,
    "src/actors/freezeflame_valve_rails_gen.h": valve_rails,
    "src/actors/freezeflame_valve_bounds_gen.h": valve_bounds,
}
