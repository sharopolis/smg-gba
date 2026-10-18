import math
import struct

from common import Dump, cstr, jhash, parse_args, s32, u16, u32, write_out
from procedural_sky import sky_space
from textures_deflate import deflate_medium, deflate_slow
from textures_png import LIBPNG_FILTERS, filter_rows, png, window_bits

EFFECT = "ParticleData/Effect.arc"
JPA_UNIT = 25.0
BEAMS = (
    ("kamellabeamfire", "KameckBeamFire00"),
    ("kamellabeamfireready", "KameckBeamFireReady00"),
    ("kamellabeamturtle", "KameckBeamTurtle00"),
    ("kamellabeamturtleready", "KameckBeamTurtleReady00"),
)
K = 0.004
SECTION = ((10.0, -10.0), (0.0, 10.0), (-10.0, -10.0))
CAGE = ((-0.24249, 0.14), (-0.0, 0.28), (0.24249, 0.14), (0.24249, -0.14), (-0.0, -0.28), (-0.24249, -0.14))
CAGE_UV = (
    "67193 33808 99421 64047 67193 65880 67193 33808 99421 35641 99421 64047 "
    "65880 33964 35641 66192 33808 33964 65880 33964 64047 66192 35641 66192 "
    "32650 00579 02411 32807 00579 00579 32650 00579 30817 32807 02411 32807 "
    "99421 32650 67193 02411 99421 00579 99421 32650 67193 30817 67193 02411 "
    "00579 66192 30817 33964 32650 66192 00579 66192 02411 33964 30817 33964 "
    "66036 32650 33808 02411 66036 00579 66036 32650 33808 30817 33808 02411"
).split()
TAIL_RADII = (85, 79, 70, 62, 55, 49, 43, 43)
TAIL_LINK = 70.0


def text(lines):
    return ("\n".join(lines) + "\n").encode()


def mtl(names, tab=True, kd=False):
    lines = []
    for name, tex in names:
        lines.append("newmtl " + name)
        if kd:
            lines.append("Kd 1 1 1")
        lines.append(("\t" if tab else "") + "map_Kd " + tex)
    return text(lines)


def creeper_stalk():
    lines = ["mtllib creeperstalk.mtl"]
    for y in (0.0, 250.0):
        for x, z in SECTION:
            lines.append("v %.5f %.5f %.5f" % (x * K, y * K, z * K))
    uv = []
    faces = []
    for k in range(3):
        a = k
        b = (k + 1) % 3
        base = len(uv)
        uv += [(1.0, 0.0), (0.0, 0.0), (0.0, 0.25), (1.0, 0.25)]
        q = [(a + 1, base + 1), (b + 1, base + 2), (b + 4, base + 3), (a + 4, base + 4)]
        faces += [[q[0], q[2], q[1]], [q[0], q[3], q[2]]]
    lines += ["vt %.5f %.5f" % t for t in uv]
    lines.append("usemtl creeperstalk_m0")
    lines += ["f " + " ".join("%d/%d" % c for c in f) for f in faces]
    return text(lines), mtl([("creeperstalk_m0", "creeperstalk_m0.png")])


def plant_stalk():
    length = 100.0
    front = (0.0, 0.0, 10.0)
    right = (10.0, 0.0, -10.0)
    left = (-10.0, 0.0, -10.0)
    pts = []
    vts = []
    flines = []
    mats = []
    for i, (a, b) in enumerate(((front, right), (left, front), (right, left))):
        mname = "plantstalk_m%d" % i
        mats.append((mname, mname + ".png"))
        quad = []
        for p, y, u in ((a, 0.0, 0.0), (b, 0.0, 1.0), (b, length, 1.0), (a, length, 0.0)):
            q = (p[0], y, p[2])
            if q not in pts:
                pts.append(q)
            vts.append((u, 1.0 - y / 30.0))
            quad.append((pts.index(q) + 1, len(vts)))
        p0 = pts[quad[0][0] - 1]
        p1 = pts[quad[1][0] - 1]
        p3 = pts[quad[3][0] - 1]
        e1 = [p1[c] - p0[c] for c in range(3)]
        e2 = [p3[c] - p0[c] for c in range(3)]
        nx = e1[1] * e2[2] - e1[2] * e2[1]
        nz = e1[0] * e2[1] - e1[1] * e2[0]
        if nx * (a[0] + b[0]) / 2 + nz * (a[2] + b[2]) / 2 < 0:
            quad = quad[::-1]
        flines.append("usemtl " + mname)
        for t in ((0, 1, 2), (0, 2, 3)):
            flines.append("f " + " ".join("%d/%d" % quad[k] for k in t))
    obj = ["mtllib plantstalk.mtl"] + ["v %.5f %.5f %.5f" % tuple(c * K for c in p) for p in pts]
    obj += ["vt %.5f %.5f" % t for t in vts] + flines
    return text(obj), mtl(mats)


def lod_of(obj):
    out = []
    for line in obj.decode().splitlines():
        t = line.split()
        if t and t[0] in ("v", "vt"):
            line = t[0] + " " + " ".join("%.6f" % float(x) for x in t[1:])
        out.append(line)
    return text(out)


def tail_chain():
    sides = len(CAGE)
    lines = ["mtllib dinopackuntailchain.mtl"]
    for r, radius in enumerate(TAIL_RADII):
        for x, y in CAGE:
            n = math.sqrt(x * x + y * y)
            rad = radius * K
            lines.append("v %.5f %.5f %.5f" % (rad * (x / n) + 0.0, rad * (y / n) + 0.0, r * TAIL_LINK * K))
    for i in range(0, len(CAGE_UV), 2):
        lines.append("vt 0.%s 0.%s" % (CAGE_UV[i], CAGE_UV[i + 1]))
    lines.append("usemtl dinopackuntailchain_m0")
    for s in range(len(TAIL_RADII) - 1):
        for k in range(sides):
            a = s * sides + k + 1
            b = s * sides + (k + 1) % sides + 1
            t = 6 * k + 1
            lines.append("f %d/%d %d/%d %d/%d" % (a, t, b + sides, t + 1, b, t + 2))
            lines.append("f %d/%d %d/%d %d/%d" % (a, t + 3, a + sides, t + 4, b + sides, t + 5))
    obj = text(lines)
    m = mtl([("dinopackuntailchain_m0", "dinopackuntailchain_m0.png")])
    return obj, obj.replace(b"dinopackuntailchain.mtl", b"lod.mtl"), m


def billboard(name, material, texture, half, fmt):
    h = fmt % half
    lines = ["mtllib %s.mtl" % name]
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        lines.append("v %s%s %s%s 0" % ("-" if sx < 0 else "", h, "-" if sy < 0 else "", h))
    lines += ["vt 0 0", "vt 1 0", "vt 1 1", "vt 0 1", "usemtl " + material, "f 1/1 2/2 3/3", "f 1/1 3/3 4/4"]
    return text(lines), mtl([(material, texture)], tab=False, kd=True)


def rope():
    x = 19.0 * K
    z = -500.0 * K
    v = 500.0 * 0.003
    lines = ["mtllib trapezerope.mtl"]
    lines += ["v %g 0 %g" % (sx, sz) for sz in (0.0, z) for sx in (-x, x)]
    lines += ["vt 0 0", "vt 1 0", "vt 0 %g" % v, "vt 1 %g" % v, "usemtl rope"]
    lines += ["f 1/1 2/2 3/3", "f 2/2 4/4 3/3", "f 3/3 2/2 1/1", "f 3/3 4/4 2/2"]
    return text(lines), mtl([("rope", "trapezerope.png")], tab=False)


def particle_ids(data):
    n, nf, off, size = struct.unpack_from(">iiiI", data, 0)
    cols = {}
    for i in range(nf):
        cols[u32(data, 16 + i * 12)] = u16(data, 16 + i * 12 + 8)
    strings = off + n * size
    ids = {}
    for i in range(n):
        row = off + i * size
        ids[cstr(data, strings + u32(data, row + cols[jhash("name")]))] = s32(data, row + cols[jhash("id")])
    return ids


def particle_sizes(jpc):
    sizes = {}
    p = 16
    for _ in range(u16(jpc, 8)):
        rid, nblocks = struct.unpack_from(">HH", jpc, p)
        p += 8
        for _ in range(nblocks):
            tag, size = struct.unpack_from(">4sI", jpc, p)
            if tag == b"BSP1":
                sizes[rid] = struct.unpack_from(">2f", jpc, p + 16)
            p += size
    return sizes


def figures(dump):
    arc = dump.arc(EFFECT)
    ids = particle_ids(arc.find("particlenames.bcsv"))
    sizes = particle_sizes(arc.find("particles.jpc"))

    def half(name):
        w, h = sizes[ids[name]]
        if w != h:
            raise ValueError("non-square particle " + name)
        return w * JPA_UNIT * K

    out = {}
    fig = "assets/figures/"
    obj, m = creeper_stalk()
    out[fig + "creeperstalk/creeperstalk.obj"] = obj
    out[fig + "creeperstalk/creeperstalk.mtl"] = m
    obj, m = plant_stalk()
    out[fig + "plantstalk/plantstalk.obj"] = obj
    out[fig + "plantstalk/plantstalk.mtl"] = m
    out[fig + "plantstalk/lod.obj"] = lod_of(obj)
    out[fig + "plantstalk/lod.mtl"] = m
    obj, lod, m = tail_chain()
    d = fig + "dinopackuntailchain/"
    out[d + "dinopackuntailchain.obj"] = obj
    out[d + "dinopackuntailchain.mtl"] = m
    out[d + "lod.obj"] = lod
    out[d + "lod.mtl"] = m
    ring = half("MarioGCaptureLight00")
    obj, m = billboard("capturebubble", "CaptureLight", "capture_ring.png", ring, "%.3f")
    out[fig + "capturebubble/capturebubble.obj"] = obj
    out[fig + "capturebubble/capturebubble.mtl"] = m
    for name, particle in BEAMS:
        obj, m = billboard(name, "NativeParticle", name + ".png", half(particle), "%r")
        out[fig + "%s/%s.obj" % (name, name)] = obj
        out[fig + "%s/%s.mtl" % (name, name)] = m
    obj, m = rope()
    out[fig + "trapezerope/trapezerope.obj"] = obj
    out[fig + "trapezerope/trapezerope.mtl"] = m
    return out


def mluc(text):
    s = text.encode("utf-16-be")
    return b"mluc" + struct.pack(">5I", 0, 1, 12, 0x656E5553, len(s)) + struct.pack(">I", 28) + s


def icc_numbers(tag, values):
    return tag + bytes(4) + struct.pack(">%di" % len(values), *values)


def gimp_srgb_icc():
    trc = b"para" + bytes(4) + struct.pack(">HH5i", 3, 0, 0x26666, 0xF2A7, 0xD59, 0x13D0, 0xA5B)
    white = (0xF6D6, 0x10000, 0xD32D)
    chad = (0x10C42, 0x5DE, -0xCDB, 0x793, 0xFD90, -0x45F, -0x25E, 0x3DC, 0xC06E)
    chrm = b"chrm" + bytes(4) + struct.pack(">HH6I", 3, 0, 0xA3D7, 0x547C, 0x4CCD, 0x999A, 0x2667, 0xF5C)
    tags = [
        (b"desc", mluc("GIMP built-in sRGB")),
        (b"cprt", mluc("Public Domain")),
        (b"wtpt", icc_numbers(b"XYZ ", white)),
        (b"chad", icc_numbers(b"sf32", chad)),
        (b"rXYZ", icc_numbers(b"XYZ ", (0x6FA0, 0x38F5, 0x390))),
        (b"bXYZ", icc_numbers(b"XYZ ", (0x249F, 0xF84, 0xB6C4))),
        (b"gXYZ", icc_numbers(b"XYZ ", (0x6297, 0xB787, 0x18D9))),
        (b"rTRC", trc),
        (b"gTRC", trc),
        (b"bTRC", trc),
        (b"chrm", chrm),
        (b"dmnd", mluc("GIMP")),
        (b"dmdd", mluc("sRGB")),
    ]
    body = bytearray()
    table = bytearray()
    base = 128 + 4 + 12 * len(tags)
    placed = {}
    for sig, data in tags:
        if data not in placed:
            while len(body) % 4:
                body.append(0)
            placed[data] = base + len(body)
            body += data
        table += sig + struct.pack(">II", placed[data], len(data))
    size = base + len(body)
    head = struct.pack(">I4sI4s4s4s6H4s4sI4s4sQI", size, b"lcms", 0x04400000, b"mntr", b"RGB ", b"XYZ ",
                       2024, 9, 3, 17, 6, 35, b"acsp", b"APPL", 0, bytes(4), bytes(4), 0, 0)
    head += struct.pack(">3i", *white) + b"lcms" + bytes(44)
    return head + struct.pack(">I", len(tags)) + bytes(table) + bytes(body)


def shadow_png():
    size = 32
    px = bytearray(size * size * 4)
    for y in range(size):
        for x in range(size):
            dx = 2 * x + 1 - size
            dy = 2 * y + 1 - size
            if dx * dx + dy * dy <= size * size:
                px[(y * size + x) * 4 + 3] = 255
    raw = b"".join(filter_rows(px, size, size, 4, LIBPNG_FILTERS))
    icc = gimp_srgb_icc()
    return png(size, size, 6, [
        (b"iCCP", b"ICC profile\0\0" + deflate_slow(icc, 6, window_bits(len(icc)))),
        (b"bKGD", struct.pack(">3H", 255, 255, 255)),
        (b"pHYs", struct.pack(">IIB", 11811, 11811, 1)),
        (b"tIME", struct.pack(">H5B", 2024, 9, 3, 17, 8, 32)),
        (b"tEXt", b"Comment\0Created with GIMP"),
        (b"IDAT", deflate_slow(raw, 9, window_bits(len(raw)), True)),
    ])


def sky_png():
    rows = filter_rows(sky_space(), 256, 256, 3, (0, 2, 1, 4))
    return png(256, 256, 2, [(b"IDAT", deflate_medium(rows))])


def main():
    args = parse_args()
    files = figures(Dump(args.orig))
    files["assets/shadow.png"] = shadow_png()
    sky = sky_png()
    for stage in ("honeyhive", "honeyhive_tower", "observatory"):
        files["assets/stages/%s/sky.png" % stage] = sky
    for rel in sorted(files):
        write_out(args.out, rel, files[rel])
    print("procedural: %d files" % len(files))


if __name__ == "__main__":
    main()
