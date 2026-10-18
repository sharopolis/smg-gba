import re

import ui_lyt
from common import f32
from ui_img import bbox, index_rgba, median_cut, shrink
from ui_lyt import EYE, find_pane, iter_panes, mat4, mat_mul, mat_vec

S = 160 / 456
KX = 240 / 608
SHAPES = {(8, 8): (0, 0), (16, 16): (0, 1), (32, 32): (0, 2), (64, 64): (0, 3),
          (16, 8): (1, 0), (32, 8): (1, 1), (32, 16): (1, 2), (64, 32): (1, 3),
          (8, 16): (2, 0), (8, 32): (2, 1), (16, 32): (2, 2), (32, 64): (2, 3)}
GALAXIES = [("HONEYHIVE", "HoneyBeeKingdomGalaxy"), ("GOODEGG", "EggStarGalaxy"),
            ("BEACHBOWL", "HeavenlyBeachGalaxy"), ("FREEZEFLAME", "IceVolcanoGalaxy"),
            ("DEEPDARK", "OceanPhantomCaveGalaxy"), ("GUSTYGARDEN", "CosmosGardenGalaxy"),
            ("BUOYBASE", "OceanFloaterLandGalaxy")]
TAG = re.compile(r"(\{tag:[0-9a-f]+\})")


class UI:
    def __init__(self, ctx):
        self.ctx = ctx
        self.fonts = ctx.memo("fonts", lambda: ui_lyt.load_fonts(ctx.arc("LayoutData/Font.arc")))
        self.md = ctx.messages()
        self.msg = self.md.flat
        self.player = self.msg["System_PlayerName000"]

    def bundle(self, name):
        arc = "LayoutData/%s.arc" % name
        return self.ctx.memo(("lyt", name), lambda: ui_lyt.load_layout(self.ctx.arc(arc)))

    def layout(self, name):
        return ui_lyt.apply_language(self.bundle(name)["layout"], "UsEn")

    def shadowed(self, name):
        return opaque_shadows(self.layout(name))

    def anim(self, name, anim, f=None):
        an = self.bundle(name)["anims"][anim.lower()]
        return an, max(an["frame_size"] - 1, 0) if f is None else f

    def render(self, name, lay, states):
        return ui_lyt.render(self.bundle(name), self.fonts, states, lang=None, layout=lay)

    def msg_text(self, key, args=()):
        out, ai = [], iter(args)
        for m in TAG.split(self.msg[key]):
            t = re.match(r"\{tag:([0-9a-f]+)\}", m)
            if not t:
                out.append(m)
            elif t.group(1)[:2] == "03":
                out.append(chr(ui_lyt.PICTURE_CHAR_BASE + int(t.group(1)[2:6], 16)))
            elif t.group(1)[:2] in ("05", "06", "07"):
                out.append(str(next(ai, "")))
        return "".join(out)

    def sample(self, name, anim, element, target, f):
        for c in self.bundle(name)["anims"][anim.lower()]["contents"]:
            if c["name"] == element:
                for tr in c["tracks"]:
                    if tr["target"] == target:
                        return ui_lyt.sample_track(tr, f)
        raise KeyError((name, anim, element, target))


def hide(lay, *names):
    for n in names:
        p = find_pane(lay, n)
        if p:
            p["visible"] = False


def opaque_shadows(lay):
    for p in iter_panes(lay["root"]):
        if p["name"].startswith("Sha"):
            p["alpha"] = 255
    return lay


def only(lay, keep):
    keep_set = set()

    def mark(p, chain):
        if p["name"] in keep:
            keep_set.update(q["name"] for q in chain)
            for q in iter_panes(p):
                keep_set.add(q["name"])
        for c in p["children"]:
            mark(c, chain + [p])

    mark(lay["root"], [])
    inside = set()
    for p in iter_panes(lay["root"]):
        if p["name"] in keep:
            inside.update(q["name"] for q in iter_panes(p))

    def rec(p):
        if p["name"] not in keep_set:
            p["visible"] = False
            return
        if p["name"] not in inside and p["kind"] != "pane":
            p["kind"] = "pane"
        for c in p["children"]:
            rec(c)

    rec(lay["root"])
    return lay


def gpath(lay, name):
    path = []

    def rec(p, chain):
        if p["name"] == name:
            path.extend(chain + [p])
            return True
        return any(rec(c, chain + [p]) for c in p["children"])

    rec(lay["root"], [])
    return path


def gmat(path):
    M = EYE
    for p in path:
        M = mat_mul(M, mat4(p))
    return M


def gpos(lay, name):
    M = gmat(gpath(lay, name))
    return M[0][3], M[1][3]


def inv4(m):
    a = [row[:] + [1.0 if i == j else 0.0 for j in range(4)] for i, row in enumerate(m)]
    for c in range(4):
        piv = max(range(c, 4), key=lambda r: abs(a[r][c]))
        a[c], a[piv] = a[piv], a[c]
        d = a[c][c]
        a[c] = [v / d for v in a[c]]
        for r in range(4):
            if r != c and a[r][c] != 0.0:
                f = a[r][c]
                a[r] = [v - f * w for v, w in zip(a[r], a[c])]
    return [row[4:] for row in a]


def set_global(lay, name, x, y):
    path = gpath(lay, name)
    loc = mat_vec(inv4(gmat(path[:-1])), [x, y, 0.0, 1.0])
    path[-1]["translate"][0], path[-1]["translate"][1] = loc[0], loc[1]


def name_plate(ui, text, frame):
    lay = ui.shadowed("GalaxyNamePlate")
    hide(lay, "GalaxyNamePlateU", "NamePlate", "ShaGalaxyName")
    ui_lyt.set_text_recursive(lay, "GalaxyName", text)
    tb = bbox(ui.render("GalaxyNamePlate", lay, [ui.anim("GalaxyNamePlate", "Unknown", frame)]))
    lay = ui.shadowed("GalaxyNamePlate")
    hide(lay, "GalaxyNamePlateU")
    ui_lyt.set_text_recursive(lay, "GalaxyName", text)
    states = [ui.anim("GalaxyNamePlate", "Unknown", frame),
              ui.anim("GalaxyNamePlate", "OneLine", min(512, tb[2] - tb[0])),
              ui.anim("GalaxyNamePlate", "Appear")]
    return ui.render("GalaxyNamePlate", lay, states)


def back_button(ui):
    lay = ui.shadowed("BackButton")
    ui_lyt.set_text_recursive(lay, "BButtonText", ui.msg_text("Layout_BackButtonBack"))
    states = [ui.anim("BackButton", "ButtonAppear", 20), ui.anim("BackButton", "ButtonWait", 0)]
    return ui.render("BackButton", lay, states)


def gx(sx):
    return 120 + (sx - 304) * KX


def gy(sy):
    return 80 + (sy - 228) * S


class Art:
    def __init__(self, name, img, bank, box, slot=None, dynamic=False, pal=None, alt=None):
        self.name, self.bank, self.box = name, bank, box
        self.slot, self.dynamic, self.pal, self.alt = slot, dynamic, pal, alt
        self.w, self.h = img.w, img.h
        d = list(img.d)
        for i in range(3, len(d), 4):
            d[i] = 1.0 if d[i] >= 0.5 else 0.0
        self.px = d


class ArtList:
    def __init__(self, ui):
        self.ui = ui
        self.arts = []
        self.banks = {}
        self.by_name = {}

    def add(self, name, img, bank, box, **kw):
        if bank not in self.banks:
            self.banks[bank] = len(self.banks)
        art = Art(name, img, bank, box, **kw)
        self.arts.append(art)
        self.by_name[name] = art
        return art

    def add_canvas(self, name, canvas, bank, box=None, scale=S, **kw):
        box = box or bbox(canvas)
        return self.add(name, shrink(canvas, box, scale), bank, box, **kw)

    def add_digits(self, prefix, canvases, bank):
        boxes = [bbox(c) for c in canvases]
        win = union_box(boxes)
        for d, c in enumerate(canvases):
            self.add_canvas("%s%d" % (prefix, d), c, bank, win)
        return boxes

    def in_bank(self, bank):
        return [a for a in self.arts if a.bank == bank]


def union_box(boxes):
    boxes = list(boxes)
    return (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes),
            max(b[3] for b in boxes))


def opaque_rgb(arts):
    out = []
    for a in arts:
        d = a.px
        for i in range(0, len(d), 4):
            if d[i + 3] > 0:
                out.append((d[i], d[i + 1], d[i + 2]))
    return out


def quant_cols(rgbs, colors):
    pix = [(int(f32(r * 255.0)), int(f32(g * 255.0)), int(f32(b * 255.0))) for r, g, b in rgbs]
    return median_cut(pix, colors)


def no_fix(key, vals):
    return vals


def unit_cols(pal):
    return [tuple(v / 255 for v in c) for c in pal]


def index_px(a, cols):
    return index_rgba(ui_lyt.dbuf(a.px), a.w * a.h, cols)


def fit(w, h):
    best = None
    for sw, sh in SHAPES:
        if sw >= w and sh >= h and (best is None or sw * sh < best[0] * best[1]):
            best = (sw, sh)
    return best


def split_pieces(w, h):
    cw, ch = (32, 16) if h <= 16 else (64, 32)
    out = []
    for y in range(0, h, ch):
        for x in range(0, w, cw):
            out.append((x, y) + fit(min(cw, w - x), min(ch, h - y)))
    return out


def pieces_strict(w, h):
    f = fit(w, h)
    if f:
        return [(0, 0) + f]
    return split_pieces(w, h)


def pieces_loose(w, h):
    f = fit(w, h)
    if f and f[0] * f[1] <= 1.6 * (-(-w // 8) * 8) * (-(-h // 8) * 8):
        return [(0, 0) + f]
    return split_pieces(w, h)


def tiles_of(idx, iw, ih, x, y, w, h):
    data = bytearray()
    for ty in range(h // 8):
        for tx in range(w // 8):
            for r in range(8):
                yy = y + ty * 8 + r
                for k in range(0, 8, 2):
                    xa, xb = x + tx * 8 + k, x + tx * 8 + k + 1
                    va = idx[yy * iw + xa] if yy < ih and xa < iw else 0
                    vb = idx[yy * iw + xb] if yy < ih and xb < iw else 0
                    data.append(va | vb << 4)
    return bytes(data)
