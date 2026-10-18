import ctypes
import math
import re

import ui_lyt
from common import f32, fsum
from ui_art import KX, S, UI, gmat, gpath, hide, opaque_shadows, tiles_of
from ui_data import Tag
from ui_headers_place import STAGES, galaxy, mtx_mul, mtx_tr
from ui_img import BILINEAR, bbox, index_rgba, median_cut, resize_f32, resize_rgba
from ui_lyt import Img, find_pane, iter_panes, ptr
from ui_math import cos, powf, radians, rgb555, sin

FS_X, FS_Y = 24.3, 33.0
FONT_SIZES = {0: 0.75, 1: 1.0, 2: 1.5}
LINE_H_EVENT = (FS_Y + 0.0) * S
LINE_H_SHORT = (FS_Y - 2.0) * S
NODE_MSG, NODE_BRANCH, NODE_EVENT, NODE_END = 1, 2, 3, 0xFFFF
FOLD = {0: False, 1: False, 2: True, 5: True, 7: False, 10: False, 11: False, 13: False, 14: False}
HAND_TEX = ["HandPointerPoint.tpl", "HandPointerGrip.tpl", "HandPointerRelease.tpl"]
GALAXY_OF = {"observatory": "AstroGalaxy", "honeyhive": "HoneyBeeKingdomGalaxy", "goodegg": "EggStarGalaxy",
             "freezeflame": "IceVolcanoGalaxy", "gustygarden": "CosmosGardenGalaxy",
             "buoybase": "OceanFloaterLandGalaxy", "deepdark": "OceanPhantomCaveGalaxy"}
KINDS = {
    "rabbit": ["Rabbit"], "trickrabbit": ["TrickRabbit"], "penguinskater": ["PenguinSkater"],
    "syati": ["Syati"], "talksyati": ["TalkSyati"], "caretaker": ["Caretaker"], "buoypenguin": ["Penguin"],
    "tico": ["Tico", "TicoAstro", "TicoComet", "TicoFat", "TicoShop", "TicoRail", "TicoGalaxy"],
    "kinopio": ["Kinopio", "KinopioAstro"], "rosetta": ["Rosetta"],
    "butler": ["Butler", "ButlerMap", "ButlerExplain"], "honeybee": ["HoneyBee"], "luigi": ["LuigiEvent"],
    "ticofat": ["TicoFat"], "honeyqueen": ["HoneyQueen"],
}
KIND_OF = {"Tico": "tico", "TicoAstro": "tico", "TicoComet": "tico", "TicoShop": "tico", "TicoRail": "tico",
           "TicoGalaxy": "tico", "TicoFat": "ticofat", "Kinopio": "kinopio", "KinopioAstro": "kinopio",
           "Rosetta": "rosetta", "Butler": "butler", "ButlerMap": "butler", "ButlerExplain": "butler",
           "HoneyBee": "honeybee", "LuigiEvent": "luigi", "HoneyQueen": "honeyqueen",
           "Caretaker": "caretaker", "Penguin": "buoypenguin", "Rabbit": "rabbit",
           "TrickRabbit": "trickrabbit", "PenguinSkater": "penguinskater", "Syati": "syati",
           "TalkSyati": "talksyati"}
CTRL = {
    "PenguinSkater": ("PenguinSkater", 240, 150), "Syati": ("SyatiRing", 2500, 179),
    "TalkSyati": ("SyatiRing", 700, 240), "Caretaker": ("Caretaker", 240, 150),
    "Penguin": ("Penguin", 240, 150),
    "Tico": ("Tico", 350, 120), "TicoAstro": ("Tico", 350, 120), "TicoComet": ("TicoComet", 350, 120),
    "TicoFat": ("TicoFat", 280, 220), "TicoShop": ("TicoShop", 350, 120), "TicoRail": ("TicoRail", 350, 120),
    "Kinopio": ("Kinopio", 200, 146), "KinopioAstro": ("Kinopio", 200, 146),
    "Rosetta": ("Rosetta", 200, 230), "Butler": ("Butler", 240, 150), "ButlerMap": ("ButlerMap", 240, 150),
    "ButlerExplain": ("ButlerExplain", 240, 150), "HoneyBee": ("HoneyBee", 240, 150),
    "LuigiEvent": ("LuigiEvent", 240, 150), "HoneyQueen": ("HoneyQueen", 0, 950),
    "Rabbit": ("Rabbit", 450, 160), "TrickRabbit": ("TrickRabbit", 300, 120),
}
FRONT_ONLY = {"Rosetta", "TicoFat"}
LATE = {"HoneyBeeKingdomGalaxy": ("LuigiEvent",), "OceanPhantomCaveGalaxy": ("Syati", "TalkSyati")}


def rgb(c):
    return tuple(v / 255.0 for v in c)


TEXT_GREY = rgb((77, 77, 77))
WHITE = rgb((255, 255, 255))
TAG_COLOURS = [rgb((255, 255, 255)), rgb((220, 130, 130)), rgb((80, 170, 80)), rgb((80, 140, 210)),
               rgb((235, 200, 0)), rgb((180, 110, 200))]


class FImg:
    __slots__ = ("w", "h", "d")

    def __init__(self, w, h, d=None):
        self.w, self.h = w, h
        self.d = d if d is not None else [0.0] * (w * h * 4)

    def cbuf(self):
        return (ctypes.c_double * len(self.d))(*self.d)

    def opaque(self, thr=0.5):
        out = []
        d = self.d
        for i in range(0, len(d), 4):
            if d[i + 3] >= thr:
                out.append((d[i], d[i + 1], d[i + 2]))
        return out


def from_img(img):
    return FImg(img.w, img.h, list(img.d))


def over(dst, src, x, y):
    ui_lyt.LIB.over(dst.d, dst.w, dst.h, src.d, src.w, src.h, x, y)


def supersample(src, w, h, bw, bh, tw, th):
    big = resize_f32(src, w, h, bw, bh)
    pad = (ctypes.c_float * (tw * 8 * th * 8))()
    n = min(bw, tw * 8)
    for y in range(min(bh, th * 8)):
        pad[y * tw * 8:y * tw * 8 + n] = big[y * bw:y * bw + n]
    return resize_f32(pad, tw * 8, th * 8, tw, th)


def shrink_t(canvas, box):
    x0, y0, x1, y1 = box
    cw, ch = x1 - x0, y1 - y0
    w, h = max(1, int(math.ceil(cw * S))), max(1, int(math.ceil(ch * S)))
    n = cw * ch
    planes = (ctypes.c_float * (4 * n))()
    ui_lyt.LIB.shrink_pm(canvas.d, canvas.w, canvas.h, x0, y0, x1, y1, planes)
    bw, bh = max(1, round(cw * S * 8)), max(1, round(ch * S * 8))
    out = (ctypes.c_float * (4 * w * h))()
    for k in range(4):
        res = supersample(ctypes.byref(planes, 4 * k * n), cw, ch, bw, bh, w, h)
        ctypes.memmove(ctypes.byref(out, 4 * k * w * h), res, 4 * w * h)
    img = Img(w, h)
    ui_lyt.LIB.unpremul(out, w * h, img.d, 1)
    return from_img(img)


def crop_alpha(img):
    xs, ys = [], []
    for y in range(img.h):
        for x in range(img.w):
            if img.d[(y * img.w + x) * 4 + 3] >= 0.5:
                xs.append(x)
                ys.append(y)
    if not xs:
        return FImg(1, 1, img.d[:4]), 0, 0
    x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    return crop(img, x0, y0, x1, y1), x0, y0


def crop(img, x0, y0, x1, y1):
    out = []
    for y in range(y0, y1):
        out += img.d[(y * img.w + x0) * 4:(y * img.w + x1) * 4]
    return FImg(x1 - x0, y1 - y0, out)


class Palette:
    def __init__(self, fixed):
        self.cols = [tuple(c) for c in fixed]

    def fill(self, imgs, n=15):
        free = n - len(self.cols)
        if free <= 0:
            return
        px = []
        for im in imgs:
            px += im.opaque()
        if self.cols:
            keep = []
            for p in px:
                best = None
                for c in self.cols:
                    d0, d1, d2 = p[0] - c[0], p[1] - c[1], p[2] - c[2]
                    d = (d0 * d0 + d1 * d1) + d2 * d2
                    if best is None or d < best:
                        best = d
                if best > 0.004:
                    keep.append(p)
            px = keep
        if not px:
            return
        pix = [(int(f32(r * 255.0)), int(f32(g * 255.0)), int(f32(b * 255.0))) for r, g, b in px]
        for c in median_cut(pix, free):
            self.cols.append(tuple(v / 255 for v in c))

    def index(self, img):
        return index_rgba(img.cbuf(), img.w * img.h, self.cols)

    def words(self):
        out = [0] * 16
        for k, c in enumerate(self.cols[:15]):
            out[k + 1] = rgb555(c)
        return out


def erode(mask):
    out = []
    for i in range(128 * 128):
        y, x = divmod(i, 128)
        out.append(mask[i] and mask[((y - 1) % 128) * 128 + x] and mask[((y + 1) % 128) * 128 + x] and
                   mask[y * 128 + (x - 1) % 128] and mask[y * 128 + (x + 1) % 128])
    return out


def c_rows(decl, fmt, rows):
    return ["static const %s[] = {" % decl] + ["    " + fmt % tuple(r) + "," for r in rows] + ["};"]


def scenario_mask(scenarios, add6):
    sc = list(scenarios)
    if add6 and 1 in sc and 6 not in sc:
        sc.append(6)
    m = 0
    for s in sc:
        m |= 1 << (s - 1)
    return m


def advance_of(font, ch):
    return font.width_of(font.char_index(ch))[2]


def message_text(m):
    out = []
    for p in m.parts:
        if isinstance(p, str):
            out.append(p)
        else:
            out.append("{<%d.%d %s>}" % (p.group, p.tag, p.params.hex()))
    return "".join(out)


class Talk:
    def __init__(self, ctx):
        self.ctx = ctx
        self.ui = UI(ctx)
        self.md = ctx.messages()
        self.mfont = self.ui.fonts["messagefont26.brfnt"]
        self.pfont = self.ui.fonts["picturefont.brfnt"]

    def lay_with(self, name, states):
        anims = self.ui.bundle(name)["anims"]
        return ui_lyt.apply_anims(self.ui.layout(name), [(anims[a.lower()], f) for a, f in states])

    @staticmethod
    def screen_of(lay, name):
        M = gmat(gpath(lay, name))
        return 304 + M[0][3], 228 - M[1][3]

    def tex(self, bundle, name):
        return bundle["textures"][name.lower()]

    def draw_window(self, canvas, lay, bundle, pane_name, alpha_scale=1.0):
        p = find_pane(lay, pane_name)
        cx, cy = self.screen_of(lay, pane_name)
        w, h = p["size"]
        x0, y0 = int(round(cx - w / 2)), int(round(cy - h / 2))
        w, h = int(round(w)), int(round(h))
        mats = {m["name"].strip(): m for m in lay["materials"]}
        fmat = mats[p["frames"][0]["material"].strip()]
        cmat = mats[p["content"]["material"].strip()]
        ftex = self.tex(bundle, fmat["texmaps"][0]["texture"])

        def cc(m, key):
            return [v / 255 for v in m[key]]
        if cmat["texmaps"]:
            ctex = self.tex(bundle, cmat["texmaps"][0]["texture"])
            a, b = cc(cmat, "tev_color0_black"), cc(cmat, "tev_color1_white")
            col = [a[k] + (b[k] - a[k]) * (ctex.rgba[k] / 255.0) for k in range(4)]
        else:
            col = cc(cmat, "tev_color1_white")
        img = Img(w, h)
        f0, f1 = ui_lyt.dbuf(cc(fmat, "tev_color0_black")), ui_lyt.dbuf(cc(fmat, "tev_color1_white"))
        ui_lyt.LIB.window_img(img.d, w, h, ptr(ftex.rgba), ftex.w, ftex.h, f0, f1, ui_lyt.dbuf(col),
                              p["alpha"] / 255.0 * alpha_scale)
        over(canvas, img, x0, y0)

    def run(self):
        ev = self.ui.bundle("TalkBalloonEvent")
        ev_lay = self.lay_with("TalkBalloonEvent", [("Wait", 0)])
        ev_canvas = Img(608, 456)
        self.draw_window(ev_canvas, ev_lay, ev, "ShaBalloon", alpha_scale=255 / 100.0)
        self.draw_window(ev_canvas, ev_lay, ev, "WinBalloon")
        ev_box = bbox(ev_canvas)
        self.ev_img = shrink_t(ev_canvas, ev_box)
        self.ev_x = int(round(120 + (ev_box[0] - 304) * KX))
        self.ev_y = int(round(80 + (ev_box[1] - 228) * S))
        tx, ty = self.screen_of(ev_lay, "TxtText00")
        self.ev_text_x = (tx - 240 - ev_box[0]) * S
        self.ev_text_cy = (ty - ev_box[1]) * S
        ax, ay = self.screen_of(ev_lay, "AButtonPosition")
        self.ev_abutton = ((ax - ev_box[0]) * S, (ay - ev_box[1]) * S)
        self.ev_beaks = self.beak_art(ev, "TalkBalloonEvent", [10, 50, 90])
        self.st_beaks = self.beak_art(self.ui.bundle("TalkBalloonStretch"), "TalkBalloonStretch", [40])
        self.prompt = self.icon_a(True, 0)
        self.abtn = [self.icon_a(False, 0), self.icon_a(False, 20)]
        self.pointer()
        self.npc_list()
        self.flows()
        self.text()
        return self.output()

    def stretch_balloon(self, text_w, two_lines):
        states = [("Appear", 20), ("TwoLine" if two_lines else "OneLine", min(512.0, text_w))]
        lay = self.lay_with("TalkBalloonStretch", states)
        big = Img(1008, 656)
        st = self.ui.bundle("TalkBalloonStretch")
        for pn, sc in (("ShaBalloon", 255 / 100.0), ("WinBalloon", 1.0)):
            sub = Img(608, 456)
            self.draw_window(sub, lay, st, pn, sc)
            over(big, sub, 200, 100)
        box = bbox(big)
        img = shrink_t(big, box)
        tx, ty = self.screen_of(lay, "TxtText")
        text_left = (tx + 200 - box[0]) * S
        text_cy = (ty + 100 - box[1]) * S
        bx, by = self.screen_of(lay, "BalloonStret")
        anchor = ((bx + 200 - box[0]) * S, (by + 100 - box[1]) * S)
        return img, text_left, text_cy, anchor

    def beak_art(self, bundle, lay_name, frames):
        if lay_name == "TalkBalloonEvent":
            lay = self.lay_with(lay_name, [("Wait", 0)])
        else:
            lay = self.lay_with(lay_name, [("Appear", 20), ("OneLine", 100)])
        pic = find_pane(lay, "PicBeak")
        sha = find_pane(lay, "ShaBeak")
        mats = {m["name"].strip(): m for m in lay["materials"]}
        tex = self.tex(bundle, mats["PicBeak"]["texmaps"][0]["texture"])
        size = pic["size"][0]
        dx, dy = sha["translate"][0] - pic["translate"][0], -(sha["translate"][1] - pic["translate"][1])
        out = []
        for f in frames:
            ang = radians(-50 + f)
            ca, sa = cos(ang), sin(ang)
            n = int(size * 2)
            cv = Img(n, n)
            th, tw = tex.h, tex.w
            for (ox_, oy_, colour) in ((dx, dy, (0.0, 0.0, 0.0)), (0, 0, (1.0, 1.0, 1.0))):
                layer = Img(n, n)
                for y in range(n):
                    v = y + 0.5 - n / 4 - oy_
                    for x in range(n):
                        u = x + 0.5 - n / 2 - ox_
                        ru = u * ca - v * sa
                        rv = u * sa + v * ca
                        tu = ((ru / size) + 0.5) * tw
                        tv = (rv / size) * th
                        o = (y * n + x) * 4
                        layer.d[o] = colour[0]
                        layer.d[o + 1] = colour[1]
                        layer.d[o + 2] = colour[2]
                        if 0 <= tu < tw and 0 <= tv < th:
                            layer.d[o + 3] = tex.rgba[(int(tv) * tw + int(tu)) * 4 + 3] / 255.0
                over(cv, layer, 0, 0)
            img = shrink_t(cv, (0, 0, n, n))
            img, cx0, cy0 = crop_alpha(img)
            out.append((img, n / 2 * S - cx0, n / 4 * S - cy0))
        return out

    def icon_a(self, with_text, press_frame):
        lay = self.lay_with("IconAButton", [("Appear", 15), ("Press", press_frame)])
        if with_text:
            ui_lyt.set_text_recursive(lay, "ShaIconA", message_text(self.md.get("Layout_SystemTalk")))
        else:
            hide(lay, "PicPlate", "ShaIconA", "TxtIconA")
        c = self.ui.render("IconAButton", opaque_shadows(lay), ())
        cx, cy = self.screen_of(lay, "IconAButton")
        box = bbox(c)
        img = shrink_t(c, box)
        return img, (cx - box[0]) * S, (cy - box[1]) * S

    def pointer(self):
        dp = self.ui.bundle("DPDPointer")
        lay = self.lay_with("DPDPointer", [("Wait", 0)])
        keep = set()
        for n in ["PicFrame", "PicPointer"]:
            for q in iter_panes(find_pane(lay, n)):
                keep.add(q["name"])
            for q in gpath(lay, n):
                keep.add(q["name"])
        for p in iter_panes(lay["root"]):
            if p["name"] not in keep:
                p["visible"] = False
            if p["name"].startswith("Sha"):
                p["alpha"] = 255
        c = self.ui.render("DPDPointer", lay, ())
        cx, cy = self.screen_of(lay, "DPDPointer")
        half = 16 / S / 2
        self.pointer_star = shrink_t(c, (int(cx - half), int(cy - half), int(cx + half), int(cy + half)))
        dlay = self.lay_with("DPDPointer", [("Wait", 0)])
        dmats = {m["name"].strip(): m for m in dlay["materials"]}
        self.hands = []
        for ht in (0, 1, 2):
            cv = Img(128, 128)
            hand = self.tex(dp, HAND_TEX[ht])
            m = dmats["PicHand"]
            c0 = [v / 255 for v in m["tev_color0_black"]]
            c1 = [v / 255 for v in m["tev_color1_white"]]
            u8 = bytes(int((v / 255.0) * 255) for v in hand.rgba)
            h = [v / 255 for v in resize_rgba(u8, hand.w, hand.h, 47, 47, BILINEAR)]
            layer = Img(47, 47)
            for i in range(47 * 47):
                for k in range(3):
                    layer.d[i * 4 + k] = c0[k] + (c1[k] - c0[k]) * h[i * 4 + k]
                layer.d[i * 4 + 3] = h[i * 4 + 3]
            over(cv, layer, 64 - 23, 64 - 23)
            glove = [cv.d[i * 4 + 3] > 0.5 for i in range(128 * 128)]
            rows = [fsum(glove[y * 128:(y + 1) * 128]) for y in range(128)]
            mx = max(rows)
            pxs, pys = [], []
            for y in range(128):
                if rows[y] >= 0.6 * mx:
                    for x in range(128):
                        if glove[y * 128 + x]:
                            pxs.append(x)
                            pys.append(y)
            pcx, pcy = int(round(fsum(pxs) / len(pxs))), int(round(fsum(pys) / len(pys)))
            inner = glove[:]
            for _ in range(4):
                inner = erode(inner)
            for tname, mname, sz in (("DPDOutStar.tpl", "PicPlayer1", 14), ("DPDPointer.tpl", "PicStar", 12)):
                t = self.tex(dp, tname)
                tw, th = t.w, t.h
                vals = [v / 255.0 for v in t.rgba]
                if th * 2 == tw:
                    full = list(vals)
                    for y in range(th - 1, -1, -1):
                        full += vals[y * tw * 4:(y + 1) * tw * 4]
                    fh = th * 2
                else:
                    full, fh = vals, th
                mm = dmats[mname]
                a0 = [v / 255 for v in mm["tev_color0_black"]]
                a1 = [v / 255 for v in mm["tev_color1_white"]]
                im = resize_rgba(bytes(int(v * 255) for v in full), tw, fh, sz, sz, BILINEAR)
                im = [v / 255 for v in im]
                sx, sy = pcx - sz // 2, pcy - sz // 2
                layer = Img(sz, sz)
                for y in range(sz):
                    for x in range(sz):
                        i = y * sz + x
                        for k in range(3):
                            layer.d[i * 4 + k] = a0[k] + (a1[k] - a0[k]) * im[i * 4 + k]
                        cy_, cx_ = sy + y, sx + x
                        clipv = 1.0 if (0 <= cy_ < 128 and 0 <= cx_ < 128 and inner[cy_ * 128 + cx_]) else 0.0
                        layer.d[i * 4 + 3] = im[i * 4 + 3] * clipv
                over(cv, layer, sx, sy)
            small = crop(shrink_t(cv, (64 - 45, 64 - 45, 64 + 45, 64 + 45)), 0, 0, 32, 32)
            op = [small.d[i * 4 + 3] > 0.5 for i in range(32 * 32)]
            for i in range(32 * 32):
                y, x = divmod(i, 32)
                nb = (op[((y - 1) % 32) * 32 + x] + op[((y + 1) % 32) * 32 + x] + op[y * 32 + (x - 1) % 32] +
                      op[y * 32 + (x + 1) % 32])
                if op[i] and nb <= 1:
                    small.d[i * 4:i * 4 + 4] = [0.0, 0.0, 0.0, 0.0]
            self.hands.append(small)

    def npc_list(self):
        ctx = self.ctx
        md = self.md
        self.npcs = []
        for stage in STAGES:
            gal = GALAXY_OF.get(stage)
            if not gal:
                continue
            g = galaxy(ctx, gal)
            objs = g["objects"]
            pl = [(o, (-o.world_pos[0] / 256.0, o.world_pos[2] / 256.0, o.world_pos[1] / 256.0))
                  for o in objs]
            for kind, pos, scen in self.stage_objects(stage, gal, g):
                cands = []
                for o, gp in pl:
                    if o.name not in KINDS[kind]:
                        continue
                    if max(abs(gp[k] - pos[k]) for k in range(3)) > 0.02:
                        continue
                    if stage == "observatory":
                        ok = 1 in o.scenarios
                    else:
                        ok = any(scen >> (s - 1) & 1 for s in o.scenarios)
                    if ok:
                        cands.append(o)
                cands.sort(key=lambda o: KINDS[kind].index(o.name))
                if not cands:
                    continue
                o = cands[0]
                ctrl, dist, height = CTRL[o.name]
                mid = o.row.get("MessageId", -1)
                key = "%s_%s%03d" % (o.zone, ctrl, mid) if mid is not None and mid >= 0 else None
                if o.name == "TicoFat":
                    key = "Common_TicoFat001"
                if key and md.get(key) is None:
                    key = None
                self.npcs.append(dict(stage=stage, kind=kind, pos=[int(round(v * 256)) for v in pos], key=key,
                                      cls=o.name, dist=dist, height=height, front=o.name in FRONT_ONLY))

    def stage_objects(self, stage, gal, g):
        objs = g["objects"]
        npcs = [o for o in objs if o.name in KIND_OF and o.scenarios]
        if stage == "observatory":
            picked = [objs[i] for i in self.ctx.selection("observatory_npcs", [o.index for o in npcs])]
        else:
            late = LATE.get(gal, ())
            picked = sorted(npcs, key=lambda o: o.name in late)
        add6 = gal in ("EggStarGalaxy", "HoneyBeeKingdomGalaxy")
        zm = {p[1]: p[2] for p in g["placements"]}
        out = []
        for o in picked:
            m = mtx_tr(o.local_pos, o.local_rot)
            if o.zone_placement >= 0:
                m = mtx_mul(zm[o.zone_placement], m)
            gp = (-m[0][3] / 256.0, m[2][3] / 256.0, m[1][3] / 256.0)
            pos = [float(("%.4f" % v).replace("-0.0000", "0.0000")) for v in gp]
            scen = 1 if stage == "observatory" else scenario_mask(o.scenarios, add6)
            out.append((KIND_OF[o.name], pos, scen))
        return out

    def flows(self):
        md = self.md
        nodes = []
        node_of = {}
        self.msg_ids, self.msg_index = [], {}

        def use(name):
            if name not in self.msg_index:
                self.msg_index[name] = len(self.msg_ids)
                self.msg_ids.append(name)
            return self.msg_index[name]

        def compile_node(n):
            while n is not None and n.type == 2 and n.index in FOLD:
                n = md.branch_node(n.next_group if FOLD[n.index] else n.next_group + 1)
            if n is None:
                return NODE_END
            if n.i in node_of:
                return node_of[n.i]
            k = len(nodes)
            node_of[n.i] = k
            nodes.append(None)
            if n.type == 1:
                m = md.messages[n.index]
                if m.talk_type == 4:
                    nodes[k] = (NODE_EVENT, 255, 0, compile_node(md.next_node(n)), 0)
                else:
                    nx = compile_node(md.next_node(n))
                    nodes[k] = (NODE_MSG, m.talk_type, use(m.name), nx, 0)
            elif n.type == 2:
                t = compile_node(md.branch_node(n.next_group))
                f = compile_node(md.branch_node(n.next_group + 1))
                nodes[k] = (NODE_BRANCH, n.index, n.next, t, f)
            elif n.type == 3:
                nodes[k] = (NODE_EVENT, n.group, n.next, compile_node(md.next_node(n)), 0)
            return k

        self.tico_delight = [use("Common_Tico%03d" % k) for k in range(1, 6)]
        self.tico_fat_repeat = len(nodes)
        nodes.append((NODE_MSG, 0, use("Common_TicoFat002"), self.tico_fat_repeat + 1, 0))
        nodes.append((NODE_EVENT, 0, 0, self.tico_fat_repeat, 0))
        first = len(nodes)
        nodes.append((NODE_MSG, 2, use("Common_TicoFat001"), self.tico_fat_repeat, 0))
        self.tico_fat_full = len(nodes)
        nodes.append((NODE_MSG, 2, use("Common_TicoFat008"), self.tico_fat_full + 1, 0))
        nodes.append((NODE_EVENT, 0, 1, NODE_END, 0))
        for npc in self.npcs:
            if npc["cls"] == "TicoFat":
                npc["root"] = first
                continue
            if not npc["key"]:
                npc["root"] = NODE_END
                continue
            fn = md.find_node(npc["key"])
            if fn is None:
                m = md.get(npc["key"])
                if m.is_empty():
                    npc["root"] = NODE_END
                    continue
                k = len(nodes)
                nodes.append((NODE_MSG, m.talk_type, use(m.name), NODE_END, 0))
                npc["root"] = k
            else:
                npc["root"] = compile_node(fn)
        self.nodes = nodes

    def make_glyph(self, ch, size, picture):
        key = (ch, size, picture)
        if key in self.glyph_of:
            return self.glyph_of[key]
        mf, pf = self.mfont, self.pfont
        if picture:
            font = pf
            scx = scy = FS_Y / pf.fh() * S * size
        else:
            font = mf
            scx, scy = FS_X / mf.fw() * S * size, FS_Y / mf.fh() * S * size
        gi = font.char_index(ch)
        left, gw, _ = font.width_of(gi)
        g = font.glyph(gi)
        if g is None or gw <= 0:
            self.glyph_of[key] = None
            return None
        s, gx0, gy0 = g
        sheet = font.sheet(s)
        h_ = min(font.cell_height, font.sheet_height - gy0)
        w_ = min(gw, font.cell_width, font.sheet_width - gx0)
        tw, th = max(1, int(math.ceil(w_ * scx))), max(1, int(math.ceil(h_ * scy)))
        n = w_ * h_
        pm = (ctypes.c_float * (4 * n))()
        for y in range(h_):
            for x in range(w_):
                o = ((gy0 + y) * font.sheet_width + gx0 + x) * 4
                i = y * w_ + x
                if picture:
                    a = sheet[o + 3] / 255.0
                    for k in range(3):
                        pm[k * n + i] = (sheet[o + k] / 255.0) * a
                else:
                    a = sheet[o + 3] / 255.0
                    for k in range(3):
                        pm[k * n + i] = 1.0 * a
                pm[3 * n + i] = a
        bw, bh = max(1, round(w_ * scx * 8)), max(1, round(h_ * scy * 8))
        out = [list(supersample(ctypes.byref(pm, 4 * k * n), w_, h_, bw, bh, tw, th)) for k in range(4)]
        a = out[3]
        oy = 0
        if picture:
            oy = int(round((mf.ascent * FS_Y / mf.fh() - pf.ascent * FS_Y / (pf.height or 32)) * S * size))
            pix = []
            for i in range(tw * th):
                av = a[i]
                den = av if av > f32(1e-4) else f32(1e-4)
                for k in range(3):
                    v = f32(out[k][i] / den) if av > f32(1e-4) else 0.0
                    pix.append(min(max(v, 0.0), 1.0))
                pix.append(min(max(av, 0.0), 1.0))
            kind = 1
            on = [pix[i * 4 + 3] >= 0.5 for i in range(tw * th)]
        else:
            pix = []
            for i in range(tw * th):
                av = min(max(a[i], 0.0), 1.0)
                p = f32(f32(powf(av, 0.7)) * 3.0)
                pix.append(min(max(int(round(p)), 0), 3))
            kind = 0
            on = [v > 0 for v in pix]
        xs = [i % tw for i in range(tw * th) if on[i]]
        ys = [i // tw for i in range(tw * th) if on[i]]
        if not xs:
            self.glyph_of[key] = None
            return None
        x0, y0, x1, y1 = min(xs), min(ys), max(xs) + 1, max(ys) + 1
        if kind == 1:
            img = crop(FImg(tw, th, pix), x0, y0, x1, y1)
        else:
            img = FImg(x1 - x0, y1 - y0, [pix[y * tw + x] for y in range(y0, y1) for x in range(x0, x1)])
        idx = len(self.glyphs)
        self.glyphs.append(dict(kind=kind, pix=img, ox=int(x0 + round(left * scx)), oy=int(y0 + oy)))
        self.glyph_of[key] = idx
        return idx

    def advance(self, ch, size, picture):
        if picture:
            sc = FS_Y / self.pfont.fh() * size
            return advance_of(self.pfont, ch) * sc + 2.0
        return advance_of(self.mfont, ch) * FS_X / self.mfont.fw() * size + 2.0

    def tokens(self, m, number=None):
        out = []
        size, colour = 1.0, 0
        for p in m.parts:
            if isinstance(p, str):
                for c in p:
                    if c == "\n":
                        out.append(("nl",))
                    elif ord(c) >= 0x20:
                        out.append(("ch", c, size, colour, False))
                continue
            g, t = p.group, p.tag
            if g == 1 and t == 0:
                out.append(("wait", p.u16(0)))
            elif g == 1 and t == 1:
                out.append(("page",))
            elif g == 3:
                pic = 0x12 if t == 0x2B else t
                out.append(("ch", chr(0x30 + pic), size, colour, True))
            elif g == 4:
                size = FONT_SIZES.get(t, 1.0)
            elif g == 5:
                for c in self.player:
                    out.append(("ch", c, size, colour, False))
            elif g == 255 and t == 0:
                colour = p.u8(0)
            elif g == 6 and number is not None:
                for c in str(number):
                    out.append(("ch", c, size, colour, False))
        clean = []
        for tk in out:
            if tk[0] == "nl" and clean and clean[-1][0] == "page":
                continue
            clean.append(tk)
        return clean

    def layout_pages(self, m, number=None):
        pages, lines, line = [], [], []
        x, wait = 0.0, 0
        for tk in self.tokens(m, number):
            if tk[0] == "ch":
                _, c, size, colour, pic = tk
                g = self.make_glyph(c, size, pic)
                line.append((g, x, colour, wait))
                wait = 0
                x += self.advance(c, size, pic)
            elif tk[0] == "wait":
                wait += tk[1]
            elif tk[0] == "nl":
                lines.append((line, x))
                line, x = [], 0.0
            elif tk[0] == "page":
                lines.append((line, x))
                pages.append(lines)
                lines, line, x = [], [], 0.0
        lines.append((line, x))
        pages.append(lines)
        out = []
        for pg in pages:
            while pg and not pg[-1][0]:
                pg = pg[:-1]
            if pg:
                out.append(pg)
        return out

    def text(self):
        md = self.md
        self.player = message_text(md.get("System_PlayerName000"))
        self.glyphs, self.glyph_of = [], {}
        self.talk_chars, self.talk_pages, self.talk_messages = [], [], []
        self.number_variants = []
        jobs = [(name, None) for name in self.msg_ids]
        for name in list(self.msg_ids):
            m = md.get(name)
            numbered = any(isinstance(p, Tag) and p.group == 6 for p in m.parts)
            if name.startswith("Common_TicoFat") and numbered:
                base = self.msg_index[name]
                first = len(jobs)
                self.number_variants.append((base, first, 100))
                jobs += [(name, n) for n in range(101)]
        for name, number in jobs:
            m = md.get(name)
            short = m.talk_type == 1 or bool(re.fullmatch(r"Common_Tico\d{3}", name))
            line_h = LINE_H_SHORT if short else LINE_H_EVENT
            if number is None and name.startswith("Common_TicoFat"):
                number = 100
            pages = self.layout_pages(m, number)
            if not pages:
                pages = [[([], 0.0)]]
            first_page = len(self.talk_pages)
            max_w = max(max(w for _, w in pg) for pg in pages) - 2.0
            max_lines = max(len(pg) for pg in pages)
            if short:
                bg, text_left, text_cy, anchor = self.stretch_balloon(max_w, max_lines > 1)
            else:
                bg, text_left, text_cy, anchor = self.ev_img, self.ev_text_x, self.ev_text_cy, None
            for pg in pages:
                first = len(self.talk_chars)
                top = text_cy - len(pg) * line_h / 2
                for li, (line, _) in enumerate(pg):
                    for g, xl, colour, wait in line:
                        if g is None:
                            continue
                        gl = self.glyphs[g]
                        gx_ = int(round(text_left + xl * S)) + gl["ox"]
                        gy_ = int(round(top + li * line_h)) + gl["oy"]
                        self.talk_chars.append((g, gx_, gy_, colour, min(wait, 255)))
                self.talk_pages.append((first, len(self.talk_chars) - first))
            self.talk_messages.append(dict(name=name, first_page=first_page, pages=len(pages),
                                           talk_type=m.talk_type, balloon=m.balloon_type, read=m.already_read,
                                           bg=bg, short=short, anchor=anchor))

    def output(self):
        tag_used = sorted({c[3] for c in self.talk_chars if 0 < c[3] < 6})
        fixed = [WHITE, TEXT_GREY, tuple(g * 2 / 3 + w / 3 for g, w in zip(TEXT_GREY, WHITE)),
                 tuple(g / 3 + w * 2 / 3 for g, w in zip(TEXT_GREY, WHITE))]
        fixed += [tuple(a * b for a, b in zip(TAG_COLOURS[k], TEXT_GREY)) for k in tag_used]
        talk_pal = Palette(fixed)
        text_aa = [0, 4, 3, 2]
        tag_index = {0: 2}
        for k, t in enumerate(tag_used):
            tag_index[t] = 5 + k
        pic_imgs = [g["pix"] for g in self.glyphs if g["kind"] == 1]
        art_imgs = [self.ev_img] + [b[0] for b in self.ev_beaks + self.st_beaks] + \
                   [self.prompt[0], self.abtn[0][0], self.abtn[1][0]] + [t["bg"] for t in self.talk_messages]
        talk_pal.fill(pic_imgs, len(talk_pal.cols) + 6)
        talk_pal.fill(art_imgs)
        ptr_pal = Palette([])
        ptr_pal.fill([self.pointer_star] + self.hands)
        images = []
        cache = {}

        def add_image(name, img, pal):
            k = (id(img), id(pal))
            if k not in cache:
                cache[k] = pal.index(img)
            idx = cache[k]
            cols, rows = (img.w + 7) // 8, (img.h + 7) // 8
            data = tiles_of(idx, img.w, img.h, 0, 0, cols * 8, rows * 8)
            images.append((name, cols, rows, data, img.w, img.h))
            return len(images) - 1

        img_event = add_image("EVENT_BALLOON", self.ev_img, talk_pal)
        for t in self.talk_messages:
            t["bg_id"] = img_event
            if t["short"]:
                t["bg_id"] = add_image("SHORT_" + t["name"].upper(), t["bg"], talk_pal)
        for i, b in enumerate(self.ev_beaks):
            add_image("EVENT_BEAK%d" % i, b[0], talk_pal)
        add_image("SHORT_BEAK", self.st_beaks[0][0], talk_pal)
        add_image("PROMPT", self.prompt[0], talk_pal)
        for i, a in enumerate(self.abtn):
            add_image("ABUTTON%d" % i, a[0], talk_pal)
        add_image("POINTER", self.pointer_star, ptr_pal)
        for i, h in enumerate(self.hands):
            add_image("HAND%d" % i, h, ptr_pal)
        glyph_rows = []
        gdata = bytearray()
        for g in self.glyphs:
            pix = g["pix"]
            vals = talk_pal.index(pix) if g["kind"] == 1 else pix.d
            glyph_rows.append((pix.w, pix.h, g["ox"], g["oy"], g["kind"], len(gdata)))
            gdata += bytes(int(v) for v in vals)
        h = ["",
             "typedef struct { uint8_t w, h; int8_t ox, oy; uint8_t kind, pad; uint16_t pad2; "
             "uint32_t data; } TalkGlyph;",
             "typedef struct { uint16_t glyph; uint8_t x, y, colour, wait; } TalkChar;",
             "typedef struct { uint32_t first; uint16_t count, pad; } TalkPage;",
             "typedef struct { uint16_t first_page; uint8_t pages, talk_type, balloon, image; "
             "int8_t read_id; uint8_t pad;",
             "                 int8_t anchor_x, anchor_y; uint16_t pad2; } TalkMessage;",
             "typedef struct { uint8_t type, a; uint16_t b, next, alt; } TalkNode;",
             "typedef struct { uint8_t stage, kind, front_only, pad; int32_t pos[3]; "
             "uint16_t root, distance, height, pad2; } TalkNpc;",
             "typedef struct { uint8_t cols, rows, w, h; uint32_t data; } TalkImage;", ""]


        h.append("")
        h.append("static const uint16_t talk_palette[16] = { %s };" % ", ".join("0x%04X" % v
                                                                              for v in talk_pal.words()))
        h.append("static const uint16_t pointer_palette[16] = { %s };" % ", ".join("0x%04X" % v
                                                                                 for v in ptr_pal.words()))
        h.append("static const uint8_t talk_text_aa[4] = { %s };" % ", ".join(str(v) for v in text_aa))
        h.append("")
        h.append("enum {")
        for im in images:
            h.append("    TALK_IMG_%s," % re.sub(r"[^A-Z0-9_]", "_", im[0]))
        h.append("    TALK_IMG_COUNT")
        h.append("};")
        imgdata = bytearray()
        img_rows = []
        for im in images:
            img_rows.append((im[1], im[2], im[4], im[5], len(imgdata)))
            imgdata += im[3]
        h += c_rows("TalkImage talk_images", "{ %d, %d, %d, %d, %d }", img_rows)
        h.append("static const uint8_t talk_image_data[%d] __attribute__((aligned(4))) = {" % len(imgdata))
        for k in range(0, len(imgdata), 32):
            h.append("    " + ",".join("0x%02X" % b for b in imgdata[k:k + 32]) + ",")
        h.append("};")
        h.append("")
        h.append("")
        h += c_rows("TalkGlyph talk_glyphs", "{ %d, %d, %d, %d, %d, 0, 0, %d }", glyph_rows)
        h.append("static const uint8_t talk_glyph_data[%d] = {" % len(gdata))
        for k in range(0, len(gdata), 40):
            h.append("    " + ",".join(str(b) for b in gdata[k:k + 40]) + ",")
        h.append("};")
        h.append("")
        chars = [(g, x, y, tag_index.get(colour, 2), wait) for g, x, y, colour, wait in self.talk_chars]
        h += c_rows("TalkChar talk_chars", "{ %d, %d, %d, %d, %d }", chars)
        h += c_rows("TalkPage talk_pages", "{ %d, %d, 0 }", self.talk_pages)
        h.append("static const TalkMessage talk_messages[] = {")
        for t in self.talk_messages:
            ax_, ay_ = t["anchor"] if t["anchor"] else (0, 0)
            h.append("    { %d, %d, %d, %d, %d, %d, 0, %d, %d, 0 }," % (
                t["first_page"], t["pages"], t["talk_type"], t["balloon"], t["bg_id"], t["read"], round(ax_),
                round(ay_)))
        h.append("};")
        h.append("")
        h.append("enum { TALK_NODE_MSG = 1, TALK_NODE_BRANCH = 2, TALK_NODE_EVENT = 3 };")
        h.append("#define TALK_NODE_END 0xFFFF")
        h.append("#define TALK_TICOFAT_REPEAT %d" % self.tico_fat_repeat)
        h.append("#define TALK_TICOFAT_FULL %d" % self.tico_fat_full)
        h.append("typedef struct { uint16_t base, first, max; } TalkNumberVariants;")
        h += c_rows("TalkNumberVariants talk_number_variants", "{ %d, %d, %d }", self.number_variants)
        h.append("#define TALK_NUMBER_VARIANT_COUNT %d" % len(self.number_variants))
        h += c_rows("TalkNode talk_nodes", "{ %d, %d, %d, %d, %d }", self.nodes)
        h.append("static const TalkNpc talk_npcs[] = {")
        for n in self.npcs:
            h.append("    { STAGE_%s, OBJ_%s, %d, 0, { %d, %d, %d }, %d, %d, %d, 0 }," % (
                n["stage"].upper(), n["kind"].upper(), int(n["front"]), n["pos"][0], n["pos"][1], n["pos"][2],
                n["root"], n["dist"], n["height"]))
        h.append("};")
        h.append("#define TALK_NPC_COUNT %d" % len(self.npcs))
        h.append("#define TALK_MSG_TICO_DELIGHT %d" % self.tico_delight[0])
        h.append("#define TALK_MESSAGE_COUNT %d" % len(self.talk_messages))
        h.append("")
        h.append("")
        h.append("#define TALK_EVENT_X %d" % self.ev_x)
        h.append("#define TALK_EVENT_Y %d" % self.ev_y)
        h.append("#define TALK_EVENT_ABTN_X %d" % round(self.ev_abutton[0]))
        h.append("#define TALK_EVENT_ABTN_Y %d" % round(self.ev_abutton[1]))
        h.append("#define TALK_EVENT_BEAK_Y %d" % round(self.ev_img.h - 3))
        h.append("static const int8_t talk_beak_pivot[4][2] = { %s };" % ", ".join(
            "{ %d, %d }" % (round(b[1]), round(b[2])) for b in self.ev_beaks + self.st_beaks))
        h.append("#define TALK_PROMPT_CX %d" % round(self.prompt[1]))
        h.append("#define TALK_PROMPT_CY %d" % round(self.prompt[2]))
        h.append("static const int8_t talk_abtn_centre[2][2] = { %s };" % ", ".join(
            "{ %d, %d }" % (round(a[1]), round(a[2])) for a in self.abtn))
        return h


def talk(ctx):
    return Talk(ctx).run()
