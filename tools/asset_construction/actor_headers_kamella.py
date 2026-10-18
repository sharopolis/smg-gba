import math
import struct
import zlib

from actor_headers_lib import BCK, BCSV, KCL, CameraAnim, Model, mtx_mul, mtx_tr, r3, rint, wound
from common import fsum, s16, u16, u32

GALAXY = "OceanPhantomCaveGalaxy"
BOSS_ARC = "ObjectData/BossKameck.arc"
DEMO_ARC = "ObjectData/BossKameckBattleDemo.arc"
SMALL_ARC = "ObjectData/Kameck.arc"
BARRIER_ARC = "ObjectData/BossKameckBarrier.arc"
FIGURE = "assets/figures/bosskameckbarrier/bosskameckbarrier_m%d.png"
FIGURE_COUNT = 3
STEP = 4
CLIPS = ["wait", "hide", "move", "appear", "attackwait", "attack", "damage", "down", "guard", "summonwait",
         "summon", "demobosskameckstart2", "demobosskameckpowerup2", "demobosskameckdown2"]
SMALL_CLIPS = ["wait", "appear", "attackwait", "attack", "hide", "move", "guard", "down", "press", "hit",
               "glad"]
LAYERS = ["common"] + ["layer" + chr(ord("a") + i) for i in range(16)]
JMP_DIRS = ["placement", "mapparts", "start", "generalpos", "childobj", "debug"]
IDENTITY = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0]]


class Entry:
    def __init__(self, fname, table, row):
        self.fname = fname
        self.table = table
        self.row = row

    def get(self, name, default=None):
        v = self.table.get(self.row, name)
        return default if v is None else v

    def pos(self):
        return [self.get("pos_x", 0.0), self.get("pos_y", 0.0), self.get("pos_z", 0.0)]

    def rot(self):
        return [self.get("dir_x", 0.0), self.get("dir_y", 0.0), self.get("dir_z", 0.0)]


class Zone:
    def __init__(self, ctx, name):
        self.entries = []
        self.paths = []
        rel = "StageData/" + name + ".arc"
        if not ctx.exists(rel):
            return
        tables = {}
        for path, blob in ctx.arc(rel).files:
            parts = path.split("/")
            tables.setdefault("/".join(parts[1:-1]).lower(), []).append((parts[-1], blob))
        for d in JMP_DIRS:
            for layer in LAYERS:
                for fname, blob in tables.get("jmp/" + d + "/" + layer, []):
                    t = BCSV(blob)
                    for i in range(t.n):
                        self.entries.append(Entry(fname, t, i))
        for fname, blob in tables.get("jmp/path", []):
            if fname.lower() == "commonpathinfo":
                t = BCSV(blob)
                self.paths = [t.get(i, "l_id") for i in range(t.n)]


class Galaxy:
    def __init__(self, ctx, name):
        scen = ctx.arc("StageData/" + name + "/" + name + "Scenario.arc")
        zl = BCSV(scen.find("ZoneList.bcsv"))
        self.zone_list = [zl.get(i, "ZoneName") for i in range(zl.n)]
        self.zones = {z: Zone(ctx, z) for z in self.zone_list}
        self.instances = [(name, -1, IDENTITY)]
        self.placements = 0
        self.place(name, IDENTITY, 0)
        self.rail_key = {}
        n = 0
        for zone, pidx, _ in self.instances:
            for l_id in self.zones[zone].paths:
                self.rail_key[(zone, pidx, l_id)] = n
                n += 1
        self.objects = []
        for zone, pidx, mtx in self.instances:
            for e in self.zones[zone].entries:
                self.objects.append((zone, pidx, mtx, e))

    def place(self, zone, parent, depth):
        if depth > 4:
            return
        for e in self.zones[zone].entries:
            if e.fname.lower() != "stageobjinfo":
                continue
            child = e.get("name")
            world = mtx_mul(parent, mtx_tr(e.pos(), e.rot()))
            pi = self.placements
            self.placements += 1
            if child in self.zones:
                self.instances.append((child, pi, [r3(row) for row in world]))
                self.place(child, world, depth + 1)

    def rail(self, zone, pidx, e):
        return self.rail_key[(zone, pidx, e.get("CommonPath_ID", -1))]


def joint_matrix(model, bck, joint, frame):
    return model.pose(bck, frame, model.chain([joint]))[joint]


def unit16384(m, c):
    x, y, z = m[0][c], m[1][c], m[2][c]
    n = math.sqrt(x * x + y * y + z * z)
    return [rint(x / n * 16384), rint(y / n * 16384), rint(z / n * 16384)]


def quant_pose(m):
    return [rint(m[0][3]), rint(m[1][3]), rint(m[2][3])] + unit16384(m, 1) + unit16384(m, 2)


def pack_wand(points):
    data = bytearray()
    offsets = []
    seen = {}
    for begin in range(0, len(points), 16):
        rows = points[begin:begin + 16]
        block = bytearray(struct.pack("<hhh", *rows[0]))
        prev = rows[0]
        for row in rows[1:]:
            delta = [b - a for a, b in zip(prev, row)]
            if all(-127 <= x <= 127 for x in delta):
                block += struct.pack("<bbb", *delta)
            else:
                block += b"\x80" + struct.pack("<hhh", *row)
            prev = row
        block = bytes(block)
        if block not in seen:
            seen[block] = len(data)
            data += block
        offsets.append(seen[block])
    return list(data) + offsets


def boss_and_children(ctx):
    g = Galaxy(ctx, GALAXY)
    out = []
    bosses = [o for o in g.objects if o[3].get("name") == "BossKameck2"]
    parent = bosses[0][3].get("l_id")
    for z, p, m, e in bosses:
        out += [2000 + g.zone_list.index(z) * 1000 + e.get("SW_A", -1), e.get("Obj_arg0", -1)]
    kids = [o for o in g.objects if o[3].get("ParentID", -1) == parent]
    rails = [o for o in kids if o[3].get("name") == "BossKameckMoveRail"]
    rails.sort(key=lambda o: o[3].get("Obj_arg0", -1))
    out.append(len(rails))
    out += [g.rail(z, p, e) for z, p, m, e in rails]
    children = [o for o in kids if o[3].get("name") == "ChildKameck"]
    out.append(len(children))
    for z, p, m, e in children:
        w = mtx_mul(m, mtx_tr(e.pos(), e.rot()))
        pos = r3([w[0][3], w[1][3], w[2][3]])
        q = [[-w[0][0], -w[0][1], -w[0][2], -pos[0]],
             [w[2][0], w[2][1], w[2][2], pos[2]],
             [w[1][0], w[1][1], w[1][2], pos[1]]]
        out += quant_pose(q) + [g.rail(z, p, e)]
    return out


def kcl_values(ctx, arc, name):
    k = KCL(ctx.arc_file(arc, name))
    out = []
    for t, n in zip(k.triangles, k.normals):
        out += [rint(v) for p in wound(t, n) for v in p]
    return out + [len(k.triangles)]


def kamella(ctx, spec):
    present = spec[0] | spec[1] << 8
    out = boss_and_children(ctx)
    model = Model(ctx.arc_file(BOSS_ARC, "bosskameck.bdl"))
    demo_model = Model(ctx.arc_file(DEMO_ARC, "bosskameckbattledemo.bdl"))
    wand = model.joint("PowerStarC")
    boss_joint = demo_model.joint("KameckPosition")
    mario_joint = demo_model.joint("MarioPosition")
    lengths = []
    for k, name in enumerate(CLIPS):
        a = BCK(ctx.arc_file(BOSS_ARC, name + ".bck"))
        lengths.append(a.frame_count)
        if present >> k & 1:
            out.append(a.frame_count)
        points = []
        for f in range(a.frame_count + 1):
            m = joint_matrix(model, a, wand, f)
            points.append([rint(m[0][3]), rint(m[1][3]), rint(m[2][3])])
        out += pack_wand(points)
        if not name.startswith("demoboss"):
            continue
        b = BCK(ctx.arc_file(DEMO_ARC, name + ".bck"))
        cam = CameraAnim(ctx.arc_file(DEMO_ARC, name + ".canm"))
        for f in range(0, ((a.frame_count + STEP - 1) // STEP) * STEP + 1, STEP):
            frame = min(f, a.frame_count)
            c = cam.sample(min(frame, cam.frames - 1))
            out += quant_pose(joint_matrix(demo_model, b, boss_joint, frame))
            out += quant_pose(joint_matrix(demo_model, b, mario_joint, frame))
            out += [rint(x) for x in c[:6]] + [rint(c[6] * 256), rint(c[7] * 256)]
    out += lengths
    small = Model(ctx.arc_file(SMALL_ARC, "kameck.bdl"))
    wand = small.joint("Wand")
    for name in SMALL_CLIPS:
        a = BCK(ctx.arc_file(SMALL_ARC, name + ".bck"))
        out.append(a.frame_count)
        points = []
        for f in range(a.frame_count + 1):
            m = joint_matrix(small, a, wand, f)
            points.append([rint(m[i][1] * 110 + m[i][3]) for i in range(3)])
        out += pack_wand(points)
    out += kcl_values(ctx, BARRIER_ARC, "bosskameckbarrier.kcl")
    out += kcl_values(ctx, "ObjectData/CandlestandIceVolcano.arc", "candlestandicevolcano.kcl")
    return out


def png_rgba(data):
    pos = 8
    idat = b""
    while pos < len(data):
        n, kind = struct.unpack_from(">I4s", data, pos)
        body = data[pos + 8:pos + 8 + n]
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or ctype != 6 or interlace != 0:
                raise ValueError("unsupported png")
        elif kind == b"IDAT":
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = w * 4
    prev = bytearray(stride)
    pixels = []
    p = 0
    for _ in range(h):
        f = raw[p]
        line = bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        for i in range(stride):
            a = line[i - 4] if i >= 4 else 0
            b = prev[i]
            c = prev[i - 4] if i >= 4 else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                q = a + b - c
                pa, pb, pc = abs(q - a), abs(q - b), abs(q - c)
                if pa <= pb and pa <= pc:
                    pr = a
                elif pb <= pc:
                    pr = b
                else:
                    pr = c
                line[i] = (line[i] + pr) & 255
        pixels += [tuple(line[i:i + 4]) for i in range(0, stride, 4)]
        prev = line
    return pixels


def sample_track(vals, count, index, tangent, frame):
    if count == 1:
        return vals[index]
    stride = 3 if tangent == 0 else 4
    keys = [vals[index + i * stride:index + i * stride + stride] for i in range(count)]
    if frame <= keys[0][0]:
        return keys[0][1]
    for i in range(count - 1):
        k0, k1 = keys[i], keys[i + 1]
        if k0[0] <= frame <= k1[0]:
            out_t = k0[3] if stride == 4 else k0[2]
            d = k1[0] - k0[0]
            u = (frame - k0[0]) / d if d else 0.0
            u2 = u * u
            u3 = u2 * u
            return ((2 * u3 - 3 * u2 + 1) * k0[1] + (u3 - 2 * u2 + u) * d * out_t
                    + (-2 * u3 + 3 * u2) * k1[1] + (u3 - u2) * d * k1[2])
    return keys[-1][1]


def brk_colour_table(b, tab, n, vals):
    if not n:
        return None
    comps = [(u16(b, tab + c * 6), u16(b, tab + c * 6 + 2), u16(b, tab + c * 6 + 4)) for c in range(4)]
    return comps, vals


def brk_colour(entry, frame):
    comps, vals = entry
    out = []
    for k in range(4):
        v = round(sample_track(vals[k], comps[k][0], comps[k][1], comps[k][2], frame))
        out.append(max(-1024, min(1023, v)))
    return out


class BRK:
    def __init__(self, b):
        o = 0x20
        self.duration = u16(b, o + 0x0A)
        nreg, nkon = u16(b, o + 0x0C), u16(b, o + 0x0E)
        off = [o + u32(b, o + 0x20 + i * 4) for i in range(14)]
        counts = [u16(b, o + 0x10 + i * 2) for i in range(8)]
        vals = [[s16(b, off[6 + i] + k * 2) for k in range(counts[i])] for i in range(8)]
        self.reg = brk_colour_table(b, off[0], nreg, vals[:4])
        self.konst = brk_colour_table(b, off[1], nkon, vals[4:])


def material_alpha(model):
    b = model.data
    o = model.sections["MAT3"]
    init = o + u32(b, o + 0x0C)
    remap = o + u32(b, o + 0x10)
    matcol = o + u32(b, o + 0x0C + 5 * 4)
    p = init + u16(b, remap) * 0x14C
    mc = u16(b, p + 8)
    return b[matcol + mc * 4 + 3] / 255


def gains(ctx, rgba, classes):
    groups = {i: [] for i in classes}
    for k in range(FIGURE_COUNT):
        px = png_rgba(ctx.built_file(FIGURE % k))
        for c in sorted(set(p[:3] for p in px if p[3] >= 128)):
            best, nearest = None, 0
            for i in range(1, 256):
                d = fsum((rgba[i][j] - c[j]) ** 2 for j in range(3))
                if best is None or d < best:
                    best, nearest = d, i
            if nearest in groups:
                groups[nearest].append((c[0] / 36 + c[2] / 44) / 2)
    return [(i, fsum(groups[i]) / len(groups[i])) for i in classes]


def additive_row(pal, reg, g, alpha):
    added = [min(255.0, reg[k] * g) * alpha * 31 / 255 for k in range(3)]
    if max(added) == 0:
        return None
    row = bytearray(256)
    for i in range(256):
        rgb = [min(31.0, max(0.0, pal[i][k] + added[k])) for k in range(3)]
        best, out = None, 0
        for j in range(1, 256):
            q = pal[j]
            d0 = rgb[0] - q[0]
            d1 = rgb[1] - q[1]
            d2 = rgb[2] - q[2]
            d = d0 * d0 + d1 * d1 + d2 * d2
            if best is None or d < best:
                best, out = d, j
        row[i] = out
    return bytes(row)


def barrier_material(ctx, spec):
    classes = list(spec)
    brk = BRK(ctx.arc_file(BARRIER_ARC, "wait.brk"))
    alpha_mat = material_alpha(Model(ctx.arc_file(BARRIER_ARC, "bosskameckbarrier.bdl")))
    raw = ctx.built_file("assets/palette.rgba")
    rgba = [tuple(raw[i * 4:i * 4 + 4]) for i in range(256)]
    pal = [tuple(min(31, c // 8 + 1) for c in col[:3]) for col in rgba]
    gain = gains(ctx, rgba, classes)
    identity = bytes(range(256))
    rows = [identity]
    row_ids = {identity: 0}
    maps = []
    map_ids = {}
    frames = []
    cache = {}
    for frame in range(brk.duration + 1):
        reg = brk_colour(brk.reg, frame)
        konst = brk_colour(brk.konst, frame)
        alpha = min(1.0, max(0.0, 2 * konst[3] / 255 * alpha_mat))
        mapping = [0] * 256
        for index, g in gain:
            key = (tuple(reg[:3]), g, alpha)
            row = cache.get(key)
            if row is None:
                row = cache[key] = additive_row(pal, reg, g, alpha) or identity
            if row not in row_ids:
                row_ids[row] = len(rows)
                rows.append(row)
            mapping[index] = row_ids[row]
        packed = tuple(mapping)
        if packed not in map_ids:
            map_ids[packed] = len(maps)
            maps.append(packed)
        frames.append(map_ids[packed])
    out = [brk.duration] + frames
    for m in maps:
        out += list(m)
    for r in rows:
        out += list(r)
    return out


PREDICTORS = {
    "src/actors/kamella_gen.h": kamella,
    "src/actors/kamella_barrier_material_gen.h": barrier_material,
}
