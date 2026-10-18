import json
import math
import os
import struct
import subprocess

import common
import textures_bake_bee
import textures_bake_ff
import textures_bake_jpa
import textures_bake_parts
import textures_gx
import textures_ops
import textures_tev
from actor_headers_kcl_mesh import placement_rows, rotation, round_dec, row_vec, to_euler, zone_matrix
from actor_headers_placement_galaxy import mtx_mul, mtx_tr
from figures_j3d import (BCK, PI, Model, bpk_colours, brk_regs, btp_textures, bva_visible, mat_apply, mat_inv,
                         mat_mul, material_info, round_half_even, sincos, trs)

OBJ_SCALE = 0.004
AA = [(0.0, 0.0)]
HIGH_KEYS = ("src", "btp", "brk", "bva", "excl", "pose", "mode", "stages", "bpk", "raw", "static", "dolcol")
FLAG_AA_TABLE = 32
FLAG_PARTS = 65536
FLAG_CUT = 131072
FLAG_PART_IDS = 1048576
FLAG_FAR = 2097152
FLAG_REPROJECT = 4194304
FLAG_NEAR_MAP = 8388608
FLAG_RAYCAST = 33554432
FLAG_POINT_NEAR = 67108864
PLAIN_MATERIAL = dict(tex=-1, ws=1, wt=1, factor=[1.0, 1.0, 1.0, 1.0], xf=None, am=0, cut=0.5)
DATA_DIR = os.path.join(common.HERE, "..", "..", "asset_construction", "textures")
cache = {}


def cached(key, make):
    if key not in cache:
        cache[key] = make()
    return cache[key]


def model_bytes(dump, src):
    return cached(("raw", src), lambda: dump.load(src))


def run_core(bin_dir, payload, n_out, pre=()):
    exe = common.build_c("textures_bake_core", bin_dir, shared=False)
    pin = os.path.join(bin_dir, "textures_bake_in_%d.bin" % os.getpid())
    pout = os.path.join(bin_dir, "textures_bake_out_%d.bin" % os.getpid())
    with open(pin, "wb") as f:
        f.write(payload)
    subprocess.check_call([exe] + list(pre) + [pin, pout])
    with open(pout, "rb") as f:
        data = f.read() if n_out is None else f.read(n_out)
    os.remove(pin)
    os.remove(pout)
    return data


def tev_payload(model, mi, max_stages, regs_override):
    m = model.materials[mi]
    regs = [list(r) for r in m.regs]
    if regs_override:
        for k, v in regs_override.items():
            regs[k] = [x / 255 for x in v]
    texs = {}
    base = None
    for st in m.stages:
        o = m.order[st["idx"]]
        if o and o[1] != 0xFF and o[1] < 8 and m.tex[o[1]] is not None and m.tex[o[1]] < len(model.textures):
            t = model.textures[m.tex[o[1]]]
            if base is None:
                base = (t[1], t[0])
            texs[o[1]] = t
    h, w = base or (1, 1)
    first_tex = None
    for k, st in enumerate(m.stages):
        if any(x in (8, 9) for x in st["ci"]):
            first_tex = k
            break
    if first_tex is None or first_tex > 0:
        return None
    if max_stages == "auto":
        max_stages = 1
        if len(m.stages) > 1:
            s1 = m.stages[1]
            if (s1["ci"][0] == 15 and s1["cop"] == 0 and s1["cscale"] in (1, 2)
                    and not any(x in (8, 9) for x in s1["ci"]) and 0 in s1["ci"][1:]):
                max_stages = 2
    base_col = list(m.mat_color)
    if m.ras_from_vertex:
        vc = model.vertex_color_mean(mi)
        if vc is not None:
            base_col = [1.0] * 4
            base_col[:len(vc)] = vc[:4]
    out = bytearray(struct.pack("<iiii", h, w, 0, len(texs)))
    for r in (0, 1, 2, 3):
        out += struct.pack("<4d", *regs[r])
    out += struct.pack("<4d", *base_col)
    for slot in sorted(texs):
        tw, th, px = texs[slot]
        out += struct.pack("<iii", slot, tw, th) + bytes(px)
    stages = m.stages[:max(max_stages, first_tex + 1)]
    for st in stages:
        o = m.order[st["idx"]]
        slot = o[1] if o and o[1] != 0xFF and o[1] in texs else -1
        chan = o[2] if o else 0xFF
        rs, ts = m.swap[st["idx"]]
        kc = textures_tev._konst(m, m.kcsel[st["idx"]], False)
        ka = textures_tev._konst(m, m.kasel[st["idx"]], True)
        cop = st["cop"] if st["cop"] < 2 else 0
        aop = st["aop"] if st["aop"] < 2 else 0
        out += struct.pack("<ii", slot, chan)
        out += struct.pack("<4i", *m.swap_tables[rs % 4]) + struct.pack("<4i", *m.swap_tables[ts % 4])
        out += struct.pack("<4d", *kc) + struct.pack("<4d", *ka)
        out += struct.pack("<4i", *st["ci"]) + struct.pack("<4i", *st["ai"])
        out += struct.pack("<10i", cop, aop, st["cbias"], st["cscale"], st["cclamp"], st["abias"],
                           st["ascale"], st["aclamp"], st["creg"], st["areg"])
    struct.pack_into("<i", out, 8, len(stages))
    return h, w, bytes(out)


def tev_texture(tm, mi, regs, stages, bin_dir):
    regs = regs.get(tm.materials[mi].name)
    p = tev_payload(tm, mi, stages, regs)
    if p is None:
        return textures_tev.effective_rgba(tm, mi, stages, regs)
    h, w, payload = p
    return w, h, run_core(bin_dir, struct.pack("<i", 1) + payload, h * w * 4, ["-tev"])


def apply_options(dump, tm, spec):
    regs = {}
    if spec.get("btp"):
        path, frame = spec["btp"]
        for name, slot, tex in btp_textures(dump.load(path), frame):
            for m in tm.materials:
                if m.name == name and slot < len(m.tex):
                    m.tex[slot] = tex
    if spec.get("brk"):
        path, frame = spec["brk"]
        regs = brk_regs(dump.load(path), frame)
    if spec.get("dolcol"):
        name, off, idx = spec["dolcol"]
        with open(os.path.join(dump.root, "..", "sys", "main.dol"), "rb") as f:
            f.seek(off + idx * 4)
            rgba = f.read(4)
        for m in tm.materials:
            if m.name == name:
                m.mat_color = [x / 255.0 for x in rgba]
    if spec.get("bpk"):
        path, frame = spec["bpk"]
        for name, rgba in bpk_colours(dump.load(path), frame).items():
            for m in tm.materials:
                if m.name == name:
                    m.mat_color = [min(1.0, max(0.0, x / 255.0)) for x in rgba]
    return regs


def gx_material(dump, spec):
    tm = textures_tev.Model(model_bytes(dump, spec["src"]))
    regs = apply_options(dump, tm, spec)
    for mi, m in enumerate(tm.materials):
        if m.name == spec["gmat"]:
            return tm, mi, regs
    raise KeyError(spec["gmat"])


def mean_colour(tm, mi, stages, regs, channels):
    _, _, img = textures_tev.effective(tm, mi, stages, regs.get(tm.materials[mi].name))
    acc = [0.0] * channels
    for px in img:
        for c in range(channels):
            acc[c] += px[c]
    return [a / len(img) for a in acc]


def wrap_mode(v):
    return v if v in (0, 1, 2) else 1


def material(tm, mi, inf, wraps, regs, texs, spec, bin_dir):
    m = tm.materials[mi]
    raw = spec.get("raw")
    md = dict(tex=-1, ws=1, wt=1, factor=[1.0, 1.0, 1.0, 1.0], xf=None,
              am={"OPAQUE": 0, "MASK": 1, "BLEND": 2}[inf["alpha_mode"]], cut=inf["alpha_ref"], name=m.name)
    first = inf["first"]
    ti = m.tex[first] if first is not None else None
    if ti is not None and ti < len(tm.textures):
        if raw:
            tex = tm.textures[ti]
        else:
            stages = spec.get("stages", "auto")
            tkey = json.dumps([spec.get(k) for k in ("src", "btp", "brk", "bpk", "dolcol")])
            tex = cached(("tev", tkey, mi, stages), lambda: tev_texture(tm, mi, regs, stages, bin_dir))
        md["tex"] = len(texs)
        texs.append(tex)
        md["ws"] = wrap_mode(wraps[ti][0])
        md["wt"] = wrap_mode(wraps[ti][1])
        srt = inf["srt"]
        if srt is not None:
            cen, ss, st, rot, ts, tt = srt
            if (ss, st) != (1.0, 1.0) or ts or tt or rot:
                md["xf"] = [cen[0] * (1 - ss) + ts, cen[1] * (1 - st) + tt, ss, st]
    else:
        md["factor"] = list(m.mat_color) if raw else mean_colour(tm, mi, "auto", regs, 4)
    return md


def transform_normal(m, n):
    a, b, c = m[0][:3], m[1][:3], m[2][:3]
    cof = [[b[1] * c[2] - b[2] * c[1], b[2] * c[0] - b[0] * c[2], b[0] * c[1] - b[1] * c[0]],
           [c[1] * a[2] - c[2] * a[1], c[2] * a[0] - c[0] * a[2], c[0] * a[1] - c[1] * a[0]],
           [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]]
    det = a[0] * cof[0][0] + a[1] * cof[0][1] + a[2] * cof[0][2]
    sg = -1.0 if det < 0 else 1.0
    return tuple(sg * (cof[i][0] * n[0] + cof[i][1] * n[1] + cof[i][2] * n[2]) for i in range(3))


def noscale_fix(model):
    n = len(model.joints)
    if all(all(abs(x - 1.0) < 1e-9 for x in jt.scale) for jt in model.joints):
        return None
    wg, wn = [None] * n, [None] * n
    for j in model.order:
        jt = model.joints[j]
        s = list(jt.scale)
        if jt.parent >= 0 and model.mtx_calc == 2 and jt.ssc == 1:
            ps = model.joints[jt.parent].scale
            s = [a / b if b != 0 else a for a, b in zip(s, ps)]
        s = [x if abs(x) > 1e-6 else 1.0 for x in s]
        lg = trs(jt.trans, jt.rot, s)
        ln = trs(jt.trans, jt.rot, (1.0, 1.0, 1.0))
        wg[j] = lg if jt.parent < 0 else mat_mul(wg[jt.parent], lg)
        wn[j] = ln if jt.parent < 0 else mat_mul(wn[jt.parent], ln)
    return [mat_mul(wn[j], mat_inv(wg[j])) for j in range(n)]


def blend_fix(model, drw, fix):
    infl = model.influences(drw)
    tot = textures_bake_parts.plain_sum(w for _, w in infl) or 1.0
    m = [[0.0] * 4 for _ in range(3)]
    for j, w in infl:
        for a in range(3):
            for c in range(4):
                m[a][c] += w / tot * fix[j][a][c]
    return m


def pose_matrices(dump, model, pose_spec):
    src, frame = pose_spec
    bck = BCK(dump.load(src))
    pose = []
    for j, jt in enumerate(model.joints):
        if j < bck.joint_count:
            pose.append(bck.sample(j, frame))
        else:
            pose.append((jt.scale, jt.rot, jt.trans))
    return model.fit_world(pose)


def build_high(dump, spec, bin_dir):
    key = json.dumps([spec.get(k) for k in HIGH_KEYS])
    return cached(key, lambda: make_high(dump, spec, bin_dir))


def make_high(dump, spec, bin_dir):
    data = model_bytes(dump, spec["src"])
    model = Model(data)
    tm = textures_tev.Model(data)
    regs = apply_options(dump, tm, spec)
    info = material_info(data)
    wraps = [(data[h + 6], data[h + 7]) for h in textures_gx.j3d_texture_headers(data)]
    excl = set(spec.get("excl", []))
    keep = None
    if spec.get("bva"):
        path, frame = spec["bva"]
        keep = set(bva_visible(dump.load(path), frame))
    pose = None
    if spec.get("pose"):
        pose = pose_matrices(dump, model, spec["pose"])
    fix = None
    if spec.get("mode", "bake") == "bake" and pose is None and not spec.get("static"):
        fix = noscale_fix(model)
    mats, texs, mat_index = [], [], {}
    pos_out, uv_out, col_out, tri_out, mat_out, nrm_out, drw_out = [], [], [], [], [], [], []
    pos = model.arrays.get(9, [])
    nrm = model.arrays.get(10) or model.arrays.get(25)
    nattr = 10 if 10 in model.arrays else 25
    cols = model.arrays.get(11)
    tex_attrs = sorted(a for a in model.arrays if 13 <= a <= 20)
    vm = {}
    for si, (mi, verts, tris) in enumerate(model.shapes):
        if keep is not None and si not in keep:
            continue
        if mi is None or mi >= len(tm.materials) or tm.materials[mi].name in excl:
            continue
        if not tris:
            continue
        if mi not in mat_index:
            mat_index[mi] = len(mats)
            mats.append(material(tm, mi, info[mi], wraps, regs, texs, spec, bin_dir))
        avail = [a for a in tex_attrs if all(a in att for _, att in verts)]
        want = 13 + max(info[mi]["uv_src"], 0)
        ua = want if want in avail else (avail[0] if avail else None)
        has_col = cols is not None and all(11 in att for _, att in verts)
        base = len(pos_out) // 3
        for drw, att in verts:
            m = vm.get(drw)
            if m is None:
                m = model.vertex_matrix(drw, pose)
                if fix is not None:
                    m = mat_mul(blend_fix(model, drw, fix), m)
                vm[drw] = m
            p = pos[min(att.get(9, 0), len(pos) - 1)]
            pos_out.extend(mat_apply(m, p))
            drw_out.append(drw)
            if nrm is not None and nattr in att:
                nrm_out.extend(transform_normal(m, nrm[min(att[nattr], len(nrm) - 1)]))
            else:
                nrm_out.extend((0.0, 0.0, 0.0))
            if ua is not None:
                arr = model.arrays[ua]
                u = arr[min(att[ua], len(arr) - 1)]
                uv_out.extend((u[0], u[1]))
            else:
                uv_out.extend((0.0, 1.0))
            if has_col:
                c = cols[min(att[11], len(cols) - 1)]
                col_out.extend((c[0] / 255.0, c[1] / 255.0, c[2] / 255.0, c[3] / 255.0))
            else:
                col_out.extend((1.0, 1.0, 1.0, 1.0))
        for a, b, c in tris:
            if a == b or b == c or a == c:
                continue
            tri_out.extend((base + a, base + c, base + b))
            mat_out.append(mat_index[mi])
    return dict(P=pos_out, UV=uv_out, C=col_out, N=nrm_out, T=tri_out, M=mat_out, D=drw_out, vm=vm, mats=mats,
                texs=texs, model=model)


def plain_high(dump, src, bin_dir, excl=None):
    spec = dict(src=src, mode="parts", raw=True, static=True)
    if excl is not None:
        spec["excl"] = excl
    return build_high(dump, spec, bin_dir)


def parse_obj(text, mat=None):
    verts, uvs, faces = [], [], []
    cur = None
    for line in text.splitlines():
        t = line.split()
        if not t:
            continue
        if t[0] == "v":
            verts.append((float(t[1]) / OBJ_SCALE, float(t[2]) / OBJ_SCALE, float(t[3]) / OBJ_SCALE))
        elif t[0] == "vt":
            uvs.append((float(t[1]), float(t[2])))
        elif t[0] == "usemtl":
            cur = t[1]
        elif t[0] == "f" and (mat is None or cur == mat):
            faces.append([tuple(int(a) - 1 for a in q.split("/")[:2]) for q in t[1:]])
    return verts, uvs, faces


def fan(faces):
    return [[f[0], f[k], f[k + 1]] for f in faces for k in range(1, len(f) - 1)]


def extent(spec, verts):
    lo = [min(p[k] for p in verts) for k in range(3)] if verts else [0.0] * 3
    hi = [max(p[k] for p in verts) for k in range(3)] if verts else [0.0] * 3
    return max(hi[k] - lo[k] for k in range(3)) * spec.get("extf", 0.06)


def pack(low, high, flags, size, margin, aa, ext):
    verts, uvs, faces = low
    polys, loops = [], []
    for f in faces:
        polys.append((len(loops), len(f)))
        for vi, ti in f:
            loops.append((vi, uvs[ti]))
    tris = []
    for pi, (s, n) in enumerate(polys):
        for k in range(1, n - 1):
            tris.append((s, s + k, s + k + 1, pi))
    edge = {}
    adj = [-1] * len(loops)
    for s, n in polys:
        for k in range(n):
            a, b = loops[s + k][0], loops[s + (k + 1) % n][0]
            e = (a, b) if a < b else (b, a)
            if e not in edge:
                edge[e] = s + k
            else:
                adj[s + k] = edge[e]
                adj[edge[e]] = s + k
    out = bytearray(b"TBK1")
    if isinstance(aa, tuple):
        ns, table = aa
        out += struct.pack("<iiiif", size, size, margin, flags | FLAG_AA_TABLE, ext)
        out += struct.pack("<i", ns)
        out += table
    else:
        out += struct.pack("<iiiif", size, size, margin, flags, ext)
        out += struct.pack("<i", len(aa))
        for x, y in aa:
            out += struct.pack("<ff", x, y)
    out += struct.pack("<i", len(verts))
    for p in verts:
        out += struct.pack("<fff", *p)
    out += struct.pack("<i", len(tris))
    for a, b, c, pi in tris:
        out += struct.pack("<iii", loops[a][0], loops[b][0], loops[c][0])
        for lp in (a, b, c):
            out += struct.pack("<ff", *loops[lp][1])
        out += struct.pack("<i", pi)
    out += struct.pack("<ii", len(polys), len(loops))
    for s, n in polys:
        out += struct.pack("<ii", s, n)
    for li, (vi, uv) in enumerate(loops):
        out += struct.pack("<iffi", vi, uv[0], uv[1], adj[li])
    out += pack_high(high)
    return bytes(out)


def pack_high(high):
    if "packed" in high:
        return high["packed"]
    out = bytearray()
    nv = len(high["P"]) // 3
    out += struct.pack("<i", nv)
    out += struct.pack("<%df" % (nv * 3), *high["P"])
    out += struct.pack("<%df" % (nv * 2), *high["UV"])
    out += struct.pack("<%df" % (nv * 4), *high["C"])
    out += struct.pack("<%df" % (nv * 3), *high["N"])
    tris, mats = high["T"], high["M"]
    out += struct.pack("<i", len(mats))
    for i in range(len(mats)):
        out += struct.pack("<iiii", tris[i * 3], tris[i * 3 + 1], tris[i * 3 + 2], mats[i])
    out += struct.pack("<i", len(high["mats"]))
    for m in high["mats"]:
        xf = m["xf"] or [0.0, 0.0, 1.0, 1.0]
        out += struct.pack("<iiiii", m["tex"], m["ws"], m["wt"], m["am"], 1 if m["xf"] else 0)
        out += struct.pack("<4f", *m["factor"])
        out += struct.pack("<f", m["cut"])
        out += struct.pack("<4f", *xf)
    out += struct.pack("<i", len(high["texs"]))
    for w, h, px in high["texs"]:
        out += struct.pack("<ii", w, h)
        out += bytes(px)
    high["packed"] = bytes(out)
    return high["packed"]


def load_aa_table(size):
    path = os.path.join(DATA_DIR, "bake_aa%d.bin" % size)
    if not os.path.exists(path):
        return AA
    with open(path, "rb") as f:
        raw = f.read()
    q = []
    for i in range(0, len(raw), 3):
        v = raw[i] << 16 | raw[i + 1] << 8 | raw[i + 2]
        q.extend((v >> 18, (v >> 12) & 63, (v >> 6) & 63, v & 63))
    vals = []
    for p in range(size * size):
        vals.extend((0.0, 0.0))
        for k in range(6):
            vals.append((q[p * 6 + k] + 0.5) / 64.0 - 0.5)
    return 4, struct.pack("<%df" % len(vals), *vals)


def reproject(dump, spec, obj_text, bin_dir, tw, th):
    data = model_bytes(dump, spec["src"])
    w, h, px = textures_gx.bti_levels(data, textures_gx.j3d_texture_headers(data)[0], False)[0]
    high = dict(build_high(dump, dict(spec, mode="parts"), bin_dir), texs=[(w, h, px)])
    high.pop("packed", None)
    flags = FLAG_REPROJECT | (FLAG_RAYCAST if spec.get("ray") else 0)
    low = parse_obj(obj_text)
    raw = run_core(bin_dir, pack(low, high, flags, tw, th, AA, extent(spec, low[0])), tw * th * 4)
    out = textures_ops.flip_rows(bytearray(raw), tw, th)
    y0, y1 = spec.get("copy", [0, 0])
    for y in range(y0, y1):
        for x in range(tw):
            k = (y * tw + x) * 4
            q = ((y % h) * w + x % w) * 4
            out[k:k + 4] = px[q:q + 4]
    return out


def mario_fire(dump, spec, obj_text, bin_dir, base):
    data = model_bytes(dump, spec["src"])
    normal = textures_tev.Model(data)
    fire = textures_tev.Model(data)
    path, frame = spec["btp"]
    for name, _, tex in btp_textures(dump.load(path), frame):
        for m in fire.materials:
            if m.name == name:
                fire.textures[m.tex[0]] = fire.textures[tex]
    model = Model(data)
    info = material_info(data)
    pos = model.arrays.get(9, [])
    tex_attrs = sorted(a for a in model.arrays if 13 <= a <= 20)
    pos_out, uv_out, tri_out, mat_out = [], [], [], []
    for mi, verts, tris in model.shapes:
        if mi is None or not tris:
            continue
        avail = [a for a in tex_attrs if all(a in att for _, att in verts)]
        want = 13 + max(info[mi]["uv_src"], 0)
        base_i = len(pos_out) // 3
        for drw, att in verts:
            p = pos[min(att.get(9, 0), len(pos) - 1)]
            pos_out.extend(mat_apply(model.vertex_matrix(drw), p))
            if want in avail:
                u = model.arrays[want][min(att[want], len(model.arrays[want]) - 1)]
                uv_out.extend((u[0], u[1]))
            else:
                uv_out.extend((0.0, 0.0))
        for a, b, c in tris:
            if a == b or b == c or a == c:
                continue
            tri_out.extend((base_i + a, base_i + c, base_i + b))
            mat_out.append(mi)
    high = dict(P=pos_out, UV=uv_out, C=[1.0] * (len(pos_out) // 3 * 4), N=[0.0] * len(pos_out), T=tri_out,
                M=[0] * len(mat_out), texs=[], mats=[PLAIN_MATERIAL])
    w, h, bpx = base
    verts, uvs, faces = parse_obj(obj_text, spec["mat"])
    payload = pack((verts, uvs, fan(faces)), high, FLAG_NEAR_MAP, w, h, AA, extent(spec, verts))
    raw = run_core(bin_dir, payload, None)
    n = struct.unpack_from("<i", raw, 0)[0]
    rec = struct.unpack_from("<%df" % (n * 5), raw, 4)
    planes = {}
    acc = [0.0] * (w * h * 4)
    cnt = [0] * (w * h)
    for r in range(n):
        pix, k, b0, b1, b2 = rec[r * 5:r * 5 + 5]
        pix, k = int(pix), int(k)
        mi = mat_out[k]
        if mi not in planes:
            hn, wn, nimg = textures_tev.effective(normal, mi)
            planes[mi] = (wn, hn, nimg, textures_tev.effective(fire, mi)[2])
        tw, th, nimg, fimg = planes[mi]
        a, b, c = tri_out[k * 3:k * 3 + 3]
        s0 = b0 * uv_out[a * 2] + b1 * uv_out[b * 2] + b2 * uv_out[c * 2]
        s1 = b0 * uv_out[a * 2 + 1] + b1 * uv_out[b * 2 + 1] + b2 * uv_out[c * 2 + 1]
        xx = min(tw - 1, max(0, int(s0 % 1 * tw)))
        yy = min(th - 1, max(0, int(s1 % 1 * th)))
        q = yy * tw + xx
        for ch in range(4):
            acc[pix * 4 + ch] += fimg[q][ch] - nimg[q][ch]
        cnt[pix] += 1
    out = bytearray(w * h * 4)
    for i in range(w * h):
        c = cnt[i] if cnt[i] > 0 else 1
        for ch in range(3):
            v = bpx[i * 4 + ch] / 255.0 + acc[i * 4 + ch] / c
            v = 0.0 if v < 0.0 else 1.0 if v > 1.0 else v
            out[i * 4 + ch] = round_half_even(v * 255)
        out[i * 4 + 3] = bpx[i * 4 + 3]
    return out


def nearest_fn(bin_dir, maxd):
    def near(tris, pts):
        nv = len(tris) * 3
        pos = [x for corners in tris for p in corners for x in p]
        high = dict(P=pos, UV=[0.0] * (nv * 2), C=[1.0] * (nv * 4), N=[0.0] * (nv * 3), T=list(range(nv)),
                    M=[0] * len(tris), texs=[], mats=[PLAIN_MATERIAL])
        raw = run_core(bin_dir, pack((pts, [], []), high, FLAG_POINT_NEAR, 0, 0, AA, maxd), len(pts) * 16)
        vals = struct.unpack("<%df" % (len(pts) * 4), raw)
        return [(int(vals[i * 4]), vals[i * 4 + 1], vals[i * 4 + 2]) for i in range(len(pts))]
    return near


def mario_bee(dump, spec, obj_text, bin_dir, base):
    bee = plain_high(dump, spec["src"], bin_dir)
    mario = plain_high(dump, spec["src2"], bin_dir)
    verts, uvs, faces = parse_obj(obj_text, spec["mat"])
    return textures_bake_bee.suit_texture(bee, mario, verts, uvs, fan(faces), spec["mat"], base,
                                     nearest_fn(bin_dir, 25.0))


def place(low, m, unit):
    verts, uvs, faces = low
    used = sorted(set(vi for f in faces for vi, _ in f))
    remap = {v: i for i, v in enumerate(used)}
    a, b, c = m[0][:3], m[1][:3], m[2][:3]
    cof = [[b[1] * c[2] - b[2] * c[1], c[1] * a[2] - c[2] * a[1], a[1] * b[2] - a[2] * b[1]],
           [b[2] * c[0] - b[0] * c[2], c[2] * a[0] - c[0] * a[2], a[2] * b[0] - a[0] * b[2]],
           [b[0] * c[1] - b[1] * c[0], c[0] * a[1] - c[1] * a[0], a[0] * b[1] - a[1] * b[0]]]
    det = a[0] * cof[0][0] + a[1] * cof[1][0] + a[2] * cof[2][0]
    out = []
    for v in used:
        w = [verts[v][k] * OBJ_SCALE * unit - m[k][3] for k in range(3)]
        out.append(tuple((cof[k][0] * w[0] + cof[k][1] * w[1] + cof[k][2] * w[2]) / det for k in range(3)))
    return out, uvs, [[(remap[vi], ti) for vi, ti in f] for f in faces]


def placement(dump, galaxy, zone, name, index):
    zm = zone_matrix(dump, "StageData/%s.arc" % galaxy, zone)
    b, i = placement_rows(dump, "StageData/%s.arc" % zone, "Placement", (name,))[index]
    wm = mtx_mul(zm, mtx_tr(row_vec(b, i, "pos_", 0.0), row_vec(b, i, "dir_", 0.0)))
    r = rotation(to_euler(wm))
    return [r[k] + [round_dec(wm[k][3], 3)] for k in range(3)]


def ff_wall(dump, spec, obj_text, bin_dir):
    tm = textures_tev.Model(model_bytes(dump, spec["src"]))
    regs = {m.name: (m.regs[0][:3], m.regs[1][:3]) for m in tm.materials if m.name in spec["only"]}
    excl = sorted(set(m.name for m in tm.materials if m.name not in spec["only"]))
    high = plain_high(dump, spec["src"], bin_dir, excl)
    galaxy, zone, name, index, unit = spec["placement"]
    low = parse_obj(obj_text, spec["mat"])
    verts, uvs, faces = place(low, placement(dump, galaxy, zone, name, index), unit)
    pts = [verts[vi] for vi, _ in faces[0]]
    uv = [uvs[ti] for _, ti in faces[0]]
    near = nearest_fn(bin_dir, 1e9)
    return textures_bake_ff.wall(high, regs, spec["gain"], pts, uv, near, spec.get("face", 0.01))


def jpa_texture(dump, spec, tw, th):
    res, texs = textures_bake_jpa.load(dump.load(spec["src"]))
    rid = spec["rid"]
    w, h, px = textures_gx.bti_levels(texs[res[rid]["tex"][0]], 0, False)[0]
    img = textures_bake_jpa.resize_rgba(px, w, h, tw, th)
    lo_id, lo_key = spec.get("lo", [rid, "env"])
    hi_id, hi_key = spec.get("hi", [rid, "prm"])
    lo = res[lo_id][lo_key]
    hi = res[hi_id][hi_key]
    out = bytearray(tw * th * 4)
    for i in range(tw * th):
        it = img[i * 4] / 255.0
        if spec.get("keep"):
            out[i * 4:i * 4 + 4] = img[i * 4:i * 4 + 4]
            continue
        for c in range(3):
            v = lo[c] * (1 - it) + hi[c] * it
            if spec.get("scale"):
                v = v * it
            v = 0.0 if v < 0 else 255.0 if v > 255 else v
            out[i * 4 + c] = int(v)
        out[i * 4 + 3] = 255 if it >= spec["thr"] else 0
    return out


def bilerp(tex, u, v, wrap):
    w, h, px = tex
    x = u * w - 0.5
    y = v * h - 0.5
    x0, y0 = int(x // 1), int(y // 1)
    fx, fy = x - x0, y - y0
    if wrap:
        xi, yi, xj, yj = x0 % w, y0 % h, (x0 + 1) % w, (y0 + 1) % h
    else:
        xi, yi = min(max(x0, 0), w - 1), min(max(y0, 0), h - 1)
        xj, yj = min(max(x0 + 1, 0), w - 1), min(max(y0 + 1, 0), h - 1)
    out = []
    for c in range(4):
        out.append(px[(yi * w + xi) * 4 + c] / 255.0 * (1 - fx) * (1 - fy)
                   + px[(yi * w + xj) * 4 + c] / 255.0 * fx * (1 - fy)
                   + px[(yj * w + xi) * 4 + c] / 255.0 * (1 - fx) * fy
                   + px[(yj * w + xj) * 4 + c] / 255.0 * fx * fy)
    return out


def nth_root(x, n):
    if x <= 0.0:
        return 0.0
    r = x if x > 1.0 else 1.0
    for _ in range(400):
        p = 1.0
        for _ in range(n - 1):
            p *= r
        nr = ((n - 1) * r + x / p) / n
        if nr >= r:
            break
        r = nr
    return r


def lab(rgb):
    lin = []
    for c in rgb:
        if c > 0.04045:
            t = (c + 0.055) / 1.055
            lin.append(t * t * nth_root(t * t, 5))
        else:
            lin.append(c / 12.92)
    mt = ((0.4124, 0.3576, 0.1805), (0.2126, 0.7152, 0.0722), (0.0193, 0.1192, 0.9505))
    ref = (0.9505, 1.0, 1.089)
    f = []
    for i in range(3):
        v = (mt[i][0] * lin[0] + mt[i][1] * lin[1] + mt[i][2] * lin[2]) / ref[i]
        f.append(nth_root(v, 3) if v > 0.008856 else 7.787 * v + 16 / 116)
    return (116 * f[1] - 16, 500 * (f[0] - f[1]), 200 * (f[1] - f[2]))


def palette_labs(pal, first):
    cols = [[pal[i * 4 + c] / 255.0 for c in range(3)] for i in range(first, len(pal) // 4)]
    return cols, [lab(c) for c in cols]


def quantize(rgb, pal, first):
    cols, labs = cached(("pal", bytes(pal), first), lambda: palette_labs(pal, first))
    q = lab(rgb)
    best, bi = None, 0
    for i, l in enumerate(labs):
        d0, d1, d2 = q[0] - l[0], (q[1] - l[1]) * 2.0, (q[2] - l[2]) * 2.0
        d = d0 * d0 + d1 * d1 + d2 * d2
        if best is None or d < best:
            best, bi = d, i
    return cols[bi]


def clamp01(v):
    return 0.0 if v < 0 else 1.0 if v > 1 else v


def power_star(dump, spec, tw, th, base):
    tm = textures_tev.Model(model_bytes(dump, spec["src"]))
    out = bytearray(tw * th * 4)
    layers = []
    for ti, scale, rot, tx, ty, wrap in spec["layers"]:
        s, c = sincos(rot / 32768 * PI)
        layers.append((tm.textures[ti], scale, c, s, tx, ty, wrap))
    m = None
    if spec.get("gmat"):
        m = [x for x in tm.materials if x.name == spec["gmat"]][0]
    for y in range(th):
        for x in range(tw):
            sx = (x + 0.5) / tw * 2 - 1
            sy = -((y + 0.5) / th * 2 - 1)
            r2 = sx * sx + sy * sy
            k = math.sqrt(0.995 / (r2 if r2 > 1e-9 else 1e-9)) if r2 > 0.995 else 1.0
            nx, ny = sx * k, sy * k
            hs = []
            for tex, scale, c, s, tx, ty, wrap in layers:
                u, v = 0.5 * nx, -0.5 * ny
                u, v = (c * u - s * v) * scale, (s * u + c * v) * scale
                hs.append(bilerp(tex, u + 0.5 + tx, v + 0.5 + ty, wrap))
            if spec["kind"] == "sum":
                col = [hs[0][i] + hs[1][i] for i in range(3)] + [1.0]
            elif spec["kind"] == "gain":
                col = [spec["gain"] * hs[0][i] for i in range(3)] + [1.0]
            else:
                c0, c1 = m.regs[0], m.regs[1]
                col = [c0[i] + hs[0][i] * c1[i] for i in range(3)]
                col.append(clamp01(c0[3] + hs[0][3]) * m.mat_color[3])
            o = (y * tw + x) * 4
            if spec.get("pal"):
                col = quantize([clamp01(v) for v in col[:3]], spec["pal"], 0) + col[3:]
            elif base is not None:
                col = quantize([clamp01(v) for v in col[:3]], base[2], 1) + col[3:]
            for i in range(3):
                out[o + i] = round_half_even(clamp01(col[i]) * 255)
            out[o + 3] = (255 if (x + y) % 2 == 0 else 0) if spec.get("stipple") else 255
    return out


def stalk(dump, spec, tw, th):
    w, h, px = textures_gx.bti_levels(dump.load(spec["src"]), 0, False)[0]
    bw, bh = w // tw, h // th
    ca, cb = spec["ramp"]
    out = bytearray(tw * th * 4)
    for j in range(th):
        for i in range(tw):
            u = (i + 0.5) / tw
            ramp = (ca * (1 - u) + cb * u) / 255.0
            for c in range(3):
                acc = 0.0
                for dy in range(bh):
                    for dx in range(bw):
                        acc += px[((j * bh + dy) * w + i * bw + dx) * 4 + c] / 255.0
                v = acc / (bw * bh) * ramp
                out[(j * tw + i) * 4 + c] = round_half_even(v * 255)
            out[(j * tw + i) * 4 + 3] = 255
    return out


def rock_channels(tex, tw, th):
    w, h, px = tex
    return [textures_bake_jpa.box_resize([px[i * 4 + c] for i in range(w * h)], w, h, tw, th)
            for c in range(3)]


def rock_ball(dump, spec):
    tm = textures_tev.Model(model_bytes(dump, spec["src"]))
    names = Model(model_bytes(dump, spec["src"])).texture_names
    texs = [tm.textures[names.index(n)] for n in spec["texs"]]
    red, gain = spec["red"], spec["gain"]
    base, lift = spec["ruby"]
    g = rock_channels(texs[0], 32, 24)
    r = rock_channels(texs[1], 32, 24)
    total = 0
    for c in range(3):
        for v in r[c]:
            total += v
    mean = total / (32 * 24 * 3)
    crystal = rock_channels(texs[2], 32, 8)[0]
    glint = rock_channels(texs[3], 32, 8)[0]
    out = bytearray(32 * 32 * 4)
    for i in range(32 * 24):
        for c in range(3):
            v = gain * g[c][i] * (r[c][i] / mean)
            out[i * 4 + c] = int(min(max(v, 0.0), 255.0))
        out[i * 4 + 3] = 255
    for i in range(32 * 8):
        k = (32 * 24 + i) * 4
        cr = crystal[i] / 255.0
        gl = glint[i] / 255.0
        for c in range(3):
            v = red[c] * (base + lift * cr) + 255 * gl
            out[k + c] = int(min(max(v, 0.0), 255.0))
        out[k + 3] = 255
    return out


def float_to_byte(v):
    v = textures_ops.f32(v)
    if v <= 0.0:
        return 0
    if v > textures_ops.TOP:
        return 255
    return int(textures_ops.f32(textures_ops.f32(textures_ops.F255 * v) + textures_ops.HALF))


def flat_mean(dump, spec, tw, th, base):
    tm, mi, regs = gx_material(dump, spec)
    col = [round_half_even(a * 255) for a in mean_colour(tm, mi, spec.get("stages", "auto"), regs, 3)]
    out = bytearray(tw * th * 4)
    for i in range(tw * th):
        out[i * 4:i * 4 + 3] = bytes(col)
        out[i * 4 + 3] = base[2][i * 4 + 3] if base else 255
    return out


def box_save(px, w, h, tw, th, spec):
    crop = spec.get("crop")
    return textures_ops.box_save(bytearray(px), w, h, tw, th, crop=crop, alpha_mode=spec.get("alpha", 0))


def material_texture(dump, spec, tw, th):
    tm, mi, regs = gx_material(dump, spec)
    if spec.get("raw"):
        first = material_info(model_bytes(dump, spec["src"]))[mi]["first"]
        w, h, px = tm.textures[tm.materials[mi].tex[first]]
    else:
        w, h, px = textures_tev.effective_rgba(tm, mi, "auto", regs.get(tm.materials[mi].name))
    return box_save(px, w, h, tw, th, spec)


def parts(dump, spec, obj_text, bin_dir):
    high = build_high(dump, spec, bin_dir)
    verts, uvs, faces = parse_obj(obj_text, spec["mat"])
    if spec.get("lowxf"):
        cx, cy, sx, sy, ty = spec["lowxf"]
        verts = [((p[0] - cx) * sx, (p[1] - cy) * sy + cy + ty, p[2]) for p in verts]
    if spec.get("flip"):
        faces = [f[::-1] for f in faces]
    if spec.get("pose"):
        rest = build_high(dump, dict(spec, pose=None), bin_dir)
        verts = textures_bake_parts.unpose(high, rest, verts)
        high = rest
    flags = FLAG_PARTS
    if spec.get("cut"):
        flags |= FLAG_CUT
    if spec.get("far"):
        flags |= FLAG_FAR
    extra = b""
    if spec.get("kmax"):
        key = ("parts", json.dumps([spec.get(k) for k in HIGH_KEYS]), spec["kmax"])
        ids = cached(key, lambda: textures_bake_parts.part_ids(high, spec["kmax"]))
        extra = struct.pack("<%di" % len(ids), *ids)
        flags |= FLAG_PART_IDS
    payload = pack((verts, uvs, faces), high, flags, 64, 0, AA, extent(spec, verts)) + extra
    raw = run_core(bin_dir, payload, 32 * 32 * 4)
    return textures_ops.flip_rows(bytearray(raw), 32, 32)


def bake(dump, spec, obj_text, bin_dir, tw, th):
    high = build_high(dump, spec, bin_dir)
    size = spec.get("bake", 64)
    verts, uvs, faces = parse_obj(obj_text)
    if spec.get("lowv"):
        verts = [tuple(c / OBJ_SCALE for c in p) for p in spec["lowv"]]
        faces = [f for f in faces if all(vi < len(verts) for vi, _ in f)]
    if spec.get("flip"):
        faces = [f[::-1] for f in faces]
    aa = cached(("aa", size), lambda: load_aa_table(size))
    payload = pack((verts, uvs, faces), high, 0, size, 4, aa, extent(spec, verts))
    raw = run_core(bin_dir, payload, size * size * 4)
    px = textures_ops.flip_rows(bytearray(raw), size, size)
    return textures_ops.box_save(px, size, size, tw, th, alpha_mode=spec.get("alpha", 0))


def stack(orig_root, spec, obj_text, bin_dir, base, tw, th):
    out = None
    for layer in spec["layers"]:
        sub = {k: v for k, v in spec.items() if k != "layers"}
        sub["mode"] = "parts"
        sub.update(layer)
        px = predict(orig_root, sub, obj_text, bin_dir, base)
        if out is None:
            out = bytearray(px)
            continue
        x0, y0, x1, y1 = layer.get("rect", [0, 0, tw, th])
        for y in range(y0, y1):
            for x in range(x0, x1):
                k = (y * tw + x) * 4
                if not layer.get("cut") or px[k + 3] >= 128:
                    out[k:k + 3] = px[k:k + 3]
    for k in range(3, len(out), 4):
        out[k] = 255
    return out


def predict(orig_root, spec, obj_text, bin_dir, base=None):
    dump = cached(("dump", orig_root), lambda: common.Dump(orig_root))
    mode = spec.get("mode", "bake")
    tw, th = spec.get("out", [32, 32])
    if mode == "tex":
        return material_texture(dump, spec, tw, th)
    if mode == "reproj":
        return reproject(dump, spec, obj_text, bin_dir, tw, th)
    if mode == "stalk":
        return stalk(dump, spec, tw, th)
    if mode == "pstar":
        return power_star(dump, spec, tw, th, base)
    if mode == "jpa":
        return jpa_texture(dump, spec, tw, th)
    if mode == "bti":
        w, h, px = textures_gx.bti_levels(dump.load(spec["src"]), 0, False)[0]
        return box_save(px, w, h, tw, th, spec)
    if mode == "mariofire":
        return mario_fire(dump, spec, obj_text, bin_dir, base)
    if mode == "mariobee":
        return mario_bee(dump, spec, obj_text, bin_dir, base)
    if mode == "beeparts":
        wing = plain_high(dump, spec["src"], bin_dir)
        body = plain_high(dump, spec["src2"], bin_dir)
        return textures_bake_bee.parts_atlas(wing, body, spec["signs"])
    if mode == "rockball":
        return rock_ball(dump, spec)
    if mode == "ffwall":
        return ff_wall(dump, spec, obj_text, bin_dir)
    if mode == "ffice":
        tm = textures_tev.Model(model_bytes(dump, spec["src"]))
        m = [x for x in tm.materials if x.name == spec["mat"]][0]
        return textures_bake_ff.ice(tm.textures[spec["tex"]], m.mat_color, m.konst[0], spec.get("scale", 2.0))
    if mode == "flatmean":
        return flat_mean(dump, spec, tw, th, base)
    if mode == "flat":
        tm, mi, regs = gx_material(dump, spec)
        col = mean_colour(tm, mi, "auto", regs, 4)
        return bytearray([float_to_byte(col[0]), float_to_byte(col[1]), float_to_byte(col[2]), 255] * tw * th)
    if mode == "stack":
        return stack(orig_root, spec, obj_text, bin_dir, base, tw, th)
    if mode == "parts":
        return parts(dump, spec, obj_text, bin_dir)
    return bake(dump, spec, obj_text, bin_dir, tw, th)
