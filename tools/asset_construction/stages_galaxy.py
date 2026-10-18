import struct

from common import RARC, cstr, jhash, yaz0_decompress
from stages_math import PI, cos, mtx_mul, mtx_to_euler, mtx_tr, radians, sin, xform_point

LAYERS = ["Common"] + ["Layer" + chr(ord("A") + i) for i in range(16)]
JMP_DIRS = ["Placement", "MapParts", "Start", "GeneralPos", "ChildObj", "Debug"]
STD_FIELDS = {"name", "l_id", "pos_x", "pos_y", "pos_z", "dir_x", "dir_y", "dir_z", "scale_x", "scale_y",
              "scale_z"}
SWITCH_FIELDS = ["SW_APPEAR", "SW_DEAD", "SW_A", "SW_B", "SW_SLEEP", "SW_AWAKE", "SW_PARAM"]
FIELD_NAMES = (
    "name l_id pos_x pos_y pos_z dir_x dir_y dir_z scale_x scale_y scale_z SW_APPEAR SW_DEAD SW_A "
    "SW_B SW_SLEEP SW_AWAKE SW_PARAM CommonPath_ID ClippingGroupId GroupId DemoGroupId MapParts_ID "
    "Obj_ID CameraSetId CastId ViewGroupId MessageId ShapeModelNo Range Distance Distant Priority "
    "Inverse Power Gravity_type Gravity_id GeneratorID FollowId AreaShapeNo ParentID ParamScale "
    "ParentId ChildObjId FarClip MarioNo Camera_id Camera_through Floor_code Wall_code Sound_code "
    "ScenarioNo ScenarioName PowerStarId AppearPowerStarObj Comet LuigiModeTimer ZoneName IsHidden "
    "ErrorCheck PowerStarType Hidden Validity DemoName TimeSheetName DemoSkip no closed num_pnt usage "
    "id type camtype dist angleA angleB camint camendint evfrm num1 num2 fovy roll vpanuse upper "
    "lower uplay lplay loffset loffsetv string gflag.thru eflag.enableErpFrame "
    "eflag.enableEndErpFrame flag.antibluroff flag.noreset"
)
GAME_UNIT = 256.0
GRAVITY_KINDS = {
    "GlobalPointGravity": "gpoint", "GlobalCubeGravity": "gcube", "GlobalPlaneGravity": "gplane",
    "GlobalPlaneGravityInBox": "gplanebox", "GlobalPlaneGravityInCylinder": "gplanecyl",
    "GlobalDiskGravity": "gdisk", "GlobalDiskTorusGravity": "gdisktorus", "GlobalConeGravity": "gcone",
    "GlobalSegmentGravity": "gsegment", "GlobalWireGravity": "gwire",
}
AREAS = {
    "SwitchCube": ("switch", "box"), "SwitchSphere": ("switch", "sphere"),
    "SwitchCylinder": ("switch", "cylinder"),
    "CubeCameraBox": ("camera", "cbox"), "CubeCameraCylinder": ("camera", "cylinder"),
    "CubeCameraSphere": ("camera", "sphere"), "CubeCameraBowl": ("camera", "bowl"),
    "DeathCube": ("death", "box"), "DeathSphere": ("death", "sphere"), "DeathCylinder": ("death", "cylinder"),
    "RestartCube": ("restart", "box"), "TowerModeCylinder": ("tower", "cylinder"),
    "BlackHole": ("blackhole", "sphere"), "BlackHoleCube": ("blackhole", "cbox"),
}
SWITCHES = ("SW_A", "SW_B", "SW_APPEAR", "SW_DEAD", "SW_SLEEP")
SWITCHED_PARTS = {"StarEggRoadEggPlanetD": "eggd"}
SWAPPED_PARTS = {"HatchWaterPlanet": (("buoy_hatch_before", "hide"), ("buoy_hatch_after", "show"))}
SUPER_SPIN = "\u30b9\u30fc\u30d1\u30fc\u30b9\u30d4\u30f3\u30c9\u30e9\u30a4\u30d0\u30fc"
MULTI_EVENT_CAMERAS = {"JumpGuarder": ("\u30b8\u30e3\u30f3\u30d7\u30ac\u30fc\u30c0\u30fc", 2)}
SPIN_DRIVER_CAMERA_NAMES = {
    "ScenarioStarter": "\u30b7\u30ca\u30ea\u30aa\u30b9\u30bf\u30fc\u30bf\u30fc",
    "SuperSpinDriver": SUPER_SPIN,
    "SuperSpinDriverGreen": "\u30b0\u30ea\u30fc\u30f3" + SUPER_SPIN,
    "SuperSpinDriverPink": "\u30d4\u30f3\u30af" + SUPER_SPIN,
    "SpinDriver": "\u30b9\u30d4\u30f3\u30c9\u30e9\u30a4\u30d0",
}
NTH_CAMERA = "e:%s:%03d:%02d\u756a\u76ee"
PENGUIN_CAMERA = "e:\u30da\u30f3\u30ae\u30f3\u30b9\u30b1\u30fc\u30bf\u30fc\u56fa\u6709%s%d"
PENGUIN_PHASES = ("\u958b\u59cb", "\u7d42\u4e86")
TRAPEZE_CAMERA = "e:\u7a7a\u4e2d\u30d6\u30e9\u30f3\u30b3\u56fa\u6709%03d"
BEE_CAMERA = "e:\u65b0\u5909\u8eab\u30aa\u30d6\u30b8\u30a7[\u30cf\u30c1]\u56fa\u6709\u51fa\u73fe%d%03d"
BOSS_STINKBUG = "\u30dc\u30b9\u30ab\u30e1\u30e0\u30b7-"
GCAPTURE_DEMO = "G\u30ad\u30e3\u30d7\u30c1\u30e3\u30fc\u51fa\u73fe"
GCAPTURE_CAMERA = "e:" + GCAPTURE_DEMO + "[\u51fa\u73fe\u30ab\u30e1\u30e9]"
FLOATER_DEMO = "\u30d5\u30ed\u30fc\u30bf\u30fc\u56fa\u5b9a\u5177\uff11"
FLOATER_CAMERA = "e:" + FLOATER_DEMO + "[%s]"
FLOATER_PARTS = ("\u56fa\u5b9a\u5177\uff11\u7834\u58ca", "\u9396\uff11\u7834\u58ca",
                 "\u30d5\u30ed\u30fc\u30bf\u30fc\u4e0a\u6607")


def field_names():
    names = FIELD_NAMES.split()
    for i in range(8):
        names += ["Obj_arg%d" % i, "path_arg%d" % i, "point_arg%d" % i]
    for i in range(3):
        names += ["pnt%d_%s" % (i, c) for c in "xyz"]
    for v in ("axis", "wpoint", "up", "woffset", "vpanaxis"):
        names += ["%s.%s" % (v, c) for c in "XYZ"]
    return names


class Names:
    def __init__(self):
        self.map = {}
        self.add(field_names())

    def add(self, names):
        for n in names:
            if n:
                self.map.setdefault(jhash(n), n)

    def name(self, h):
        return self.map.get(h, "0x%08X" % h)


def read_bcsv(data, names):
    data = yaz0_decompress(data)
    n_entries, n_fields, data_off, entry_size = struct.unpack_from(">iiiI", data, 0)
    fields = []
    for i in range(n_fields):
        h, mask, off, shift, typ = struct.unpack_from(">IIHBB", data, 16 + i * 12)
        fields.append((names.name(h), mask, off, shift, typ))
    str_table = data_off + n_entries * entry_size
    rows = []
    for i in range(n_entries):
        base = data_off + i * entry_size
        row = {}
        raw = {}
        for name, mask, off, shift, typ in fields:
            p = base + off
            if typ in (0, 3):
                v = (struct.unpack_from(">I", data, p)[0] & mask) >> shift
                if mask == 0xFFFFFFFF and shift == 0 and v >= 0x80000000:
                    v -= 0x100000000
            elif typ == 4:
                v = (struct.unpack_from(">H", data, p)[0] & mask) >> shift
                if mask == 0xFFFF and shift == 0 and v >= 0x8000:
                    v -= 0x10000
            elif typ == 5:
                v = (data[p] & mask) >> shift
                if mask == 0xFF and shift == 0 and v >= 0x80:
                    v -= 0x100
            elif typ == 2:
                raw[name] = struct.unpack_from(">f", data, p)[0]
                v = round(raw[name], 5)
            elif typ == 1:
                v = data[p:p + 32].split(b"\0", 1)[0].decode("shift_jis", "replace")
            elif typ == 6:
                v = cstr(data, str_table + struct.unpack_from(">I", data, p)[0])
            else:
                v = None
            row[name] = v
        row["@raw"] = raw
        rows.append(row)
    return rows


class Arc:
    def __init__(self, data):
        self.files = dict(RARC(data).files)

    def get(self, path):
        want = path.strip("/").lower()
        for fp, blob in self.files.items():
            low = fp.lower()
            if low == want or low.split("/", 1)[-1] == want:
                return yaz0_decompress(blob)
        return None

    def listdir(self, path):
        want = path.strip("/").lower()
        out = []
        for fp, blob in self.files.items():
            d = fp.rsplit("/", 1)[0].lower()
            if d == want or d.split("/", 1)[-1] == want:
                out.append((fp.rsplit("/", 1)[1], yaz0_decompress(blob)))
        return out


def r3(v):
    return [round(float(x), 3) for x in v]


def raw_trs(r):
    raw = r.get("@raw", {})
    out = []
    for name, default in (("pos", 0.0), ("dir", 0.0), ("scale", 1.0)):
        keys = [name + "_" + c for c in "xyz"]
        out.append([raw.get(k, r.get(k, default)) for k in keys])
    return out


def row_vec(r, name, default):
    return [r.get(name + "_" + c, default) for c in "xyz"]


def active_scenarios(scen, zone_scen, zone, layer):
    lb = LAYERS.index(layer)
    return [s["no"] for s in scen if s["no"] in zone_scen and (s["zone_masks"].get(zone, 1) >> lb) & 1]


class Galaxy:
    def __init__(self, dump, name):
        self.name = name
        self.names = Names()
        scen_arc = Arc(dump.read("StageData/" + name + "/" + name + "Scenario.arc"))
        self.zone_list = [r["ZoneName"] for r in read_bcsv(scen_arc.get("ZoneList.bcsv"), self.names)]
        self.names.add(self.zone_list)
        self.scenario_rows = read_bcsv(scen_arc.get("ScenarioData.bcsv"), self.names)
        self.zone_arcs = {}
        for z in self.zone_list:
            try:
                self.zone_arcs[z] = Arc(dump.read("StageData/" + z + ".arc"))
            except OSError:
                self.zone_arcs[z] = None

    def scenarios(self):
        out = []
        for r in self.scenario_rows:
            masks = {}
            for z in self.zone_list:
                masks[z] = ((r.get(z, 0) or 0) * 2) | 1
            out.append({"no": r["ScenarioNo"], "appear_power_star_obj": r.get("AppearPowerStarObj"),
                        "comet": r.get("Comet") or None, "zone_masks": masks})
        return out

    def read_zone(self, zone):
        arc = self.zone_arcs.get(zone)
        res = {"entries": [], "rails": []}
        if arc is None:
            return res
        for d in JMP_DIRS:
            for layer in LAYERS:
                for fname, blob in arc.listdir("jmp/%s/%s" % (d, layer)):
                    for r in read_bcsv(blob, self.names):
                        res["entries"].append((layer, fname, r))
        info = arc.get("jmp/Path/CommonPathInfo")
        if info is not None:
            for idx, r in enumerate(read_bcsv(info, self.names)):
                pf = arc.get("jmp/Path/CommonPathPointInfo.%d" % idx)
                pts = read_bcsv(pf, self.names) if pf is not None else []
                res["rails"].append((r, pts))
        return res

    def bcam_rows(self, zone):
        arc = self.zone_arcs.get(zone)
        if arc is None:
            return []
        f = arc.get("stage/camera/cameraparam.bcam") or arc.get("camera/cameraparam.bcam")
        return read_bcsv(f, self.names) if f is not None else []

    def build(self):
        scen = self.scenarios()
        scen_nos = [s["no"] for s in scen]
        zone_data = {z: self.read_zone(z) for z in self.zone_list}
        placements = []

        def place(zone, parent_idx, parent_mtx, active_scen, depth):
            if depth > 4:
                return
            for layer, fname, r in zone_data[zone]["entries"]:
                if fname.lower() != "stageobjinfo":
                    continue
                child = r["name"]
                world = mtx_mul(parent_mtx, mtx_tr(row_vec(r, "pos", 0.0), row_vec(r, "dir", 0.0)))
                act = active_scenarios(scen, active_scen, zone, layer)
                pi = len(placements)
                placements.append({"index": pi, "zone": child, "matrix": [r3(row) for row in world],
                                   "scenarios": act, "parent": parent_idx, "raw": raw_trs(r)})
                if child in zone_data:
                    place(child, pi, world, act, depth + 1)

        identity = mtx_tr((0, 0, 0), (0, 0, 0))
        place(self.name, -1, identity, scen_nos, 0)
        zone_instances = [(self.name, -1, identity, scen_nos)]
        for p in placements:
            zone_instances.append((p["zone"], p["index"], p["matrix"], p["scenarios"]))
        objects = []
        rails = []
        rail_key = {}
        for zone, pidx, mtx, zone_scen in zone_instances:
            zd = zone_data.get(zone)
            if zd is None:
                continue
            for r, pts in zd["rails"]:
                points = []
                for p in pts:
                    pp = [[p.get("pnt%d_%s" % (k, ax), 0.0) for ax in "xyz"] for k in range(3)]
                    points.append({"pos": r3(xform_point(mtx, pp[0])), "ctrl_in": r3(xform_point(mtx, pp[1])),
                                   "ctrl_out": r3(xform_point(mtx, pp[2])),
                                   "args": [p.get("point_arg%d" % k, -1) for k in range(8)]})
                rail_key[(zone, pidx, r.get("l_id"))] = len(rails)
                rails.append({"index": len(rails), "zone": zone, "closed": r.get("closed"),
                              "args": [r.get("path_arg%d" % k, -1) for k in range(8)], "points": points})
            for layer, fname, r in zd["entries"]:
                pos = row_vec(r, "pos", 0.0)
                rot = row_vec(r, "dir", 0.0)
                scl = row_vec(r, "scale", 1.0)
                wm = mtx_mul(mtx, mtx_tr(pos, rot))
                args = [r[k] for k in ("Obj_arg%d" % n for n in range(8)) if k in r]
                if fname.lower() == "startinfo":
                    args = [r.get("Obj_arg0", -1)]
                fields = {}
                for k, v in r.items():
                    if k in STD_FIELDS or k in SWITCH_FIELDS or k == "@raw" or k.startswith("Obj_arg"):
                        continue
                    fields[k] = v
                o = {"index": len(objects), "name": r.get("name", ""), "zone": zone, "zone_placement": pidx,
                     "local": {"pos": r3(pos), "rot": r3(rot), "scale": r3(scl)},
                     "world": {"pos": r3([wm[0][3], wm[1][3], wm[2][3]]), "rot": mtx_to_euler(wm),
                               "scale": r3(scl)},
                     "args": args,
                     "switches": {k: r[k] for k in SWITCH_FIELDS if k in r and r[k] != -1},
                     "fields": fields, "scenarios": active_scenarios(scen, zone_scen, zone, layer),
                     "raw": raw_trs(r)}
                cp = r.get("CommonPath_ID", -1)
                if cp is not None and cp >= 0:
                    o["rail"] = rail_key.get((zone, pidx, cp))
                objects.append(o)
        return {"galaxy": self.name, "zone_list": self.zone_list, "zone_placements": placements,
                "scenarios": scen, "objects": objects, "rails": rails}


def switch_key(zone_index, sw):
    return sw if sw >= 1000 else 2000 + 1000 * zone_index + sw


def g2v(v):
    return [0.0 - v[0], 0.0 + v[2], 0.0 + v[1]]


def g2p(v):
    return [x / GAME_UNIT for x in g2v(v)]


def fmt(v):
    return " ".join("%.4f" % float(x) for x in v).replace("-0.0000", "0.0000")


def fmt_csv(v):
    return ",".join("%.4f" % float(x) for x in v).replace("-0.0000", "0.0000")


def scen_mask(scen):
    m = 0
    for s in scen:
        m |= 1 << (s - 1)
    return m


def arg_opts(args, n=8):
    a = (list(args) + [-1] * n)[:n]
    return " ".join("a%d=%d" % (i, int(x)) for i, x in enumerate(a) if int(x) != -1)


def col(m, k):
    return [m[0][k], m[1][k], m[2][k]]


def matvec(r, v):
    return [0.0 + r[i][0] * v[0] + r[i][1] * v[1] + r[i][2] * v[2] for i in range(3)]


def dist_opt(x):
    u = -1 if x is None or x < 0 else x / GAME_UNIT
    return "-1" if u == -1 else "%.4f" % u


def signed16(value):
    return (value & 65535) - (65536 if value & 32768 else 0)


class GalaxyLines:
    def __init__(self, data, galaxy, kinds):
        self.d = data
        self.galaxy = galaxy
        if data["galaxy"] in ("EggStarGalaxy", "HoneyBeeKingdomGalaxy"):
            for o in data["objects"]:
                if 1 in o["scenarios"] and not (o["name"] == "PowerStar" and o["args"][0] == 1):
                    if 6 not in o["scenarios"]:
                        o["scenarios"].append(6)
        self.kinds = kinds
        self.zone_index = {z: i for i, z in enumerate(data["zone_list"])}
        self.zone_mtx = {-1: mtx_tr((0, 0, 0), (0, 0, 0))}
        for z in data["zone_placements"]:
            self.zone_mtx[z["index"]] = z["matrix"]
        self.bcam = {}
        self.cameras = []
        self.camera_key = {}
        self.lines = []

    def world(self, o):
        loc = o["local"]
        m = mtx_tr(loc["pos"], loc["rot"])
        if o["zone_placement"] >= 0:
            m = mtx_mul(self.zone_mtx[o["zone_placement"]], m)
        return m, [float(x) for x in loc["scale"]]

    def frame(self, o):
        m, s = self.world(o)
        return "%s  %s  %s  %s  %s" % (fmt(g2p(col(m, 3))), fmt(g2v(col(m, 0))), fmt(g2v(col(m, 1))),
                                         fmt(g2v(col(m, 2))), fmt(s))

    def zone_of(self, o):
        return self.zone_index.get(o["zone"], 0)

    def bcam_rows(self, zone):
        if zone not in self.bcam:
            self.bcam[zone] = {r.get("id"): r for r in self.galaxy.bcam_rows(zone)}
        return self.bcam[zone]

    def camera_index(self, zone, zone_placement, cid):
        key = (zone_placement, cid)
        if key in self.camera_key:
            return self.camera_key[key]
        row = self.bcam_rows(zone).get(cid)
        if row is None or row.get("gflag.thru", 0) == 1:
            return None
        idx = len(self.cameras)
        self.camera_key[key] = idx
        self.cameras.append((zone_placement, row))
        return idx

    def starts(self):
        for o in self.d["objects"]:
            if o["name"] != "Mario" or not o["scenarios"]:
                continue
            m, _ = self.world(o)
            f = o["fields"]
            opts = ["id=%d" % f.get("MarioNo", 0), "scen=%x" % scen_mask(o["scenarios"]),
                    "zone=%d" % self.zone_of(o)]
            cam = f.get("Camera_id", -1)
            if cam is not None and cam >= 0:
                ci = self.camera_index(o["zone"], o["zone_placement"], "s:%04x" % cam)
                if ci is not None:
                    opts.append("camera=%d" % ci)
            self.lines.append("start %s  %s  %s  %s" % (fmt(g2p(col(m, 3))), fmt(g2v(col(m, 1))),
                                                      fmt(g2v(col(m, 2))), " ".join(opts)))

    def gravity(self):
        for o in self.d["objects"]:
            kind = GRAVITY_KINDS.get(o["name"])
            if not kind or not o["scenarios"]:
                continue
            f = o["fields"]
            if f.get("Gravity_type") == "Shadow":
                continue
            opts = ["range=" + dist_opt(f.get("Range")), "distant=" + dist_opt(f.get("Distant"))]
            a = (list(o["args"]) + [-1, -1, -1])[:3]
            opts += ["a%d=%d" % (i, a[i]) for i in range(3)]
            if (f.get("Inverse") or 0) >= 1:
                opts.append("inverse")
            opts.append("scen=%x" % scen_mask(o["scenarios"]))
            for field in ("SW_A", "SW_B", "SW_APPEAR", "SW_SLEEP"):
                sw = o.get("switches", {}).get(field)
                if sw is not None:
                    opts.append("%s=%d" % (field.replace("_", "").lower(), switch_key(self.zone_of(o), sw)))
            if kind == "gwire" and o.get("rail") is not None:
                opts.append("rail=%d" % o["rail"])
            if f.get("Power") == "Light":
                opts.append("light")
            self.lines.append("gravity %s %d  %s  %s" % (kind, int(f.get("Priority") or 0), self.frame(o),
                                                       " ".join(opts)))

    def rails(self):
        for r in self.d["rails"]:
            zi = self.zone_index.get(r["zone"], 0)
            opts = [arg_opts(r["args"]), "zone=%d" % zi]
            if r.get("closed") == "CLOSE":
                opts.insert(0, "closed")
            self.lines.append("rail %d %s" % (r["index"], " ".join(x for x in opts if x)))
            for p in r["points"]:
                line = "rpoint %s  %s  %s  %s" % (fmt(g2p(p["pos"])), fmt(g2p(p["ctrl_in"])),
                                                  fmt(g2p(p["ctrl_out"])), arg_opts(p["args"]))
                self.lines.append(line.rstrip())

    def areas(self):
        for o in self.d["objects"]:
            spec = AREAS.get(o["name"])
            if not spec or not o["scenarios"]:
                continue
            cls, shape = spec
            f = o["fields"]
            sw = o.get("switches", {})
            if f.get("Validity") == "Invalid" or ("SW_A" in sw and cls != "switch") or "SW_APPEAR" in sw:
                continue
            opts = [arg_opts(o["args"]), "scen=%x" % scen_mask(o["scenarios"]), "zone=%d" % self.zone_of(o)]
            if cls == "switch":
                if "SW_A" not in sw:
                    continue
                opts.append("sw=%d" % switch_key(self.zone_of(o), sw["SW_A"]))
            if cls == "camera":
                ci = self.camera_index(o["zone"], o["zone_placement"], "c:%04x" % o["args"][0])
                opts.append("camera=%d" % (-1 if ci is None else ci))
            opts = " ".join(x for x in opts if x)
            self.lines.append("area %s %s  %s  %s" % (cls, shape, self.frame(o), opts))

    def buoy_late(self, o):
        return self.d["galaxy"] == "OceanFloaterLandGalaxy" and o["name"] in ("BigBirikyu", "Dossun")

    def first_camera(self, o, cids):
        first = None
        for cid in cids:
            ci = self.camera_index(o["zone"], o["zone_placement"], cid)
            if ci is not None and first is None:
                first = ci
        return first

    def object_line(self, o, kind, flags, gate=False):
        m, s = self.world(o)
        args = list(o["args"])
        if kind in ("flippanel", "flippanelobserver"):
            args += [-1] * (8 - len(args))
            args[6] = o["fields"].get("GroupId", -1)
        opts = ["scen=%x" % scen_mask(o["scenarios"]), arg_opts(args), "id=%d" % o["index"],
                "zone=%d" % self.zone_of(o)]
        if o.get("rail") is not None:
            opts.append("rail=%d" % o["rail"])
        if any(abs(x - 1.0) > 1e-4 for x in s):
            opts.append("scale=%s" % fmt_csv(s))
        if kind == "powerstar" and self.star_hidden(o):
            flags |= 1
        if flags:
            opts.append("flags=%d" % flags)
        sw = dict(o.get("switches", {}))
        if kind == "astrodomebluestar":
            sw.pop("SW_APPEAR", None)
        if not gate and kind in ("launchstar", "slingstar"):
            sw.pop("SW_APPEAR", None)
            sw.pop("SW_SLEEP", None)
        if any(k in sw for k in SWITCHES):
            opts.append("sw=%s" % ",".join(str(switch_key(self.zone_of(o), sw[k]) if k in sw else -1)
                                            for k in SWITCHES))
        jp = SPIN_DRIVER_CAMERA_NAMES.get(o["name"])
        set_id = o["fields"].get("CameraSetId", -1)
        has_set = set_id is not None and set_id >= 0
        multi = MULTI_EVENT_CAMERAS.get(o["name"])
        if multi and has_set:
            first = self.first_camera(o, [NTH_CAMERA % (multi[0], set_id, i) for i in range(multi[1])])
            if first is not None:
                opts.append("camera=%d" % first)
        if kind == "penguinskater" and has_set:
            self.first_camera(o, [PENGUIN_CAMERA % (phase, set_id) for phase in PENGUIN_PHASES])
        if jp and has_set:
            count = o["args"][3] if len(o["args"]) > 3 and o["args"][3] > 0 else 1
            first = self.first_camera(o, [NTH_CAMERA % (jp, set_id, i) for i in range(count)])
            if first is not None:
                opts.append("camera=%d" % first)
        if kind == "trapeze" and has_set:
            ci = self.first_camera(o, [TRAPEZE_CAMERA % set_id])
            if ci is not None:
                opts.append("camera=%d" % ci)
        if kind == "beemushroom" and has_set and args[5] > 0:
            first = self.first_camera(o, [BEE_CAMERA % (i, set_id) for i in range(1, 1 + min(args[5], 2))])
            if first is not None:
                opts.append("camera=%d" % first)
        return "object %s  %s  %s  %s  %s" % (kind, fmt(g2p(col(m, 3))), fmt(g2v(col(m, 1))),
                                              fmt(g2v(col(m, 2))), " ".join(x for x in opts if x))

    def object_order(self):
        return sorted(self.d["objects"], key=lambda o: (
            o["name"] == "TimerSwitch",
            self.d["galaxy"] == "HoneyBeeKingdomGalaxy" and o["name"] == "LuigiEvent",
            o["name"] == "ScenarioStarter" or self.buoy_late(o),
            self.buoy_late(o)))

    def objects(self):
        for o in self.object_order():
            k = self.kinds.get(o["name"])
            if not o["scenarios"]:
                continue
            if o["name"] == "TimerSwitch" and not (
                    (self.d["galaxy"] == "EggStarGalaxy" and o["index"] == 785) or
                    (self.d["galaxy"] == "IceVolcanoGalaxy" and o["index"] in (1196, 1197, 1198))):
                continue
            if not k:
                continue
            self.lines.append(self.object_line(o, k[0], k[1]))

    def parts(self):
        seen = set()
        for o in self.d["objects"]:
            name = SWITCHED_PARTS.get(o["name"])
            sw = o.get("switches", {})
            if not name or not o["scenarios"] or name in seen:
                continue
            key = sw.get("SW_SLEEP", sw.get("SW_APPEAR"))
            if key is None:
                continue
            seen.add(name)
            self.lines.append("part %s sw=%d" % (name, switch_key(self.zone_of(o), key)))
        for o in self.d["objects"]:
            pair = SWAPPED_PARTS.get(o["name"])
            if not pair or not o["scenarios"] or o["name"] in seen:
                continue
            demo = [d for d in self.d["objects"] if d["name"] == "DemoGroup" and d["zone"] == o["zone"]
                    and "SW_APPEAR" in d.get("switches", {})]
            if not demo:
                continue
            seen.add(o["name"])
            key = switch_key(self.zone_of(demo[0]), demo[0]["switches"]["SW_APPEAR"])
            for name, mode in pair:
                self.lines.append("part %s sw=%d mode=%s" % (name, key, mode))

    def star_hidden(self, o):
        sid = o["args"][0] if o["args"] else -1
        for sc in self.d.get("scenarios", []):
            if sc.get("no") == sid and (sc.get("appear_power_star_obj") or sc.get("comet") == "Purple"):
                return True
        return False

    def camera_line(self, idx, zone_placement, r):
        zm = self.zone_mtx[zone_placement]
        rot = [row[:3] for row in zm]

        def vec(name, default):
            return [float(r.get("%s.%s" % (name, c), d)) for c, d in zip("XYZ", default)]

        def zpoint(v):
            w = matvec(rot, v)
            return g2p([w[0] + zm[0][3], w[1] + zm[1][3], w[2] + zm[2][3]])

        def zdir(v):
            return g2v(matvec(rot, v))

        def div(v):
            return [x / GAME_UNIT for x in v]

        t = r.get("camtype", "CAM_TYPE_XZ_PARA")
        dist = r.get("dist", 1200.0)
        angle_a, angle_b = r.get("angleA", 0.0), r.get("angleB", 0.3)
        axis, wpoint, up = vec("axis", (0, 1, 0)), vec("wpoint", (0, 0, 0)), vec("up", (0, 1, 0))
        woff = vec("woffset", (0, 100, 0))
        o = ["dist=%.4f" % (dist / GAME_UNIT)]
        if t == "CAM_TYPE_XZ_PARA":
            ax, ay = angle_b, angle_a - PI / 2
            d = max(dist, 300.0)
            off = [d * (cos(ax) * sin(ay)), d * sin(ax), d * (cos(ax) * cos(ay))]
            o.append("offset=%s" % fmt_csv(div(zdir(off))))
        elif t == "CAM_TYPE_POINT_FIX":
            ax, ay = axis[0], axis[1]
            off = [dist * (cos(ax) * sin(ay)), dist * sin(ax), dist * (cos(ax) * cos(ay))]
            o += ["wpoint=%s" % fmt_csv(zpoint(wpoint)), "offset=%s" % fmt_csv(div(zdir(off))),
                  "up=%s" % fmt_csv(zdir(up))]
        elif t == "CAM_TYPE_EYEPOS_FIX":
            o.append("wpoint=%s" % fmt_csv(zpoint(wpoint)))
        elif t in ("CAM_TYPE_WONDER_PLANET", "CAM_TYPE_FOLLOW", "CAM_TYPE_MEDIAN_PLANET",
                   "CAM_TYPE_WATER_FOLLOW"):
            o.append("axis=%s" % fmt_csv([axis[0] / GAME_UNIT, axis[1] / GAME_UNIT, 0.0]))
            o.append("rate=%.4f" % axis[2])
            if t == "CAM_TYPE_WATER_FOLLOW":
                o.append("up=0,%.6f,0" % dist)
        elif t == "CAM_TYPE_CHARMED_FIX":
            o += ["base=%s" % fmt_csv(zpoint(axis)), "up=%s" % fmt_csv(zdir(up)),
                  "local=%s" % fmt_csv(div(wpoint))]
        elif t in ("CAM_TYPE_TOWER", "CAM_TYPE_TOWER_POS", "CAM_TYPE_FRONT_AND_BACK", "CAM_TYPE_TRUNDLE"):
            o += ["wpoint=%s" % fmt_csv(zpoint(wpoint)), "axis=%s" % fmt_csv(zdir(axis)),
                  "up=%s" % fmt_csv([up[0] / GAME_UNIT, up[1], up[2]])]
        elif t == "CAM_TYPE_CHARMED_VECREG_TOWER":
            register = str(r.get("string", ""))
            if register.startswith(BOSS_STINKBUG):
                o += ["reg=bossstinkbug,%d" % int(register.rsplit("-", 1)[1])]
            o += ["wpoint=%s" % fmt_csv(zpoint(wpoint)), "axis=%s" % fmt_csv(zdir(axis))]
        elif t == "CAM_TYPE_OBJ_PARALLEL":
            lv = [cos(angle_a) * sin(angle_b), -sin(angle_a), cos(angle_a) * cos(angle_b)]
            lu = [sin(angle_a) * sin(angle_b), cos(angle_a), sin(angle_a) * cos(angle_b)]
            o.append("local=%s" % fmt_csv([x * dist / GAME_UNIT for x in lv]))
            o.append("lup=%s" % fmt_csv(lu))
        elif t == "CAM_TYPE_SPIRAL_DEMO":
            packed = int(r.get("num1", 60 << 16))
            end, start = signed16(packed >> 16), signed16(packed)
            o += ["wpoint=%s" % fmt_csv([0.0, wpoint[1] / GAME_UNIT, wpoint[2] / GAME_UNIT]),
                  "axis=%s" % fmt_csv([0.0, axis[1] / GAME_UNIT, axis[2] / GAME_UNIT]),
                  "lup=%s,0,0" % r.get("num2", 0), "num1=%d" % end, "num2=%d" % start]
            angle_a, angle_b = wpoint[0], axis[0]
        if t == "CAM_TYPE_FRONT_AND_BACK":
            o.append("zx=%s" % fmt_csv(zdir([1.0, 0.0, 0.0])))
            angle_a, angle_b = radians(angle_a), radians(angle_b)
        o.append("zup=%s" % fmt_csv(zdir([0.0, 1.0, 0.0])))
        o += ["angleA=%.4f" % angle_a, "angleB=%.4f" % angle_b]
        o.append("woffset=%s" % fmt_csv(div(zdir(woff))))
        lo, lov = r.get("loffset", 0.0), r.get("loffsetv", 0.0)
        if lo or lov:
            o.append("loffset=%.4f,%.4f" % (lo / GAME_UNIT, lov / GAME_UNIT))
        o.append("camint=%d" % r.get("camint", 120))
        flags = 0
        for bit, key in enumerate(("eflag.enableErpFrame", "eflag.enableEndErpFrame", "flag.antibluroff",
                                   "flag.noreset")):
            if r.get(key):
                flags |= 1 << bit
        if flags:
            o.append("flags=%d" % flags)
        if r.get("camendint"):
            o.append("camendint=%d" % r["camendint"])
        if r.get("evfrm"):
            o.append("evfrm=%d" % r["evfrm"])
        for k in ("num1", "num2"):
            if r.get(k) and t != "CAM_TYPE_SPIRAL_DEMO":
                o.append("%s=%d" % (k, r[k]))
        if r.get("fovy", 45.0) != 45.0:
            o.append("fovy=%.2f" % r["fovy"])
        if r.get("roll", 0.0):
            o.append("roll=%.4f" % r["roll"])
        vp = r.get("vpanuse", 1)
        o.append("vpan=%d,%.4f,%.4f,%.4f,%.4f" % (vp, r.get("upper", 0.3), r.get("lower", 0.1),
                                                  r.get("uplay", 300.0) / GAME_UNIT,
                                                  r.get("lplay", 800.0) / GAME_UNIT))
        return "camera %d %s %s" % (idx, t, " ".join(o))

    def demo_cameras(self):
        for o in self.d["objects"]:
            if (o["name"] == "DemoGroup" and o["zone"] == "BlueStarZone" and
                    o["fields"].get("DemoName") == GCAPTURE_DEMO and
                    o["fields"].get("TimeSheetName") == "GCaptureAppear"):
                self.camera_index(o["zone"], o["zone_placement"], GCAPTURE_CAMERA)
            if (o["name"] == "DemoGroup" and o["zone"] == "FloaterLandZone" and
                    o["fields"].get("DemoName") == FLOATER_DEMO and
                    o["fields"].get("TimeSheetName") == "BreakableFixation1"):
                for part in FLOATER_PARTS:
                    self.camera_index(o["zone"], o["zone_placement"], FLOATER_CAMERA % part)

    def build(self):
        self.starts()
        self.gravity()
        self.rails()
        self.areas()
        self.objects()
        self.parts()
        self.demo_cameras()
        out = [x.rstrip() for x in self.lines]
        out.append("")
        for i, (zp, row) in enumerate(self.cameras):
            out.append(self.camera_line(i, zp, row))
        return out

    def extra_candidates(self):
        out = []
        for o in self.object_order():
            if not o["scenarios"]:
                continue
            k = self.kinds.get(o["name"])
            kind, flags = k if k else ("?", 0)
            out.append(self.object_line(o, kind, flags, gate=True))
            if not k:
                out.append(self.object_line(o, o["name"].lower(), 0, gate=True))
        for zp in sorted(self.zone_mtx):
            zone = self.d["galaxy"] if zp < 0 else self.d["zone_placements"][zp]["zone"]
            for _, row in sorted(self.bcam_rows(zone).items(), key=lambda kv: str(kv[0])):
                out.append(self.camera_line(0, zp, row))
        return out
