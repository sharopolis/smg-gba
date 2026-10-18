import math
import struct

from actor_headers_lib import BCK, Model, mtx_col, rint, spec_lines
from common import u16, u32, yaz0_decompress

QUEEN_JOINTS = ["Center", "Center", "Face", "LArm01", "LArm02", "LFoot001", "RArm01", "RArm02", "RFoot001",
                "Tactile"]


class Rig:
    def __init__(self, ctx, arc, bdl, names):
        self.ctx = ctx
        self.arc = arc
        self.model = ctx.memo(("model", arc, bdl), lambda: Model(ctx.arc_file(arc, bdl)))
        self.joints = [self.model.joint(n) for n in names]
        self.order = self.model.chain(self.joints)

    def bck(self, clip):
        name = clip + ".bck"
        return self.ctx.memo(("bck", self.arc, name), lambda: BCK(self.ctx.arc_file(self.arc, name)))

    def worlds(self, bck, frame):
        w = self.model.pose(bck, frame, self.order)
        return [w[j] for j in self.joints]


def translation(w, scale=1.0):
    return [w[i][3] * scale for i in range(3)]


def length(v):
    return math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])


def visibility_tracks(data):
    data = yaz0_decompress(data)
    off = 32
    for _ in range(u32(data, 12)):
        tag, size = struct.unpack_from(">4sI", data, off)
        if tag == b"VAF1":
            count = u16(data, off + 12)
            entries, values = struct.unpack_from(">II", data, off + 16)
            tracks = []
            for i in range(count):
                num, start = struct.unpack_from(">HH", data, off + entries + 4 * i)
                p = off + values + start
                tracks.append(list(data[p:p + num]))
            return tracks
        off += size
    raise ValueError("no VAF1 section")


def stepped(count, ticks, drop_end):
    frames = list(range(0, count + 1, ticks))
    if drop_end and frames[-1] == count:
        frames.pop()
    return frames


def packun_head(ctx, spec):
    rig = Rig(ctx, "ObjectData/PackunFlower.arc", "packunflower.bdl", ["Head"])
    out = []
    for clip, ticks in spec_lines(spec):
        bck = rig.bck(clip)
        frames = list(range(0, bck.frame_count, int(ticks)))
        out.append(len(frames))
        for f in frames:
            w = rig.worlds(bck, f)[0]
            out += [rint(w[i][0] * 120 + w[i][3]) for i in range(3)]
    return out


def honey_queen(ctx, spec):
    rig = Rig(ctx, "ObjectData/HoneyQueen.arc", "honeyqueen.bdl", QUEEN_JOINTS)
    out = []
    for clip, ticks in spec_lines(spec):
        bck = rig.bck(clip)
        frames = stepped(bck.frame_count, int(ticks), True)
        out.append(len(frames))
        for f in frames:
            for w in rig.worlds(bck, f):
                out += [rint(v) for v in translation(w)]
                out += [rint(v * 16384) for v in mtx_col(w, 1)]
                out += [rint(v * 16384) for v in mtx_col(w, 2)]
    return out


def boss_pose(w):
    cols = [mtx_col(w, c) for c in range(3)]
    scale = [length(c) for c in cols]
    mean = (scale[0] + scale[1] + scale[2]) / 3
    return ([rint(v) for v in translation(w)] + [rint(v / scale[1] * 16384) for v in cols[1]]
            + [rint(v / scale[2] * 16384) for v in cols[2]] + [rint(mean * 256)] * 3)


def honey_boss(ctx, spec):
    rows = spec_lines(spec)
    demo = Rig(ctx, "ObjectData/BossStinkBugDemo.arc", "bossstinkbugdemo.bdl", ["MarioPosition", "Boss"])
    out = []
    for clip in rows[0]:
        bck = demo.bck(clip)
        n = bck.frame_count
        out.append(n + 1)
        for f in range(n + 1):
            for w in demo.worlds(bck, min(f, n - 0.001)):
                out += boss_pose(w)
        out.append(n)
    body = Rig(ctx, "ObjectData/BossStinkBug.arc", "bossstinkbug.bdl", ["Body"])
    counts = []
    for clip, ticks, keys in rows[1:]:
        bck = body.bck(clip)
        n = bck.frame_count
        count = max(int(ticks) * int(keys), n) + 1
        counts.append(count)
        out.append(count)
        for f in range(count):
            out += boss_pose(body.worlds(bck, min(f, n))[0])
    return out + counts


def mandibug_child(ctx, spec):
    rig = Rig(ctx, "ObjectData/StinkBugParent.arc", "stinkbugparent.bdl", ["ChildCenter"])
    out = []
    tail = []
    for clip in spec_lines(spec)[0]:
        bck = rig.bck(clip)
        n = bck.frame_count
        out.append(n + 1)
        for f in range(n + 1):
            x, y, z = [rint(v) for v in translation(rig.worlds(bck, f)[0])]
            out += [x, z, y]
        tail += [n, 1 if bck.loop_mode == 2 else 0]
    return out + tail


def mandibug_visibility(ctx, spec):
    tracks = visibility_tracks(ctx.arc_file("ObjectData/StinkBugParent.arc", "childdeath.bva"))
    n = max(len(t) for t in tracks)
    out = [n]
    for f in range(n):
        out.append(1 if all(t[min(f, len(t) - 1)] for t in tracks if t) else 0)
    return out


def obj_radius(data):
    best = 0.0
    for line in data.decode("ascii").splitlines():
        t = line.split()
        if t and t[0] == "v":
            p = [round(float(x) * 1000) / 4 for x in t[1:4]]
            best = max(best, math.sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]))
    return best


def crystal_bounds(ctx, spec):
    rows = spec_lines(spec)
    folder = rows[0][0]
    radius = max(obj_radius(ctx.built_file(folder + "/" + name)) for name in rows[1])
    return [math.ceil(radius)]


def iceking(ctx, spec):
    rig = Rig(ctx, "ObjectData/IceMerameraKing.arc", "icemerameraking.bdl", ["JointRoot", "Top"])
    center = rig.model.joint("Center")
    out = []
    counts = []
    for clip in spec_lines(spec)[0]:
        bck = rig.bck(clip)
        count = bck.frame_count + 1
        counts.append(count)
        out.append(count)
        for f in range(count):
            for w in rig.worlds(bck, f):
                out += [rint(v) for v in translation(w, 1.5)]
            out.append(rint(bck.sample(center, f)[0][0] * 130))
    shock = Rig(ctx, "ObjectData/IceMerameraKingShock.arc", "icemeramerakingshock.bdl",
                ["Hirgh1", "WideInSide"])
    bck = shock.bck("shock")
    out += counts
    for f in range(bck.frame_count + 1):
        out.append(rint(bck.sample(shock.joints[0], f)[0][1] * 15))
        out.append(rint(bck.sample(shock.joints[1], f)[0][1] * 24))
    return out


def icechild(ctx, spec):
    rig = Rig(ctx, "ObjectData/IceMeramera.arc", "icemeramera.bdl", ["JointRoot"])
    out = []
    counts = []
    for clip in spec_lines(spec)[0]:
        bck = rig.bck(clip)
        count = bck.frame_count + 1
        counts.append(count)
        out.append(count)
        for f in range(count):
            w = rig.worlds(bck, f)[0]
            out += [rint(w[i][0] * 50.0 + w[i][3]) for i in range(3)]
    return out + counts


def fluff_hang(ctx, spec):
    rig = Rig(ctx, "ObjectData/Fluff.arc", "fluff.bdl", ["MarioHang"])
    out = []
    for clip, ticks, loop in spec_lines(spec):
        bck = rig.bck(clip)
        frames = stepped(bck.frame_count, int(ticks), loop == "1")
        out.append(len(frames))
        for f in frames:
            out += [rint(v) for v in translation(rig.worlds(bck, f)[0], 4)]
    return out


def rabbit_spine(ctx, spec):
    rig = Rig(ctx, "ObjectData/TrickRabbit.arc", "trickrabbit.bdl", ["Spine"])
    out = []
    for clip, ticks, loop, first in spec_lines(spec):
        bck = rig.bck(clip)
        frames = [0] if first == "1" else stepped(bck.frame_count, int(ticks), loop == "1")
        for f in frames:
            out += [rint(v) for v in translation(rig.worlds(bck, f)[0], 4)]
    return out


def guarder_matrices(rig, clips):
    out = []
    for clip in clips:
        bck = rig.bck(clip)
        n = bck.frame_count
        frames = range(n if bck.loop_mode == 2 else n + 1)
        mats = []
        for f in frames:
            mats.append(tuple(rint(v * 65536) for row in rig.worlds(bck, f)[0] for v in row))
        out.append((n, mats))
    return out


def buoy_guarder(ctx, spec):
    rows = {r[0]: r[1:] for r in spec_lines(spec)}
    parts = [Rig(ctx, "ObjectData/JumpGuarder.arc", "jumpguarder.bdl", ["Body"]),
             Rig(ctx, "ObjectData/JumpGuarderHead.arc", "jumpguarderhead.bdl", ["SpringJoint3"])]
    pool = {}
    order = []
    out = []
    for rig, key in zip(parts, ("body", "head")):
        lengths = []
        for n, mats in guarder_matrices(rig, rows[key]):
            for m in mats:
                if m not in pool:
                    pool[m] = len(order)
                    order.append(m)
                out.append(pool[m])
            lengths.append(n)
        out += lengths
    for m in order:
        out += list(m)
    return out


PREDICTORS = {
    "src/actors/packun_head_poses_gen.h": packun_head,
    "src/actors/honey_queen_poses_gen.h": honey_queen,
    "src/actors/honey_boss_poses_gen.h": honey_boss,
    "src/actors/honey_mandibug_child_poses_gen.h": mandibug_child,
    "src/actors/honey_mandibug_visibility_gen.h": mandibug_visibility,
    "src/actors/honey_crystal_bounds_gen.h": crystal_bounds,
    "src/actors/iceking_poses_gen.h": iceking,
    "src/actors/icechild_poses_gen.h": icechild,
    "src/actors/fluff_hang_gen.h": fluff_hang,
    "src/actors/gusty_rabbit_spine_gen.h": rabbit_spine,
    "src/actors/buoy_jumpguarder_joints_gen.h": buoy_guarder,
}
