import re
import struct

import ui_lyt
from common import f32
from ui_art import (GALAXIES, KX, S, SHAPES, UI, ArtList, gmat, gpath, gpos, gx, gy, hide, index_px,
                    name_plate, opaque_rgb, pieces_strict, quant_cols, set_global, tiles_of, union_box,
                    unit_cols)
from ui_data import Dol
from ui_img import bbox, shrink
from ui_math import PI, cos, radians, rgb555, sin

MCX, MCY = 527, 89
MS = 32 / 96
NEW_TITLES = ("T_BUOYBASE_", "T_GUSTYGARDEN_")
TIMING_TABLE = 0x8053A148
RACE = [("RACE3", "Count3", "Layout_RaceThree", "Count", 30),
        ("RACE2", "Count2", "Layout_RaceTwo", "Count", 90),
        ("RACE1", "Count1", "Layout_RaceOne", "Count", 150),
        ("RACEGO", "CountGo", "Layout_RaceGo", "Count", 210),
        ("RACELOSE", "Lose", "Layout_RaceLose", "Lose", 60)]


def star_icon(ui, collected):
    tex = next(t for k, t in ui.bundle("PauseMenu")["textures"].items() if "getstaricon" in k.lower())
    w, h = tex.w, tex.h
    big = ui_lyt.Img(608, 456)
    d = big.d
    for y in range(h):
        for x in range(w):
            i = ((y + 3) * 608 + x + 4) * 4
            a = tex.rgba[(y * w + x) * 4 + 3] / 255.0
            v = 0.0 if collected else 0.25
            d[i], d[i + 1], d[i + 2], d[i + 3] = v, v, v, a
    if collected:
        for y in range(h):
            for x in range(w):
                o = (y * w + x) * 4
                a = tex.rgba[o + 3] / 255.0
                i = (y * 608 + x) * 4
                for k in range(3):
                    d[i + k] = tex.rgba[o + k] / 255.0 * a + d[i + k] * (1 - a)
                d[i + 3] = max(d[i + 3], a)
    return shrink(big, (0, 0, w + 8, h + 8), S)


def counter_icon(ui, arts, name, layname, counter_pane, hidden=(), pos_pane=None):
    lay = ui.shadowed(layname)
    ui_lyt.set_text_recursive(lay, "Counter", "")
    if pos_pane:
        x, y = gpos(lay, pos_pane)
        set_global(lay, counter_pane, x, y)
    hide(lay, *hidden)
    arts.add_canvas(name, ui.render(layname, lay, [ui.anim(layname, "Appear")]), name.lower())


def time_limit(ui, text, show, flash):
    lay = ui.shadowed("TimeLimit")
    for m in lay["materials"]:
        if m["name"] in ("ShaNumber", "ShaTime"):
            m["tev_color1_white"][3] = 255
    ui_lyt.set_text_recursive(lay, "Number", text)
    hide(lay, *[n for n in ("Number", "Time") if n != show])
    states = [ui.anim("TimeLimit", "Appear")]
    if flash:
        states.append(ui.anim("TimeLimit", "TimeLimit", 15))
    return ui.render("TimeLimit", lay, states)


def add_time(ui, arts, flash):
    c = time_limit(ui, "", "Time", False)
    box = bbox(c)
    alt = shrink(time_limit(ui, "", "Time", True), box, S) if flash else None
    arts.add("TIME_LABEL", shrink(c, box, S), "plate", box, dynamic=True, pal="slots", alt=alt)
    glyphs = "0123456789:"
    canv = {}
    for g in glyphs:
        canv[g] = (time_limit(ui, g, "Number", False), time_limit(ui, g, "Number", True) if flash else None)
    gb = {g: bbox(canv[g][0]) for g in glyphs}
    dwin = union_box(gb[g] for g in glyphs[:10])
    cwin = (gb[":"][0], dwin[1], gb[":"][2], dwin[3])
    for g in glyphs:
        win = dwin if g != ":" else cwin
        alt = shrink(canv[g][1], win, S) if flash else None
        name = "TIME%s" % (g if g != ":" else "_COLON")
        arts.add(name, shrink(canv[g][0], win, S), "plate", win, dynamic=True, pal="slots", alt=alt)


def hud_arts(ui, flash):
    arts = ArtList(ui)
    canv = []
    for d in range(10):
        lay = ui.shadowed("CoinCounter")
        ui_lyt.set_text_recursive(lay, "Counter", str(d))
        x, y = gpos(lay, "Position001")
        set_global(lay, "CoinCounter", x, y)
        hide(lay, "Coin")
        canv.append(ui.render("CoinCounter", lay, [ui.anim("CoinCounter", "Appear")]))
    arts.add_digits("DIGIT", canv, "digits")
    counter_icon(ui, arts, "COIN", "CoinCounter", "CoinCounter", pos_pane="Position001")
    counter_icon(ui, arts, "STARBIT", "StarPieceCounter", "StarPieceCounter", ("TSPieceCounter",),
                 "Position001")
    counter_icon(ui, arts, "MARIO", "PlayerLeft", "LeftCounter", ("CenterPlayerLeft",))
    counter_icon(ui, arts, "STAR", "StarCounter", "StarCounter")
    meter_states = [ui.anim("HitPointMeter", "ShowHide")]
    for hp in range(4):
        lay = ui.shadowed("HitPointMeter")
        ui_lyt.set_text_recursive(lay, "HitPointNumber", str(hp))
        hide(lay, "TxtTitle", "ShaTitle", "Title")
        c = ui.render("HitPointMeter", lay, meter_states + [ui.anim("HitPointMeter", "Count", hp)])
        arts.add_canvas("METER%d" % hp, c, "meter%d" % hp, (MCX - 46, MCY - 46, MCX + 50, MCY + 50), MS)
    lay = ui.shadowed("HitPointMeter")
    hide(lay, "PicBase", "ShaBase", "Point", "HitPoint", "PicHiLight", "HitPointNumber", "TxtNumber",
         "ShaNumber")
    c = ui.render("HitPointMeter", lay, meter_states + [ui.anim("HitPointMeter", "Count", 3)])
    arts.add_canvas("LIFE", c, "life", scale=MS)
    arts.add("STARICON", star_icon(ui, True), "staricon", None)
    arts.add("STARICON_EMPTY", star_icon(ui, False), "staricon", None)
    title_states = [ui.anim("StageTitle", "Appear")]
    for short, gname in GALAXIES:
        lay = ui.shadowed("StageTitle")
        ui_lyt.set_text_recursive(lay, "Galaxy", ui.msg["GalaxyName_" + gname])
        ui_lyt.set_text_recursive(lay, "Scenario", "")
        c = ui.render("StageTitle", lay, title_states)
        arts.add_canvas("T_%s_NAME" % short, c, "text", dynamic=True)
        for m in range(1, 7):
            key = "ScenarioName_%s%d" % (gname, m)
            if key not in ui.msg:
                continue
            lay = ui.shadowed("StageTitle")
            ui_lyt.set_text_recursive(lay, "Galaxy", "")
            ui_lyt.set_text_recursive(lay, "Scenario", ui.msg_text(key, [ui.player]))
            c = ui.render("StageTitle", lay, title_states)
            arts.add_canvas("T_%s_M%d" % (short, m), c, "text", dynamic=True)
    for short, gname in GALAXIES:
        c = name_plate(ui, ui.msg["GalaxyNameShort_" + gname], 2)
        arts.add_canvas("T_%s_PLATE" % short, c, "plate", dynamic=True)
    lay = ui.shadowed("GetStar")
    ui_lyt.set_text_recursive(lay, "GetStar", ui.msg["GetStar_PowerStar"])
    c = ui.render("GetStar", lay, [(ui.bundle("GetStar")["anims"]["getstar"], 40)])
    arts.add_canvas("T_GETSTAR", c, "getstar", dynamic=True)
    lay = ui.shadowed("TooBad")
    ui_lyt.set_text_recursive(lay, "TooBad", ui.msg["Layout_TooBadTooBad"])
    c = ui.render("TooBad", lay, [ui.anim("TooBad", "Appear")])
    arts.add_canvas("T_TOOBAD", c, "toobad", dynamic=True)
    for name, hidden in (("PCOIN", "ShaIconHundred"), ("PCOIN_100", "Coin")):
        lay = ui.shadowed("PurpleCoinCounter")
        ui_lyt.set_text_recursive(lay, "Counter", "")
        hide(lay, hidden)
        c = ui.render("PurpleCoinCounter", lay, [ui.anim("PurpleCoinCounter", "Appear")])
        arts.add_canvas(name, c, "starbit", pal="fixed")
    add_time(ui, arts, flash)
    lay = ui.shadowed("TimeUp")
    ui_lyt.set_text_recursive(lay, "TimeUp", ui.msg["Layout_TimeUpTimeUp"])
    c = ui.render("TimeUp", lay, [ui.anim("TimeUp", "Appear")])
    arts.add_canvas("T_TIMEUP", c, "getstar", dynamic=True, pal="fixed")
    for layname, key, prefix in (("FlyMeter", "Layout_FlyMeterFly", "FLY"),
                                 ("AirMeter", "Layout_AirMeterAir", "AIR")):
        meters = {}
        for danger in range(2):
            for count in range(33):
                lay = ui.shadowed(layname)
                ui_lyt.set_text_recursive(lay, "TxtTitle", ui.msg[key])
                states = [ui.anim(layname, "Appear"), ui.anim(layname, "Danger", 30 if danger else 0),
                          ui.anim(layname, "Count", count * 4)]
                meters[danger, count] = ui.render(layname, lay, states)
        box = union_box(bbox(c) for c in meters.values())
        for (danger, count), c in meters.items():
            arts.add_canvas("%s%d_%d" % (prefix, danger, count), c, "text", box, dynamic=True)
    for name, pane, key, anim, frame in RACE:
        lay = ui.shadowed("Race")
        ui_lyt.set_text_recursive(lay, pane, ui.msg_text(key))
        c = ui.render("Race", lay, [ui.anim("Race", anim, frame)])
        arts.add_canvas(name, c, "getstar" if name != "RACELOSE" else "toobad", dynamic=True, pal="fixed")
    return arts


def bank_quant(arts, bank):
    group = [a for a in arts.arts if a.bank == bank and not a.pal]
    legacy = [a for a in group if not a.name.startswith(NEW_TITLES)]
    return quant_cols(opaque_rgb(legacy or group), 15)


def slot_alias(pal, has_slots):
    seen, alias = {}, {}
    for k, col in enumerate(pal):
        alias[k] = seen.setdefault(rgb555(col), k) if has_slots else k
    return alias


def slot_group(arts, bank):
    return [a for a in arts.arts if a.bank == bank and a.pal == "slots"]


def hud_binary(ctx):
    arts = hud_arts(UI(ctx), False)
    bank_cols, bank_alias, slot_cols = {}, {}, {}
    for bank in arts.banks:
        bank_cols[bank] = unit_cols(ctx.fix_pal("hud_" + bank, bank_quant(arts, bank)))
    for bank, pal in bank_cols.items():
        group = slot_group(arts, bank)
        alias = bank_alias[bank] = slot_alias(pal, group)
        if group:
            free = [k for k in alias if alias[k] != k]
            cols = unit_cols(ctx.fix_pal("hud_slots_" + bank, quant_cols(opaque_rgb(group), len(free))))
            slot_cols[bank] = list(zip(free, cols))
    static, text = bytearray(), bytearray()
    for a in arts.arts:
        if a.pal == "slots":
            slots = slot_cols[a.bank]
            raw = index_px(a, [c for _, c in slots])
            idx = [0 if v == 0 else slots[v - 1][0] + 1 for v in raw]
        else:
            alias = bank_alias[a.bank]
            raw = index_px(a, bank_cols[a.bank])
            idx = [0 if v == 0 else alias[v - 1] + 1 for v in raw]
        store = text if a.dynamic else static
        for x, y, sw, sh in pieces_strict(a.w, a.h):
            store.extend(tiles_of(idx, a.w, a.h, x, y, sw, sh))
    pal = [0] * 256
    for bank, bi in arts.banks.items():
        for k, c in enumerate(bank_cols[bank]):
            pal[bi * 16 + 1 + k] = rgb555(c)
        for k, c in slot_cols.get(bank, []):
            pal[bi * 16 + 1 + k] = rgb555(c)
    return bytes(static), bytes(text) or bytes(32), struct.pack("<256H", *pal)


class HudHeader:
    def __init__(self, ctx):
        self.ctx = ctx
        self.ui = UI(ctx)
        self.arts = hud_arts(self.ui, True)
        self.positions = {}
        self.tables = []

    def pos(self, name, value):
        self.positions[name] = int(round(value))

    def box(self, name):
        return self.arts.by_name[name].box

    def last_box(self, pattern):
        return [a.box for a in self.arts.arts if re.fullmatch(pattern, a.name)][-1]

    def sample(self, name, anim, pane, target, f):
        return self.ui.sample(name, anim, pane, target, f)

    def table(self, decl, rows, per_line=12):
        self.tables.append("static const %s = {" % decl)
        for i in range(0, len(rows), per_line):
            self.tables.append("    " + " ".join("%s," % r for r in rows[i:i + per_line]))
        self.tables.append("};")
        self.tables.append("")

    def counters(self):
        pos = self.pos
        digit_win, coin, bit = self.box("DIGIT0"), self.box("COIN"), self.box("STARBIT")
        mario, star, life = self.box("MARIO"), self.box("STAR"), self.box("LIFE")
        pos("HUD_DIGIT_ADV_X16", 26 * S * 16)
        pos("HUD_STAR_X", gx(star[0]))
        pos("HUD_STAR_Y", gy(star[1]))
        pos("HUD_STAR_DIGITS_DX", (87 - star[0]) * S)
        pos("HUD_STAR_DIGITS_DY", (35 - star[1]) * S)
        pos("HUD_MARIO_X", gx(mario[0]))
        pos("HUD_MARIO_Y", gy(mario[1]))
        pos("HUD_MARIO_DIGITS_DX", (87 - mario[0]) * S)
        pos("HUD_MARIO_DIGITS_DY", (382 - mario[1]) * S)
        pos("HUD_COIN_RIGHT", gx(576))
        pos("HUD_COIN_Y", gy(341))
        pos("HUD_COIN_ICON_DX", (coin[0] - 550) * S)
        pos("HUD_COIN_ICON_DY", (coin[1] - 341) * S)
        pos("HUD_DIGIT_WIN_DX", (digit_win[0] - 550) * S)
        pos("HUD_DIGIT_WIN_DY", (digit_win[1] - 341) * S)
        pos("HUD_BIT_Y", gy(380.6))
        pos("HUD_BIT_ICON_DX", (bit[0] - 550) * S)
        pos("HUD_BIT_ICON_DY", (bit[1] - 380.6) * S)
        pos("HUD_METER_X", gx(MCX) - 16)
        pos("HUD_METER_Y", gy(MCY) - 16)
        pos("HUD_LIFE_X", gx(MCX) + (life[0] - MCX) * MS)
        pos("HUD_LIFE_Y", gy(MCY) + (life[1] - MCY) * MS)
        pos("HUD_SELECT_STARS_Y", gy(228 - 35) - 6)
        pos("HUD_SELECT_STAR_STEP", 140 * KX)
        pos("HUD_SELECT_STARS_DOWN_Y", gy(228 + 65) - 6)
        for name, pattern in (("HUD_TITLE_GALAXY_CY", r"T_\w+_NAME"), ("HUD_TITLE_SCENARIO_CY", r"T_\w+_M\d"),
                              ("HUD_PLATE_CY", r"T_\w+_PLATE"), ("HUD_GETSTAR_CY", "T_GETSTAR"),
                              ("HUD_TOOBAD_CY", "T_TOOBAD")):
            box = self.last_box(pattern)
            pos(name, gy((box[1] + box[3]) / 2))

    def purple_coins(self):
        pos = self.pos
        digit_win, pc_box, h_box = self.box("DIGIT0"), self.box("PCOIN"), self.box("PCOIN_100")
        path = gpath(self.ui.layout("PurpleCoinCounter"), "TxtNumber")
        x0, y0, _, _ = ui_lyt.local_rect(path[-1])
        v = ui_lyt.mat_vec(gmat(path), [x0, y0, 0.0, 1.0])
        px, py = 304 + v[0], 228 - v[1]
        pos("HUD_PCOIN_X", gx(pc_box[0]))
        pos("HUD_PCOIN_Y", gy(pc_box[1]))
        pos("HUD_PCOIN_DIGITS_DX", (px + digit_win[0] - 550 - pc_box[0]) * S)
        pos("HUD_PCOIN_DIGITS_DY", (py + digit_win[1] - 341 - pc_box[1]) * S)
        pos("HUD_PCOIN_100_DX", (h_box[0] - pc_box[0]) * S)
        pos("HUD_PCOIN_100_DY", (h_box[1] - pc_box[1]) * S)
        pos("HUD_PCOIN_DIGITS", 3)
        pos("HUD_PCOIN_COUNT_INTERVAL", 4)
        appear = self.ui.bundle("PurpleCoinCounter")["anims"]["appear"]
        pos("HUD_PCOIN_APPEAR_FRAMES", appear["frame_size"] - 1)
        rumble = [-cos(2 * PI * 2 * k / 15 + PI / 2) * 8 * (1 - k / 15) * S for k in range(15)]
        self.table("int8_t hud_pcoin_rumble_dy[15]", [int(round(v)) for v in rumble])

    def time_limit(self):
        pos, sample, ui = self.pos, self.sample, self.ui
        pivot = (304, 228 + 186)
        t_box, dwin, cwin = self.box("TIME_LABEL"), self.box("TIME0"), self.box("TIME_COLON")
        pos("HUD_TIME_CX", gx(pivot[0]))
        pos("HUD_TIME_CY", gy(pivot[1]))
        pos("HUD_TIME_LABEL_DX", (t_box[0] - pivot[0]) * S)
        pos("HUD_TIME_LABEL_DY", (t_box[1] - pivot[1]) * S)
        tp = ui_lyt.find_pane(ui.layout("TimeLimit"), "TxtNumber")
        font = ui.fonts[tp["font"].lower()]
        scx = tp["font_size"][0] / font.fw()

        def adv(ch):
            return font.width_of(font.char_index(ch))[2] * scx

        full = "00:00"
        bw = 0
        for ch in full:
            bw = bw + adv(ch)
        bw = bw + tp["char_space"] * (len(full) - 1)
        pen = (tp["size"][0] - bw) / 2
        for k, ch in enumerate(full):
            alone = (tp["size"][0] - adv(ch)) / 2
            win = dwin if ch != ":" else cwin
            pos("HUD_TIME_X%d" % k, (win[0] + (pen - alone) * 1.3 - pivot[0]) * S)
            pen += adv(ch) + tp["char_space"]
        pos("HUD_TIME_GLYPH_DY", (dwin[1] - pivot[1]) * S)
        an = ui.bundle("TimeLimit")["anims"]
        rows = []
        for f in range(an["appear"]["frame_size"]):
            dy = -(sample("TimeLimit", "appear", "TimeLimit", "trans_y", f) + 186) * S
            sc = sample("TimeLimit", "appear", "TimeLimit", "scale_x", f)
            vis = sample("TimeLimit", "appear", "TimeLimit", "visible", f)
            rows.append("{ %d, %d, %d }" % (round(dy), bool(vis), round(sc * 256)))
        self.tables.append("typedef struct { int8_t dy; uint8_t visible; uint16_t scale; } HudTimeAppear;")
        self.table("HudTimeAppear hud_time_appear[%d]" % len(rows), rows, 6)
        pos("HUD_TIME_APPEAR_FRAMES", len(rows) - 1)
        rows = []
        for f in range(an["timesignal"]["frame_size"]):
            rows.append(int(bool(sample("TimeLimit", "timesignal", "TimeLimit", "visible", f))))
        self.table("uint8_t hud_time_signal_visible[%d]" % len(rows), rows, 24)
        pos("HUD_TIME_SIGNAL_FRAMES", len(rows) - 1)
        pos("HUD_TIME_FLASH_FRAMES", an["timelimit"]["frame_size"])
        dol = Dol(self.ctx.read_sys("main.dol"))
        timing = []
        for i in range(5):
            o = dol.off(TIMING_TABLE + i * 12)
            a, b = struct.unpack_from(">II", dol.d, o)
            timing.append("{ %d, %d, %d, %d }" % (a, b, dol.d[o + 8], dol.d[o + 9]))
        self.table("uint32_t hud_time_timing[5][4]", timing, 5)

    def time_up(self):
        pos, sample = self.pos, self.sample
        box = self.box("T_TIMEUP")
        pos("HUD_TIMEUP_DX", (box[0] - 304) * S)
        pos("HUD_TIMEUP_DY", (box[1] - 228) * S)
        rows = []
        s_end = sample("TimeUp", "appear", "TimeUp", "scale_x", 60)
        frames = self.ui.bundle("TimeUp")["anims"]["appear"]["frame_size"]
        for f in range(frames):
            s = sample("TimeUp", "appear", "TimeUp", "scale_x", f) / s_end
            r = radians(sample("TimeUp", "appear", "TimeUp", "rot_z", f))
            a = sample("TimeUp", "appear", "TimeUp", "alpha", f)
            vals = (s * cos(r) * 256, s * sin(r) * 256, cos(r) / s * 256, sin(r) / s * 256, a)
            rows.append("{ %d, %d, %d, %d, %d }" % tuple(round(v) for v in vals))
        self.tables.append("typedef struct { int16_t fc, fs, pc, ps; uint8_t alpha; } HudTimeUpFrame;")
        self.table("HudTimeUpFrame hud_timeup_appear[%d]" % len(rows), rows, 4)
        pos("HUD_TIMEUP_APPEAR_FRAMES", frames - 1)
        for kind in ("FLY", "AIR"):
            box = self.box(kind + "0_0")
            pos("HUD_%s_DX" % kind, 60 * KX + (box[0] - 304) * S)
            pos("HUD_%s_DY" % kind, -70 * S + (box[1] - 228) * S)
        pos("HUD_RACE_CY", gy(228 - 20))

    def time_flash(self):
        sample = self.sample
        plate_cols = unit_cols(bank_quant(self.arts, "plate"))
        sgroup = slot_group(self.arts, "plate")
        alias = slot_alias(plate_cols, sgroup)
        free = [k for k in alias if alias[k] != k]
        slot_cols = list(zip(free, unit_cols(quant_cols(opaque_rgb(sgroup), len(free)))))
        ks = [k for k, _ in slot_cols]
        cs = [c for _, c in slot_cols]
        acc = {k: [] for k in ks}
        for a in sgroup:
            alt = list(a.alt.d)
            raw = index_px(a, cs)
            for i, v in enumerate(raw):
                if v:
                    acc[ks[v - 1]].append((alt[i * 4], alt[i * 4 + 1], alt[i * 4 + 2]))
        own = dict(slot_cols)
        red = {}
        for k in ks:
            v = acc[k]
            if not v:
                red[k] = own[k]
                continue
            sm = [0.0, 0.0, 0.0]
            for p in v:
                for j in range(3):
                    sm[j] = f32(sm[j] + p[j])
            red[k] = tuple(f32(sm[j] / len(v)) for j in range(3))
        g0 = sample("TimeLimit", "timelimit", "TxtNumber", "c0_g", 0)
        g1 = sample("TimeLimit", "timelimit", "TxtNumber", "c0_g", 15)
        rows = []
        for f in range(self.ui.bundle("TimeLimit")["anims"]["timelimit"]["frame_size"]):
            w = (g0 - sample("TimeLimit", "timelimit", "TxtNumber", "c0_g", f)) / (g0 - g1)
            cols = []
            for k in ks:
                cols.append("0x%04X" % rgb555([min(max(own[k][j] + w * (red[k][j] - own[k][j]), 0), 1)
                                               for j in range(3)]))
            rows.append("{ " + ", ".join(cols) + " }")
        self.pos("HUD_TIME_BANK", self.arts.banks["plate"])
        self.pos("HUD_TIME_SLOTS", len(ks))
        self.table("uint8_t hud_time_slot[HUD_TIME_SLOTS]", [k + 1 for k in ks])
        self.table("uint16_t hud_time_flash_pal[HUD_TIME_FLASH_FRAMES][HUD_TIME_SLOTS]", rows, 2)

    def pieces(self):
        piece_rows, art_rows = [], []
        nstat = ntext = 0
        for a in self.arts.arts:
            first = len(piece_rows)
            for x, y, sw, sh in pieces_strict(a.w, a.h):
                if a.dynamic:
                    tile, ntext = ntext, ntext + sw * sh // 64
                else:
                    tile, nstat = nstat, nstat + sw * sh // 64
                shape, size = SHAPES[(sw, sh)]
                piece_rows.append((tile, shape << 2 | size, x, y))
            end = ntext if a.dynamic else nstat
            art_rows.append((a.name, first, len(piece_rows) - first, self.arts.banks[a.bank], a.w, a.h,
                             a.dynamic, end))
        return piece_rows, art_rows

    def build(self):
        self.counters()
        self.purple_coins()
        self.time_limit()
        self.time_up()
        piece_rows, art_rows = self.pieces()
        rows_by = {r[0]: r for r in art_rows}
        t_first = piece_rows[rows_by["TIME_LABEL"][1]][0]
        self.pos("HUD_TIME_DATA", t_first * 32)
        self.pos("HUD_TIME_TILES", rows_by["TIME_COLON"][7] - t_first)
        self.time_flash()
        h = ["", "typedef struct { uint16_t tile; uint8_t obj, pad; int16_t x, y; } HudPiece;",
             "typedef struct { uint16_t first; uint8_t count, bank, dynamic; uint8_t w, h; uint16_t tiles; "
             "uint32_t data; } HudArt;", "", "enum {"]
        for r in art_rows:
            h.append("    HUD_ART_%s," % r[0])
        h += ["    HUD_ART_COUNT", "};", "", "static const HudPiece hud_pieces[] = {"]
        for tile, obj, x, y in piece_rows:
            h.append("    { %d, %d, 0, %d, %d }," % (tile, obj, x, y))
        h += ["};", "", "static const HudArt hud_arts[HUD_ART_COUNT] = {"]
        for name, first, count, bank, w, hh, dyn, end in art_rows:
            st = piece_rows[first][0]
            row = (name, first, count, bank, int(dyn), w, hh, end - st, st * 32)
            h.append("    [HUD_ART_%s] = { %d, %d, %d, %d, %d, %d, %d, %d }," % row)
        h += ["};", ""]
        for k, v in self.positions.items():
            h.append("#define %-24s %4d" % (k, v))
        h.append("")
        return h + self.tables


def hud_header(ctx):
    return HudHeader(ctx).build()
