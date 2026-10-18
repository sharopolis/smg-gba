import math

from figures_j3d import PI, sincos
from textures_bake_parts import plain_sum
from common import cross, dot

SUIT = ("Body1_v", "Cap_v", "FurCollar_v")
TEX = 32


def triangles(high):
    tris = []
    pos, uvs, mats = high["P"], high["UV"], high["M"]
    for k in range(len(mats)):
        idx = high["T"][k * 3:k * 3 + 3]
        corners = [(pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]) for i in idx]
        uv = [(uvs[i * 2], 1.0 - uvs[i * 2 + 1]) for i in idx]
        tris.append((corners, uv, mats[k]))
    return tris


def wrap_coord(x, mode):
    if mode == 2:
        x = x % 2.0
        return 2.0 - x if x > 1.0 else x
    if mode == 0:
        return 0.0 if x < 0.0 else 1.0 if x > 1.0 else x
    return x % 1.0


def sample(high, mi, uv):
    m = high["mats"][mi]
    if m["tex"] < 0:
        return None
    w, h, px = high["texs"][m["tex"]]
    mode = m["ws"] if m["ws"] == m["wt"] else 0
    x = min(int(wrap_coord(uv[0], mode) * w), w - 1)
    y = min(int(wrap_coord(uv[1], mode) * h), h - 1)
    k = ((h - 1 - y) * w + x) * 4
    return [px[k + c] / 255.0 for c in range(4)]


def antenna(high, t):
    return high["mats"][t[2]]["name"] == "Cap_v" and (t[1][0][0] + t[1][1][0] + t[1][2][0]) / 3.0 < 0.45


def samples(verts, uvs, tris):
    pts, cells = [], []
    n = TEX * 4
    for a, b, c in tris:
        ua, ub, uc = uvs[a[1]], uvs[b[1]], uvs[c[1]]
        pa, pb, pc = verts[a[0]], verts[b[0]], verts[c[0]]
        den = (ub[1] - uc[1]) * (ua[0] - uc[0]) + (uc[0] - ub[0]) * (ua[1] - uc[1])
        if abs(den) < 1e-12:
            continue
        lo = [math.floor(min(ua[k], ub[k], uc[k]) * n) for k in range(2)]
        hi = [math.ceil(max(ua[k], ub[k], uc[k]) * n) for k in range(2)]
        for sy in range(lo[1], hi[1] + 1):
            for sx in range(lo[0], hi[0] + 1):
                u, v = (sx + 0.5) / n, (sy + 0.5) / n
                l1 = ((ub[1] - uc[1]) * (u - uc[0]) + (uc[0] - ub[0]) * (v - uc[1])) / den
                l2 = ((uc[1] - ua[1]) * (u - uc[0]) + (ua[0] - uc[0]) * (v - uc[1])) / den
                l3 = 1 - l1 - l2
                if min(l1, l2, l3) < -1e-6:
                    continue
                pts.append(tuple(pa[k] * l1 + pb[k] * l2 + pc[k] * l3 for k in range(3)))
                tx = math.floor(u * TEX) % TEX
                ty = TEX - 1 - math.floor(v * TEX) % TEX
                cells.append(ty * TEX + tx)
    return pts, cells


def transfer(high, sel, pts, cells, near):
    res = near([t[0] for t in sel], pts)
    acc = [[0.0] * 4 for _ in range(TEX * TEX)]
    cnt = [0] * (TEX * TEX)
    suit = [0] * (TEX * TEX)
    for i, (ti, bv, bw) in enumerate(res):
        if ti < 0:
            continue
        _, uv, mi = sel[ti]
        l0 = 1 - bv - bw
        tuv = (uv[0][0] * l0 + uv[1][0] * bv + uv[2][0] * bw, uv[0][1] * l0 + uv[1][1] * bv + uv[2][1] * bw)
        c = sample(high, mi, tuv)
        if c is None:
            continue
        k = cells[i]
        for ch in range(4):
            acc[k][ch] += c[ch]
        cnt[k] += 1
        suit[k] += high["mats"][mi]["name"] in SUIT
    col = []
    for k in range(TEX * TEX):
        col.append([acc[k][ch] / cnt[k] for ch in range(4)] if cnt[k] else [0.0] * 4)
    return col, [c > 0 for c in cnt], [suit[k] / max(cnt[k], 1) for k in range(TEX * TEX)]


def suit_colour(px, which):
    r, g, b = px[0], px[1], px[2]
    mx, mn = max(r, g, b), min(r, g, b)
    sat = (mx - mn) / max(mx, 1e-6) if mx > 0 else 0.0
    if which == "red":
        return sat > 0.5 and r == mx and r > 0.35 and abs(g - b) < 0.12
    return sat > 0.4 and b == mx and b > r + 0.1


def suit_texture(high_bee, high_mar, verts, uvs, tris, mname, base, near):
    pts, cells = samples(verts, uvs, tris)
    bee = triangles(high_bee)
    body = [t for t in bee if not antenna(high_bee, t)]
    suit = [t for t in body if high_bee["mats"][t[2]]["name"] in SUIT]
    mc, mh, ms = transfer(high_mar, triangles(high_mar), pts, cells, near)
    bs = transfer(high_bee, body, pts, cells, near)[2]
    sc, sh, _ = transfer(high_bee, suit, pts, cells, near)
    bpx = base[2]
    g = [[bpx[k * 4 + c] / 255.0 for c in range(4)] for k in range(TEX * TEX)]
    force = [suit_colour(g[k], "red" if mname == "marioface" else "blue") for k in range(TEX * TEX)]
    rep = []
    for k in range(TEX * TEX):
        diff = max(abs(sc[k][c] - mc[k][c]) for c in range(3)) * 255
        rep.append(sh[k] and mh[k] and (ms[k] >= 0.5 or bs[k] >= 0.5 or force[k]) and diff > 60)
    for k in range(TEX * TEX):
        if rep[k]:
            g[k][:3] = sc[k][:3]
    rk = [k for k in range(TEX * TEX) if rep[k]]
    for k in range(TEX * TEX):
        if force[k] and not rep[k] and rk:
            y, x = divmod(k, TEX)
            best, bk = None, rk[0]
            for q in rk:
                qy, qx = divmod(q, TEX)
                d = (qy - y) * (qy - y) + (qx - x) * (qx - x)
                if best is None or d < best:
                    best, bk = d, q
            g[k][:3] = g[bk][:3]
    out = bytearray(TEX * TEX * 4)
    for k in range(TEX * TEX):
        for c in range(4):
            out[k * 4 + c] = min(max(math.floor(g[k][c] * 255 + 0.5), 0), 255)
    return out


def eigen3(m):
    a = [row[:] for row in m]
    v = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]
    for _ in range(100):
        off = a[0][1] * a[0][1] + a[0][2] * a[0][2] + a[1][2] * a[1][2]
        if off < 1e-30:
            break
        for p, q in ((0, 1), (0, 2), (1, 2)):
            if abs(a[p][q]) < 1e-300:
                continue
            theta = (a[q][q] - a[p][p]) / (2 * a[p][q])
            t = (1.0 if theta >= 0 else -1.0) / (abs(theta) + math.sqrt(theta * theta + 1))
            c = 1 / math.sqrt(t * t + 1)
            s = t * c
            for k in range(3):
                akp, akq = a[k][p], a[k][q]
                a[k][p], a[k][q] = c * akp - s * akq, s * akp + c * akq
            for k in range(3):
                apk, aqk = a[p][k], a[q][k]
                a[p][k], a[q][k] = c * apk - s * aqk, s * apk + c * aqk
            for k in range(3):
                vkp, vkq = v[k][p], v[k][q]
                v[k][p], v[k][q] = c * vkp - s * vkq, s * vkp + c * vkq
    order = sorted(range(3), key=lambda i: -a[i][i])
    return [[v[k][i] for k in range(3)] for i in order]


def normalize(x):
    d = math.sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2])
    return [x[0] / d, x[1] / d, x[2] / d]


def round6(x):
    s = x * 1000000.0
    f = math.floor(s)
    r = s - f
    if r > 0.5 or (r == 0.5 and f % 2 == 1):
        f += 1
    return f / 1000000.0


def convex_hull(pts):
    pts = sorted(set((round6(p[0]), round6(p[1])) for p in pts))
    if len(pts) < 3:
        return pts

    def turn(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lo, hi = [], []
    for p in pts:
        while len(lo) >= 2 and turn(lo[-2], lo[-1], p) <= 0:
            lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(hi) >= 2 and turn(hi[-2], hi[-1], p) <= 0:
            hi.pop()
        hi.append(p)
    return lo[:-1] + hi[:-1]


def enclosing_triangle(h):
    nrm = []
    for i in range(len(h)):
        e = (h[(i + 1) % len(h)][0] - h[i][0], h[(i + 1) % len(h)][1] - h[i][1])
        d = math.sqrt(e[0] * e[0] + e[1] * e[1])
        if d > 1e-9:
            nrm.append((e[1] / d, -e[0] / d))
    for k in range(72):
        s, c = sincos(k * PI / 36)
        nrm.append((c, s))
    dist = [max(p[0] * n[0] + p[1] * n[1] for p in h) for n in nrm]
    best, barea = None, 1e18
    count = len(nrm)

    def solve(a, b):
        det = nrm[a][0] * nrm[b][1] - nrm[a][1] * nrm[b][0]
        return ((dist[a] * nrm[b][1] - nrm[a][1] * dist[b]) / det,
                (nrm[a][0] * dist[b] - dist[a] * nrm[b][0]) / det)

    for i in range(count):
        for j in range(i + 1, count):
            cij = nrm[i][0] * nrm[j][1] - nrm[i][1] * nrm[j][0]
            if abs(cij) < 1e-6:
                continue
            for k in range(j + 1, count):
                cjk = nrm[j][0] * nrm[k][1] - nrm[j][1] * nrm[k][0]
                cki = nrm[k][0] * nrm[i][1] - nrm[k][1] * nrm[i][0]
                same_sign = ((cij > 1e-6 and cjk > 1e-6 and cki > 1e-6)
                             or (cij < -1e-6 and cjk < -1e-6 and cki < -1e-6))
                if not same_sign:
                    continue
                tri = [solve(i, j), solve(j, k), solve(k, i)]
                area = abs((tri[1][0] - tri[0][0]) * (tri[2][1] - tri[0][1])
                           - (tri[1][1] - tri[0][1]) * (tri[2][0] - tri[0][0])) / 2
                if area < barea:
                    best, barea = tri, area
    return best


def part_tile(high, tris, tile, normal, signs, alpha_cut, thicken):
    pts = [p for t in tris for p in t[0]]
    c = [plain_sum(p[k] for p in pts) / len(pts) for k in range(3)]
    cov = [[plain_sum((p[i] - c[i]) * (p[j] - c[j]) for p in pts) for j in range(3)] for i in range(3)]
    vt = eigen3(cov)
    vt0 = [x * signs[0] for x in vt[0]]
    n = normalize(normal) if normal is not None else [x * signs[1] for x in vt[2]]
    ax = normalize([vt0[k] - n[k] * dot(vt0, n) for k in range(3)])
    ay = cross(n, ax)

    def to2(q):
        return ((q[0] - c[0]) * ax[0] + (q[1] - c[1]) * ax[1] + (q[2] - c[2]) * ax[2],
                (q[0] - c[0]) * ay[0] + (q[1] - c[1]) * ay[1] + (q[2] - c[2]) * ay[2])

    tri2 = enclosing_triangle(convex_hull([to2(p) for p in pts]))
    cen = ((tri2[0][0] + tri2[1][0] + tri2[2][0]) / 3, (tri2[0][1] + tri2[1][1] + tri2[2][1]) / 3)
    tri2 = [(cen[0] + (q[0] - cen[0]) * 1.04, cen[1] + (q[1] - cen[1]) * 1.04) for q in tri2]
    w, h = tile[2:]
    lo2 = (min(q[0] for q in tri2), min(q[1] for q in tri2))
    hi2 = (max(q[0] for q in tri2), max(q[1] for q in tri2))
    sub = 6
    gw, gh = w * sub, h * sub
    best = [-1e18] * (gw * gh)
    col = [None] * (gw * gh)
    for pos, uv, mi in tris:
        t2 = [to2(p) for p in pos]
        d = (t2[1][1] - t2[2][1]) * (t2[0][0] - t2[2][0]) + (t2[2][0] - t2[1][0]) * (t2[0][1] - t2[2][1])
        if abs(d) < 1e-12:
            continue
        dn = [dot(p, n) for p in pos]
        for gy in range(gh):
            qy = hi2[1] - (gy + 0.5) / gh * (hi2[1] - lo2[1])
            for gx in range(gw):
                qx = lo2[0] + (gx + 0.5) / gw * (hi2[0] - lo2[0])
                l1 = ((t2[1][1] - t2[2][1]) * (qx - t2[2][0]) + (t2[2][0] - t2[1][0]) * (qy - t2[2][1])) / d
                l2 = ((t2[2][1] - t2[0][1]) * (qx - t2[2][0]) + (t2[0][0] - t2[2][0]) * (qy - t2[2][1])) / d
                l3 = 1 - l1 - l2
                if l1 < 0 or l2 < 0 or l3 < 0:
                    continue
                depth = l1 * dn[0] + l2 * dn[1] + l3 * dn[2]
                k = gy * gw + gx
                if depth <= best[k]:
                    continue
                u = (uv[0][0] * l1 + uv[1][0] * l2 + uv[2][0] * l3,
                     uv[0][1] * l1 + uv[1][1] * l2 + uv[2][1] * l3)
                c4 = sample(high, mi, u) or [1.0, 1.0, 1.0, 1.0]
                if alpha_cut > 0 and c4[3] < alpha_cut:
                    continue
                col[k] = c4
                best[k] = depth
    out = []
    for ty in range(h):
        for tx in range(w):
            acc = [0.0, 0.0, 0.0]
            nc = 0
            for sy in range(sub):
                for sx in range(sub):
                    k = (ty * sub + sy) * gw + tx * sub + sx
                    if best[k] > -1e17:
                        for ch in range(3):
                            acc[ch] += col[k][ch]
                        nc += 1
            covf = nc / (sub * sub)
            rgb = [a / nc for a in acc] if nc else [0.0, 0.0, 0.0]
            solid = covf >= 0.5 or (thicken and nc > 0)
            out.append(rgb + [1.0 if solid else 0.0])
    return out


PARTS = (("wingL", "w", "WingMat2", None, (0, 0, 16, 16), 0.5, 0),
         ("wingR", "w", "WingMat2(2)", None, (16, 0, 16, 16), 0.5, 0),
         ("wingfast", "w", "WingMat", None, (0, 16, 20, 16), 0.0, 0),
         ("antennaL", "b", 1, (0.7071, 0.0, -0.7071), (20, 16, 6, 16), 0.5, 1),
         ("antennaR", "b", -1, (-0.7071, 0.0, -0.7071), (26, 16, 6, 16), 0.5, 1))


def parts_atlas(high_wing, high_bee, signs):
    wing = triangles(high_wing)
    bee = triangles(high_bee)
    out = bytearray(TEX * TEX * 4)
    for name, which, sel, normal, tile, cut, thick in PARTS:
        if which == "w":
            high = high_wing
            tris = [t for t in wing if high_wing["mats"][t[2]]["name"] == sel]
        else:
            high = high_bee
            tris = [t for t in bee
                    if antenna(high_bee, t) and (t[0][0][0] + t[0][1][0] + t[0][2][0]) * sel > 0]
        if not tris:
            continue
        cols = part_tile(high, tris, tile, normal, signs.get(name, [1, 1]), cut, thick)
        x0, y0, w, h = tile
        for ty in range(h):
            for tx in range(w):
                c = cols[ty * w + tx]
                k = ((y0 + ty) * TEX + x0 + tx) * 4
                for ch in range(4):
                    out[k + ch] = min(max(math.floor(c[ch] * 255 + 0.5), 0), 255)
    return out
