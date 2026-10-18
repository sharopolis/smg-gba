import math
import struct

from actor_headers_lib import BCSV, PI, mtx_apply, mtx_mul, mtx_tr, r3

LAYERS = ["Common"] + ["Layer" + chr(ord("A") + i) for i in range(16)]
JMP_DIRS = ["Placement", "MapParts", "Start", "GeneralPos", "ChildObj", "Debug"]
SWITCH_FIELDS = ["SW_APPEAR", "SW_DEAD", "SW_A", "SW_B", "SW_SLEEP", "SW_AWAKE", "SW_PARAM"]
IDENTITY = mtx_tr((0, 0, 0), (0, 0, 0))

ATANHI = [4.63647609000806093515e-01, 7.85398163397448278999e-01, 9.82793723247329054082e-01,
          1.57079632679489655800e+00]
ATANLO = [2.26987774529616870924e-17, 3.06161699786838301793e-17, 1.39033110312309984516e-17,
          6.12323399573676603587e-17]
AT = [3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01,
      -1.11111104054623557880e-01, 9.09088713343650656196e-02, -7.69187620504482999495e-02,
      6.66107313738753120669e-02, -5.83357013379057348645e-02, 4.97687799461593236017e-02,
      -3.65315727442169155270e-02, 1.62858201153657823623e-02]
PI_LO = 1.2246467991473531772e-16
PIO2_HI = 1.57079632679489655800e+00
PIO2_LO = 6.12323399573676603587e-17
PIO4_HI = 7.85398163397448278999e-01
PS = [1.66666666666666657415e-01, -3.25565818622400915405e-01, 2.01212532134862925881e-01,
      -4.00555345006794114027e-02, 7.91534994289814532176e-04, 3.47933107596021167570e-05]
QS = [-2.40339491173441421878e+00, 2.02094576023350569471e+00, -6.88283971605453293030e-01,
      7.70381505559019352791e-02]


def atan(x):
    neg = x < 0
    x = -x if neg else x
    if x >= 7.378697629483821e+19:
        z = ATANHI[3] + ATANLO[3]
        return -z if neg else z
    if x < 0.4375:
        if x < 1.862645149230957e-09:
            return -x if neg else x
        k = -1
    elif x < 1.1875:
        if x < 0.6875:
            k = 0
            x = (2.0 * x - 1.0) / (2.0 + x)
        else:
            k = 1
            x = (x - 1.0) / (x + 1.0)
    elif x < 2.4375:
        k = 2
        x = (x - 1.5) / (1.0 + 1.5 * x)
    else:
        k = 3
        x = -1.0 / x
    z = x * x
    w = z * z
    s1 = z * (AT[0] + w * (AT[2] + w * (AT[4] + w * (AT[6] + w * (AT[8] + w * AT[10])))))
    s2 = w * (AT[1] + w * (AT[3] + w * (AT[5] + w * (AT[7] + w * AT[9]))))
    if k < 0:
        z = x - x * (s1 + s2)
    else:
        z = ATANHI[k] - ((x * (s1 + s2) - ATANLO[k]) - x)
    return -z if neg else z


def atan2(y, x):
    if x == 1.0:
        return atan(y)
    ny = math.copysign(1.0, y) < 0
    if y == 0:
        if math.copysign(1.0, x) > 0:
            return y
        return -PI if ny else PI
    if x == 0:
        return -PIO2_HI - PIO2_LO if ny else PIO2_HI + PIO2_LO
    z = atan(abs(y / x))
    if x > 0:
        return -z if ny else z
    if ny:
        return (z - PI_LO) - PI
    return PI - (z - PI_LO)


def asin(x):
    ax = abs(x)
    if ax >= 1.0:
        return x * PIO2_HI + x * PIO2_LO
    if ax < 0.5:
        if ax < 7.450580596923828e-09:
            return x
        t = x * x
        p = t * (PS[0] + t * (PS[1] + t * (PS[2] + t * (PS[3] + t * (PS[4] + t * PS[5])))))
        q = 1.0 + t * (QS[0] + t * (QS[1] + t * (QS[2] + t * QS[3])))
        return x + x * (p / q)
    w = 1.0 - ax
    t = w * 0.5
    p = t * (PS[0] + t * (PS[1] + t * (PS[2] + t * (PS[3] + t * (PS[4] + t * PS[5])))))
    q = 1.0 + t * (QS[0] + t * (QS[1] + t * (QS[2] + t * QS[3])))
    s = math.sqrt(t)
    if ax >= 0.975:
        t = PIO2_HI - (2.0 * (s + s * (p / q)) - PIO2_LO)
    else:
        w = struct.unpack(">d", struct.pack(">d", s)[:4] + bytes(4))[0]
        c = (t - w * w) / (s + w)
        p = 2.0 * s * (p / q) - (PIO2_LO - 2.0 * c)
        q = PIO4_HI - 2.0 * w
        t = PIO4_HI - (p - q)
    return t if x > 0 else -t


def mtx_to_euler(m):
    r20 = max(-1.0, min(1.0, m[2][0]))
    ry = asin(-r20)
    if abs(r20) < 0.99999:
        rx = atan2(m[2][1], m[2][2])
        rz = atan2(m[1][0], m[0][0])
    else:
        rz = 0.0
        rx = atan2(-m[1][2], m[1][1])
    return [round(a * (180.0 / PI), 4) for a in (rx, ry, rz)]


class Row:
    def __init__(self, table, i):
        self.table = table
        self.i = i

    def get(self, name, default=None):
        v = self.table.get(self.i, name, default)
        if isinstance(v, float):
            v = round(v, 5)
        return v

    def has(self, name):
        return self.table.has(name)


class Rail:
    def __init__(self, row, pts, mtx):
        self.closed = row.get("closed")
        self.points = []
        for p in pts:
            pp = [[p.get("pnt%d_%s" % (k, ax), 0.0) for ax in "xyz"] for k in range(3)]
            self.points.append({"pos": r3(mtx_apply(mtx, pp[0])), "ctrl_in": r3(mtx_apply(mtx, pp[1])),
                                "ctrl_out": r3(mtx_apply(mtx, pp[2])),
                                "args": [p.get("point_arg%d" % k, -1) for k in range(8)]})


def row_pos(r):
    return [r.get("pos_x", 0.0), r.get("pos_y", 0.0), r.get("pos_z", 0.0)]


def row_rot(r):
    return [r.get("dir_x", 0.0), r.get("dir_y", 0.0), r.get("dir_z", 0.0)]


class Obj:
    def __init__(self, index, zone, zone_placement, mtx, fname, r, scenarios, rail):
        self.index = index
        self.row = r
        self.name = r.get("name", "")
        self.zone = zone
        self.zone_placement = zone_placement
        self.l_id = r.get("l_id")
        self.scenarios = scenarios
        self.scale = r3([r.get("scale_x", 1.0), r.get("scale_y", 1.0), r.get("scale_z", 1.0)])
        world = mtx_mul(mtx, mtx_tr(row_pos(r), row_rot(r)))
        self.pos = r3([world[k][3] for k in range(3)])
        self.m = mtx_tr(self.pos, mtx_to_euler(world))
        if fname.lower() == "startinfo":
            self.args = [r.get("Obj_arg0", -1)]
        else:
            self.args = [r.get("Obj_arg%d" % n) for n in range(8) if r.has("Obj_arg%d" % n)]
        self.switches = {k: r.get(k) for k in SWITCH_FIELDS if r.has(k) and r.get(k) != -1}
        self.rail = rail


class Galaxy:
    def __init__(self, ctx, name):
        self.ctx = ctx
        scen = "StageData/%s/%sScenario.arc" % (name, name)
        zl = BCSV(ctx.arc_file(scen, "ZoneList.bcsv"))
        self.zone_list = [zl.get(i, "ZoneName") for i in range(zl.n)]
        sd = BCSV(ctx.arc_file(scen, "ScenarioData.bcsv"))
        self.scenarios = []
        for i in range(sd.n):
            r = Row(sd, i)
            masks = {z: ((r.get(z, 0) or 0) * 2) | 1 for z in self.zone_list}
            self.scenarios.append((r.get("ScenarioNo"), masks))
        self.zone_data = {z: self.read_zone(z) for z in self.zone_list}
        nos = [no for no, _ in self.scenarios]
        self.placements = []
        self.place(name, IDENTITY, nos, 0)
        instances = [(name, -1, IDENTITY, nos)]
        for pi, (child, m, act) in enumerate(self.placements):
            instances.append((child, pi, m, act))
        self.objects = []
        self.rails = []
        rail_key = {}
        for zone, pidx, mtx, zscen in instances:
            zd = self.zone_data[zone]
            for r, pts in zd["rails"]:
                rail_key[(zone, pidx, r.get("l_id"))] = len(self.rails)
                self.rails.append(Rail(r, pts, mtx))
            for layer, fname, r in zd["entries"]:
                cp = r.get("CommonPath_ID", -1)
                rail = rail_key.get((zone, pidx, cp)) if cp is not None and cp >= 0 else None
                self.objects.append(Obj(len(self.objects), zone, pidx, mtx, fname, r,
                                        self.active(zone, layer, zscen), rail))

    def read_zone(self, zone):
        rel = "StageData/%s.arc" % zone
        res = {"entries": [], "rails": []}
        if not self.ctx.exists(rel):
            return res
        arc = self.ctx.arc(rel)
        files = {}
        byname = {}
        for path, blob in arc.files:
            parts = path.split("/")
            files.setdefault("/".join(parts[1:-1]).lower(), []).append((parts[-1], blob))
            byname.setdefault(path.split("/", 1)[-1].lower(), blob)
        for d in JMP_DIRS:
            for layer in LAYERS:
                for fname, blob in files.get(("jmp/%s/%s" % (d, layer)).lower(), []):
                    t = BCSV(blob)
                    for i in range(t.n):
                        res["entries"].append((layer, fname, Row(t, i)))
        info = byname.get("jmp/path/commonpathinfo")
        if info is not None:
            t = BCSV(info)
            for idx in range(t.n):
                pf = byname.get("jmp/path/commonpathpointinfo.%d" % idx)
                pts = []
                if pf is not None:
                    pt = BCSV(pf)
                    pts = [Row(pt, k) for k in range(pt.n)]
                res["rails"].append((Row(t, idx), pts))
        return res

    def active(self, zone, layer, within):
        lb = LAYERS.index(layer)
        return [no for no, masks in self.scenarios if no in within and (masks.get(zone, 1) >> lb) & 1]

    def place(self, zone, parent_mtx, within, depth):
        if depth > 4:
            return
        for layer, fname, r in self.zone_data[zone]["entries"]:
            if fname.lower() != "stageobjinfo":
                continue
            child = r.get("name")
            world = [r3(row) for row in mtx_mul(parent_mtx, mtx_tr(row_pos(r), row_rot(r)))]
            act = self.active(zone, layer, within)
            self.placements.append((child, world, act))
            if child in self.zone_data:
                self.place(child, world, act, depth + 1)

    def find(self, name, zone=None):
        return [o for o in self.objects if o.name == name and (zone is None or o.zone == zone)]


def galaxy(ctx, name):
    return ctx.memo("galaxy:" + name, lambda: Galaxy(ctx, name))
