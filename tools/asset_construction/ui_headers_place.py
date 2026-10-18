from common import yaz0_decompress
from ui_data import arc_get, bcsv
from ui_math import cos, radians, sin

STAGES = ["observatory", "honeyhive", "goodegg", "honeyhive_tower", "beachbowl", "freezeflame", "buoybase",
          "deepdark", "gustygarden", "astrodome"]
LAYERS = ["Common"] + ["Layer" + chr(ord("A") + i) for i in range(16)]
JMP_DIRS = ["Placement", "MapParts", "Start", "GeneralPos", "ChildObj", "Debug"]


def mtx_tr(t, r):
    rx, ry, rz = (radians(a) for a in r)
    sx, sy, sz = sin(rx), sin(ry), sin(rz)
    cx, cy, cz = cos(rx), cos(ry), cos(rz)
    return [[cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx, t[0]],
            [sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx, t[1]],
            [-sy, cy * sx, cy * cx, t[2]]]


def mtx_mul(a, b):
    out = [[0.0] * 4 for _ in range(3)]
    for i in range(3):
        for j in range(3):
            out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j]
        out[i][3] = a[i][0] * b[0][3] + a[i][1] * b[1][3] + a[i][2] * b[2][3] + a[i][3]
    return out


def r3(v):
    return [round(float(x), 3) for x in v]


IDENTITY = mtx_tr((0, 0, 0), (0, 0, 0))


class Obj:
    __slots__ = ("index", "name", "zone", "zone_placement", "local_pos", "local_rot", "world_pos",
                 "scenarios", "row")


def arc_listdir(arc, path):
    want = path.lower()
    out = []
    for p, blob in arc.files:
        d, name = p.rsplit("/", 1)
        d = d.lower()
        if d == want or d.split("/", 1)[-1] == want:
            out.append((name, yaz0_decompress(blob)))
    return out


def zone_list(ctx, name):
    scen = ctx.arc("StageData/%s/%sScenario.arc" % (name, name))
    return [r["ZoneName"] for r in bcsv(arc_get(scen, "ZoneList.bcsv"))]


def zone_entries(ctx, zone):
    try:
        arc = ctx.arc("StageData/%s.arc" % zone)
    except OSError:
        return []
    entries = []
    for d in JMP_DIRS:
        for layer in LAYERS:
            for fname, data in arc_listdir(arc, "jmp/%s/%s" % (d, layer)):
                for row in bcsv(data):
                    entries.append((layer, fname, row))
    return entries


def placement(r):
    pos = [r.get("pos_x", 0.0), r.get("pos_y", 0.0), r.get("pos_z", 0.0)]
    rot = [r.get("dir_x", 0.0), r.get("dir_y", 0.0), r.get("dir_z", 0.0)]
    return pos, rot


def build_galaxy(ctx, name):
    scen_arc = ctx.arc("StageData/%s/%sScenario.arc" % (name, name))
    zones = zone_list(ctx, name)
    scen = []
    for r in bcsv(arc_get(scen_arc, "ScenarioData.bcsv")):
        scen.append((r["ScenarioNo"], {z: ((r.get(z, 0) or 0) * 2) | 1 for z in zones}))
    scen_nos = [s[0] for s in scen]
    zone_data = {z: zone_entries(ctx, z) for z in zones}
    placements = []

    def active(zone, layer, among):
        lb = LAYERS.index(layer)
        return [no for no, zl in scen if no in among and (zl.get(zone, 1) >> lb) & 1]

    def place(zone, parent_mtx, among, depth):
        if depth > 4:
            return
        for layer, fname, r in zone_data[zone]:
            if fname.lower() != "stageobjinfo":
                continue
            child = r["name"]
            world = mtx_mul(parent_mtx, mtx_tr(*placement(r)))
            act = active(zone, layer, among)
            pi = len(placements)
            placements.append((child, pi, [r3(row) for row in world], act))
            if child in zone_data:
                place(child, world, act, depth + 1)

    place(name, IDENTITY, scen_nos, 0)
    objects = []
    for zone, pidx, mtx, zone_scen in [(name, -1, IDENTITY, scen_nos)] + placements:
        if zone not in zone_data:
            continue
        for layer, fname, r in zone_data[zone]:
            pos, rot = placement(r)
            wm = mtx_mul(mtx, mtx_tr(pos, rot))
            o = Obj()
            o.scenarios = active(zone, layer, zone_scen)
            o.index = len(objects)
            o.name = r.get("name", "")
            o.zone = zone
            o.zone_placement = pidx
            o.local_pos, o.local_rot = r3(pos), r3(rot)
            o.world_pos = r3([wm[0][3], wm[1][3], wm[2][3]])
            o.row = r
            objects.append(o)
    return {"placements": placements, "objects": objects}


def galaxy(ctx, name):
    return ctx.memo(("galaxy", name), lambda: build_galaxy(ctx, name))
