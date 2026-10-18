import json
import os

from common import Dump, IntReader, fsum, parse_args, read_pak, write_out, yaz0_decompress
from figures_j3d import BCK, Model, bind_geometry, det3, mat_apply, mat_inv, mat_mul

DEFAULT_XF = {"perm": [0, 1, 2], "sign": [1, 1, 1], "s": 0.004, "t": [0, 0, 0]}


def parse_tok(t):
    neg = t[0] == "-"
    if neg:
        t = t[1:]
    if "." in t:
        a, b = t.split(".")
        return neg, int(a + b), len(b)
    return neg, int(t), -1


def fmt_tok(tok):
    neg, q, dec = tok
    if dec < 0:
        s = str(q)
    elif dec == 0:
        s = str(q) + "."
    else:
        s = str(q).rjust(dec + 1, "0")
        s = s[:-dec] + "." + s[-dec:]
    return "-" + s if neg else s


def fmt_row(tag, row):
    return tag + " " + " ".join(fmt_tok(t) for t in row)


def signed(tok):
    return -tok[1] if tok[0] else tok[1]


def grid_n(p, fmt):
    g = int(fmt.split(":")[1])
    return int((p * g + 0.5) // 1)


def grid_tok(n, fmt):
    d, g = fmt[1:].split(":")
    return parse_tok("%.*f" % (int(d), n / int(g)))


def ptok(p, fmt):
    if fmt[0] == "f":
        return parse_tok("%.*f" % (int(fmt[1:]), p))
    if fmt[0] == "q":
        return grid_tok(grid_n(p, fmt), fmt)
    s = "%.*g" % (int(fmt[1:]), p)
    if "e" in s or "n" in s:
        s = "%.15f" % p
    return parse_tok(s)


def decode_num(p, fmt, r):
    if fmt[0] == "q":
        return grid_tok(grid_n(p, fmt) + r, fmt)
    tok = ptok(p, fmt)
    if r == 0:
        return tok
    v = signed(tok) + r
    return (v < 0, abs(v), tok[2])


def exc_num(p, code, extra, tok):
    if code == 0:
        return (not tok[0], tok[1], tok[2])
    c = code - 2
    nz, dec = c & 1, (c >> 1) - 1
    base = ptok(p, "f%d" % max(dec, 0))
    v = signed(base) + extra
    return (v < 0 or (v == 0 and nz == 1), abs(v), dec)


def read_numbers(rd, preds, fmt):
    toks = [decode_num(p, fmt, rd.int()) for p in preds]
    pos = -1
    for _ in range(rd.uvarint()):
        pos += 1 + rd.uvarint()
        code = rd.uvarint()
        extra = rd.int() if code else 0
        toks[pos] = exc_num(preds[pos], code, extra, toks[pos])
    return toks


def unique(values):
    seen = {}
    out = []
    for v in values:
        if v not in seen:
            seen[v] = len(out)
            out.append(v)
    return out


class Source:
    def __init__(self, dump, spec, unhide):
        self.model = Model(dump.load(spec))
        if unhide:
            for jt in self.model.joints:
                if all(abs(x) < 1e-6 for x in jt.scale):
                    jt.scale = (1.0, 1.0, 1.0)
            self.model.bind = self.model.world_matrices(None)
        self.point_list = None
        self.vertex_ids = None
        self.uv_list = None
        self.tri_list = None

    def points(self):
        if self.point_list is None:
            seen = {}
            pts = []
            self.vertex_ids = []
            for pv in bind_geometry(self.model):
                ids = []
                for p in pv:
                    k = seen.get(p)
                    if k is None:
                        k = seen[p] = len(pts)
                        pts.append(p)
                    ids.append(k)
                self.vertex_ids.append(ids)
            self.point_list = pts
        return self.point_list

    def posed_points(self, over):
        m = self.model
        pts = self.points()
        rest = [(jt.scale, jt.rot, jt.trans) for jt in m.joints]
        w = m.fit_world(rest, over)
        mats = [mat_mul(w[j], mat_inv(m.bind[j])) for j in range(len(w))]
        out = [None] * len(pts)
        for (_, verts, _), ids in zip(m.shapes, self.vertex_ids):
            for (drw, _), k in zip(verts, ids):
                if out[k] is None:
                    acc = [0.0, 0.0, 0.0]
                    for j, wt in m.influences(drw):
                        q = mat_apply(mats[j], pts[k])
                        acc[0] += wt * q[0]
                        acc[1] += wt * q[1]
                        acc[2] += wt * q[2]
                    out[k] = tuple(acc)
        return out

    def raw_points(self):
        return unique(self.model.arrays.get(9, []))

    def uvs(self):
        if self.uv_list is None:
            arr = self.model.arrays.get(13, [])
            found = []
            for _, verts, _ in self.model.shapes:
                for _, att in verts:
                    if 13 in att and arr:
                        found.append(arr[min(att[13], len(arr) - 1)])
            self.uv_list = unique(found)
        return self.uv_list

    def tris(self):
        if self.tri_list is None:
            self.points()
            self.tri_list = []
            for (_, _, tris), ids in zip(self.model.shapes, self.vertex_ids):
                for a, b, c in tris:
                    self.tri_list.append((ids[a], ids[b], ids[c]))
        return self.tri_list

    def raw_tris(self):
        pos = self.model.arrays.get(9, [])
        seen = {}
        for p in pos:
            seen.setdefault(p, len(seen))
        out = []
        for _, verts, tris in self.model.shapes:
            ids = [seen[pos[min(att.get(9, 0), len(pos) - 1)]] for _, att in verts]
            for a, b, c in tris:
                out.append((ids[a], ids[b], ids[c]))
        return out

    def profiles(self):
        m = self.model
        out = {(): 0}
        for j in range(len(m.joints)):
            out[((j, 1.0),)] = 0
        for d in range(len(m.drw)):
            full = sorted(m.influences(d), key=lambda x: (-x[1], x[0]))
            for k in (2, 3, 4):
                inf = full[:k]
                tot = fsum(w for _, w in inf) or 1.0
                out[tuple((j, w / tot) for j, w in inf)] = 0
        return sorted(out)


class Figure:
    def __init__(self, dump, cfg, streams):
        self.dump = dump
        self.cfg = cfg
        self.rds = dict((k, IntReader(streams[k])) for k in "mkuf")
        unhide = cfg.get("unhide", [])
        self.srcs = [Source(dump, s, i in unhide) for i, s in enumerate(cfg.get("src", []))]
        xf = cfg.get("xf", DEFAULT_XF)
        self.perm, self.sign, self.scale, self.offset = xf["perm"], xf["sign"], xf["s"], xf["t"]
        self.files = {}
        self.clouds = {}
        self.pose_cache = {}
        self.skin = None

    def fwd(self, g):
        return tuple(self.sign[i] * g[self.perm[i]] * self.scale + self.offset[i] for i in range(3))

    def inv(self, o):
        g = [0.0, 0.0, 0.0]
        for i in range(3):
            g[self.perm[i]] = (o[i] - self.offset[i]) * self.sign[i] / self.scale
        return tuple(g)

    def names(self, s):
        out = []
        i = 0
        while i < len(s):
            if s[i] == "{":
                e = s.index("}", i)
                src, idx = s[i + 2:e].split(".")
                out.append(self.srcs[int(src)].model.texture_names[int(idx)])
                i = e + 1
            else:
                out.append(s[i])
                i += 1
        return "".join(out)

    def cloud(self, spec):
        c = self.clouds.get(spec)
        if c is None:
            kind, arg = spec.split(":", 1)
            if kind == "bind":
                c = [self.fwd(p) for i in arg.split(",") for p in self.srcs[int(i)].points()]
            elif kind == "pose":
                over = tuple(tuple(o) for o in self.cfg["cpose"])
                c = [self.fwd(p) for p in self.srcs[int(arg)].posed_points(over)]
            elif kind == "raw":
                c = [self.fwd(p) for p in self.srcs[int(arg)].raw_points()]
            elif kind == "uv":
                c = self.srcs[int(arg)].uvs()
            elif kind == "uvf":
                c = [(u, 1.0 - v) for u, v in self.srcs[int(arg)].uvs()]
            else:
                c = [tuple(float(fmt_tok(t)) for t in row) for row in self.files[arg][kind]]
            self.clouds[spec] = c
        return c

    def pose(self, key):
        mats = self.pose_cache.get(key)
        if mats is None:
            sk = self.skin
            m = sk["model"]
            pose = []
            if key[0] < 0:
                for jt in m.joints:
                    pose.append((jt.scale, jt.rot, jt.trans))
            else:
                bck = sk["bcks"][key[0]]
                frame = key[1] / key[2]
                for j, jt in enumerate(m.joints):
                    pose.append(bck.sample(j, frame) if j < bck.joint_count else (jt.scale, jt.rot, jt.trans))
            if sk["world"] == 2:
                w = m.world_matrices(pose, False)
            elif sk["world"]:
                w = m.world_matrices(pose)
            else:
                w = m.fit_world(pose, key[3] if len(key) > 3 else None)
            mats = [mat_mul(w[j], sk["irest"][j]) for j in range(len(w))]
            self.pose_cache[key] = mats
        return mats

    def setup_skin(self, sc, nv):
        rd = self.rds["m"]
        src = self.srcs[sc["src"]]
        m = src.model
        arc = self.dump.arc(sc["arc"])
        files = dict((p.split("/")[-1].lower(), b) for p, b in arc.files)
        bcks = [BCK(yaz0_decompress(files[name.lower()])) for name in sc["bcks"]]
        rest = []
        for jt in m.joints:
            rest.append((tuple(x if abs(x) > 1e-6 else 1.0 for x in jt.scale), jt.rot, jt.trans))
        irest = [mat_inv(w) for w in m.world_matrices(rest)]
        profs = src.profiles()
        weights = [profs[rd.uvarint()] for _ in range(nv)]
        self.skin = {"model": m, "bcks": bcks, "irest": irest, "weights": weights, "fine": sc["fine"],
                     "bkey": tuple(sc["bkey"]) if "bkey" in sc else None, "world": sc.get("exact", 0)}

    def blend(self, mats, prof):
        if not prof:
            return None
        m = [[0.0] * 4 for _ in range(3)]
        for j, w in prof:
            mj = mats[j]
            for a in range(3):
                for c in range(4):
                    m[a][c] += w * mj[a][c]
        return m

    def rest_points(self, vtoks, fmt):
        sk = self.skin
        rd = self.rds["m"]
        dec = int(fmt[1:].split(":")[0])
        scale = 10 ** (dec + sk["fine"])
        bmats = self.pose(sk["bkey"]) if sk["bkey"] else None
        pts = []
        for row, prof in zip(vtoks, sk["weights"]):
            fin = []
            for t in row:
                q = signed(t) * 10 ** sk["fine"] + rd.int()
                fin.append(q / scale)
            g = self.inv(tuple(fin))
            if bmats is not None:
                m = self.blend(bmats, prof)
                if m is not None and abs(det3(m)) > 1e-12:
                    g = mat_apply(mat_inv(m), g)
            pts.append(g)
        sk["rest"] = pts

    def skin_point(self, mats, prof, x):
        m = self.blend(mats, prof)
        return x if m is None else mat_apply(m, x)

    def skinned(self, key):
        mats = self.pose(key)
        sk = self.skin
        return [self.fwd(self.skin_point(mats, prof, x)) for x, prof in zip(sk["rest"], sk["weights"])]

    def vblock(self, n, pred, fname):
        kind = pred["p"]
        rd = self.rds["k" if kind == "pose" else "u" if kind == "lit" else "m"]
        dim = pred.get("dim", 3)
        preds = []
        if kind == "anchor":
            cl = self.cloud(pred["c"])
            a = -1
            rec = self.files[fname].setdefault("a", [])
            posed = pred.get("posed") and self.skin["bkey"]
            if posed:
                bm = self.pose(self.skin["bkey"])
                gp = self.srcs[int(pred["c"].split(":")[1])].points()
            for i in range(n):
                a += 1 + rd.int()
                rec.append(a)
                if posed:
                    preds.extend(self.fwd(self.skin_point(bm, self.skin["weights"][i], gp[a])))
                else:
                    preds.extend(cl[a][:dim])
        elif kind == "copy":
            for row in self.cloud(pred["c"])[:n]:
                preds.extend(row[:dim])
        elif kind == "pose":
            for row in self.skinned(pred["key"]):
                preds.extend(row)
        else:
            preds = [0.0] * (n * dim)
        toks = read_numbers(rd, preds, pred["fmt"])
        return [toks[i * dim:(i + 1) * dim] for i in range(n)]

    def fblock(self, n, spec, fname):
        rd = self.rds["f"]
        lines = []
        pv, mvt = 0, 0
        tris = amap = None
        if "t" in spec:
            src = self.srcs[spec["t"]]
            tris = src.raw_tris() if spec.get("tk") == "raw" else src.tris()
            amap = {}
            for i, a in enumerate(self.files[fname]["a"]):
                amap.setdefault(a, i + 1)
        pt = -1
        for _ in range(n):
            cs = []
            code = rd.uvarint() if tris is not None else 0
            if code:
                pt += 1 + rd.int()
                a, b, c = tris[pt]
                order = ((a, b, c), (b, c, a), (c, a, b), (a, c, b), (c, b, a), (b, a, c))[code - 1]
                vs = [amap[x] for x in order]
            for ci in range(3):
                if code:
                    pv = vs[ci]
                else:
                    pv += rd.int()
                t = mvt + 1 + rd.int()
                mvt = max(mvt, t)
                cs.append("%d/%d" % (pv, t))
            lines.append("f " + " ".join(cs))
        return lines

    def build(self, f):
        name = f["n"]
        lines = []
        rec = {"v": [], "vt": []}
        self.files[name] = rec
        for op in f["prog"]:
            if op[0] == "s":
                lines.append(self.names(self.cfg["strings"][op[1]]))
            elif op[0] in ("v", "vt"):
                rest = op[0] == "v" and op[2].get("rest") and self.skin is None
                if rest:
                    self.setup_skin(self.cfg["skin"], op[1])
                rows = self.vblock(op[1], op[2], name)
                rec[op[0]].extend(rows)
                for row in rows:
                    lines.append(fmt_row(op[0], row))
                if rest:
                    self.rest_points(rows, op[2]["fmt"])
            elif op[0] == "f":
                lines.extend(self.fblock(op[1], op[2], name))
        return ("\n".join(lines) + "\n").encode()

    def write(self, out_root, rel_dir):
        for f in self.cfg["files"]:
            write_out(out_root, os.path.join(rel_dir, f["n"]), self.build(f))
        keys = self.cfg.get("keys") or []
        for k in keys:
            key = tuple(k[1:4])
            if len(k) > 4:
                key += (tuple(tuple(o) for o in k[4]),)
            pred = {"p": "pose", "key": key, "fmt": self.cfg["skin"]["kfmt"]}
            rows = self.vblock(len(self.skin["rest"]), pred, k[0])
            data = "".join(fmt_row("v", row) + "\n" for row in rows).encode()
            write_out(out_root, os.path.join(rel_dir, k[0]), data)
        return len(self.cfg["files"]) + len(keys)


def main():
    args = parse_args()
    dump = Dump(args.orig)
    pak = read_pak(os.path.join(args.data, "figures", "figures.pak"))
    count = 0
    for cfg in json.loads(pak["index.json"]):
        fig = Figure(dump, cfg, dict((k, pak.get(k + "/" + cfg["name"], b"")) for k in "mkuf"))
        count += fig.write(args.out, os.path.join("assets", "figures", cfg["name"]))
    print("figures: %d files" % count)


if __name__ == "__main__":
    main()
