import hashlib
import os
import re
import struct
import sys

import ui_headers_anim
import ui_headers_cosmic
import ui_headers_debug
import ui_headers_talk
import ui_lyt
from common import IntReader, parse_args, read_pak, write_out
from ui_art import UI, hide
from ui_data import Context
from ui_dome import dome_header
from ui_hud import hud_header
from ui_ss import ss_header

NUM = re.compile(r"(?<![\w.])(0x[0-9A-Fa-f]+|-?\d+)(?![\w.])")
LIFEUP_GROUPS = [("COUNT", 0), ("RECOVERY", 7), ("POWER", 14), ("COUNTER", 200), ("BREAK", 213),
                 ("RETURN", 573),
                 ("DAMAGE", 606), ("RECOVER", 687)]
LIFEUP_FRAMES = 720
LIFEUP_RECORD = 130


class Ctx(Context):
    def __init__(self, orig, out, pak):
        Context.__init__(self, orig)
        self.orig = orig
        self.out = out
        self.sel = {}
        self.blobs = {}
        for name, data in pak.items():
            if name.startswith("sel:"):
                r = IntReader(data)
                self.sel[name[4:]] = [r.uvarint() for _ in range(r.uvarint())]
            elif name.startswith("blob:"):
                self.blobs[name[5:]] = data

    def read_sys(self, name):
        with open(os.path.join(os.path.dirname(os.path.abspath(self.orig)), "sys", name), "rb") as f:
            return f.read()

    def read_out(self, rel):
        path = os.path.join(self.out, rel)
        if not os.path.exists(path):
            sys.exit("error: %s not found, build the textures first" % rel)
        with open(path, "rb") as f:
            return f.read()

    def selection(self, key, cands):
        sel = self.sel.get(key)
        if sel is None:
            return cands
        return [cands[i] for i in sel]


def lifeup_index(ctx):
    ui = UI(ctx)
    anims = ui.bundle("HitPointMeter")["anims"]
    names = sorted(anims)
    r = IntReader(ctx.blobs["lifeup_timeline"])
    seen, cache, idx = {}, {}, []
    for _ in range(LIFEUP_FRAMES):
        hp = r.uvarint()
        states = tuple((names[r.uvarint()], r.uvarint()) for _ in range(r.uvarint()))
        key = (hp, states)
        if key not in cache:
            lay = ui.layout("HitPointMeter")
            ui_lyt.set_text_recursive(lay, "HitPointNumber", str(hp))
            hide(lay, "TxtTitle", "ShaTitle", "Title")
            c = ui.render("HitPointMeter", lay, [(anims[a], f) for a, f in states])
            cache[key] = hashlib.sha256(bytes(c.d)).hexdigest()
        h = cache[key]
        if h not in seen:
            seen[h] = len(seen)
        idx.append(seen[h])
    return idx


def lifeup_meter(ctx):
    idx = ctx.memo("lifeup_index", lambda: lifeup_index(ctx))
    h = ["", "#define HUD_LIFEUP_TILE 72", "#define HUD_LIFEUP_TILES 64",
         "#define HUD_LIFEUP_FRAMES %d" % LIFEUP_FRAMES]
    for name, base in LIFEUP_GROUPS:
        h.append("#define LU_%s_BASE %d" % (name, base))
    h.append("static const uint16_t lifeup_meter_index[HUD_LIFEUP_FRAMES]={")
    for i in range(0, LIFEUP_FRAMES, 24):
        h.append("    " + ",".join(str(v) for v in idx[i:i + 24]) + ",")
    h.append("};")
    return h


def lifeup_pool(ctx):
    images = max(ctx.memo("lifeup_index", lambda: lifeup_index(ctx))) + 1
    tile_offset = (images * LIFEUP_RECORD + 3) & ~3
    pool = ctx.read_out("assets/lifeup/lifeup_meter_pool.bin")
    refs = struct.unpack_from("<%dH" % (images * 65), pool)
    tiles = max(refs[i] for i in range(len(refs)) if i % 65) + 1
    return ["", "#define HUD_LIFEUP_POOL_RECORD_BYTES %d" % LIFEUP_RECORD,
            "#define HUD_LIFEUP_POOL_TILE_OFFSET %d" % tile_offset,
            "#define HUD_LIFEUP_POOL_PALETTE_OFFSET %d" % (tile_offset + tiles * 32)]


TARGETS = [
    ("src/game/scenario_intro_gen.h", ui_headers_anim.scenario_intro),
    ("src/game/astro_dome_gen.h", ui_headers_anim.astro_dome),
    ("src/game/powerstar_anim_gen.h", ui_headers_anim.powerstar),
    ("src/game/cosmic_replay_gen.h", ui_headers_cosmic.cosmic),
    ("src/ui/debug_points_gen.h", ui_headers_debug.debug_points),
    ("src/ui/debug_menu_art_gen.h", ui_headers_debug.debug_menu_art),
    ("src/ui/astro_dome_ui_gen.h", dome_header),
    ("src/ui/scenario_select_gen.h", ss_header),
    ("src/ui/hud_tiles_gen.h", hud_header),
    ("src/ui/talk_gen.h", ui_headers_talk.talk),
    ("src/ui/lifeup_meter_gen.h", lifeup_meter),
    ("src/ui/hud_lifeup_pool_gen.h", lifeup_pool),
]


def clean(lines):
    out = []
    for line in "\n".join(lines).split("\n"):
        line = line.rstrip()
        if line == "" and (not out or out[-1] == ""):
            continue
        out.append(line)
    while out and out[-1] == "":
        out.pop()
    return "\n".join(out) + "\n"


def split_line(line):
    parts = NUM.split(line)
    return parts[0::2], parts[1::2]


def num_value(tok):
    return int(tok, 16) if tok.startswith("0x") else int(tok)


def num_text(pred_tok, value):
    if pred_tok.startswith("0x"):
        digits = len(pred_tok) - 2
        if pred_tok[2:] == pred_tok[2:].upper():
            return "0x%0*X" % (digits, value)
        return "0x%0*x" % (digits, value)
    return str(value)


def apply_ops(r, pred, out):
    pairs = []
    pos = 0
    for _ in range(r.uvarint()):
        op = r.uvarint()
        n = r.uvarint()
        if op == 0:
            for k in range(n):
                pairs.append(len(out))
                out.append(pred[pos + k])
            pos += n
        elif op == 1:
            pos += n
        else:
            for _ in range(n):
                ln = r.uvarint()
                out.append(bytes(r.data[r.pos:r.pos + ln]).decode("utf-8"))
                r.pos += ln
    for k in range(pos, len(pred)):
        pairs.append(len(out))
        out.append(pred[k])
    return pairs


def apply(pred_text, corr):
    r = IntReader(corr)
    out = []
    pairs = apply_ops(r, pred_text.split("\n"), out)
    nres = r.uvarint()
    if nres:
        targets = []
        for li in pairs:
            for j in range(len(split_line(out[li])[1])):
                targets.append((li, j))
        edits = {}
        idx = -1
        for _ in range(nres):
            idx += r.uvarint() + 1
            edits.setdefault(targets[idx][0], {})[targets[idx][1]] = r.int()
        for li, ch in edits.items():
            texts, nums = split_line(out[li])
            for j, d in ch.items():
                nums[j] = num_text(nums[j], num_value(nums[j]) + d)
            s = texts[0]
            for j in range(len(nums)):
                s += nums[j] + texts[j + 1]
            out[li] = s
    return "\n".join(out)


def main():
    args = parse_args()
    ui_lyt.open_lib(os.path.join(args.out, ".bin"))
    corr = read_pak(os.path.join(args.data, "ui_headers", "ui_headers.pak"))
    ctx = Ctx(args.orig, args.out, corr)
    for rel, gen in TARGETS:
        write_out(args.out, rel, apply(clean(gen(ctx)), corr[rel]).encode("utf-8"))
    print("ui_headers: %d headers" % len(TARGETS))


if __name__ == "__main__":
    main()
