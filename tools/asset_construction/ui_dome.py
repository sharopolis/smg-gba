import struct

import ui_lyt
from ui_art import (S, SHAPES, UI, ArtList, back_button, gx, gy, hide, index_px, name_plate, no_fix, only,
                    opaque_rgb, pieces_loose, quant_cols, tiles_of, unit_cols)
from ui_img import bbox
from ui_math import rgb555

OPEN = [("GOODEGG", "EggStarGalaxy"), ("HONEYHIVE", "HoneyBeeKingdomGalaxy")]


def star_count(ui, pane, text):
    lay = only(ui.shadowed("GalaxyInfo"), [pane])
    ui_lyt.set_text_recursive(lay, pane, text)
    return ui.render("GalaxyInfo", lay, [ui.anim("GalaxyInfo", "Appear")])


def dome_arts(ui):
    arts = ArtList(ui)
    for short, gname in OPEN:
        arts.add_canvas("PLATE_" + short, name_plate(ui, ui.msg["GalaxyNameShort_" + gname], 2), "plate")
    arts.add_canvas("PLATE_UNKNOWN", name_plate(ui, ui.msg["GalaxyNameShort_Unknown"], 0), "plate")
    infos = [("INFO_" + short, ui.msg["GalaxyName_" + gname], "") for short, gname in OPEN]
    for name, galaxy_text, unknown_text in infos + [("INFO_HATENA", "", ui.msg["GalaxyInfo_Hatena"])]:
        lay = ui.shadowed("GalaxyInfo")
        hide(lay, "CometName", "Star", "Complete", "PicDummy")
        ui_lyt.set_text_recursive(lay, "GalaxyName", galaxy_text)
        ui_lyt.set_text_recursive(lay, "Unknown", unknown_text)
        c = ui.render("GalaxyInfo", lay, [ui.anim("GalaxyInfo", "Appear")])
        arts.add_canvas(name, c, "info", slot=True)
    for name, pane in (("STAR_ON", "StarIcon"), ("STAR_OFF", "TxtStarBase")):
        arts.add_canvas(name, star_count(ui, pane, "7"), "star")
    lay = ui.shadowed("GalaxySelect")
    hide(lay, "Counter")
    ui_lyt.set_text_recursive(lay, "TxtStart", ui.msg["Layout_GalaxySelectStart"])
    ui_lyt.set_text_recursive(lay, "ShaStart", ui.msg["Layout_GalaxySelectStart"])
    c = ui.render("GalaxySelect", lay, [ui.anim("GalaxySelect", "ButtonAppear"),
                                        ui.anim("GalaxySelect", "ButtonSelectIn")])
    arts.add_canvas("START", c, "start")
    arts.add_canvas("BACK", back_button(ui), "back")
    lay = ui.shadowed("DPDPointer")
    hide(lay, "PicPlayer2", "StarPointer")
    arts.add_canvas("HAND", ui.render("DPDPointer", lay, []), "hand")
    lay = ui.shadowed("StarCounter")
    ui_lyt.set_text_recursive(lay, "Counter", "")
    c = ui.render("StarCounter", lay, [ui.anim("StarCounter", "Appear")])
    arts.add_canvas("COUNTER", c, "counter")
    canv = []
    for d in range(10):
        lay = ui.shadowed("CoinCounter")
        ui_lyt.set_text_recursive(lay, "Counter", str(d))
        hide(lay, "Coin")
        canv.append(ui.render("CoinCounter", lay, [ui.anim("CoinCounter", "Appear")]))
    arts.add_digits("DIGIT", canv, "digits")
    return arts


def bank_palettes(arts, fix):
    out = {}
    for bank in arts.banks:
        out[bank] = unit_cols(fix("dome_" + bank, quant_cols(opaque_rgb(arts.in_bank(bank)), 15)))
    return out


def dome_binary(ctx):
    arts = dome_arts(UI(ctx))
    bank_cols = bank_palettes(arts, ctx.fix_pal)
    tiles, slot = bytearray(), bytearray()
    for a in arts.arts:
        idx = index_px(a, bank_cols[a.bank])
        for x, y, sw, sh in pieces_loose(a.w, a.h):
            data = tiles_of(idx, a.w, a.h, x, y, sw, sh)
            if any(data):
                (slot if a.slot else tiles).extend(data)
    pal = [0] * (16 * len(arts.banks))
    for bank, bi in arts.banks.items():
        for k, c in enumerate(bank_cols[bank]):
            pal[bi * 16 + 1 + k] = rgb555(c)
    return bytes(tiles), bytes(slot), struct.pack("<%dH" % len(pal), *pal)


def dome_header(ctx):
    ui = UI(ctx)
    arts = dome_arts(ui)
    defs = {}

    def define(name, value):
        defs[name] = int(round(value))

    def box(name):
        return arts.by_name[name].box

    for name in ("PLATE_GOODEGG", "PLATE_HONEYHIVE", "PLATE_UNKNOWN"):
        define("AD_%s_TIP_DX" % name, (304 - box(name)[0]) * S)
        define("AD_%s_TIP_DY" % name, (box(name)[3] - box(name)[1]) * S)
    define("AD_INFO_X", gx(box("INFO_HATENA")[0]))
    define("AD_INFO_Y", gy(box("INFO_HATENA")[1]))
    for name in ("STAR_ON", "STAR_OFF"):
        define("AD_%s_X" % name, gx(box(name)[0]))
        define("AD_%s_Y" % name, gy(box(name)[1]))
    b, b2 = box("STAR_OFF"), bbox(star_count(ui, "StarIcon", "77"))
    define("AD_STAR_ADV16", ((b2[2] - b2[0]) - (b[2] - b[0])) * S * 16)
    start = box("START")
    define("AD_START_X", gx(start[0]))
    define("AD_START_Y", gy(start[1]))
    define("AD_START_W", (start[2] - start[0]) * S)
    define("AD_START_H", (start[3] - start[1]) * S)
    define("AD_BACK_X", gx(box("BACK")[0]))
    define("AD_BACK_Y", gy(box("BACK")[1]))
    define("AD_HAND_DX", (box("HAND")[0] - 304) * S)
    define("AD_HAND_DY", (box("HAND")[1] - 228) * S)
    counter = box("COUNTER")
    define("AD_COUNTER_X", gx(counter[0]))
    define("AD_COUNTER_Y", gy(counter[1]))
    define("AD_COUNTER_DIGITS_DX", (87 - counter[0]) * S)
    define("AD_COUNTER_DIGITS_DY", (35 - counter[1]) * S)
    define("AD_DIGIT_ADV16", 26 * S * 16)
    del defs["AD_STAR_OFF_X"]
    bank_cols = bank_palettes(arts, no_fix)
    piece_rows, art_rows = [], []
    ntiles = slot_bytes = 0
    for a in arts.arts:
        data_off = slot_bytes
        idx = index_px(a, bank_cols[a.bank])
        first = len(piece_rows)
        t0 = 0
        for x, y, sw, sh in pieces_loose(a.w, a.h):
            data = tiles_of(idx, a.w, a.h, x, y, sw, sh)
            if not any(data):
                continue
            shape, size = SHAPES[(sw, sh)]
            if a.slot:
                piece_rows.append((t0, shape << 2 | size, x, y))
                t0 += sw * sh // 64
                slot_bytes += len(data)
            else:
                piece_rows.append((ntiles, shape << 2 | size, x, y))
                ntiles += len(data) // 32
        if not a.slot:
            data_off = 0
        art_rows.append((a.name, first, len(piece_rows) - first, arts.banks[a.bank], a.w, a.h, t0, data_off))
    h = ["", "typedef struct { uint16_t tile; uint8_t obj, pad; int16_t x, y; } AdPiece;",
         "typedef struct { uint8_t first, count, bank, slot; uint8_t w, h; uint16_t tiles; uint32_t data; } "
         "AdArt;", "", "enum {"]
    for r in art_rows:
        h.append("    AD_ART_%s," % r[0])
    h += ["    AD_ART_COUNT", "};", "#define AD_SLOT_BASE %d" % ntiles]
    h.append("static const AdPiece ad_pieces[] = {")
    for tile, obj, x, y in piece_rows:
        h.append("    { %d, %d, 0, %d, %d }," % (tile, obj, x, y))
    h += ["};", "static const AdArt ad_arts[AD_ART_COUNT] = {"]
    for name, first, count, bank, w, hh, nt, off in art_rows:
        row = (name, first, count, bank, 1 if nt else 0, w, hh, nt, off)
        h.append("    [AD_ART_%s] = { %d, %d, %d, %d, %d, %d, %d, %d }," % row)
    h.append("};")
    for k, v in defs.items():
        h.append("#define %-26s %6d" % (k, v))
    return h
