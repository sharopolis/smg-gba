import math

from textures_bake_bee import TEX, cross


def wrap_index(i, n, mode):
    if mode == 1:
        return i % n
    if mode == 2:
        i = i % (2 * n)
        return 2 * n - 1 - i if i >= n else i
    return min(max(i, 0), n - 1)


def bilerp(tex, ws, wt, u, v):
    w, h, px = tex
    fx = u * w - 0.5
    fy = v * h - 0.5
    x0 = math.floor(fx)
    y0 = math.floor(fy)
    ax = fx - x0
    ay = fy - y0
    out = [0.0, 0.0, 0.0]
    for dy, wy in ((0, 1 - ay), (1, ay)):
        yy = wrap_index(y0 + dy, h, wt)
        for dx, wx in ((0, 1 - ax), (1, ax)):
            k = (yy * w + wrap_index(x0 + dx, w, ws)) * 4
            for c in range(3):
                out[c] = out[c] + px[k + c] * wx * wy
    return out


def edge(a, b, p):
    return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])


def sub(a, b):
    return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]


def interior(uv):
    area = edge(uv[0], uv[1], uv[2])
    pts, cells = [], []
    for y in range(TEX):
        for x in range(TEX):
            p = ((x + 0.5) / TEX, 1.0 - (y + 0.5) / TEX)
            e0 = edge(uv[1], uv[2], p) / area
            e1 = edge(uv[2], uv[0], p) / area
            e2 = edge(uv[0], uv[1], p) / area
            if e0 >= 0 and e1 >= 0 and e2 >= 0:
                pts.append((e0, e1, e2))
                cells.append(y * TEX + x)
    return pts, cells


def fill(rgb, cells):
    out = bytearray(TEX * TEX * 4)
    pos = [divmod(k, TEX) for k in cells]
    have = set(cells)
    for k in range(TEX * TEX):
        src = k
        if k not in have:
            y, x = divmod(k, TEX)
            best = None
            for q, (qy, qx) in zip(cells, pos):
                d = (qy - y) * (qy - y) + (qx - x) * (qx - x)
                if best is None or d <= best:
                    best, src = d, q
        out[k * 4:k * 4 + 3] = bytes(rgb[src])
        out[k * 4 + 3] = 255
    return out


def corner(pos, i):
    return (pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2])


def wall(high, regs, gain, quad, uv, near, face):
    bary, cells = interior(uv)
    n = cross(sub(quad[1], quad[0]), sub(quad[2], quad[0]))
    tri, pos = high["T"], high["P"]
    sel = []
    for k, mi in enumerate(high["M"]):
        a, b, c = tri[k * 3:k * 3 + 3]
        pa, pb, pc = corner(pos, a), corner(pos, b), corner(pos, c)
        nn = cross(sub(pb, pa), sub(pc, pa))
        d = -(nn[0] * n[0] + nn[1] * n[1] + nn[2] * n[2])
        if d > face * math.sqrt((nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2])
                                * (n[0] * n[0] + n[1] * n[1] + n[2] * n[2])):
            sel.append(((pa, pb, pc), (a, b, c), mi))
    pts = []
    for l0, l1, l2 in bary:
        pts.append(tuple(quad[0][i] * l0 + quad[1][i] * l1 + quad[2][i] * l2 for i in range(3)))
    res = near([t[0] for t in sel], pts)
    uvs, vcol = high["UV"], high["C"]
    rgb = {}
    for cell, (ti, bv, bw) in zip(cells, res):
        col = [0, 0, 0]
        if ti >= 0:
            _, idx, mi = sel[ti]
            w = (1 - bv - bw, bv, bw)
            u = uvs[idx[0] * 2] * w[0] + uvs[idx[1] * 2] * w[1] + uvs[idx[2] * 2] * w[2]
            v = uvs[idx[0] * 2 + 1] * w[0] + uvs[idx[1] * 2 + 1] * w[1] + uvs[idx[2] * 2 + 1] * w[2]
            vc = [vcol[idx[0] * 4 + c] * w[0] + vcol[idx[1] * 4 + c] * w[1] + vcol[idx[2] * 4 + c] * w[2]
                  for c in range(3)]
            m = high["mats"][mi]
            s = bilerp(high["texs"][m["tex"]], m["ws"], m["wt"], u, v)
            c0, c1 = regs[m["name"]]
            for c in range(3):
                f = gain * (c1[c] + (c0[c] - c1[c]) * vc[c])
                col[c] = min(max(math.floor(min(max(s[c] * f, 0.0), 255.0) + 0.5), 0), 255)
        rgb[cell] = col
    return fill(rgb, cells)


def ice(tex, matcol, konst, scale):
    w, h, px = tex
    out = bytearray(TEX * TEX * 4)
    for y in range(TEX):
        for x in range(TEX):
            k = (math.floor((y + 0.5) * h / TEX) * w + math.floor((x + 0.5) * w / TEX)) * 4
            for c in range(3):
                v = scale * (px[k + c] / 255.0 * matcol[c] + konst[c] - 0.5)
                out[(y * TEX + x) * 4 + c] = min(max(math.floor(v * 255.0 + 0.5), 0), 255)
            out[(y * TEX + x) * 4 + 3] = 255
    return out
