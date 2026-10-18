import struct

from common import yaz0_decompress
from ui_data import arc_get, bcsv
from ui_math import PI, cos, cube, radians, sin, tan


INTRO_STAGES = [("STAGE_HONEYHIVE", "HoneyBeeKingdomGalaxy", 3), ("STAGE_GOODEGG", "EggStarGalaxy", 6),
                ("STAGE_BEACHBOWL", "HeavenlyBeachGalaxy", 6), ("STAGE_FREEZEFLAME", "IceVolcanoGalaxy", 6),
                ("STAGE_DEEPDARK", "OceanPhantomCaveGalaxy", 6),
                ("STAGE_GUSTYGARDEN", "CosmosGardenGalaxy", 6),
                ("STAGE_BUOYBASE", "OceanFloaterLandGalaxy", 2)]


def arc_find_name(arc, name):
    low = name.lower()
    for p, b in arc.files:
        if p.lower().endswith("/" + low):
            return yaz0_decompress(b)
    return None


def hermite(v0, v1, o0, i1, s):
    s2, s3 = s * s, s * s * s
    return v0 * (2 * s3 - 3 * s2 + 1) + v1 * (-2 * s3 + 3 * s2) + o0 * (s3 - 2 * s2 + s) + i1 * (s3 - s2)


def key_index(keys, f):
    i = 0
    while i + 1 < len(keys) and keys[i + 1][0] <= f:
        i += 1
    return i


class CameraAnim:
    def __init__(self, data):
        self.kind = data[4:8].decode()
        self.frames, voff = struct.unpack_from(">II", data, 0x18)
        entry = 0x20
        pool_at = entry + voff
        nfloats = struct.unpack_from(">I", data, pool_at)[0] // 4
        pool = struct.unpack_from(">%df" % nfloats, data, pool_at + 4)
        self.tracks = []
        for c in range(8):
            if self.kind == "CKAN":
                self.tracks.append(self.read_keys(pool, *struct.unpack_from(">III", data, entry + c * 12)))
            else:
                count, first = struct.unpack_from(">II", data, entry + c * 8)
                self.tracks.append([(float(k), pool[first + k], None, None) for k in range(count)])

    @staticmethod
    def read_keys(pool, count, first, typ):
        if count == 1:
            return [(0.0, pool[first], 0.0, 0.0)]
        stride = 3 if typ == 0 else 4
        keys = []
        for k in range(max(count, 1)):
            v = pool[first + k * stride:first + k * stride + stride]
            keys.append((v[0], v[1], v[2], v[2] if stride == 3 else v[3]))
        return keys

    def value(self, c, t):
        keys = self.tracks[c]
        if len(keys) == 1:
            return keys[0][1]
        if self.kind != "CKAN":
            i = int(t)
            r = min(max(t - i, 0.0), 1.0)
            if i < len(keys) - 1:
                return keys[i][1] + (keys[i + 1][1] - keys[i][1]) * r
            return keys[-1][1]
        if t <= keys[0][0]:
            return keys[0][1]
        if t >= keys[-1][0]:
            return keys[-1][1]
        i = max(k for k in range(len(keys)) if keys[k][0] <= t)
        t0, v0, _, m0 = keys[i]
        t1, v1, m1, _ = keys[i + 1]
        d = t1 - t0
        u = (t - t0) / d
        m0, m1 = m0 / 30.0, m1 / 30.0
        u3, u2 = cube(u), u * u
        return ((2 * u3 - 3 * u2 + 1) * v0 + (u3 - 2 * u2 + u) * d * m0 +
                (-2 * u3 + 3 * u2) * v1 + (u3 - u2) * d * m1)

    def sample(self, t):
        return [self.value(c, t) for c in range(8)]


class ActorCanm:
    def __init__(self, data):
        self.frame_count = struct.unpack_from(">I", data, 0x18)[0]
        desc = [struct.unpack_from(">III", data, 0x20 + i * 12) for i in range(8)]
        nflt = struct.unpack_from(">I", data, 0x80)[0] // 4
        flt = struct.unpack_from(">%df" % nflt, data, 0x84)
        self.tracks = []
        for cnt, first, mode in desc:
            if cnt == 1:
                self.tracks.append(flt[first])
            else:
                stride = 4 if mode == 1 else 3
                self.tracks.append([flt[first + i * stride:first + i * stride + stride] for i in range(cnt)])

    @staticmethod
    def sample_track(k, f):
        if not isinstance(k, list):
            return float(k)
        if f <= k[0][0]:
            return float(k[0][1])
        if f >= k[-1][0]:
            return float(k[-1][1])
        i = key_index(k, f)
        t0, v0, o0 = k[i][0], k[i][1], k[i][-1]
        t1, v1, i1 = k[i + 1][0], k[i + 1][1], k[i + 1][2]
        d = t1 - t0
        if d <= 0:
            return float(v1)
        return float(hermite(v0, v1, o0, i1, (f - t0) / d))

    def sample(self, f):
        return [self.sample_track(t, f) for t in self.tracks]


class BckTrack:
    def __init__(self, keys=None, const=None):
        self.keys, self.const = keys, const

    def sample(self, f):
        if self.keys is None:
            return self.const
        k = self.keys
        if f <= k[0][0]:
            return k[0][1]
        if f >= k[-1][0]:
            return k[-1][1]
        i = key_index(k, f)
        t0, v0, _, o0 = k[i]
        t1, v1, i1, _ = k[i + 1]
        d = t1 - t0
        if d <= 0:
            return v1
        s = (f - t0) / d
        s2, s3 = s * s, s * s * s
        return (v0 * (2 * s3 - 3 * s2 + 1) + v1 * (-2 * s3 + 3 * s2)
                + o0 * d * (s3 - 2 * s2 + s) + i1 * d * (s3 - s2))


class Bck:
    def __init__(self, data):
        o = 0x20
        for _ in range(struct.unpack_from(">I", data, 0x0C)[0]):
            if data[o:o + 4] == b"ANK1":
                break
            o += struct.unpack_from(">I", data, o + 4)[0]
        rot_shift = data[o + 9]
        self.frame_count = struct.unpack_from(">H", data, o + 0x0A)[0]
        nj, ns, nr, nt = struct.unpack_from(">HHHH", data, o + 0x0C)
        joff, soff, roff, toff = (o + v for v in struct.unpack_from(">IIII", data, o + 0x14))
        tables = {"s": ([float(v) for v in struct.unpack_from(">%df" % ns, data, soff)], 1.0, 1.0),
                  "r": ([float(v) for v in struct.unpack_from(">%dh" % nr, data, roff)], 0.0,
                        float(1 << rot_shift) * PI / 32768.0),
                  "t": ([float(v) for v in struct.unpack_from(">%df" % nt, data, toff)], 0.0, 1.0)}
        self.tracks = []
        for j in range(nj):
            comp = {"s": [None] * 3, "r": [None] * 3, "t": [None] * 3}
            for axis in range(3):
                base = joff + j * 54 + axis * 18
                for k, key in enumerate("srt"):
                    dat, default, vs = tables[key]
                    cnt, idx, tmode = struct.unpack_from(">HHH", data, base + k * 6)
                    comp[key][axis] = self.track(dat, default, vs, cnt, idx, tmode)
            self.tracks.append(comp)

    @staticmethod
    def track(dat, default, vs, cnt, idx, tmode):
        if cnt == 0:
            return BckTrack(const=default)
        if cnt == 1:
            return BckTrack(const=dat[idx] * vs)
        stride = 4 if tmode == 1 else 3
        keys = []
        for i in range(cnt):
            raw = dat[idx + i * stride:idx + i * stride + stride]
            keys.append((raw[0], raw[1] * vs, raw[2] * vs, (raw[3] if stride == 4 else raw[2]) * vs))
        return BckTrack(keys=keys)

    def sample(self, joint, frame):
        c = self.tracks[joint]
        return tuple([float(t.sample(frame)) for t in c[key]] for key in "srt")


def cam_samples(anim, step):
    out = []
    for f in range(0, anim.frames + step, step):
        px, py, pz, wx, wy, wz = anim.sample(min(f, anim.frames))[:6]
        out.append((round(-px), round(pz), round(py), round(-wx), round(wz), round(wy)))
    return out


def scenario_intro(ctx):
    step = 4
    rows, samples = [], []
    for stage, galaxy, missions in INTRO_STAGES:
        arc = ctx.arc("StageData/%s.arc" % galaxy)
        for m in range(1, missions + 1):
            data = arc_find_name(arc, "StartScenario%d.canm" % m)
            if data is None:
                continue
            anim = CameraAnim(data)
            first = len(samples)
            samples += cam_samples(anim, step)
            rows.append((stage, m, anim.frames, first, len(samples) - first))
    h = ["", "#define INTRO_STEP %d" % (step), "",
         "typedef struct { int16_t stage; uint8_t mission, pad; uint16_t frames, first, count; } "
         "IntroCamera;",
         "typedef struct { int32_t eye[3], watch[3]; } IntroSample;", "",
         "static const IntroCamera intro_cameras[] = {"]
    for r in rows:
        h.append("    { %s, %d, 0, %d, %d, %d }," % r)
    h.append("};")
    h.append("#define INTRO_CAMERA_COUNT %d" % len(rows))
    h.append("static const IntroSample intro_samples[] = {")
    for s in samples:
        h.append("    { { %d, %d, %d }, { %d, %d, %d } }," % s)
    h.append("};")
    return h


DOME_GALAXY = {"EggStarGalaxy": "GALAXY_GOODEGG", "HoneyBeeKingdomGalaxy": "GALAXY_HONEYHIVE"}
DOME_POWER_STARS = {"EggStarGalaxy": 0, "HoneyBeeKingdomGalaxy": 3, "SurfingLv1Galaxy": 5,
                    "FlipPanelExGalaxy": 7}
DOME_FIGURE = {0: None, 1: "figure_minihatenagalaxy", 2: "figure_miniaturegalaxyunknownkoopa"}
DOME_OPEN_FIGURE = {"EggStarGalaxy": "figure_minieggstargalaxy",
                    "HoneyBeeKingdomGalaxy": "figure_minihoneybeekingdomgalaxy"}


def astro_dome(ctx):
    arc = ctx.arc("StageData/AstroDome.arc")

    def table(path):
        return bcsv(arc_get(arc, path))

    minis = [r for r in table("jmp/placement/layera/objinfo") if r["name"].startswith("Mini")]

    def index_of(r):
        name = r["name"][4:]
        if r["Obj_arg0"] == 2:
            return len([o for o in minis if o["Obj_arg0"] != 2])
        return len([o for o in minis if o["Obj_arg0"] != 2 and o is not r and
                    DOME_POWER_STARS[o["name"][4:]] < DOME_POWER_STARS[name]])

    rows = []
    for r in minis:
        name = r["name"][4:]
        gba = DOME_GALAXY.get(name, "-1")
        fig = DOME_OPEN_FIGURE.get(name) if gba != "-1" else DOME_FIGURE[r["Obj_arg0"]]
        rows.append((index_of(r), name, r["Obj_arg0"], gba, fig or "figure_minihatenagalaxy"))
    rows.sort()
    cube = [r for r in table("jmp/placement/common/areaobjinfo") if r["name"] == "AstroChangeStageCube"][0]
    cx, cy, cz = cube["pos_x"], cube["pos_y"], cube["pos_z"]
    hx, hh, hz = 500 * cube["scale_x"], 1000 * cube["scale_y"], 500 * cube["scale_z"]
    obs = [r for r in table("jmp/placement/common/objinfo") if r["name"] == "AstroDomeDemoAstroGalaxy"][0]
    sheet = ctx.arc("ObjectData/DemoSheet.arc")
    parts = bcsv(arc_get(sheet, "demosheet/demomariojumpout/demomariojumpouttime.bcsv"))
    cam = CameraAnim(arc_find_name(ctx.arc("ObjectData/AstroDomeDemoCamera.arc"), "AstroDomeDemoCamera.canm"))
    step = 4
    samples = cam_samples(cam, step)
    fovy = cam.sample(0)[7]
    h = ["", "typedef struct { const char* name; int8_t type, galaxy; const uint8_t* figure; } AdMini;", "",
         "static const AdMini ad_minis[%d] = {" % len(rows)]
    for idx, name, typ, gba, fig in rows:
        h.append('    { "%s", %d, %s, %s },' % (name, typ, gba, fig))
    h.append("};")
    h.append("#define AD_MINI_COUNT %d" % len(rows))
    h.append("")
    h.append("")
    h.append("#define AD_EXIT_MIN_X %d" % round(-(cx + hx)))
    h.append("#define AD_EXIT_MAX_X %d" % round(-(cx - hx)))
    h.append("#define AD_EXIT_MIN_Y %d" % round(cz - hz))
    h.append("#define AD_EXIT_MAX_Y %d" % round(cz + hz))
    h.append("#define AD_EXIT_MIN_Z %d" % round(cy))
    h.append("#define AD_EXIT_MAX_Z %d" % round(cy + hh))
    h.append("")
    h.append("#define AD_DEMO_OBS_X %d" % round(-obs["pos_x"]))
    h.append("#define AD_DEMO_OBS_Y %d" % round(obs["pos_z"]))
    h.append("#define AD_DEMO_OBS_Z %d" % round(obs["pos_y"]))
    h.append("")
    h.append("")
    for p, n in zip(parts, ("AD_PART_APPEAR", "AD_PART_START", "AD_PART_JUMPOUT")):
        h.append("#define %-18s %3d" % (n, p["TotalStep"]))
    h.append("")
    h.append("")
    h.append("#define AD_CAM_STEP %d" % step)
    h.append("#define AD_CAM_FRAMES %d" % cam.frames)
    h.append("#define AD_CAM_FOVY_X10 %d" % round(fovy * 10))
    h.append("static const int32_t ad_cam_samples[%d][6] = {" % len(samples))
    for s in samples:
        h.append("    { %d, %d, %d, %d, %d, %d }," % s)
    h.append("};")
    return h


def rot_cols(rx, ry, rz):
    sx, cx = sin(rx), cos(rx)
    sy, cy = sin(ry), cos(ry)
    sz, cz = sin(rz), cos(rz)
    mx = [[1, 0, 0], [0, cx, -sx], [0, sx, cx]]
    my = [[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]
    mz = [[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]]
    m = matmul3(matmul3(mz, my), mx)
    return [m[r][c] for c in range(3) for r in range(3)]


def matmul3(a, b):
    return [[a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] for j in range(3)] for i in range(3)]


def powerstar(ctx):
    arc = ctx.arc("ObjectData/PowerStar.arc")
    h = ["", "typedef struct { int16_t axis[3][3]; int16_t t[3]; int16_t scale; } StarAnimFrame;",
         "typedef struct { int16_t eye[3], target[3]; int16_t twist16; } StarCamFrame;", ""]
    for name, sym in (("scenarioappear", "star_anim_scenario_appear"),
                      ("scenariohide", "star_anim_scenario_hide"),
                      ("powerstarget", "star_anim_get")):
        b = Bck(arc_get(arc, "powerstar/%s.bck" % name))
        rows = []
        for f in range(b.frame_count + 1):
            s, r, t = b.sample(0, f)
            axis = [round(v * 16384) for v in rot_cols(*r)]
            rows.append(tuple(axis + [round(v) for v in t] + [round(s[0] * 256)]))
        h.append("")
        h.append("#define %s_FRAMES %d" % (sym.upper(), b.frame_count))
        h.append("static const StarAnimFrame %s[%d] = {" % (sym, len(rows)))
        for r in rows:
            h.append("    { { { %d, %d, %d }, { %d, %d, %d }, { %d, %d, %d } }, { %d, %d, %d }, %d }," % r)
        h.append("};")
        h.append("")
    cam = ActorCanm(arc_get(arc, "powerstar/powerstarget.canm"))
    fovy = cam.sample(0)[7]
    h.append("")
    h.append("#define STAR_CAM_FRAMES %d" % cam.frame_count)
    h.append("#define STAR_CAM_FOVY_TAN_X4096 %d" % (round(tan(radians(fovy / 2)) * 4096)))
    h.append("static const StarCamFrame star_cam_get[%d] = {" % (cam.frame_count + 1))
    for f in range(cam.frame_count + 1):
        s = cam.sample(f)
        row = tuple(round(v) for v in s[0:6]) + (round(s[6] * 16),)
        h.append("    { { %d, %d, %d }, { %d, %d, %d }, %d }," % row)
    h.append("};")
    return h
