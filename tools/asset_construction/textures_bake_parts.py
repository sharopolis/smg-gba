import math

from figures_j3d import mat_apply, mat_inv, mat_mul, round_half_even


def plain_sum(xs):
    r = 0.0
    for x in xs:
        r += x
    return r


def find_root(par, x):
    while par[x] != x:
        par[x] = par[par[x]]
        x = par[x]
    return x


def dominant_joints(model, drws):
    dom = []
    for d in drws:
        best = None
        for j, w in model.influences(d):
            if best is None or w > best[1] or (w == best[1] and j < best[0]):
                best = (j, w)
        dom.append(best[0])
    return dom


def part_ids(high, kmax):
    pos, tri = high["P"], high["T"]
    nv, nt = len(pos) // 3, len(tri) // 3
    if nt == 0:
        return []
    dom = dominant_joints(high["model"], high["D"])
    used = sorted(set(tri))
    lo = [min(pos[i * 3 + k] for i in used) for k in range(3)]
    hi = [max(pos[i * 3 + k] for i in used) for k in range(3)]
    diag = math.sqrt(plain_sum((hi[k] - lo[k]) * (hi[k] - lo[k]) for k in range(3)))
    eps = max(diag * 1e-4, 1e-3)
    keys, weld = {}, [0] * nv
    for i in range(nv):
        k = tuple(round_half_even(pos[i * 3 + c] / eps) for c in range(3))
        weld[i] = keys.setdefault(k, len(keys))
    par = list(range(len(keys)))
    for t in range(nt):
        a, b, c = tri[t * 3:t * 3 + 3]
        ra, rb = find_root(par, weld[a]), find_root(par, weld[b])
        par[rb] = ra
        par[find_root(par, weld[c])] = ra
    area, cent = [], []
    for t in range(nt):
        a, b, c = tri[t * 3:t * 3 + 3]
        pa, pb, pc = pos[a * 3:a * 3 + 3], pos[b * 3:b * 3 + 3], pos[c * 3:c * 3 + 3]
        u = [pb[k] - pa[k] for k in range(3)]
        v = [pc[k] - pa[k] for k in range(3)]
        cr = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
        area.append(0.5 * math.sqrt(cr[0] * cr[0] + cr[1] * cr[1] + cr[2] * cr[2]))
        cent.append([(pa[k] + pb[k] + pc[k]) / 3.0 for k in range(3)])
    parts = {}
    for t in range(nt):
        a, b, c = tri[t * 3:t * 3 + 3]
        bs = [dom[a], dom[b], dom[c]]
        bone = min(set(bs), key=lambda x: (-bs.count(x), x))
        parts.setdefault((find_root(par, weld[a]), bone), []).append(t)
    min_area = plain_sum(area) * 0.002

    def part_area(k):
        return plain_sum(area[t] for t in parts[k])

    def part_verts(ts):
        return set(weld[v] for t in ts for v in tri[t * 3:t * 3 + 3])

    def centre(k):
        return [plain_sum(cent[t][i] for t in parts[k]) / len(parts[k]) for i in range(3)]

    changed = True
    while changed:
        changed = False
        for k in sorted(parts, key=part_area):
            if len(parts) <= 1:
                break
            if part_area(k) >= min_area and len(parts[k]) >= 4:
                continue
            mine = part_verts(parts[k])
            best, bs = None, -1
            for k2, t2 in parts.items():
                if k2 == k or k2[0] != k[0]:
                    continue
                sh = len(mine & part_verts(t2))
                if sh > bs:
                    best, bs = k2, sh
            if best is None or bs == 0:
                if part_area(k) >= min_area * 0.25:
                    continue
                c0 = centre(k)

                def dist(k2):
                    c2 = centre(k2)
                    return math.sqrt(plain_sum((c2[i] - c0[i]) * (c2[i] - c0[i]) for i in range(3)))

                best = min((k2 for k2 in parts if k2 != k), key=dist)
            parts[best] += parts.pop(k)
            changed = True
            break
    plist = list(parts.items())
    while len(plist) > kmax:
        info = []
        for k, ts in plist:
            pts = [pos[v * 3:v * 3 + 3] for t in ts for v in tri[t * 3:t * 3 + 3]]
            mean = [plain_sum(p[i] for p in pts) / len(pts) for i in range(3)]
            ext = [max(p[i] for p in pts) - min(p[i] for p in pts) for i in range(3)]
            size = math.sqrt(ext[0] * ext[0] + ext[1] * ext[1] + ext[2] * ext[2])
            info.append((mean, size))
        best = None
        for i in range(len(plist)):
            for j in range(i + 1, len(plist)):
                dv = [info[i][0][k] - info[j][0][k] for k in range(3)]
                d = math.sqrt(dv[0] * dv[0] + dv[1] * dv[1] + dv[2] * dv[2]) + 0.5 * (info[i][1] + info[j][1])
                if plist[i][0][1] != plist[j][0][1]:
                    d *= 2.0
                if best is None or d < best[0]:
                    best = (d, i, j)
        _, i, j = best
        plist[i] = (plist[i][0], plist[i][1] + plist[j][1])
        plist.pop(j)
    out = [0] * nt
    for pi, (_, ts) in enumerate(plist):
        for t in ts:
            out[t] = pi
    return out


def unpose(high_pose, high_rest, verts):
    pos = high_pose["P"]
    cell = 4.0
    grid = {}
    for i in range(len(pos) // 3):
        key = (int(pos[i * 3] // cell), int(pos[i * 3 + 1] // cell), int(pos[i * 3 + 2] // cell))
        grid.setdefault(key, []).append(i)
    mats = {}
    out = []
    for p in verts:
        kx, ky, kz = int(p[0] // cell), int(p[1] // cell), int(p[2] // cell)
        best = None
        r = 0
        while best is None or r <= 1:
            for dx in range(-r, r + 1):
                for dy in range(-r, r + 1):
                    for dz in range(-r, r + 1):
                        if max(abs(dx), abs(dy), abs(dz)) != r:
                            continue
                        for i in grid.get((kx + dx, ky + dy, kz + dz), ()):
                            dx, dy, dz = pos[i * 3] - p[0], pos[i * 3 + 1] - p[1], pos[i * 3 + 2] - p[2]
                            d = dx * dx + dy * dy + dz * dz
                            if best is None or d < best[0]:
                                best = (d, i)
            r += 1
            if r > 64:
                break
        drw = high_pose["D"][best[1]]
        m = mats.get(drw)
        if m is None:
            m = mats[drw] = mat_mul(high_rest["vm"][drw], mat_inv(high_pose["vm"][drw]))
        out.append(mat_apply(m, p))
    return out
