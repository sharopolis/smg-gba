import math
import struct

from common import cross, dot, jhash
from ui_data import Dol, arc_get, bcsv
from ui_headers_place import galaxy, mtx_tr
from ui_math import PI, cos, ipow, sin

ANIME_TABLES = ((0x805A66F8, 4), (0x805A695C, 3), (0x805A6A88, 2), (0x805A8624, 1))
EXTRA_CLIPS = ["watch", "racestart", "racestartghost", "racestartcrouch", "appearghost", "winghost",
               "dieevent", "wait"]
OBJECTS = [(56, "ghost_start"), (65, "mario_start"), (66, "mario_demo"), (67, "ghost_demo"),
           (621, "mario_lose")]
CAMERA_PREFIX = "e:\u30b4\u30fc\u30b9\u30c8\u30de\u30ea\u30aa\u56fa\u6709"
CAMERAS = ["\u30ec\u30fc\u30b9\u958b\u59cb1000", "\u30ec\u30fc\u30b9\u958b\u59cb2000",
           "\u30ec\u30fc\u30b9\u958b\u59cb3000", "\u30ec\u30fc\u30b9\u7d42\u4e86000"]


def anime_groups(ctx):
    dol = Dol(ctx.read_sys("main.dol"))
    groups = {}
    for base, n in ANIME_TABLES:
        stride = 4 + 8 * n if n > 1 else 16
        a = base
        while True:
            w = dol.u32s(a, stride // 4)
            name = dol.cstr(w[0])
            if not name:
                break
            files = [dol.cstr(w[1 + 2 * i]) for i in range(n)] if n > 1 else [dol.cstr(w[1])]
            groups[name] = [f for f in files if f]
            a += stride
    return groups


class Cursor:
    def __init__(self, data, pos):
        self.data, self.pos = data, pos

    def read(self, fmt):
        v = struct.unpack_from(">" + fmt, self.data, self.pos)
        self.pos += struct.calcsize(">" + fmt)
        return v


def decode(data, groups):
    hashes = {jhash(n): n for n in groups}
    position = [0.0, 0.0, 0.0]
    velocity = [0.0, 0.0, 0.0]
    rotation = [0.0, 0.0, 0.0]
    weights = [0, 0, 0, 127]
    name, frame, rate = "\u57fa\u672c", 0, 1
    records, offset = [], 0
    while offset + 4 <= len(data):
        _, size, flags = struct.unpack_from(">BBH", data, offset)
        if not size:
            break
        cur = Cursor(data, offset + 4)
        if flags & 1:
            position = [float(v) * 4 for v in cur.read("hhh")]
        if flags & 0x800:
            velocity = [float(v) for v in cur.read("bbb")]
        if flags & 0x400:
            cur.read("bbb")
        for axis, flag in enumerate((2, 4, 8)):
            if flags & flag:
                rotation[axis] = cur.read("b")[0] * 180 / 128
        if flags & 0x10:
            end = data.index(0, cur.pos)
            name = data[cur.pos:end].decode("shift_jis")
            cur.pos = end + 1
        if flags & 0x2000:
            name = hashes[cur.read("I")[0]]
        if flags & 0x20:
            frame = cur.read("h")[0] / 32
        for track in range(4):
            if flags & (0x40 << track):
                w = cur.read("b")[0]
                weights[track] = 127 if w == -128 else w
        if flags & 0x1000:
            rate = cur.read("b")[0] / 8
        position = [position[k] + velocity[k] for k in range(3)]
        files = groups.get(name, ["PowerStarGet" if name == "powerstarget" else name])
        sel = min(max(range(len(files)), key=lambda k: weights[k]), len(files) - 1)
        records.append(dict(pos=list(position), rot=list(rotation), animation=name, clip=files[sel].lower(),
                            frame=max(frame, 0), wait_weight=weights[3] / 128))
        frame += rate
        offset += size
    return records


def gp(v):
    return [-v[0], v[2], v[1]]


def vals(v):
    return ", ".join(str(round(x)) for x in v)


def col(m, c):
    return [m[0][c], m[1][c], m[2][c]]


def scale(v, s):
    return [x * s for x in v]


def norm(a):
    return math.sqrt(dot(a, a))


def axes(m):
    return vals(gp(scale(col(m, 1), 16384))), vals(gp(scale(col(m, 2), 16384)))


def replay_lines(ctx):
    arc = ctx.arc("ObjectData/GhostDataHoneyBeeKingdomGalaxy.arc")
    records = decode(arc_get(arc, "HoneyBeeKingdomGalaxy.gst"), anime_groups(ctx))
    first = next(i for i, r in enumerate(records) if r["wait_weight"] <= .9) + 11
    finish = next(i for i, r in enumerate(records) if r["animation"] == "powerstarget")
    used = sorted({r["clip"] for r in records[first:finish]})
    used += [x for x in EXTRA_CLIPS if x not in used]
    clip_ids = {n: i for i, n in enumerate(used)}
    h = []
    for n, i in clip_ids.items():
        if n in EXTRA_CLIPS[:7]:
            h.append("#define COSMIC_%s %d" % (n.upper(), i))
    h.append("#define COSMIC_REPLAY_FRAMES %d" % (finish - first))
    h.append("static const CosmicFrame cosmic_replay[] = {")
    for r in records[first:finish]:
        up, front = axes(mtx_tr((0, 0, 0), r["rot"]))
        pos = vals([x / 4 for x in gp(r["pos"])])
        h.append("    { { %s }, { %s }, { %s }, %d, 0, %d }," % (pos, up, front, clip_ids[r["clip"]],
                                                               round(r["frame"] * 8)))
    h.append("};")
    return h


def chase_offset(row, gm, tick):
    end, start = row["num1"] >> 16, row["num1"] & 65535
    t = min(max((tick - start) / max(end - start, 1), 0), 1)
    if row["num2"] == 1:
        t = 6 * ipow(t, 5) - 15 * ipow(t, 4) + 10 * ipow(t, 3)
    angle = row["wpoint.X"] * (1 - t) + row["axis.X"] * t
    height = row["wpoint.Y"] * (1 - t) + row["axis.Y"] * t
    distance = row["wpoint.Z"] * (1 - t) + row["axis.Z"] * t
    sa, ca = sin(angle), cos(angle)
    side = [col(gm, 0)[k] * sa + col(gm, 2)[k] * ca for k in range(3)]
    offset = [side[k] * distance + col(gm, 1)[k] * height for k in range(3)]
    nrm = norm(offset)
    direction = [x / nrm for x in offset]
    axis = cross(side, direction)
    sine = norm(axis)
    cosine = dot(side, direction)
    axis = [x / sine for x in axis] if sine > 1e-9 else [1.0, 0.0, 0.0]
    bu = col(gm, 1)
    cr = cross(axis, bu)
    ad = dot(axis, bu)
    return offset, [bu[k] * cosine + cr[k] * sine + axis[k] * ad * (1 - cosine) for k in range(3)]


def orbit_offset(row):
    pitch, yaw = row["angleB"], row["angleA"] - PI / 2
    cp, sp, cy, sy = cos(pitch), sin(pitch), cos(yaw), sin(yaw)
    return [row["dist"] * (cp * sy), row["dist"] * sp, row["dist"] * (cp * cy)], [0.0, 1.0, 0.0]


def camera_lines(ctx, objs):
    rows = {}
    for r in bcsv(arc_get(ctx.arc("StageData/HoneyBeeKingdomGalaxy.arc"), "stage/camera/cameraparam.bcam")):
        rows[r.get("id")] = r
    gs, gd = objs[56], objs[67]
    gm = mtx_tr(gd.world_pos, gd.local_rot)
    h = ["typedef struct { int32_t eye[3], watch[3]; int16_t up[3]; } CosmicCamera;",
         "static const CosmicCamera cosmic_camera_samples[4][240] = {"]
    for cno, suffix in enumerate(CAMERAS):
        row = rows[CAMERA_PREFIX + suffix]
        target = gs.world_pos if cno in (0, 2) else gd.world_pos
        woff = [row.get("woffset." + a, 0) for a in "XYZ"]
        watch = [target[k] + woff[k] for k in range(3)]
        h.append("    {")
        for tick in range(240):
            offset, up = chase_offset(row, gm, tick) if cno == 1 else orbit_offset(row)
            eye = [watch[k] + offset[k] for k in range(3)]
            h.append("        { { %s }, { %s }, { %s } }," % (vals(gp(eye)), vals(gp(watch)),
                                                            vals(scale(gp(up), 16384))))
        h.append("    },")
    h.append("};")
    return h


def cosmic(ctx):
    h = ["", "typedef struct { int16_t pos[3], up[3], front[3]; uint8_t clip, pad; uint16_t frame; } "
         "CosmicFrame;"]
    h += replay_lines(ctx)
    objs = galaxy(ctx, "HoneyBeeKingdomGalaxy")["objects"]
    for oid, name in OBJECTS:
        o = objs[oid]
        up, front = axes(mtx_tr(o.local_pos, o.local_rot))
        h.append("static const int32_t cosmic_%s_pos[3] = { %s };" % (name, vals(gp(o.world_pos))))
        h.append("static const int16_t cosmic_%s_up[3] = { %s };" % (name, up))
        h.append("static const int16_t cosmic_%s_front[3] = { %s };" % (name, front))
    return h + camera_lines(ctx, objs)
