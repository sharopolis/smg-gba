import json
import os
import re

from common import Dump, IntReader, parse_args, read_pak, unzigzag, write_out
from stages_galaxy import Galaxy, GalaxyLines
from stages_math import rot_matrix
from stages_pool import ArchiveCache, build_pool, name_candidates, texture_repeats

TOKEN = re.compile(r"-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?|[A-Za-z]+|[^-0-9A-Za-z]|-")
NUMBER = re.compile(r"-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?\Z")
K_TEXT, K_V, K_VT, K_F = 0, 1, 2, 3
MODE_BOX, MODE_ORIG, MODE_AFFINE = 1, 2, 3
GAME = 1000.0 / 256.0
TILE_SCALE = 10 ** 9
AFF_SCALE = 10 ** 7
IDENTITY_FRAME = ([[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]], [0.0, 0.0, 0.0])


class Streams:
    def __init__(self, data):
        self.readers = {k: IntReader(v) for k, v in data.items()}

    def get(self, name):
        return self.readers[name].int()

    def text(self, name):
        n = self.get(name)
        return bytes(self.get(name) for _ in range(n)).decode("utf-8")


def is_num(t):
    return NUMBER.match(t) is not None


def parse_num(t):
    neg = t.startswith("-")
    body = t[1:] if neg else t
    if "." in body:
        a, b = body.split(".")
        dec = len(b)
        v = int(a + b)
    else:
        dec = 0
        v = int(body)
    if neg:
        v = -v
    return v, dec


def format_num(v, dec, negzero):
    s = str(abs(v))
    if dec:
        s = s.rjust(dec + 1, "0")
        s = s[:-dec] + "." + s[-dec:]
    if v < 0 or negzero:
        s = "-" + s
    return s


def rescale(v, src, dst):
    if dst >= src:
        return v * 10 ** (dst - src)
    q = 10 ** (src - dst)
    return (v + q // 2) // q


def number_from(streams, ref):
    rv, rdec = parse_num(ref)
    c = streams.get("ln.res")
    r = unzigzag(c >> 2)
    if c & 2:
        dec = streams.get("ln.dec")
        return format_num(rescale(rv, rdec, dec) + r, dec, c & 1)
    return format_num(rv + r, rdec, c & 1)


def decode_line(streams, ref, strings):
    rt = TOKEN.findall(ref)
    rnums = [t for t in rt if is_num(t)]
    out = []
    pos = 0
    for _ in range(streams.get("ln.nops")):
        kind = streams.get("ln.op")
        n = streams.get("ln.n")
        if kind == 0:
            for t in rt[pos:pos + n]:
                out.append(number_from(streams, t) if is_num(t) else t)
            pos += n
        elif kind == 1:
            pos += n
        else:
            for _ in range(n):
                c = streams.get("ln.ins")
                if c > 0:
                    out.append(number_from(streams, rnums[c - 1]))
                elif c == 0:
                    u = streams.get("ln.lit")
                    out.append(format_num(unzigzag(u >> 1), streams.get("ln.litdec"), u & 1))
                else:
                    out.append(strings[streams.get("ln.str")])
    return "".join(out)


def decode_lines(streams, n, cands):
    strings = [streams.text("ls.strs") for _ in range(streams.get("ls.nstr"))]
    out = []
    prev = -1
    for _ in range(n):
        d = streams.get("ls.ref")
        r = -1 if d == -(1 << 30) else prev + 1 + d
        if streams.get("ls.exact"):
            out.append(cands[r])
        else:
            out.append(decode_line(streams, cands[r] if r >= 0 else "", strings))
        if r >= 0:
            prev = r
    return out


def frame_of(obj):
    w = obj["world"]
    r = rot_matrix(w["rot"])
    g = [[-r[0][0], -r[0][1], -r[0][2]], [r[2][0], r[2][1], r[2][2]], [r[1][0], r[1][1], r[1][2]]]
    axes = [[g[0][0], g[1][0], g[2][0]], [-g[0][2], -g[1][2], -g[2][2]], [g[0][1], g[1][1], g[2][1]]]
    p = w["pos"]
    return axes, [-p[0] / 256.0, p[2] / 256.0, p[1] / 256.0]


def box_project(frame, pts):
    axes, loc = frame
    loc_pts = []
    for x, y, z in pts:
        g = (-x * GAME - loc[0], z * GAME - loc[1], y * GAME - loc[2])
        loc_pts.append([axes[k][0] * g[0] + axes[k][1] * g[1] + axes[k][2] * g[2] for k in range(3)])
    a, b, c = loc_pts
    e1 = [b[k] - a[k] for k in range(3)]
    e2 = [c[k] - a[k] for k in range(3)]
    n = [-(e1[1] * e2[2] - e1[2] * e2[1]), -(e1[2] * e2[0] - e1[0] * e2[2]), -(e1[0] * e2[1] - e1[1] * e2[0])]
    ax = 0
    for k in (1, 2):
        if abs(n[k]) > abs(n[ax]):
            ax = k
    return loc_pts, ax, 1.0 if n[ax] > 0 else -1.0


def box_uv(p, ax, sg, tile):
    if ax == 0:
        return p[1] * sg / tile, p[2] / tile
    if ax == 1:
        return -p[0] * sg / tile, p[2] / tile
    return p[0] / tile, p[1] * sg / tile


def affine_uv(params, p):
    a = [x / AFF_SCALE for x in params]
    return (a[0] * p[0] + a[1] * p[1] + a[2] * p[2] + a[3], a[4] * p[0] + a[5] * p[1] + a[6] * p[2] + a[7])


def scaled(x, dec):
    return int(round(x * 10 ** dec))


def face_groups(kinds, texts):
    cur_o = -1
    cur_m = ""
    groups = {}
    out = []
    ti = 0
    for i, k in enumerate(kinds):
        if k == K_TEXT:
            t = texts[ti]
            ti += 1
            if t.startswith("o "):
                cur_o = i
            elif t.startswith("usemtl "):
                cur_m = t
        elif k == K_F:
            out.append(groups.setdefault((cur_o, cur_m), len(groups)))
    return out


def read_group_params(streams):
    out = []
    for _ in range(streams.get("g.n")):
        g = {"mode": streams.get("g.mode")}
        if g["mode"] == MODE_BOX:
            g["frame"] = streams.get("g.frame")
            g["tile"] = streams.get("g.tile")
            g["conv"] = streams.get("g.conv")
            g["tsrc"] = streams.get("g.tsrc")
        if g["mode"] in (MODE_ORIG, MODE_AFFINE):
            g["aff"] = [streams.get("g.aff") for _ in range(8)]
        out.append(g)
    return out


def box_face_uvs(streams, g, pts, frames, objects, repeats):
    fr = frames.get(g["frame"])
    if fr is None:
        fr = frames[g["frame"]] = frame_of(objects[g["frame"]]) if g["frame"] >= 0 else IDENTITY_FRAME
    lp, ax, sg = box_project(fr, pts)
    if g["conv"] == 1 and ax == 2:
        sg = 1.0
    elif g["conv"] == 2:
        sg = (1.0, -1.0, 1.0)[ax]
    c = streams.get("vt.ax")
    if c:
        c -= 1
        ax = c >> 1
        sg = 1.0 if c & 1 else -1.0
    tile = g["tile"] / TILE_SCALE
    if g["tsrc"]:
        rep = repeats(g["frame"])
        tile = (int(round(rep[sorted(rep)[g["tsrc"] - 1]] * TILE_SCALE)) + g["tile"]) / TILE_SCALE
    return [box_uv(p, ax, sg, tile) for p in lp]


def predict_uvs(streams, faces, fgroups, gparams, verts, anchors, pool, objects, repeats):
    pred = {}
    frames = {}
    for fi, face in enumerate(faces):
        g = gparams[fgroups[fi]]
        mode = g["mode"]
        pts = [verts[v - 1] for v, _ in face]
        if mode == MODE_BOX:
            uvs = box_face_uvs(streams, g, pts, frames, objects, repeats)
        elif mode == MODE_ORIG:
            uvs = []
            for (v, t), p in zip(face, pts):
                if t in pred:
                    uvs.append(None)
                    continue
                options = list(pool.uvs[anchors[v - 1]]) + [affine_uv(g["aff"], p)]
                uvs.append(options[streams.get("vt.oc")])
        elif mode == MODE_AFFINE:
            uvs = [affine_uv(g["aff"], p) for p in pts]
        else:
            uvs = [(0.0, 0.0)] * 3
        for (_, t), uv in zip(face, uvs):
            if t not in pred and uv is not None:
                pred[t] = uv
    return pred


def decode_obj(streams, nlines, pool, objects, cands, repeats):
    kinds = []
    while len(kinds) < nlines:
        k = streams.get("o.kind")
        kinds += [k] * streams.get("o.run")
    texts = decode_lines(streams, kinds.count(K_TEXT), cands)
    vrows = []
    verts = []
    anchors = []
    a = 0
    for _ in range(kinds.count(K_V)):
        a += streams.get("v.anc")
        anchors.append(a)
        res = [streams.get("v.res") for _ in range(3)]
        dc = streams.get("v.dec")
        decs = (dc >> 8, (dc >> 4) & 15, dc & 15)
        nz = streams.get("v.nz")
        row = []
        vals = []
        for k in range(3):
            v = scaled(pool.pts[a][k], decs[k]) + res[k]
            row.append(format_num(v, decs[k], ((nz >> k) & 1) ^ (v == 0 and pool.pts[a][k] < 0)))
            vals.append(v / 10 ** decs[k])
        vrows.append("v " + " ".join(row))
        worst = max(abs(x) for x in res)
        verts.append(pool.pts[a] if worst <= 1 and min(decs) >= 5 else tuple(vals))
    faces = []
    pv = pt = 0
    for _ in range(kinds.count(K_F)):
        face = []
        for _ in range(3):
            pv += streams.get("f.v")
            pt += streams.get("f.t")
            face.append((pv, pt))
        faces.append(face)
    gparams = read_group_params(streams)
    fgroups = face_groups(kinds, texts)
    pred = predict_uvs(streams, faces, fgroups, gparams, verts, anchors, pool, objects, repeats)
    vtrows = []
    last = (0.0, 0.0)
    for i in range(kinds.count(K_VT)):
        p = pred.get(i + 1, last)
        last = p
        res = [streams.get("vt.res") for _ in range(2)]
        dc = streams.get("vt.dec")
        decs = (dc >> 4, dc & 15)
        nz = streams.get("vt.nz")
        row = []
        for k in range(2):
            v = scaled(p[k], decs[k]) + res[k]
            row.append(format_num(v, decs[k], ((nz >> k) & 1) ^ (v == 0 and p[k] < 0)))
        vtrows.append("vt " + " ".join(row))
    rows = {K_TEXT: iter(texts), K_V: iter(vrows), K_VT: iter(vtrows)}
    faces_left = iter(faces)
    out = []
    for k in kinds:
        if k == K_F:
            out.append("f " + " ".join("%d/%d" % c for c in next(faces_left)))
        else:
            out.append(next(rows[k]))
    return out


def build_stage(dump, pak_path, out_root):
    stage = os.path.basename(pak_path)[:-4]
    pak = read_pak(pak_path)
    meta = json.loads(pak["meta"].decode("utf-8"))
    gal = Galaxy(dump, meta["galaxy"])
    data = gal.build()
    objects = data["objects"]
    arcs = ArchiveCache(dump)
    sources = [tuple(s) for s in meta["sources"]]
    pool = None
    names = None
    galaxy_cands = None
    repeats_cache = {}

    def repeats(index):
        if index not in repeats_cache:
            repeats_cache[index] = texture_repeats(arcs, objects[index])
        return repeats_cache[index]

    for fname, nlines in meta["files"]:
        prefix = fname + "/"
        streams = Streams({k[len(prefix):]: v for k, v in pak.items() if k.startswith(prefix)})
        if fname.endswith((".obj", ".mtl")) and names is None:
            names = name_candidates(arcs, sources, objects, data["zone_list"])
        if fname.endswith(".obj"):
            if pool is None:
                pool = build_pool(arcs, objects, sources, data["zone_placements"])
            lines = decode_obj(streams, nlines, pool, objects, names, repeats)
        elif fname.endswith(".mtl"):
            lines = decode_lines(streams, nlines, names)
        else:
            if galaxy_cands is None:
                ex = GalaxyLines(data, gal, {k: tuple(v) for k, v in meta["kinds"].items()})
                galaxy_cands = ex.build() + ex.extra_candidates()
            lines = decode_lines(streams, nlines, galaxy_cands)
        write_out(out_root, "assets/stages/%s/%s" % (stage, fname), "\n".join(lines).encode("utf-8"))


def main():
    args = parse_args()
    dump = Dump(args.orig)
    root = os.path.join(args.data, "stages")
    paks = sorted(p for p in os.listdir(root) if p.endswith(".pak"))
    for p in paks:
        build_stage(dump, os.path.join(root, p), args.out)
    print("stages: %d stages" % len(paks))


if __name__ == "__main__":
    main()
