import math
import struct

from actor_headers_lib import (BCK, BCSV, KCL, CameraAnim, Model, gba, mtx_apply, mtx_col, rint, spec_lines,
                               wound)
from common import yaz0_decompress

MARIO_ARC = "ObjectData/Mario.arc"
MARIO_ANIME = "ObjectData/MarioAnime.arc"
DINO_ARC = "ObjectData/DinoPackun.arc"
DINO_DEMO_ARC = "ObjectData/DinoPackunDemoPosition.arc"
DINO_ZONE = "StageData/DinoPackunBattleZone.arc"
DINO_TAIL = ("Tail1", "Tail2", "Tail3", "Tail4", "Tail5", "Tail6", "Tail7", "TailBall")
DINO_HEAD_HIT = (140.0, -110.0, 0.0)
DINO_STEP = 4
BURROWS_STEP = 6
HAND_STEP = 3


class ActorCanm:
    def __init__(self, data):
        data = yaz0_decompress(data)
        self.frame_count = struct.unpack_from(">I", data, 0x18)[0]
        desc = [struct.unpack_from(">III", data, 0x20 + i * 12) for i in range(8)]
        nflt = struct.unpack_from(">I", data, 0x80)[0] // 4
        flt = struct.unpack_from(">%df" % nflt, data, 0x84)
        self.tracks = []
        for cnt, first, mode in desc:
            if cnt == 1:
                self.tracks.append((None, float(flt[first])))
            else:
                stride = 4 if mode == 1 else 3
                keys = []
                for i in range(cnt):
                    keys.append(tuple(float(x) for x in flt[first + i * stride:first + i * stride + stride]))
                self.tracks.append((keys, None))

    def sample(self, f):
        return [canm_track_value(t, f) for t in self.tracks]


def canm_track_value(track, f):
    k, c = track
    if k is None:
        return c
    if f <= k[0][0]:
        return k[0][1]
    if f >= k[-1][0]:
        return k[-1][1]
    i = 0
    while i + 1 < len(k) and k[i + 1][0] <= f:
        i += 1
    t0, v0, o0 = k[i][0], k[i][1], k[i][-1]
    t1, v1, i1 = k[i + 1][0], k[i + 1][1], k[i + 1][2]
    d = t1 - t0
    if d <= 0:
        return v1
    s = (f - t0) / d
    s2, s3 = s * s, s * s * s
    return v0 * (2 * s3 - 3 * s2 + 1) + v1 * (-2 * s3 + 3 * s2) + o0 * (s3 - 2 * s2 + s) + i1 * (s3 - s2)


def ints3(v):
    return [rint(v[0]), rint(v[1]), rint(v[2])]


def hand_keys(ctx, bck, first, last):
    model = ctx.memo("mario_model", lambda: Model(ctx.arc_file(MARIO_ARC, "mario.bdl")))
    hand = model.joint("HandR")
    order = model.chain([hand])
    keys = []
    duration = last - first
    for f in range(0, ((duration + 2) // 3) * 3 + 1, HAND_STEP):
        w = model.pose(bck, min(first + f, last), order)
        keys.append(ints3(mtx_col(w[hand], 3)))
    return keys


def fire_hand(ctx, spec):
    vals = []
    table = []
    for t in spec_lines(spec):
        bck = BCK(ctx.arc_file(MARIO_ANIME, (t[1] if len(t) > 1 else t[0]) + ".bck"))
        first = int(t[2]) if len(t) > 2 else 0
        last = int(t[3]) if len(t) > 3 else bck.frame_count
        keys = hand_keys(ctx, bck, first, last)
        for k in keys:
            vals += k
        table += [last - first, len(keys)]
    return vals + table


def mario_hand(ctx, spec):
    vals = []
    table = []
    for t in spec_lines(spec):
        bck = BCK(ctx.arc_file(MARIO_ANIME, t[0] + ".bck"))
        keys = hand_keys(ctx, bck, 0, bck.frame_count)
        for k in keys:
            vals += k
        table += [bck.frame_count, len(keys)]
    return vals + table


def burrows_cams(ctx, spec):
    vals = []
    frames = []
    for name in spec_lines(spec)[0]:
        c = ActorCanm(ctx.arc_file("ObjectData/Dodoryu.arc", name + ".canm"))
        for f in range(0, c.frame_count + BURROWS_STEP, BURROWS_STEP):
            s = c.sample(min(f, c.frame_count))
            vals += [rint(v) for v in s[:6]]
        frames.append(c.frame_count)
    return vals + frames


def unit127(v):
    n = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    if n > 1e-9:
        v = (v[0] / n, v[1] / n, v[2] / n)
    return [max(-127, min(127, rint(x * 127.0))) for x in v]


def pos8(v):
    return [rint(x / 8.0) for x in v]


def dino_keys(ctx, clips):
    model = ctx.memo("dino_model", lambda: Model(ctx.arc_file(DINO_ARC, "dinopackun.bdl")))
    root, head, egg = model.joint("TailRoot"), model.joint("Head"), model.joint("EggShell")
    tail = [model.joint(n) for n in DINO_TAIL]
    keys, eggs, tails, egg_first, tail_first = [], [], [], [], []
    for action, tpk, mode, flags in clips:
        bck = BCK(ctx.arc_file(DINO_ARC, action + ".bck"))
        end = bck.frame_count
        frames = list(range(0, end + 1, tpk))
        if mode == "loop" and len(frames) > 1 and frames[-1] == end:
            frames.pop()
        egg_first.append(len(eggs) if "e" in flags else -1)
        tail_first.append(len(tails) if "t" in flags else -1)
        targets = [root, head] + (tail if "t" in flags else []) + ([egg] if "e" in flags else [])
        order = model.chain(targets)
        for f in frames:
            w = model.pose(bck, f, order)
            r = w[root]
            keys.append(pos8(gba(mtx_col(r, 3))) + unit127(gba(mtx_col(r, 0)))
                        + pos8(gba(mtx_apply(w[head], DINO_HEAD_HIT))))
            if "t" in flags:
                row = []
                for j in tail:
                    row += pos8(gba(mtx_col(w[j], 3)))
                tails.append(row)
            if "e" in flags:
                e = w[egg]
                eggs.append(pos8(gba(mtx_col(e, 3))) + unit127(gba(mtx_col(e, 2)))
                            + unit127(gba(mtx_col(e, 1))))
    return keys, eggs, tails, egg_first, tail_first


def dino_cams(ctx, names):
    out = []
    index = []
    for name in names:
        c = CameraAnim(ctx.arc_file(DINO_ARC, name + ".canm"))
        index += [len(out), c.frames]
        for f in range(0, c.frames + DINO_STEP, DINO_STEP):
            v = c.sample(min(f, c.frames))
            out.append([rint(x) for x in gba(v[0:3]) + gba(v[3:6])])
    return index, out


def dino_mario(ctx):
    model = Model(ctx.arc_file(DINO_DEMO_ARC, "dinopackundemoposition.bdl"))
    bck = BCK(ctx.arc_file(DINO_DEMO_ARC, "openingdemo.bck"))
    j = model.joint("MarioPosition")
    order = model.chain([j])
    rows = []
    for f in range(0, bck.frame_count + DINO_STEP, DINO_STEP):
        m = model.pose(bck, min(f, bck.frame_count), order, False)[j]
        rows.append([rint(x) for x in gba(mtx_col(m, 3))] + unit127(gba(mtx_col(m, 2)))
                    + unit127(gba(mtx_col(m, 1))))
    while len(rows) > 1 and rows[-1] == rows[-2]:
        rows.pop()
    return rows


def decimal5(v):
    a, b = ("%.5f" % v).split(".")
    return [int(a), int(b)]


def dino_area(ctx, cam_id):
    t = BCSV(ctx.arc_file(DINO_ZONE, "camera/cameraparam.bcam"))
    i = [k for k in range(t.n) if t.get(k, "id") == cam_id][0]
    ax = [t.get(i, h, 0.0) for h in (0xAC52894B, 0xAC52894C, 0xAC52894D)]
    return ([rint(ax[0]), rint(ax[1])] + decimal5(t.get(i, "angleA")) + decimal5(ax[2])
            + decimal5(t.get(i, "angleB")) + [rint(t.get(i, "dist"))])


def dinopackun(ctx, spec):
    lines = spec_lines(spec)
    clips = [(t[1], int(t[2]), t[3], t[4] if len(t) > 4 else "") for t in lines[2:]]
    keys, eggs, tails, egg_first, tail_first = dino_keys(ctx, clips)
    index, cams = dino_cams(ctx, lines[0])
    mario = dino_mario(ctx)
    vals = [len(keys)]
    for k in keys:
        vals += k
    vals += [v for v in egg_first if v >= 0] + [len(eggs)]
    for k in eggs:
        vals += k
    vals += [v for v in tail_first if v >= 0] + [len(tails)]
    for k in tails:
        vals += k
    vals += index + [len(cams)]
    for k in cams:
        vals += k
    vals += [len(mario)]
    for k in mario:
        vals += k
    return vals + dino_area(ctx, lines[1][0])


def gate_pose(w):
    out = [rint(w[0][3]), rint(w[1][3]), rint(w[2][3])]
    sc = [math.sqrt(w[0][c] * w[0][c] + w[1][c] * w[1][c] + w[2][c] * w[2][c]) for c in range(3)]
    for c in (1, 2):
        out += [rint(w[r][c] / sc[c] * 16384) for r in range(3)]
    return out + [rint(s * 16384) for s in sc]


def gate(ctx, spec):
    vals = []
    for t in spec_lines(spec):
        name = t[0]
        arc = "ObjectData/" + name + ".arc"
        model = Model(ctx.arc_file(arc, name.lower() + ".bdl"))
        for clip in t[1:]:
            bck = BCK(ctx.arc_file(arc, clip.rstrip("+") + ".bck"))
            vals.append(len(model.joints))
            for f in range(bck.frame_count + 1):
                w = model.pose(bck, f)
                for j in range(len(model.joints)):
                    vals += gate_pose(w[j])
            if clip.endswith("+"):
                vals.append(bck.frame_count)
        k = KCL(ctx.arc_file(arc, name.lower() + ".kcl"))
        pa = BCSV(ctx.arc_file(arc, name.lower() + ".pa"))
        for tri, n in zip(k.triangles, k.normals):
            for p in wound(tri, n):
                vals += ints3(p)
        for attr in k.attributes:
            vals += [pa.get(attr, "Floor_code"), pa.get(attr, "Wall_code")]
    return vals


PREDICTORS = {
    "src/player/mario_fire_hand_gen.h": fire_hand,
    "src/player/mario_hand_gen.h": mario_hand,
    "src/actors/gusty_burrows_cam_gen.h": burrows_cams,
    "src/actors/dinopackun_gen.h": dinopackun,
    "src/actors/mario_launcher_gate_gen.h": gate,
}
