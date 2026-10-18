import os
import struct

import ui_lyt
from common import IntReader, f32, read_pak, write_out
from ui_art import UI, hide, tiles_of, union_box
from ui_data import Context
from ui_dome import dome_binary
from ui_hud import MCX, MCY, hud_binary
from ui_img import LANCZOS, median_cut, nearest_i, resize_rgba, shrink
from ui_math import rgb555, ulp_shift
from ui_ss import ss_binary


class Ctx(Context):
    def __init__(self, orig, data, palette_rgba):
        Context.__init__(self, orig)
        self.data = data
        self.palette_rgba = palette_rgba

    def fix_floats(self, key, vals):
        r = self.data.get("float/" + key)
        if not r:
            return list(vals)
        return [ulp_shift(v, d) for v, d in zip(vals, IntReader(r).ints(len(vals)))]

    def fix_pal(self, key, pal):
        r = self.data.get("pal/" + key)
        if not r:
            return pal
        vals = IntReader(r).ints(len(pal) * 3)
        return [tuple(c[k] + vals[i * 3 + k] for k in range(3)) for i, c in enumerate(pal)]


def apply_bytes(pred, corr):
    if not corr:
        return pred
    rd = IntReader(corr)
    n = rd.uvarint()
    out = bytearray(pred[:n].ljust(n, b"\0"))
    pos = -1
    for _ in range(rd.uvarint()):
        pos += rd.uvarint() + 1
        out[pos] = (out[pos] + rd.int()) & 0xFF
    return bytes(out)


def trig_fix(raw):
    fix = {}
    if raw:
        rd = IntReader(raw)
        k = -1
        for _ in range(rd.uvarint()):
            k += rd.uvarint() + 1
            fix[k] = (rd.int(), rd.int())
    return fix


def alpha_bbox(rgba, w, h):
    alpha = bytes(rgba[3::4])
    x0, y0, x1, y1 = w, h, -1, -1
    for y in range(h):
        row = alpha[y * w:(y + 1) * w]
        n = len(row.rstrip(b"\0"))
        if n:
            x0 = min(x0, w - len(row.lstrip(b"\0")))
            x1 = max(x1, n - 1)
            y0, y1 = min(y0, y), y
    return None if x1 < 0 else (x0, y0, x1 + 1, y1 + 1)


def crop(rgba, w, box):
    x0, y0, x1, y1 = box
    return b"".join(bytes(rgba[(y * w + x0) * 4:(y * w + x1) * 4]) for y in range(y0, y1))


def blue_chip(ctx):
    ui = UI(ctx)
    b = ui.bundle("BlueChipCounter")
    an = b["anims"]
    frames = []
    for animation, count in (("complete", 61), ("end", 21)):
        for frame in range(count):
            states = [(an["chipget"], 1, "Chip" + str(i)) for i in range(1, 6)]
            states += [(an["showhide"], 20), (an[animation], frame)]
            frames.append(ui_lyt.tex_of_img(ui_lyt.render(b, ui.fonts, states)).rgba)
    box = union_box(bb for bb in (alpha_bbox(f, 608, 456) for f in frames) if bb)
    scale = 160 / 456
    w = round((box[2] - box[0]) * scale)
    h = round((box[3] - box[1]) * scale)
    cw, ch = box[2] - box[0], box[3] - box[1]
    cropped = [resize_rgba(crop(f, 608, box), cw, ch, w, h, LANCZOS) for f in frames]
    bx, by = (w + 31) // 32, (h + 15) // 16
    x = round(120 + (box[0] - 304) * 240 / 608)
    y = round(80 + (box[1] - 228) * scale)
    colors = []
    for im in cropped:
        for i in range(0, len(im), 4):
            if im[i + 3] >= 128:
                colors.append((im[i], im[i + 1], im[i + 2]))
    palette = ctx.fix_pal("bluechip", median_cut(colors, 15))
    pal15 = [0] + [c[0] >> 3 | (c[1] >> 3) << 5 | (c[2] >> 3) << 10 for c in palette]
    blob = bytearray(struct.pack("<HHhh16H", bx, by, x, y, *pal15))
    W, H = bx * 32, by * 16
    for im in cropped:
        rgb = [v for i in range(0, len(im), 4) for v in im[i:i + 3]]
        near = nearest_i(rgb, palette)
        idx = [0] * (W * H)
        for yy in range(h):
            for xx in range(w):
                i = yy * w + xx
                if im[i * 4 + 3] >= 128:
                    idx[yy * W + xx] = near[i] + 1
        for row in range(by):
            for col in range(bx):
                blob += tiles_of(idx, W, H, col * 32, row * 16, 32, 16)
    return bytes(blob)


def lifeup_frames(ctx, names):
    rd = IntReader(ctx.data["lifeup"])
    out = []
    for _ in range(rd.int()):
        hp = rd.int()
        out.append((hp, [(names[rd.int()], rd.int()) for _ in range(rd.int())]))
    return out


def lifeup(ctx):
    ui = UI(ctx)
    anims = ui.bundle("HitPointMeter")["anims"]
    out = bytearray()
    for i, (hp, states) in enumerate(lifeup_frames(ctx, sorted(anims))):
        lay = ui.layout("HitPointMeter")
        ui_lyt.set_text_recursive(lay, "HitPointNumber", str(hp))
        hide(lay, "TxtTitle", "ShaTitle", "Title")
        c = ui.render("HitPointMeter", lay, [(anims[n], f) for n, f in states])
        d = shrink(c, (MCX - 96, MCY - 96, MCX + 96, MCY + 96), 1 / 3).d
        mask = [d[k * 4 + 3] >= 0.5 for k in range(64 * 64)]
        rgb = []
        for k in range(64 * 64):
            if mask[k]:
                rgb.append(tuple(min(max(round(f32(d[k * 4 + j] * 255.0)), 0), 255) for j in range(3)))
        p8 = ctx.fix_pal("lifeup_%d" % i, median_cut(rgb, 15))
        pal = [0] + [rgb555(c, 255) for c in p8]
        pal += [0] * (16 - len(pal))
        near = iter(nearest_i([v for c in rgb for v in c], p8))
        idx = [next(near) + 1 if m else 0 for m in mask]
        out += struct.pack("<16H", *pal)
        out += tiles_of(idx, 64, 64, 0, 0, 64, 64)
    return bytes(out)


def text_pool(raw):
    count = len(raw) // 32
    unique, lookup, refs = [], {}, []
    for at in range(0, len(raw), 32):
        tile = raw[at:at + 32]
        if tile not in lookup:
            lookup[tile] = len(unique)
            unique.append(tile)
        refs.append(lookup[tile])
    off = (12 + count * 2 + 3) & ~3
    data = b"UTP1" + struct.pack("<2I", count, len(unique)) + struct.pack("<%dH" % count, *refs)
    return data + bytes(off - len(data)) + b"".join(unique)


def intern(item, table, index):
    if item not in index:
        index[item] = len(table)
        table.append(item)
    return index[item]


def lifeup_pool(raw):
    pals, pmap, tiles, tmap, refs = [], {}, [], {}, []
    for i in range(len(raw) // 2080):
        d = raw[i * 2080:(i + 1) * 2080]
        rec = [intern(d[:32], pals, pmap)]
        for k in range(64):
            rec.append(intern(d[32 + k * 32:64 + k * 32], tiles, tmap))
        refs.append(struct.pack("<65H", *rec))
    r = b"".join(refs)
    return r + bytes(((len(r) + 3) & ~3) - len(r)) + b"".join(tiles) + b"".join(pals)


def build_ui(orig_root, data_dir, out_root, palette_rgba):
    data = read_pak(os.path.join(data_dir, "ui.pak"))
    ui_lyt.open_lib(os.path.join(out_root, ".bin"))
    ui_lyt.TRIG.fix = trig_fix(data.get("trig"))
    ctx = Ctx(orig_root, data, palette_rgba)

    def fixed(rel, pred):
        return apply_bytes(pred, data.get("bytes/" + rel))

    hud_tiles, hud_text, hud_pal = hud_binary(ctx)
    ss_tiles, ss_text, ss_pal, ss_sky = ss_binary(ctx)
    dome_tiles, dome_slot, dome_pal = dome_binary(ctx)
    files = {"assets/hud/hud_tiles.bin": hud_tiles, "assets/hud/hud_palette.bin": hud_pal,
             "assets/hud/ss_tiles.bin": ss_tiles, "assets/hud/ss_palette.bin": ss_pal,
             "assets/hud/ss_sky.bin": ss_sky, "assets/hud/dome_tiles.bin": dome_tiles,
             "assets/hud/dome_slot.bin": dome_slot, "assets/hud/dome_palette.bin": dome_pal,
             "assets/hud/blue_chip_counter.bin": blue_chip(ctx)}
    meter = fixed("assets/lifeup/lifeup_meter.bin", lifeup(ctx))
    files["assets/text_tiles/hud_text_pool.bin"] = text_pool(fixed("text/hud_text.bin", hud_text))
    files["assets/text_tiles/ss_text_pool.bin"] = text_pool(fixed("text/ss_text.bin", ss_text))
    files["assets/lifeup/lifeup_meter_pool.bin"] = lifeup_pool(meter)
    for rel, blob in files.items():
        write_out(out_root, rel, fixed(rel, blob))
