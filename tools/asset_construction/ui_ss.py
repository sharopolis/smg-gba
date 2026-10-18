import struct

import ui_lyt
from common import u16, u32
from ui_art import (GALAXIES, KX, S, SHAPES, UI, ArtList, back_button, gx, gy, hide, index_px, no_fix, only,
                    opaque_rgb, pieces_loose, quant_cols, tiles_of, unit_cols)
from ui_img import bbox
from ui_lyt import dbuf, ptr
from ui_math import PI, cos, fma, radians, rgb555, sin, tan

MISSIONS = {"BUOYBASE": 2}
COMETS = [("RED", "Red", 0), ("GHOST", "Ghost", 4), ("PURPLE", "Purple", 3), ("QUICK", "Quick", 1),
          ("DARK", "Dark", 1)]
NEW_ART = ("G_BUOYBASE", "S_BUOYBASE_", "G_GUSTYGARDEN", "S_GUSTYGARDEN_")
NEW_COMET = ("CT_DARK", "CM_DARK")
SLOTS = ["galaxy", "scenario", "comettext", "cometmark"]
SKY_Z, SKY_SCALE, EYE_Z = -10000.0, 10.0, 1000.0
BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]
BANDS = (("BAND_U", 48, [(0, 0, 32, 32), (0, 32, 32, 16)]), ("BAND_D", 40, [(0, 0, 32, 32), (0, 32, 32, 8)]))
VTX_FORMATS = {0: ("B", 1), 1: ("b", 1), 2: ("H", 2), 3: ("h", 2), 4: ("f", 4)}


class Bdl:
    def __init__(self, b):
        self.b = b
        self.sec = {}
        o = 0x20
        for _ in range(u32(b, 0x0C)):
            self.sec[b[o:o + 4]] = o
            o += u32(b, o + 4)
        self.arrays = self.read_arrays()

    def read_arrays(self):
        b, o = self.b, self.sec[b"VTX1"]
        size = u32(b, o + 4)
        offs = [u32(b, o + 0x0C + i * 4) for i in range(13)]
        slot_of = {9: 0, 11: 3, 13: 5}
        fmts = {}
        p = o + u32(b, o + 8)
        while True:
            attr, cc, ct, frac = struct.unpack_from(">IIIB", b, p)
            p += 16
            if attr == 0xFF:
                break
            fmts[attr] = (cc, ct, frac)
        srt = sorted(x for x in offs if x)
        arrays = {}
        for attr in (9, 11, 13):
            cc, ct, frac = fmts[attr]
            start = offs[slot_of[attr]]
            nxt = [x for x in srt if x > start]
            raw = b[o + start:o + (nxt[0] if nxt else size)]
            if attr == 11:
                arrays[attr] = [tuple(raw[i:i + 4]) for i in range(0, len(raw) // 4 * 4, 4)]
                continue
            if attr == 9:
                ncomp = 3 if cc == 1 else 2
            else:
                ncomp = 2 if cc == 1 else 1
            code, isz = VTX_FORMATS[ct]
            cnt = len(raw) // (isz * ncomp)
            vals = struct.unpack_from(">%d%s" % (cnt * ncomp, code), raw, 0)
            if ct == 4:
                vals = [float(v) for v in vals]
            else:
                vals = [v / float(1 << frac) for v in vals]
            arrays[attr] = [vals[i * ncomp:(i + 1) * ncomp] for i in range(cnt)]
        return arrays

    def textures(self):
        b, o = self.b, self.sec[b"TEX1"]
        hdr = o + u32(b, o + 0x0C)
        out = []
        for i in range(u16(b, o + 8)):
            h = hdr + i * 0x20
            fmt, w, hh = b[h], u16(b, h + 2), u16(b, h + 4)
            img = h + u32(b, h + 0x1C)
            rgba = ui_lyt.decode_texture(fmt, w, hh, b[img:img + ui_lyt.tex_size(fmt, w, hh)])
            if fmt in (0, 1):
                for k in range(0, len(rgba), 4):
                    rgba[k + 3] = 255
            out.append(ui_lyt.Tex(w, hh, rgba, fmt))
        return out

    def shape0(self):
        b, o = self.b, self.sec[b"SHP1"]
        init = o + u32(b, o + 0x0C)
        remap = o + u32(b, o + 0x10)
        desc_base = o + u32(b, o + 0x18)
        dl_base = o + u32(b, o + 0x20)
        drawinit = o + u32(b, o + 0x28)
        p = init + u16(b, remap) * 0x28
        npk, desc_off, first_draw = u16(b, p + 2), u16(b, p + 4), u16(b, p + 8)
        desc = []
        q = desc_base + desc_off
        while True:
            attr, typ = struct.unpack_from(">II", b, q)
            q += 8
            if attr == 0xFF:
                break
            desc.append((attr, typ))
        verts = []
        for pk in range(npk):
            size, off = struct.unpack_from(">II", b, drawinit + (first_draw + pk) * 8)
            p, end = dl_base + off, dl_base + off + size
            while p < end:
                op = b[p]
                p += 1
                if op == 0:
                    continue
                if op & 0xF8 not in (0x80, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8):
                    break
                cnt = u16(b, p)
                p += 2
                for _ in range(cnt):
                    att = {}
                    for attr, typ in desc:
                        if typ in (1, 2):
                            val = b[p]
                            p += 1
                        elif typ == 3:
                            val = u16(b, p)
                            p += 2
                        else:
                            continue
                        if attr > 8:
                            att[attr] = val
                    verts.append(att)
        return verts


def sky_bdl(ctx):
    return Bdl(ctx.arc("ObjectData/ScenarioSelectSky.arc").find("scenarioselectsky/scenarioselectsky.bdl"))


def solve_fma(m, rhs):
    a = [list(r) for r in m]
    x = [list(r) for r in rhs]
    n = len(a)
    for k in range(n):
        piv = max(range(k, n), key=lambda r: abs(a[r][k]))
        a[k], a[piv] = a[piv], a[k]
        x[k], x[piv] = x[piv], x[k]
        r = 1.0 / a[k][k]
        for i in range(k + 1, n):
            a[i][k] = a[i][k] * r
        for i in range(k + 1, n):
            for j in range(k + 1, n):
                a[i][j] = fma(-a[i][k], a[k][j], a[i][j])
    for c in range(len(x[0])):
        for i in range(n):
            for j in range(i):
                x[i][c] = fma(-a[i][j], x[j][c], x[i][c])
        for i in range(n - 1, -1, -1):
            x[i][c] = x[i][c] * (1.0 / a[i][i])
            for j in range(i):
                x[j][c] = fma(-a[j][i], x[i][c], x[j][c])
    return x


def sky_geometry(bdl, fix):
    verts = bdl.shape0()
    qp = [bdl.arrays[9][v[9]] for v in verts]
    qc = [[c / 255 for c in bdl.arrays[11][v[11]][:3]] for v in verts]
    qt = [bdl.arrays[13][v[13]] for v in verts]
    p = [[c * SKY_SCALE for c in q] for q in qp]
    m = [[p[i][0], p[i][1], 1.0] for i in range(3)]
    uv = fix("sky_uv", [v for r in solve_fma(m, qt[:3]) for v in r])
    col = fix("sky_col", [v for r in solve_fma(m, qc[:3]) for v in r])
    dist = EYE_Z - (SKY_Z + SKY_SCALE * qp[0][2])
    t = fix("sky_tan", [tan(radians(22.5))])[0]
    pix = 2 * t * dist / 160
    du = solve_fma([[uv[0], uv[2]], [uv[1], uv[3]]], [[1.0], [0.0]])
    du = fix("sky_du", [du[0][0], du[1][0]])
    dw = [v * (1.0 / 0.16) for v in du]
    drift = [dw[0] / pix, -dw[1] / pix]
    w = int(240 + abs(-(-drift[0] // 1))) + 2
    h = int(80 + abs(-(-(drift[1] / 2) // 1))) + 1
    w += w % 2
    return pix, drift, uv, col, w, h


def sky_raster(ctx):
    bdl = sky_bdl(ctx)
    pix, drift, uv, col, w, h = sky_geometry(bdl, ctx.fix_floats)
    pal = ctx.palette_rgba
    cols = [pal[i * 4 + k] / 255 for i in range(1, 256) for k in range(3)]
    bayer = [v / 16.0 - 0.5 for v in BAYER]
    par = [pix, max(drift[0], 0), max(drift[1], 0)] + uv + col
    t0, t1 = bdl.textures()[:2]
    out = bytearray(w * h)
    ui_lyt.LIB.sky(ptr(out), w, h, dbuf(par), ptr(t0.rgba), t0.w, t0.h, ptr(t1.rgba), t1.w, t1.h, dbuf(cols),
                   dbuf(bayer))
    return bytes(out)


def sky_defines(ctx, define):
    _, dp, _, _, w, h = sky_geometry(sky_bdl(ctx), no_fix)
    define("SS_SKY_W", w)
    define("SS_SKY_H", h)
    define("SS_SKY_X0_16", max(dp[0], 0) * 16)
    define("SS_SKY_Y0_16", max(dp[1], 0) / 2 * 16)
    define("SS_SKY_DX_16", dp[0] * 16)
    define("SS_SKY_DY_16", dp[1] / 2 * 16)


def ss_arts(ui):
    arts = ArtList(ui)

    def text_art(name, pane, text, slot, states):
        lay = only(ui.shadowed("ScenarioSelect"), [pane])
        ui_lyt.set_text_recursive(lay, pane, text)
        arts.add_canvas(name, ui.render("ScenarioSelect", lay, states), "text", slot=slot)

    def pane_art(name, pane, bank, states, text=None):
        lay = only(ui.shadowed("ScenarioSelect"), [pane])
        if text is not None:
            ui_lyt.set_text_recursive(lay, "ShaMarioNum", text)
        arts.add_canvas(name, ui.render("ScenarioSelect", lay, states), bank)

    wait = [ui.anim("ScenarioSelect", "Appear", 30), ui.anim("ScenarioSelect", "SelectIn", 20)]
    for short, gname in GALAXIES:
        text_art("G_" + short, "Galaxy", ui.msg["GalaxyName_" + gname], "galaxy", wait)
        for m in range(1, MISSIONS.get(short, 6) + 1):
            text = ui.msg_text("ScenarioName_%s%d" % (gname, m), [ui.player])
            text_art("S_%s_%d" % (short, m), "Scenario", text, "scenario", wait)
    comet = [ui.anim("ScenarioSelect", "CometAppear", 20)]
    for short, cname, cf in COMETS:
        text = ui.msg_text("GalaxyInfo_CometAppearR", [ui.msg["CometName_" + cname]])
        text_art("CT_" + short, "TextComet", text, "comettext", comet)
        lay = only(ui.shadowed("ScenarioSelect"), ["Comet"])
        c = ui.render("ScenarioSelect", lay, comet + [ui.anim("ScenarioSelect", "Color", cf)])
        arts.add_canvas("CM_" + short, c, "comet_" + short.lower(), slot="cometmark")
    appear = [ui.anim("ScenarioSelect", "Appear", 30)]
    pane_art("LIVES_ICON", "Mario", "lives", appear, "")
    canv = []
    for d in range(10):
        lay = only(ui.shadowed("ScenarioSelect"), ["ShaMarioNum"])
        ui_lyt.set_text_recursive(lay, "ShaMarioNum", str(d))
        canv.append(ui.render("ScenarioSelect", lay, appear))
    arts.digit_boxes = arts.add_digits("LDIGIT", canv, "lives")
    pane_art("NEW", "New", "new", appear)
    arts.add_canvas("BACK", back_button(ui), "back")
    lay = only(ui.shadowed("DPDPointer"), ["HandPointer"])
    hide(lay, "PicPlayer2")
    arts.add_canvas("HAND", ui.render("DPDPointer", lay, []), "hand")
    return arts


def band(h, rows):
    idx = [0] * (rows * 32)
    for r in range(min(h, rows)):
        v = 1 + min(14, int((r + 0.5) / h * 15))
        for x in range(32):
            idx[r * 32 + x] = v
    return idx


def ss_binary(ctx):
    arts = ss_arts(UI(ctx))
    sky = sky_raster(ctx)
    bank_cols = {}
    for bank in arts.banks:
        group = arts.in_bank(bank)
        legacy = [a for a in group if not a.name.startswith(NEW_ART) and a.name not in NEW_COMET]
        bank_cols[bank] = unit_cols(ctx.fix_pal("ss_" + bank, quant_cols(opaque_rgb(legacy or group), 15)))
    fixed, text = bytearray(), bytearray()
    for (_, rows, parts), h in zip(BANDS, (round(132 * S), round(96 * S))):
        idx = band(h, rows)
        for x, y, w, ph in parts:
            fixed.extend(tiles_of(idx, 32, rows, x, y, w, ph))
    for a in arts.arts:
        idx = index_px(a, bank_cols[a.bank])
        store = fixed if a.slot is None else text
        for x, y, sw, sh in pieces_loose(a.w, a.h):
            store.extend(tiles_of(idx, a.w, a.h, x, y, sw, sh))
    pal = [0] * 256
    for bank, bi in arts.banks.items():
        for k, c in enumerate(bank_cols[bank]):
            pal[bi * 16 + 1 + k] = rgb555([v * 255 for v in c], 255)
    return bytes(fixed), bytes(text), struct.pack("<256H", *pal), sky


def join(vals):
    return ", ".join(str(v) for v in vals)


def reaction(x):
    if x < 0.5:
        return sin(x * 2 * PI / 2)
    t = (x - 0.5) * 2
    return 1 + (1 - cos(PI * 1.5 * t)) * (1 - t) * -0.25


def ss_header(ctx):
    ui = UI(ctx)
    defs = {}

    def define(name, value):
        defs[name] = int(round(value))

    def sample(anim, element, target, f, layname="ScenarioSelect"):
        return ui.sample(layname, anim, element, target, f)

    def frames(anim, element, target, n, layname="ScenarioSelect"):
        return [sample(anim, element, target, f, layname) for f in range(n)]

    def centre_y(box):
        return gy((box[1] + box[3]) / 2)

    define("SS_BAND_D_H", round(96 * S))
    wait_cols = []
    for f in range(0, 1800, 15):
        row = []
        for el in ("FrameU", "FrameD"):
            for corner in ("lt", "lb"):
                row.append(rgb555([sample("Wait", el, corner + "_" + ch, f) for ch in "rgb"], 255))
        wait_cols.append(row)
    pane = ui_lyt.find_pane(ui.bundle("ScenarioSelect")["layout"], "FrameU")
    vals = [c[3] for c in pane["vertex_colors"]]
    define("SS_BAND_EVA", ((vals[0] + vals[1]) + (vals[2] + vals[3])) / 4 / 255 * 16)
    arts = ss_arts(ui)
    box = arts.by_name
    define("SS_GALAXY_CY", centre_y(box["G_BUOYBASE"].box))
    define("SS_SCENARIO_CY", centre_y(box["S_BUOYBASE_2"].box))
    define("SS_COMET_TEXT_CY", centre_y(box["CT_DARK"].box))
    mark = box["CM_DARK"].box
    define("SS_COMET_MARK_CX", gx((mark[0] + mark[2]) / 2))
    define("SS_COMET_MARK_CY", centre_y(mark))
    define("SS_LIVES_ICON_X", gx(box["LIVES_ICON"].box[0]))
    define("SS_LIVES_ICON_Y", gy(box["LIVES_ICON"].box[1]))
    win = box["LDIGIT0"].box
    define("SS_LIVES_DIGIT_X", gx(win[0]))
    define("SS_LIVES_DIGIT_Y", gy(win[1]))
    lay = only(ui.layout("ScenarioSelect"), ["ShaMarioNum"])
    ui_lyt.set_text_recursive(lay, "ShaMarioNum", "88")
    b2 = bbox(ui.render("ScenarioSelect", lay, [ui.anim("ScenarioSelect", "Appear", 30)]))
    define("SS_LIVES_DIGIT_ADV16", (b2[2] - arts.digit_boxes[8][2]) * S * 16)
    define("SS_LIVES_TWO_DX", -10 * KX)
    define("SS_NEW_DX", (box["NEW"].box[0] - 304) * S)
    define("SS_NEW_DY", (box["NEW"].box[1] - 228) * S)
    define("SS_BACK_X", gx(box["BACK"].box[0]))
    define("SS_BACK_Y", gy(box["BACK"].box[1]))
    define("SS_HAND_DX", (box["HAND"].box[0] - 304) * S)
    define("SS_HAND_DY", (box["HAND"].box[1] - 228) * S)
    star_rows = [[sample("StarPositionTop", "Star%d" % (k + 1), "trans_x", n - 1) for k in range(n)]
                 for n in range(1, 4)]
    star_rows_d = [[sample("StarPositionDown", "Star%d" % (k + 4), "trans_x", n - 1) for k in range(n)]
                   for n in range(1, 5)]
    define("SS_STAR_TOP_Y16", gy(228 - 35) * 16)
    define("SS_STAR_TOP_ALONE_Y16", (gy(228 - 35) + 50 * S) * 16)
    define("SS_STAR_DOWN_Y16", gy(228 + 65) * 16)
    sky_defines(ctx, define)
    piece_rows, art_rows, slot_sizes, fixed = [], [], {}, 0
    banks = arts.banks
    for (name, rows, parts), bank in zip(BANDS, (len(banks), len(banks) + 1)):
        first = len(piece_rows)
        for x, y, w, h in parts:
            shape, size = SHAPES[(w, h)]
            piece_rows.append((fixed, shape << 2 | size, x, y))
            fixed += w * h // 64
        art_rows.append((name, first, len(piece_rows) - first, bank, 32, rows, None, 0, 0))
    text_bytes = 0
    for a in arts.arts:
        first = len(piece_rows)
        if a.slot is None:
            for x, y, sw, sh in pieces_loose(a.w, a.h):
                shape, size = SHAPES[(sw, sh)]
                piece_rows.append((fixed, shape << 2 | size, x, y))
                fixed += sw * sh // 64
            art_rows.append((a.name, first, len(piece_rows) - first, banks[a.bank], a.w, a.h, None, 0, 0))
        else:
            data_off = text_bytes
            t0 = 0
            for x, y, sw, sh in pieces_loose(a.w, a.h):
                shape, size = SHAPES[(sw, sh)]
                piece_rows.append((t0, shape << 2 | size, x, y))
                t0 += sw * sh // 64
                text_bytes += sw * sh // 2
            slot_sizes[a.slot] = max(slot_sizes.get(a.slot, 0), t0)
            art_rows.append((a.name, first, len(piece_rows) - first, banks[a.bank], a.w, a.h, a.slot, t0,
                             data_off))
    slot_base = {}
    t = fixed
    for s in SLOTS:
        slot_base[s] = t
        t += slot_sizes[s]
    h = ["", "typedef struct { uint16_t tile; uint8_t obj, pad; int16_t x, y; } SsPiece;",
         "typedef struct { uint8_t first, count, bank, slot; uint8_t w, h; uint16_t tiles; uint32_t data; } "
         "SsArt;", "",
         "enum { SS_SLOT_FIXED, " + ", ".join("SS_SLOT_" + s.upper() for s in SLOTS) + ", SS_SLOT_COUNT };",
         "static const uint16_t ss_slot_base[SS_SLOT_COUNT] = { 0, %s };" % join(slot_base[k] for k in SLOTS),
         "static const uint16_t ss_slot_size[SS_SLOT_COUNT] = { 0, %s };" % join(map(slot_sizes.get, SLOTS)),
         "#define SS_BANK_BAND_U %d" % len(banks), "#define SS_BANK_BAND_D %d" % (len(banks) + 1),
         "#define SS_BANK_COUNT %d" % (len(banks) + 2), "", "enum {"]
    for r in art_rows:
        h.append("    SS_ART_%s," % r[0])
    h += ["    SS_ART_COUNT", "};", "static const SsPiece ss_pieces[] = {"]
    for tile, obj, x, y in piece_rows:
        h.append("    { %d, %d, 0, %d, %d }," % (tile, obj, x, y))
    h += ["};", "static const SsArt ss_arts[SS_ART_COUNT] = {"]
    slot_of = {None: 0, "galaxy": 1, "scenario": 2, "comettext": 3, "cometmark": 4}
    for name, first, count, bank, w, hh, slot, ntiles, data in art_rows:
        row = (name, first, count, bank, slot_of[slot], w, hh, ntiles, data)
        h.append("    [SS_ART_%s] = { %d, %d, %d, %d, %d, %d, %d, %d }," % row)
    h += ["};", ""]
    for k, v in defs.items():
        h.append("#define %-26s %6d" % (k, v))
    h.append("")

    def table(name, ctype, vals):
        h.append("")
        h.append("static const %s %s[%d] = { %s };" % (ctype, name, len(vals), join(round(v) for v in vals)))

    table("ss_appear_dy", "int8_t", [round((228 - v) * S) for v in frames("Appear", "FrameU", "trans_y", 31)])
    table("ss_appear_alpha", "uint8_t", [round(v / 255 * 16) for v in frames("Appear", "FrameU", "c1_a", 31)])
    table("ss_end_dy", "int8_t", [round((228 - v) * S) for v in frames("End", "FrameU", "trans_y", 31)])
    table("ss_end_alpha", "uint8_t", [round(v / 255 * 16) for v in frames("End", "FrameU", "c1_a", 31)])
    table("ss_galaxy_dy", "int8_t", [round((170 - v) * S) for v in frames("Appear", "Galaxy", "trans_y", 31)])
    table("ss_selin_dx", "int8_t", [round(v * KX) for v in frames("SelectIn", "Scenario", "trans_x", 21)])
    table("ss_selin_a", "uint8_t", [int(v) for v in frames("SelectIn", "Scenario", "alpha", 21)])
    table("ss_selout_dx", "int8_t", [round(v * KX) for v in frames("SelectOut", "Scenario", "trans_x", 21)])
    table("ss_selout_a", "uint8_t", [int(v) for v in frames("SelectOut", "Scenario", "alpha", 21)])
    back = [round((-176 - v) * S) for v in frames("ButtonAppear", "Back", "trans_y", 21, "BackButton")]
    table("ss_back_appear_dy", "int8_t", back)
    back = [round((-176 - v) * S) for v in frames("ButtonEnd", "Back", "trans_y", 21, "BackButton")]
    table("ss_back_end_dy", "int8_t", back)
    h += ["", "#define SS_WAIT_STEP %d" % 15,
          "static const uint16_t ss_wait_colours[%d][4] = {" % len(wait_cols)]
    for row in wait_cols:
        h.append("    { 0x%04x, 0x%04x, 0x%04x, 0x%04x }," % tuple(row))
    h.append("};")
    table("ss_getstar_dy", "int8_t",
          [(20 - v) * S for v in frames("GetStar", "GetStar", "trans_y", 81, "GetStar")])
    table("ss_getstar_alpha", "uint8_t",
          [v / 255 * 16 for v in frames("GetStar", "GetStar", "alpha", 81, "GetStar")])
    h.append("#define SS_GETSTAR_FOLLOW_DY %d" % round(80 * S))
    table("ss_point_scale", "uint16_t", [(0.9 + (1.5 - 0.9) * reaction(f / 20)) * 256 for f in range(20)])
    h.append("")
    rows = []
    for xs in star_rows:
        rows.append("{ %s }" % ", ".join(str(round(gx(304 + x) * 16)) for x in xs + [0] * (3 - len(xs))))
    h.append("static const int16_t ss_star_top_x16[3][3] = { %s };" % ", ".join(rows))
    rows = []
    for xs in star_rows_d:
        rows.append("{ %s }" % ", ".join(str(round(gx(304 + x) * 16)) for x in xs + [0] * (4 - len(xs))))
    h.append("static const int16_t ss_star_down_x16[4][4] = { %s };" % ", ".join(rows))
    return h
