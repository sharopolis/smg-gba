import json
import re

import ui_lyt
from ui_art import UI
from ui_headers_place import STAGES, zone_list
from ui_img import BOX, resize_u8
from ui_lyt import ptr
from ui_math import powf

SOURCE = {"honeyhive": "HoneyBeeKingdomGalaxy", "honeyhive_tower": "HoneyBeeKingdomGalaxy",
          "goodegg": "EggStarGalaxy", "beachbowl": "HeavenlyBeachGalaxy",
          "buoybase": "OceanFloaterLandGalaxy", "freezeflame": "IceVolcanoGalaxy",
          "gustygarden": "CosmosGardenGalaxy", "deepdark": "OceanPhantomCaveGalaxy"}
ZONES = dict(SOURCE, astrodome="AstroDome")
TAG = re.compile(r"\{tag:[0-9a-f]+\}")


def stage_titles(md):
    t = {s: md.flat["GalaxyName_" + g] for s, g in SOURCE.items()}
    t["observatory"] = TAG.split(md.flat["AstroGalaxy_Rosetta102"])[1]
    t["honeyhive_tower"] = t["honeyhive"].split()[0] + " Tower"
    t["astrodome"] = md.flat["ScenarioName_AstroDome1"]
    return t


def debug_points(ctx):
    md = ctx.messages()
    titles = stage_titles(md)
    out = ["", "static const char* const debug_stage_names[STAGE_COUNT] = {"]
    for s in STAGES:
        out.append(' [STAGE_%s] = "%s",' % (s.upper(), titles[s]))
    out.append("};")
    out.append("static const char* const debug_mission_names[STAGE_COUNT][6] = {")
    tag5 = "{tag:0500000000}"
    for s in STAGES:
        if s not in SOURCE:
            continue
        count = 2 if s == "buoybase" else 6
        names = [md.flat["ScenarioName_%s%d" % (SOURCE[s], i)] for i in range(1, count + 1)]
        player = md.flat["System_PlayerName000"]
        names = [re.sub(r"\s+", " ", n.replace(tag5, player)).strip() for n in names]
        out.append(" [STAGE_%s] = {" % s.upper() + ",".join(json.dumps(n) for n in names) + "},")
    out.append("};")
    out.append("typedef struct { uint8_t stage, zone; const char* name; } DebugZoneName;")
    out.append("static const DebugZoneName debug_zone_names[] = {")
    for s in STAGES:
        if s in ZONES:
            names = list(enumerate(zone_list(ctx, ZONES[s])))
        else:
            names = [(0, titles[s])]
        for zone, name in names:
            name = re.sub(r"(Galaxy|Zone)$", "", name)
            name = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", name).replace("Bee Kingdom", "hive")
            out.append(' { STAGE_%s, %d, "%s" },' % (s.upper(), zone, name))
    out.append("};")
    return out


def c_array(name, values, shape):
    lines = [",".join(str(int(x)) for x in values[i:i + 32]) for i in range(0, len(values), 32)]
    body = [l + "," for l in lines[:-1]] + [lines[-1], "};"]
    return ["static const uint8_t %s%s = {" % (name, shape)] + body


def menu_background(ctx):
    ui = UI(ctx)
    fonts = ui.fonts
    bundle = ui.bundle("PauseMenu")
    lay = ui_lyt.deep(bundle["layout"])
    for child in lay["root"]["children"]:
        if child["name"] not in ("BG", "PicStagebase", "PicStagebase1"):
            child["visible"] = False
        else:
            child["children"] = []
    canvas = ui_lyt.render(bundle, fonts, [(bundle["anims"]["appear"], 29)], layout=lay)
    n = canvas.w * canvas.h
    rgb = bytearray(n * 3)
    ui_lyt.LIB.composite_bg(canvas.d, n, ui_lyt.dbuf([0.015, 0.025, 0.07]), ptr(rgb))
    bg = resize_u8(rgb, canvas.w, canvas.h, 3, 240, 160, BOX)
    pal = bytearray(ctx.read_out("assets/palette.rgba"))
    idx = bytearray(240 * 160)
    ui_lyt.LIB.nearest_pal(ptr(bg), 240 * 160, ptr(pal), 256, ui_lyt.ibuf([3, 4, 2]), ptr(idx))
    return fonts, idx


def glyph_alpha(font, gi):
    gw = font.width_of(gi)[1]
    s, gx0, gy0 = font.glyph(gi)
    sheet = font.sheet(s)
    gh = min(font.cell_height, font.sheet_height - gy0)
    gcw = min(gw, font.cell_width, font.sheet_width - gx0)
    if gw <= 0:
        return bytearray(32), 1, 32
    alpha = bytearray(gcw * gh)
    for y in range(gh):
        for x in range(gcw):
            alpha[y * gcw + x] = sheet[((gy0 + y) * font.sheet_width + gx0 + x) * 4 + 3]
    return alpha, gcw, gh


def debug_menu_art(ctx):
    fonts, idx = menu_background(ctx)
    font = fonts["messagefont26.brfnt"]
    masks, widths, advances = [], [], []
    for code in range(32, 127):
        gi = font.char_index(chr(code))
        _, gw, cw = font.width_of(gi)
        alpha, aw, ah = glyph_alpha(font, gi)
        w = max(1, min(11, round(gw * 11 / 32)))
        small = resize_u8(alpha, aw, ah, 1, w, 11, BOX)
        mask = [0] * 121
        for y in range(11):
            for x in range(w):
                v = small[y * w + x] / 255
                mask[y * 11 + x] = min(max(round(powf(v, 0.7) * 3), 0), 3)
        masks += mask
        widths.append(w)
        advances.append(max(3, round(cw * 11 / 32)))
    out = [""]
    out += c_array("debug_menu_background", list(idx), "[240*160]")
    out += c_array("debug_font", masks, "[95][11][11]")
    out += c_array("debug_font_width", widths, "[95]")
    out += c_array("debug_font_advance", advances, "[95]")
    return out
