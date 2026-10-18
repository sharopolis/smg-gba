import math
import struct

from actor_headers_lib import (BCK, BCSV, KCL, PI, Model, cos, cross, dot, gba, mtx_apply, mtx_col, rint, sin,
                               sub)
from actor_headers_placement_galaxy import galaxy
from common import f32, fsum

WATER_STAGES = ["EggStarGalaxy", "HeavenlyBeachGalaxy", "IceVolcanoGalaxy", "OceanPhantomCaveGalaxy",
                "OceanFloaterLandGalaxy"]
WATER_COMPILER_STAGES = WATER_STAGES[:4]
WATER_KINDS = ["WaterCube", "WaterCylinder", "WaterSphere", "OceanBowl", "OceanSphere"]
BUOY_JOINTS = [("ScrewSwitch", "screwswitchon", "Screw", 1),
               ("ScrewSwitch", "screwswitchon", "ScrewReceive", 0),
               ("ScrewSwitchReverse", "screwswitchreverseon", "Screw", 1),
               ("ValveSwitch", "valveswitchon", "Valve", 1),
               ("ValveSwitch", "valveswitchreverseon", "Valve", 1)]
ROTATE_FIELDS = ["MoveConditionType", "RotateSpeed", "RotateAngle", "RotateAxis"]
SURFACE = 30.0
SPLIT = (2, 4, 1)
CELL = 64.0
RAY_STEP = 1e-4 * 256


def gi(v, s=1.0):
    return [rint(x * s) for x in gba(v)]


def gfx(v):
    out = []
    for x in gba(v):
        g = f32(float("%.6f" % (x / 256.0)))
        out.append(int(f32(g * 256.0)))
    return out


def mask_of(scenarios):
    m = 0
    for n in scenarios:
        m |= 1 << (n - 1)
    return m


def hex_digits(v):
    return int("%x" % v)


def switch_key(g, o, sw):
    return sw if sw >= 1000 else 2000 + 1000 * g.zone_list.index(o.zone) + sw


def pose(o):
    return gi(o.pos) + gi(mtx_col(o.m, 1), 16384) + gi(mtx_col(o.m, 2), 16384)


def add(a, b):
    return [a[k] + b[k] for k in range(3)]


def scale(a, s):
    return [a[k] * s for k in range(3)]


def length(a):
    return math.sqrt(dot(a, a))


def unit(a):
    return scale(a, 1.0 / length(a))


def bezier(cp, t):
    u = 1.0 - t
    return [u * u * u * cp[0][k] + 3 * u * u * t * cp[1][k] + 3 * u * t * t * cp[2][k] + t * t * t * cp[3][k]
            for k in range(3)]


def rail_nearest(rail, p):
    pts = rail.points
    n = len(pts)
    segs = n if rail.closed == "CLOSE" else n - 1
    acc = 0.0
    best = None
    prev = None
    for i in range(segs):
        a, b = pts[i], pts[(i + 1) % n]
        cp = [a["pos"], a["ctrl_out"], b["ctrl_in"], b["pos"]]
        for k in range(0 if i == 0 else 1, 4097):
            q = bezier(cp, k / 4096)
            if prev is not None:
                acc += length(sub(q, prev))
            prev = q
            d = dot(sub(q, p), sub(q, p))
            if best is None or d < best[0]:
                best = (d, acc)
    return int(best[1])


def water_rows(ctx):
    rows = []
    spans = []
    for g in WATER_STAGES:
        first = len(rows)
        for o in galaxy(ctx, g).objects:
            if o.name not in WATER_KINDS:
                continue
            n = o.name
            if n == "WaterCube":
                mul = [500, 1000, 500]
            elif n in ("WaterCylinder", "WaterSphere"):
                mul = [500] * 3
            else:
                mul = [100] * 3
            cold = 1 if len(o.args) > 1 and o.args[1] == 1 else 0
            basis = []
            for i in range(3):
                basis += gi(mtx_col(o.m, i), 16384)
            dims = [rint(o.scale[k] * mul[k]) for k in range(3)]
            rows.append((g, mask_of(o.scenarios), cold, [WATER_KINDS.index(n)] + gi(o.pos) + basis + dims))
        spans += [first, len(rows) - first]
    return rows, spans


def water_runtime(ctx, spec):
    rows, spans = water_rows(ctx)
    out = []
    for _, mask, cold, vals in rows:
        out += vals[:1] + [mask, cold] + vals[1:]
    return out + spans


def water_compiler(ctx, spec):
    rows, _ = water_rows(ctx)
    out = []
    for g, _, _, vals in rows:
        if g in WATER_COMPILER_STAGES:
            out += vals
    return out


def deep_tables(ctx, spec):
    g = galaxy(ctx, "OceanPhantomCaveGalaxy")
    out = []
    for o in g.objects:
        if o.name != "GlaringLightAreaCylinder" or o.zone != "GhostShipCaveZone" or "SW_A" not in o.switches:
            continue
        out += gi(o.pos) + gi(mtx_col(o.m, 1), 16384)
        out += [rint(500 * o.scale[0]), rint(500 * o.scale[1]), switch_key(g, o, o.switches["SW_A"])]
    multi = g.find("ArrowSwitchMulti")[0]
    sensor = [multi.pos[i] + dot(multi.m[i][:3], [0, 50, 250]) for i in range(3)]
    out += gi(sensor)
    targets = [-1] * 4
    for o in g.find("ArrowSwitchTarget"):
        if o.args[0] == multi.args[0]:
            targets[o.args[1]] = switch_key(g, o, o.switches["SW_A"])
    out += targets
    for o in g.find("ChangeBgmCube"):
        if o.args[0] != 0:
            continue
        out += gi(o.pos)
        for k in range(3):
            out += gi(mtx_col(o.m, k), 16384)
        out += [rint(500 * o.scale[0]), rint(1000 * o.scale[1]), rint(500 * o.scale[2]), o.args[1],
                mask_of(o.scenarios)]
    return out


def purple_rail(ctx, name):
    coins = galaxy(ctx, name).find("PurpleRailCoin")
    mask = 0
    for o in coins:
        mask |= mask_of(o.scenarios)
    return [fsum(o.args[0] for o in coins), hex_digits(mask)] + [o.index for o in coins]


def deep_purple_rail(ctx, spec):
    return purple_rail(ctx, "OceanPhantomCaveGalaxy")


def honey_rail(ctx, spec):
    return purple_rail(ctx, "HoneyBeeKingdomGalaxy")


def gusty_gold(ctx, spec):
    g = galaxy(ctx, "CosmosGardenGalaxy")
    out = []
    for o in g.find("WanwanRollingGold"):
        out += [o.rail, rail_nearest(g.rails[o.rail], o.pos)] + gfx(o.pos)
    return out


def gusty_star_appear(ctx, spec):
    g = galaxy(ctx, "CosmosGardenGalaxy")
    out = []
    for s in g.find("PowerStar"):
        group = s.row.get("GroupId", -1)
        if group is None or group < 0:
            continue
        for p in g.find("PowerStarAppearPoint", s.zone):
            if p.zone_placement != s.zone_placement or p.row.get("GroupId", -1) != group:
                continue
            m = mask_of(s.scenarios) & mask_of(p.scenarios)
            if not m:
                continue
            out += [s.index, p.index, g.zone_list.index(s.zone), hex_digits(m)] + gfx(p.pos)
    return out


def buoy_penguin(ctx, spec):
    g = galaxy(ctx, "OceanFloaterLandGalaxy")
    out = []
    for o in g.find("Penguin"):
        if o.rail is not None:
            out += [o.index, rail_nearest(g.rails[o.rail], o.pos)]
    return out


def same_zone(a, b):
    return a.zone == b.zone and a.zone_placement == b.zone_placement


def iceking(ctx, spec):
    g = galaxy(ctx, "IceVolcanoGalaxy")
    boss = g.find("IceMerameraKing")[0]
    children = [o for o in g.find("ChildIceMeramera")
                if same_zone(o, boss) and o.row.get("ParentID") == boss.l_id]
    named = []
    for key in ["\u30e1\u30e9\u30ad\u30f3\u30c7\u30e2\u5f8c\u30bb\u30c3\u30c8\u4f4d\u7f6e",
                "\u30e1\u30e9\u30ad\u30f3\u30de\u30ea\u30aa\u518d\u30bb\u30c3\u30c8\u4f4d\u7f6e"]:
        named += [o for o in g.find("GeneralPos") if same_zone(o, boss) and o.row.get("PosName") == key]
    demo = [o for o in g.find("DemoGroup") if same_zone(o, boss) and
            o.row.get("DemoName") == "\u30e1\u30e9\u30ad\u30f3\u30aa\u30fc\u30d7\u30cb\u30f3\u30b0"][0]
    out = [switch_key(g, demo, demo.switches["SW_APPEAR"]), switch_key(g, boss, boss.switches["SW_A"])]
    for o in [boss] + named + children:
        out += pose(o)
    return out


def penguin_skater(ctx, spec):
    g = galaxy(ctx, "IceVolcanoGalaxy")
    out = []
    count = 0
    for o in g.find("PenguinSkater"):
        if o.rail is None:
            continue
        group = o.row.get("GroupId", -1)
        members = [q for q in g.find("NPCSupportRail")
                   if same_zone(q, o) and q.row.get("GroupId", -1) == group]
        loops = [q.rail for q in members if g.rails[q.rail].closed == "CLOSE"]
        connectors = [q.rail for q in members if g.rails[q.rail].closed != "CLOSE"]
        named = {}
        for q in g.find("GeneralPos"):
            if same_zone(q, o):
                named[q.row.get("PosName")] = q
        pp = pose(named["\u30da\u30f3\u30ae\u30f3\u79fb\u52d5\u5f8c"])
        mp = pose(named["\u30de\u30ea\u30aa\u79fb\u52d5\u5f8c"])
        out += [o.rail, loops[0], len(connectors)] + connectors + [-1] * (16 - len(connectors))
        out += gi(o.pos) + pp[:3] + mp[:3] + pp[3:6] + pp[6:] + mp[3:6] + mp[6:]
        count += 1
    return out + [count]


def brlan_pai1(data):
    hs, n = struct.unpack_from(">HH", data, 12)
    o = hs
    for _ in range(n):
        size = struct.unpack_from(">I", data, o + 4)[0]
        if data[o:o + 4] == b"pai1":
            return o
        o += size
    raise KeyError("pai1")


def brlan_frames(data):
    return struct.unpack_from(">H", data, brlan_pai1(data) + 8)[0]


def brlan_visibility_keys(data, pane):
    o = brlan_pai1(data)
    na, eo = struct.unpack_from(">HI", data, o + 14)
    for off in struct.unpack_from(">%dI" % na, data, o + eo):
        p = o + off
        if data[p:p + 20].split(b"\0")[0] != pane:
            continue
        ntags = data[p + 20]
        for to in struct.unpack_from(">%dI" % ntags, data, p + 24):
            q = p + to
            if data[q:q + 4] != b"RLVI":
                continue
            for go in struct.unpack_from(">%dI" % data[q + 4], data, q + 8):
                r = q + go
                nk, ko = struct.unpack_from(">H2xI", data, r + 4)
                return [(struct.unpack_from(">f", data, r + ko + k * 8)[0],
                         struct.unpack_from(">H", data, r + ko + k * 8 + 4)[0]) for k in range(nk)]
    raise KeyError(pane)


def bck_frames(ctx, arc, name):
    return BCK(ctx.arc_file(arc, name)).frame_count


def brk_frames(ctx, arc, name):
    data = ctx.arc_file(arc, name)
    o = 0x20
    while data[o:o + 4] != b"TRK1":
        o += struct.unpack_from(">I", data, o + 4)[0]
    return struct.unpack_from(">H", data, o + 10)[0]


def syati(ctx, spec):
    g = galaxy(ctx, "OceanPhantomCaveGalaxy")
    o = g.find("Syati")[0]
    arc = "ObjectData/Syati.arc"
    model = Model(ctx.arc_file(arc, "syati.bdl"))
    head = mtx_col(model.pose(BCK(ctx.arc_file(arc, "star.bck")), 0)[model.joint("Head")], 3)
    race = ctx.arc_file("LayoutData/Race.arc", "count.brlan")
    go = [int(f) for f, v in brlan_visibility_keys(race, b"CountGo") if v]
    ring = "ObjectData/PrizeRing.arc"
    out = [o.rail, o.args[0], o.args[3]] + gi(o.pos) + gi(mtx_col(o.m, 1), 16384) + gi(mtx_col(o.m, 2), 16384)
    out += [rint(v) for v in head]
    out += [bck_frames(ctx, arc, "turnreverse.bck"), bck_frames(ctx, arc, "screw.bck"),
            bck_frames(ctx, arc, "turn.bck"), bck_frames(ctx, ring, "start.bck"),
            bck_frames(ctx, ring, "end.bck"), brk_frames(ctx, ring, "timeout.brk"), go[0], brlan_frames(race)]
    return out


def rabbit_graph(ctx, spec):
    g = galaxy(ctx, "CosmosGardenGalaxy")
    rabbit = g.find("TrickRabbit")[0]
    m = g.placements[rabbit.zone_placement][1]
    rails = {r.get("l_id"): (r, pts) for r, pts in g.zone_data[rabbit.zone]["rails"]}
    nodes, edges = [], []

    def node(p):
        world = [fsum(m[k][t] * p[t] for t in range(3)) + m[k][3] for k in range(3)]
        best = -1
        bd = None
        for i, n in enumerate(nodes):
            d = fsum((w - n[k]) * (w - n[k]) for k, w in enumerate(world))
            if bd is None or d < bd:
                best, bd = i, d
        if best >= 0 and bd < 10000:
            return best
        nodes.append(world)
        return len(nodes) - 1

    local = rabbit.row.get("CommonPath_ID")
    seen = []
    while local != -1 and local not in seen:
        seen.append(local)
        row, pts = rails[local]
        first = previous = -1
        prior = [-1] * 8
        for p in pts:
            cur = node([p.get("pnt0_" + a, 0.0) for a in "xyz"])
            if first < 0:
                first = cur
            if previous >= 0 and previous != cur:
                edges.append(([previous, cur], prior))
            previous = cur
            prior = [p.get("point_arg%d" % k, -1) for k in range(8)]
        if row.get("closed") == "CLOSE" and previous != first:
            edges.append(([previous, first], prior))
        local = row.get("Path_ID", -1)
    out = []
    flat = []
    for n in range(len(nodes)):
        entries = [i for i, e in enumerate(edges) if n in e[0]]
        out += gi(nodes[n]) + [len(flat), len(entries)]
        flat += entries
    for ns, args in edges:
        out += [args[1], args[3], ns[0], ns[1], args[0], args[7]]
    return out + flat


def warppods(ctx, spec):
    g = galaxy(ctx, "AstroGalaxy")
    pods = g.find("WarpPod")
    arc = "StageData/AstroGalaxy.arc"
    path = [n for n in ctx.arc_names(arc) if n.lower().endswith("/cameraparam.bcam")][0]
    cams = BCSV(ctx.arc_file(arc, path))
    cam = {cams.get(i, "id"): i for i in range(cams.n)}
    out = []
    for i, o in enumerate(pods):
        group = o.row.get("GroupId")
        pair = [j for j, p in enumerate(pods) if j != i and p.row.get("GroupId") == group][0]
        q = pods[pair]
        pos, up, front = gba(o.pos), gba(mtx_col(o.m, 1)), gba(mtx_col(o.m, 2))
        end = add(gba(q.pos), scale(gba(mtx_col(q.m, 1)), 160))
        start = add(pos, scale(up, 160))
        delta = sub(end, start)
        dist = length(delta)
        axis = unit(cross(delta, scale(up, -1)))
        normal = unit(cross(axis, delta))
        angle = PI / 4
        radius = 0.5 * dist / sin(angle)
        rise = math.sqrt(radius * radius - 0.25 * dist * dist)
        center = add(scale(add(start, end), 0.5), scale(normal, rise))
        arcstart = scale(normal, -radius)
        c = cam["e:\u30ef\u30fc\u30d7\u30ab\u30e1\u30e9 %d-%s" % (group, chr(o.args[0] + 65))]
        eye = gba([round(cams.get(c, "wpoint." + a), 5) for a in "XYZ"])
        woff = gba([round(cams.get(c, "woffset." + a), 5) for a in "XYZ"])
        mode = o.args[1] if o.args[1] >= 0 else 1
        out += [rint(v) for v in start] + [rint(v * 16384) for v in up] + [rint(v * 16384) for v in front]
        out += [rint(v) for v in eye] + [rint(v) for v in woff] + [max(int(radius / 25), 120), pair, mode]
        ca = cross(axis, arcstart)
        da = dot(axis, arcstart)
        for n in range(65):
            t = (1 + sin((n / 64 - 0.5) * PI)) * 0.5
            th = -angle + 2 * angle * t
            ct, st = cos(th), sin(th)
            vec = [arcstart[k] * ct + ca[k] * st + axis[k] * da * (1 - ct) for k in range(3)]
            out += [rint(v) for v in add(center, vec)]
    return out


def buoybase(ctx, spec):
    g = galaxy(ctx, "OceanFloaterLandGalaxy")
    out = []
    for o in g.objects:
        if o.name.startswith(("OceanFloater", "OceanHexagon")):
            out.append(o.index)
            out += [o.row.get(k, 0) for k in ROTATE_FIELDS]
    for name, clip, joint, define in BUOY_JOINTS:
        arc = "ObjectData/%s.arc" % name
        model = Model(ctx.arc_file(arc, name.lower() + ".bdl"))
        bck = BCK(ctx.arc_file(arc, clip + ".bck"))
        ji = model.joint(joint)
        for frame in range(bck.frame_count + 1):
            f = min(frame, bck.frame_count - .001)
            m = model.pose(bck, f)[ji]
            out += [rint(v) for v in mtx_col(m, 3)]
            for c in (1, 2):
                v = mtx_col(m, c)
                out += [rint(x / length(v) * 16384) for x in v]
        if define:
            out.append(bck.frame_count)
    out.append(bck_frames(ctx, "ObjectData/HatchWaterPlanet.arc", "hatchwaterplanet.bck"))
    return out


def oceanbowl(ctx, spec):
    o = galaxy(ctx, "OceanFloaterLandGalaxy").find("OceanBowl")[0]
    r = o.scale[0] * 100
    out = []
    for k in range(64):
        a = 2 * PI * k / 64
        out += [rint(r * cos(a)), rint(r * sin(a))]
    return out


def dist2(a, b):
    d = sub(a, b)
    return dot(d, d)


def placed_triangles(ctx, name, o):
    k = KCL(ctx.arc_file("ObjectData/WormEatenPlanet.arc", name))
    m = o.m
    out = []
    for tri in k.triangles:
        pts = []
        for v in tri:
            v = [round(v[i], 3) * o.scale[i] for i in range(3)]
            pts.append(mtx_apply(m, v))
        out.append(tuple(pts))
    return out


def closest_on_triangle(p, a, b, c):
    ab, ac, ap = sub(b, a), sub(c, a), sub(p, a)
    d1, d2 = dot(ab, ap), dot(ac, ap)
    if d1 <= 0 and d2 <= 0:
        return a
    bp = sub(p, b)
    d3, d4 = dot(ab, bp), dot(ac, bp)
    if d3 >= 0 and d4 <= d3:
        return b
    vc = d1 * d4 - d3 * d2
    if vc <= 0 and d1 >= 0 and d3 <= 0:
        v = d1 / (d1 - d3)
        return (a[0] + ab[0] * v, a[1] + ab[1] * v, a[2] + ab[2] * v)
    cp = sub(p, c)
    d5, d6 = dot(ab, cp), dot(ac, cp)
    if d6 >= 0 and d5 <= d6:
        return c
    vb = d5 * d2 - d1 * d6
    if vb <= 0 and d2 >= 0 and d6 <= 0:
        w = d2 / (d2 - d6)
        return (a[0] + ac[0] * w, a[1] + ac[1] * w, a[2] + ac[2] * w)
    va = d3 * d6 - d5 * d4
    if va <= 0 and (d4 - d3) >= 0 and (d5 - d6) >= 0:
        w = (d4 - d3) / ((d4 - d3) + (d5 - d6))
        return (b[0] + (c[0] - b[0]) * w, b[1] + (c[1] - b[1]) * w, b[2] + (c[2] - b[2]) * w)
    den = 1.0 / (va + vb + vc)
    v, w = vb * den, vc * den
    return (a[0] + ab[0] * v + ac[0] * w, a[1] + ab[1] * v + ac[1] * w, a[2] + ab[2] * v + ac[2] * w)


def cell(v):
    return (math.floor(v[0] / CELL), math.floor(v[1] / CELL), math.floor(v[2] / CELL))


def near_grid(tris, pad):
    grid = {}
    for i, t in enumerate(tris):
        lo = cell([min(p[k] for p in t) - pad for k in range(3)])
        hi = cell([max(p[k] for p in t) + pad for k in range(3)])
        for x in range(lo[0], hi[0] + 1):
            for y in range(lo[1], hi[1] + 1):
                for z in range(lo[2], hi[2] + 1):
                    grid.setdefault((x, y, z), []).append(i)
    return grid


def ray_grid(tris):
    grid = {}
    for i, t in enumerate(tris):
        y0 = math.floor(min(p[1] for p in t) / CELL)
        y1 = math.floor(max(p[1] for p in t) / CELL)
        z0 = math.floor(min(p[2] for p in t) / CELL)
        z1 = math.floor(max(p[2] for p in t) / CELL)
        for y in range(y0, y1 + 1):
            for z in range(z0, z1 + 1):
                grid.setdefault((y, z), []).append(i)
    return grid


def ray_hits(tris, ids, o):
    ts = []
    for i in ids:
        a, b, c = tris[i]
        e1, e2 = sub(b, a), sub(c, a)
        det = e1[1] * e2[2] - e1[2] * e2[1]
        if det == 0:
            continue
        f = 1.0 / det
        s = sub(o, a)
        u = f * (s[1] * e2[2] - s[2] * e2[1])
        if u < 0 or u > 1:
            continue
        q = cross(s, e1)
        v = -f * q[0]
        if v < 0 or u + v > 1:
            continue
        t = f * dot(e2, q)
        if t >= 0:
            ts.append(t)
    return sorted(ts)


def inside(tris, grid, p):
    ts = ray_hits(tris, grid.get((math.floor(p[1] / CELL), math.floor(p[2] / CELL)), []), p)
    hits = 0
    pos = 0.0
    for t in ts:
        if hits >= 64:
            break
        if t >= pos:
            hits += 1
            pos = t + RAY_STEP
    return hits % 2 == 1


def rail_samples(rail):
    pts = rail.points
    samples, coords, point_coord = [], [], []
    acc = 0.0
    for i in range(len(pts) - 1):
        a, b = pts[i]["pos"], pts[i + 1]["pos"]
        h1, h2 = pts[i]["ctrl_out"], pts[i + 1]["ctrl_in"]
        point_coord.append(acc)
        prev = a
        for k in range(0 if i == 0 else 1, 129):
            q = bezier([a, h1, h2, b], k / 128)
            acc += math.sqrt(dist2(q, prev))
            prev = q
            samples.append(q)
            coords.append(acc)
    point_coord.append(acc)
    return samples, coords, point_coord, acc


def nearest_sample(samples, grid, p):
    c = cell(p)
    best = None
    ring = 0
    while True:
        for x in range(c[0] - ring, c[0] + ring + 1):
            for y in range(c[1] - ring, c[1] + ring + 1):
                for z in range(c[2] - ring, c[2] + ring + 1):
                    if max(abs(x - c[0]), abs(y - c[1]), abs(z - c[2])) != ring:
                        continue
                    for i in grid.get((x, y, z), ()):
                        d = dist2(samples[i], p)
                        if best is None or d < best[0] or (d == best[0] and i < best[1]):
                            best = (d, i)
        if best is not None and best[0] <= (ring * CELL) * (ring * CELL):
            return best[1]
        ring += 1


def worm_chunks(ctx, g):
    o = g.find("WormEatenPlanet")[0]
    first = placed_triangles(ctx, "first.kcl", o)
    fourth = placed_triangles(ctx, "fourth.kcl", o)
    rail = g.rails[o.rail]
    samples, coords, point_coord, total = rail_samples(rail)
    sgrid = {}
    for i, s in enumerate(samples):
        sgrid.setdefault(cell(s), []).append(i)
    rests = [i for i, p in enumerate(rail.points) if p["args"][0] == 1]
    bounds = [0.0] + [point_coord[i] for i in rests] + [total]
    ngrid = near_grid(first, SURFACE)
    rgrid = ray_grid(first)
    body = []
    for t in fourth:
        centre = tuple((t[0][k] + t[1][k] + t[2][k]) / 3 for k in range(3))
        near = False
        for i in ngrid.get(cell(centre), ()):
            if dist2(closest_on_triangle(centre, *first[i]), centre) <= SURFACE * SURFACE:
                near = True
                break
        if near or inside(first, rgrid, centre):
            continue
        body.append(coords[nearest_sample(samples, sgrid, centre)])
    rows = []
    for seg in range(len(bounds) - 1):
        lo, hi = bounds[seg], bounds[seg + 1]
        part = sorted(c for c in body if lo <= c < hi or (seg == len(bounds) - 2 and c >= hi))
        n = SPLIT[seg] if seg < len(SPLIT) else 1
        for k in range(n):
            chunk = part[len(part) * k // n: len(part) * (k + 1) // n]
            if chunk:
                rows.append((seg, round(chunk[0]), round(chunk[-1])))
    return rows


def gusty_worm(ctx, spec):
    g = galaxy(ctx, "CosmosGardenGalaxy")
    rows = worm_chunks(ctx, g)
    out = []
    for k, (seg, lo, hi) in enumerate(rows):
        out += [2000 + 1000 * g.zone_list.index("WormEatenZone") + 110 + k, seg, lo, hi]
    return out + [len(rows)]


PREDICTORS = {
    "src/world/water_volumes_gen.h": water_runtime,
    "tools/asset_compiler/water_volumes_gen.h": water_compiler,
    "src/actors/deep_tables_gen.h": deep_tables,
    "src/actors/deep_purple_rail_coin_gen.h": deep_purple_rail,
    "src/actors/honey_rail_coin_gen.h": honey_rail,
    "src/actors/gusty_gold_coord_gen.h": gusty_gold,
    "src/actors/gusty_star_appear_gen.h": gusty_star_appear,
    "src/actors/buoy_penguin_coords_gen.h": buoy_penguin,
    "src/actors/iceking_gen.h": iceking,
    "src/actors/penguin_skater_gen.h": penguin_skater,
    "src/actors/syati_gen.h": syati,
    "src/actors/gusty_rabbit_graph_gen.h": rabbit_graph,
    "src/actors/warppod_paths_gen.h": warppods,
    "src/actors/buoybase_parts_gen.h": buoybase,
    "src/graphics/oceanbowl_static_gen.h": oceanbowl,
    "src/actors/gusty_worm_gen.h": gusty_worm,
}
