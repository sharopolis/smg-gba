import json
import os
import sys

import textures_bake
import textures_gx
import textures_ops
import textures_png
import textures_sky
import textures_tev
import textures_ui
from common import Dump, IntReader, parse_args, read_pak, write_out


class Sources:
    def __init__(self, dump, table, orig, out_root, bake_specs):
        self.dump = dump
        self.table = table
        self.orig = orig
        self.out_root = out_root
        self.bin_dir = os.path.join(out_root, ".bin")
        self.bake_specs = bake_specs
        self.blobs = {}
        self.levels = {}
        self.models = {}
        self.produced = {}

    def blob(self, arc, inner):
        key = (arc, inner)
        if key not in self.blobs:
            self.blobs[key] = self.dump.load(arc + ":" + inner)
        return self.blobs[key]

    def model(self, sid):
        arc, inner, _, index, _ = self.table[sid]
        key = (arc, inner)
        if key not in self.models:
            self.models[key] = textures_tev.Model(self.blob(arc, inner))
        return self.models[key], index

    def texture(self, sid):
        arc, inner, kind, index, level = self.table[sid]
        key = (arc, inner, kind, index)
        if key not in self.levels:
            if kind == "tev":
                model, index = self.model(sid)
                self.levels[key] = [textures_tev.effective_rgba(model, index)]
            elif kind == "j3d":
                data = self.blob(arc, inner)
                self.levels[key] = textures_gx.bti_levels(data, textures_gx.j3d_texture_headers(data)[index])
            else:
                self.levels[key] = textures_gx.bti_levels(self.blob(arc, inner), 0)
        return self.levels[key][level]


def clamp8(v):
    if v < 0:
        return 0
    if v > 255:
        return 255
    return v


def predict_box(src, recipe, w, h):
    sw, sh, px = src.texture(recipe["src"])
    if recipe.get("dither"):
        px = bytearray(px)
        for y in range(sh):
            yb = sh - 1 - y
            for x in range(sw):
                px[(y * sw + x) * 4 + 3] = 255 if (x + 2 * yb) % 4 == 0 else 0
    out = textures_ops.box_save(px, sw, sh, w, h, alpha_mode=recipe.get("am", 0))
    cut = recipe.get("lumcut")
    if cut is not None:
        for i in range(0, len(out), 4):
            lum = (299 * out[i] + 587 * out[i + 1] + 114 * out[i + 2]) // 1000
            out[i + 3] = 255 if lum >= cut else 0
    return out


def predict_bake(src, recipe):
    spec = src.bake_specs[recipe["spec"]]
    obj_text = ""
    if "obj" in spec:
        path = os.path.join(src.out_root, "assets", spec["obj"])
        if not os.path.isfile(path):
            sys.exit("tex: " + path + " missing (build the figure models first)")
        with open(path) as f:
            obj_text = f.read()
    base = None
    if "base" in spec:
        base = src.produced.get("assets/" + spec["base"])
        if base is None:
            sys.exit("tex: " + spec["base"] + " must be built before " + recipe["spec"])
    if "palx" in recipe:
        raw = textures_bake.predict(src.orig, spec, obj_text, src.bin_dir, base)
        cols = sorted(set(tuple(raw[i:i + 4]) for i in range(0, len(raw), 4)))
        px = recipe["palx"]
        pal = []
        for k in range(0, len(px), 5):
            c = cols[px[k]]
            pal += [c[0] + px[k + 1], c[1] + px[k + 2], c[2] + px[k + 3], c[3] + px[k + 4]]
        spec = dict(spec)
        spec["pal"] = pal
    return bytearray(textures_bake.predict(src.orig, spec, obj_text, src.bin_dir, base))


def predict_vlerp(src, recipe, w, h):
    sw, sh, px = src.texture(recipe["src"])
    model, mi = src.model(recipe["mat"])
    mat = model.materials[mi]
    ras = model.vertex_color_mean(mi)[:3]
    c0 = mat.regs[0][:3]
    k0 = mat.konst[0][:3]
    f32 = textures_ops.f32
    bf = textures_ops.BYTE_F
    out = bytearray(len(px))
    for i in range(0, len(px), 4):
        lum = f32(f32(f32(f32(bf[px[i]]) + bf[px[i + 1]]) + bf[px[i + 2]]) / 3)
        inv = f32(1 - lum)
        for c in range(3):
            v = ras[c] + inv * c0[c] + lum * k0[c]
            v = 0.0 if v < 0.0 else 1.0 if v > 1.0 else v
            out[i + c] = textures_ops.to_byte(f32(v))
        out[i + 3] = 255
    return textures_ops.box_save(out, sw, sh, w, h)


def predict_ramp(src, recipe, w, h):
    model, mi = src.model(recipe["mat"])
    hue = textures_tev.vertex_hue(model, mi)
    row = bytearray()
    for k in range(w):
        sv = (k + 0.5) / w
        col = textures_tev.env_colour(model, mi, [hue[0] * sv, hue[1] * sv, hue[2] * sv, 1.0])
        row += bytes(textures_ops.to_byte(textures_ops.f32(v)) for v in col[:3]) + b"\xff"
    return bytearray(bytes(row) * h)


def predict(src, recipe, w, h):
    op = recipe["op"]
    if op == "id":
        return bytearray(src.texture(recipe["src"])[2])
    if op == "box":
        return predict_box(src, recipe, w, h)
    if op == "sky":
        return bytearray(textures_sky.predict_sky(src.orig, recipe["stage"], src.bin_dir))
    if op == "bake":
        return predict_bake(src, recipe)
    if op == "vlerp":
        return predict_vlerp(src, recipe, w, h)
    if op == "ramp":
        return predict_ramp(src, recipe, w, h)
    if op == "envflat":
        col = textures_tev.env_flat(*src.model(recipe["mat"]))
        return bytearray(bytes(col + [255]) * (w * h))
    if op == "ffflat":
        col = textures_tev.flat_mean(*src.model(recipe["mat"]))
        return bytearray(bytes(col + [255]) * (w * h))
    raise ValueError("unknown op " + op)


def apply_residual(px, res, channels):
    rd = IntReader(res)
    for p in range(len(px) // 4):
        for c in range(channels):
            d = rd.int()
            if d:
                px[p * 4 + c] = (px[p * 4 + c] + d) & 255
    return px


def to_channels(px, ctype):
    if ctype == 6:
        return bytes(px)
    out = bytearray(len(px) // 4 * 3)
    out[0::3] = px[0::4]
    out[1::3] = px[1::4]
    out[2::3] = px[2::4]
    return bytes(out)


def needs_base(src, img):
    pred = img["pred"]
    return pred["op"] == "bake" and "base" in src.bake_specs[pred["spec"]]


def build_images(src, pak, meta, out_root):
    chunks = [(k.encode(), bytes.fromhex(v)) for k, v in meta["blender_chunks"]]
    written = 0
    colours = set()
    images = meta["images"]
    order = [n for n in range(len(images)) if not needs_base(src, images[n])]
    order += [n for n in range(len(images)) if needs_base(src, images[n])]
    for n in order:
        img = images[n]
        w = img["w"]
        h = img["h"]
        ctype = img["ct"]
        px = predict(src, img["pred"], w, h)
        res = pak.get("r/%d" % n)
        if res:
            px = apply_residual(px, res, 4 if ctype == 6 else 3)
        if ctype != 6:
            px[3::4] = b"\xff" * (len(px) // 4)
        data = to_channels(px, ctype)
        if not img.get("np"):
            for i in range(0, len(px), 4):
                if px[i + 3] >= 128:
                    colours.add(bytes(px[i:i + 3]))
        for f in img["files"]:
            src.produced[f["path"]] = (w, h, bytes(px))
            if f["style"] == "blender":
                pre = chunks + [(b"tEXt", t.encode("latin-1")) for t in f.get("text", [])]
                blob = textures_png.blender_png(data, w, h, ctype, pre)
            else:
                blob = textures_png.pil_png(data, w, h, ctype)
            write_out(out_root, f["path"], blob)
            written += 1
    return written, sorted(colours)


def build_palette(colours, pal_data, out_root):
    rd = IntReader(pal_data)
    out = bytearray(4)
    idx = 0
    for _ in range(255):
        idx += rd.int()
        base = colours[idx]
        out += bytes(clamp8(base[c] + rd.int()) for c in range(3)) + b"\xff"
    write_out(out_root, "assets/palette.rgba", bytes(out))
    return bytes(out)


def main():
    args = parse_args()
    data_dir = os.path.join(args.data, "textures")
    pak = read_pak(os.path.join(data_dir, "textures.pak"))
    meta = json.loads(pak["meta.json"])
    textures_bake.DATA_DIR = data_dir
    src = Sources(Dump(args.orig), meta["sources"], args.orig, args.out, json.loads(pak["bake_specs.json"]))
    n, colours = build_images(src, pak, meta, args.out)
    palette = build_palette(colours, pak["palette"], args.out)
    textures_ui.build_ui(args.orig, os.path.join(data_dir, "ui"), args.out, palette)
    print("textures: %d png files" % n)


if __name__ == "__main__":
    main()
