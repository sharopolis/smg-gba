import json
import math
import os

from common import Dump, IntReader, fsum, parse_args, read_pak, write_out
from mario_figure_j3d import BCK, Model, bva_visibility, mapply, minv, mmul, rigid_of, sincos

MARIO_ARC = "ObjectData/Mario.arc"
ANIME_ARC = "ObjectData/MarioAnime.arc"
WING_ARC = "ObjectData/BeeMarioWing.arc"
OBJ_SCALE = 0.004


def rotvec_matrix(r, t):
    th2 = r[0] * r[0] + r[1] * r[1] + r[2] * r[2]
    if th2 == 0.0:
        return (1.0, 0.0, 0.0, t[0], 0.0, 1.0, 0.0, t[1], 0.0, 0.0, 1.0, t[2])
    th = math.sqrt(th2)
    s, c = sincos(th)
    kx, ky, kz = r[0] / th, r[1] / th, r[2] / th
    a = 1.0 - c
    return (c + a * kx * kx, a * kx * ky - s * kz, a * kx * kz + s * ky, t[0],
            a * kx * ky + s * kz, c + a * ky * ky, a * ky * kz - s * kx, t[1],
            a * kx * kz - s * ky, a * ky * kz + s * kx, c + a * kz * kz, t[2])


class Sources:
    def __init__(self, dump, corrections):
        self.dump = dump
        self.mario = Model(dump.load(MARIO_ARC + ":mario.bdl"))
        self.wing = Model(dump.load(WING_ARC + ":beemariowing.bdl"))
        self.corr = {}
        for j, r, t in corrections:
            self.corr[j] = self.mario.correction(j, rotvec_matrix(r, t))
        self.bcks = {}
        self.wing_bcks = {}
        self.bvas = {}
        self.poses = {}
        self.wing_poses = {}

    def joint(self, name):
        return self.mario.names.index(name)

    def bck(self, name):
        b = self.bcks.get(name)
        if b is None:
            b = self.bcks[name] = BCK(self.dump.load(ANIME_ARC + ":" + name + ".bck"))
        return b

    def wing_bck(self, name):
        b = self.wing_bcks.get(name)
        if b is None:
            b = self.wing_bcks[name] = BCK(self.dump.load(WING_ARC + ":" + name + ".bck"))
        return b

    def bva(self, name):
        v = self.bvas.get(name)
        if v is None:
            v = self.bvas[name] = bva_visibility(self.dump.load(WING_ARC + ":" + name + ".bva"))
        return v

    def pose(self, bck, frame):
        key = (bck, frame)
        p = self.poses.get(key)
        if p is None:
            if bck is None:
                lg = self.mario.pose(self.mario.rest_raw())
            else:
                lg = self.mario.pose(self.bck(bck).raw(self.mario, frame))
            p = self.poses[key] = self.mario.deform(lg, self.corr)
        return p

    def wing_pose(self, bck, frame):
        key = (bck, frame)
        p = self.wing_poses.get(key)
        if p is None:
            lg = self.wing.pose(self.wing_bck(bck).raw(self.wing, frame))
            p = self.wing_poses[key] = self.wing.deform(lg, {})
        return p


def round_int(x, scale):
    return math.floor(x * scale + 0.5)


def skin(pose, bones, weights, p):
    x = y = z = 0.0
    for b, w in zip(bones, weights):
        m = pose[b]
        x += w * (m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3] * OBJ_SCALE)
        y += w * (m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7] * OBJ_SCALE)
        z += w * (m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11] * OBJ_SCALE)
    return x, y, z


def rest_point(base_int, pd):
    return tuple((base_int[k] * 1000 + pd[k]) / 1e8 for k in range(3))


def wing_spine(src, pose):
    spine = src.joint("Spine2")
    return rigid_of(mmul(pose[spine], src.mario.bind[spine]))


def wing_state(src, spec, fr, start):
    bck, fixed = spec
    b = src.wing_bck(bck)
    if fixed is not None:
        wf = fixed
    else:
        wf = fr - start
        if b.loop_mode == 2:
            wf = wf % b.frame_count
    return bck, wf, src.wing_pose(bck, min(wf, b.frame_count))


def part_matrix(src, part, pose, wing):
    kind, joint, shape = part
    if kind == 0:
        return pose[src.joint("Head")], True
    bck, wf, wing_pose = wing
    vis = src.bva(bck)[shape]
    return mmul(wing_spine(src, pose), wing_pose[joint]), vis[min(int(wf), len(vis) - 1)] != 0


def part_rest(src, part, q):
    kind, joint, _ = part
    pose = src.pose(None, 0)
    if kind == 0:
        m = pose[src.joint("Head")]
    else:
        m = mmul(wing_spine(src, pose), src.wing_pose("wait", 0)[joint])
    g = (q[0] / OBJ_SCALE, q[1] / OBJ_SCALE, q[2] / OBJ_SCALE)
    return mapply(minv(m), g)


def predict_key_values(src, groups, spec, clip, i):
    c = spec["clips"][clip]
    group = groups[str(c["group"])]
    fr = c["start"] + i * c["step"]
    pose = src.pose(c["bck"], fr)
    out = []
    rest = group["rest"]
    for v in range(len(group["bones"])):
        out.extend(skin(pose, group["bones"][v], group["weights"][v], rest[v]))
    parts = spec.get("parts")
    if parts:
        wing = wing_state(src, c["wing"], fr, c["start"]) if c.get("wing") else None
        pts = []
        for k, part in enumerate(parts):
            m, vis = part_matrix(src, part, pose, wing)
            g = mapply(m, spec["part_rest"][k])
            pts.append(((g[0] * OBJ_SCALE, g[1] * OBJ_SCALE, g[2] * OBJ_SCALE), vis))
        for k in range(0, len(pts), 3):
            tri = pts[k:k + 3]
            if tri[0][1]:
                for p, _ in tri:
                    out.extend(p)
            else:
                a, b, d = tri[0][0], tri[1][0], tri[2][0]
                cx = (a[0] + b[0] + d[0]) / 3
                cy = (a[1] + b[1] + d[1]) / 3
                cz = (a[2] + b[2] + d[2]) / 3
                out.extend((cx, cy, cz) * 3)
    return out


def fmt(n, digits, neg):
    s = str(-n if n < 0 else n).rjust(digits + 1, "0")
    s = s[:-digits] + "." + s[-digits:]
    return "-" + s if (n < 0 or neg) else s


def fmt_row(vals, negs, pos, digits, mul=1):
    return " ".join(fmt(vals[pos + k] * mul, digits, negs[pos + k]) for k in range(3))


class Builder:
    def __init__(self, src, meta):
        self.src = src
        self.meta = meta

    def predict_vsets(self):
        out = {}
        for name in sorted(self.meta["anchors"]):
            vals = []
            negs = []
            for p, d in self.meta["anchors"][name]:
                for x in self.src.mario.anchor_point(p, d):
                    vals.append(round_int(x, 400.0))
                    negs.append(x < 0)
            out[name] = (vals, negs)
        return out

    def predict_uv(self):
        vals = []
        negs = []
        for idx, flip in self.meta["uv_anchors"]:
            u, v = self.src.mario.arrays[13][idx]
            if flip:
                v = 1.0 - v
            for x in (u, v):
                vals.append(round_int(x, 1e6))
                negs.append(x < 0)
        return vals, negs

    def groups(self, vsets):
        base = vsets["mario"]
        out = {}
        for g, ents in self.meta["groups"].items():
            bones, weights, rest = [], [], []
            for v, (b, wq, pd) in enumerate(ents):
                bones.append(b)
                weights.append([q / 1e8 for q in wq] + [(100000000 - fsum(wq)) / 1e8])
                rest.append(rest_point(base[3 * v:3 * v + 3], pd))
            out[g] = {"bones": bones, "weights": weights, "rest": rest}
        return out

    def figure_spec(self, fig, vsets):
        clips = {}
        order = []
        for ent in self.meta["figures"][fig]:
            name, n, group, bck, start, step = ent[:6]
            c = {"n": n, "group": group, "bck": bck, "start": start, "step": step}
            if len(ent) > 6:
                c["wing"] = (ent[6][0], ent[6][1])
            clips[name] = c
            order.append(name)
        spec = {"clips": clips}
        if fig == "mariobee":
            bp = vsets["beeparts"]
            parts = [tuple(p) for p in self.meta["parts"]]
            spec["parts"] = parts
            spec["part_rest"] = [part_rest(self.src, parts[k], rest_point(bp[3 * k:3 * k + 3], pd))
                                 for k, pd in enumerate(self.meta["part_pd"])]
        return order, spec

    def predict_keys(self, fig, vsets, groups):
        order, spec = self.figure_spec(fig, vsets)
        vals = []
        negs = []
        for clip in order:
            for i in range(spec["clips"][clip]["n"]):
                for x in predict_key_values(self.src, groups, spec, clip, i):
                    vals.append(round_int(x, 1e5))
                    negs.append(x < 0)
        return order, spec, vals, negs

    def render(self, vsets, vnegs, uv, keys):
        files = {}
        for rel, segs in sorted(self.meta["objs"].items()):
            lines = []
            for seg in segs:
                tag = seg[0]
                if tag == "v":
                    vals, negs = [], []
                    for name in seg[1]:
                        vals += vsets[name]
                        negs += vnegs[name]
                    mul = 10 ** (seg[2] - 5)
                    for i in range(0, len(vals), 3):
                        lines.append("v " + fmt_row(vals, negs, i, seg[2], mul))
                elif tag == "vt":
                    for idx in seg[1]:
                        u, v = uv[2 * idx], uv[2 * idx + 1]
                        lines.append("vt %s %s" % (fmt(u, 6, False), fmt(v, 6, False)))
                elif tag == "f":
                    a = seg[1]
                    for i in range(0, len(a), 6):
                        lines.append("f %d/%d %d/%d %d/%d" % tuple(a[i:i + 6]))
                else:
                    lines.append(tag + " " + seg[1])
            files[rel] = ("\n".join(lines) + "\n").encode("ascii")
        for rel, text in self.meta["mtls"].items():
            files[rel] = text.encode("ascii")
        for fig, (order, spec, vals, negs) in keys.items():
            nv = len(vals) // fsum(spec["clips"][c]["n"] for c in order) // 3
            pos = 0
            for clip in order:
                for i in range(spec["clips"][clip]["n"]):
                    lines = []
                    for _ in range(nv):
                        lines.append("v " + fmt_row(vals, negs, pos, 5))
                        pos += 3
                    files["%s/k_%s_%02d.obj" % (fig, clip, i)] = ("\n".join(lines) + "\n").encode("ascii")
        for rel, src_rel in self.meta["copies"].items():
            files[rel] = files[src_rel]
        return files


def apply(stream, pred, negs, entries):
    vals = list(pred)
    res = entries.get("res/" + stream)
    if res is not None:
        r = IntReader(res)
        for i in range(len(vals)):
            vals[i] += r.int()
    flips = set()
    neg = entries.get("neg/" + stream)
    if neg is not None:
        r = IntReader(neg)
        pos = 0
        while r.pos < len(neg):
            pos += r.int()
            flips.add(pos)
    return vals, [vals[i] == 0 and (negs[i] != (i in flips)) for i in range(len(vals))]


def build(orig, data_root):
    entries = read_pak(os.path.join(data_root, "mario_figure", "mario_figure.pak"))
    meta = json.loads(entries["meta.json"])
    corrections = [(c[0], (c[1] / 1e10, c[2] / 1e10, c[3] / 1e10), (c[4] / 1e7, c[5] / 1e7, c[6] / 1e7))
                   for c in meta["corrections"]]
    rs = Builder(Sources(Dump(orig), corrections), meta)
    vsets, vnegs = {}, {}
    for name, (pred, negs) in rs.predict_vsets().items():
        vsets[name], vnegs[name] = apply("vset:" + name, pred, negs, entries)
    pred, negs = rs.predict_uv()
    uv, _ = apply("uv", pred, negs, entries)
    groups = rs.groups(vsets)
    keys = {}
    for fig in sorted(meta["figures"]):
        order, spec, pred, negs = rs.predict_keys(fig, vsets, groups)
        vals, out_negs = apply("keys:" + fig, pred, negs, entries)
        keys[fig] = (order, spec, vals, out_negs)
    return rs.render(vsets, vnegs, uv, keys)


def main():
    args = parse_args()
    files = build(args.orig, args.data)
    for rel in sorted(files):
        write_out(args.out, "assets/figures/" + rel, files[rel])
    print("mario_figure: %d files" % len(files))


if __name__ == "__main__":
    main()
