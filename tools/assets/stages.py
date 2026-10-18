import struct

from assembly import align4, emit_body, find_body, pool_section, put_u32, u16, u32

STAGE_POINTERS = {
    "points": 28, "triangles": 32, "textures": 36, "chunks": 40, "leaf_triangle_offsets": 44,
    "leaf_triangles": 48, "leaf_draw_ranks": 52, "rank_triangles": 56, "texture_water": 60,
    "texture_scroll": 64, "sky": 68, "gravities": 76, "collision_triangles": 84, "collision_nodes": 92,
    "objects": 100, "starts": 108, "rails": 116, "rail_points": 120, "rail_parts": 124, "areas": 132,
    "cameras": 140, "point_home": 176, "leaf_point_first": 180, "gravity_grid": 188, "area_grid": 192,
    "parts": 200}
GRAVITY_BYTES = 80
WIRE_GRAVITY = 8
SKY_BYTES = 128 * 256
METADATA = {"objects": (96, 84), "starts": (104, 32), "areas": (128, 72), "cameras": (136, 120),
            "rails": (112, 32), "rail_points": (None, 28), "rail_parts": (None, 88)}


def relocate(blob, threshold, delta):
    out = bytearray(blob)

    def move(offset):
        value = u32(out, offset)
        if value >= threshold:
            put_u32(out, offset, value + delta)

    for offset in STAGE_POINTERS.values():
        move(offset)
    for field in ("gravity_grid", "area_grid"):
        grid = u32(out, STAGE_POINTERS[field])
        if grid:
            move(grid + 20)
            move(grid + 24)
    gravities = u32(out, STAGE_POINTERS["gravities"])
    if gravities:
        for i in range(u32(out, 72)):
            entry = gravities + i * GRAVITY_BYTES
            if out[entry] == WIRE_GRAVITY:
                move(entry + 44)
    return out


def sky_arrays(blob):
    slot = STAGE_POINTERS["sky"]
    return {"": (slot, u32(blob, slot), SKY_BYTES)}


def metadata_arrays(blob):
    arrays = {}
    for kind, (count_field, stride) in METADATA.items():
        if count_field is not None:
            count = u32(blob, count_field)
        else:
            rails = u32(blob, STAGE_POINTERS["rails"])
            first = 0 if kind == "rail_points" else 4
            ranges = [struct.unpack_from("<2H", blob, rails + i * 32 + first) for i in range(u32(blob, 112))]
            count = max((sum(pair) for pair in ranges), default=0)
        if count:
            slot = STAGE_POINTERS[kind]
            arrays[kind] = (slot, u32(blob, slot), count * stride)
    return arrays


def grid_arrays(blob):
    arrays = {}
    count, gravities = u32(blob, 72), u32(blob, STAGE_POINTERS["gravities"])
    if count and all(blob[gravities + i * GRAVITY_BYTES] != WIRE_GRAVITY for i in range(count)):
        arrays["gravities"] = (STAGE_POINTERS["gravities"], gravities, count * GRAVITY_BYTES)
    for field, name in (("gravity_grid", "gravity"), ("area_grid", "area")):
        grid = u32(blob, STAGE_POINTERS[field])
        if not grid:
            continue
        cells, lists = struct.unpack_from("<2I", blob, grid + 20)
        outside, top = struct.unpack_from("<2H", blob, grid + 28)
        blocks = max(struct.unpack_from(f"<{top}H", blob, cells)) + 1
        cell_words = top + (blocks << (3 * blob[grid + 19]))
        indices = set(struct.unpack_from(f"<{cell_words - top}H", blob, cells + top * 2)) | {outside}
        list_words = max(index + 1 + u16(blob, lists + index * 2) for index in indices)
        arrays[name + "_cells"] = (grid + 20, cells, cell_words * 2)
        arrays[name + "_lists"] = (grid + 24, lists, list_words * 2)
    return dict(sorted(arrays.items()))


def share_stage_arrays(assets, names, prefix, find_arrays):
    blobs = {name: assets.blob(name + ".bin") for name in names}
    found = {name: find_arrays(blob) for name, blob in blobs.items()}
    cuts = {name: [] for name in names}
    slots = {name: {} for name in names}
    labels = []
    for kind in found[names[0]]:
        rows = [found[name].get(kind) for name in names]
        if None in rows:
            continue
        arrays = {blobs[name][at:at + size] for name, (_, at, size) in zip(names, rows)}
        if len(arrays) != 1:
            continue
        label = prefix + kind
        assets.set_blob(label + ".bin", arrays.pop())
        labels.append(label)
        for name, (slot, at, size) in zip(names, rows):
            cuts[name].append((at, align4(size)))
            slots[name][slot] = f"{label} - {name}"

    for name in names:
        out = bytearray(blobs[name])
        for slot in slots[name]:
            put_u32(out, slot, 0)
        for at, size in sorted(cuts[name], reverse=True):
            out = relocate(out[:at] + out[at + size:], at + size, -size)
        start, end, words = find_body(assets.text, name)
        words = {slot - sum(size for at, size in cuts[name] if at + size <= slot): expression
                 for slot, expression in (words | slots[name]).items()}
        assets.text = assets.text[:start] + emit_body(name, len(out), words) + assets.text[end:]
        assets.set_blob(name + ".bin", out)
    for label in labels:
        assets.text += pool_section(label)
