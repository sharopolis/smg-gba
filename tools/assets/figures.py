import re
import struct
from collections import namedtuple

from assembly import align4, pool_section, put_u32, replace_once, s32, u32

FIGURE_FORMAT = struct.Struct("<4H7I2H6I")
FigureHeader = namedtuple("FigureHeader", (
    "point_count", "triangle_count", "texture_count", "keyframe_count", "points", "keyframes", "triangles",
    "axes", "textures", "clip_count", "clips", "lod_triangle_count", "lod_texture_count", "lod_triangles",
    "lod_axes", "lod_textures", "lod_point_count", "lod_points", "extent"))
FIGURE_POINTERS = {"points": 8, "keyframes": 12, "triangles": 16, "axes": 20, "textures": 24, "clips": 32,
                   "lod_triangles": 40, "lod_axes": 44, "lod_textures": 48, "lod_points": 56}
FIGURE_BYTES = 64
TEXTURE_BYTES = 2048
TRIANGLE_BYTES = 22
PACKED_KEYS = 0x8000
TICO_FIGURES = ["figure_tico"] + [f"figure_tico{i}" for i in range(1, 6)]


def figure_header(data, at=0):
    return FigureHeader(*FIGURE_FORMAT.unpack_from(data, at))


def key_count(header):
    return header.keyframe_count & 0x7FFF


def descriptor_at(data, origin, header, key):
    return origin + (u32(data, origin + header.keyframes + 4 * key) & ~1)


def decode_pose(data, at, point_count):
    mode, relative = struct.unpack_from("<Hi", data, at + 2)
    if mode == 0:
        end = at + 8 + point_count * 6
        return data[at + 8:end], align4(end), mode, None
    anchor = at + relative
    bind = struct.unpack_from(f"<{point_count * 3}h", data, anchor)
    values = []
    cursor = at + 8
    for first in range(0, point_count, 32):
        low = struct.unpack_from("<3h", data, cursor)
        packed = struct.unpack_from("<H", data, cursor + 6)[0]
        scale = 2 + (mode >> 1) if packed & 0x8000 else 0
        widths = [packed >> (5 * axis) & 31 for axis in range(3)]
        points = min(32, point_count - first)
        size = (points * sum(widths) + 7) // 8
        bits = int.from_bytes(data[cursor + 8:cursor + 8 + size], "little")
        for i in range(points):
            for axis, width in enumerate(widths):
                value = bind[(first + i) * 3 + axis] + low[axis] + ((bits & ((1 << width) - 1)) << scale)
                bits >>= width
                values.append((value + 32768) % 65536 - 32768)
        cursor += 8 + size
    return struct.pack(f"<{len(values)}h", *values), align4(cursor), mode, anchor


def all_poses(data, header):
    return [decode_pose(data, descriptor_at(data, 0, header, k), header.point_count)[0]
            for k in range(key_count(header))]


def array(data, header, field, size):
    at = getattr(header, field)
    return data[at:at + size]


def tico_shared_arrays(header):
    return [("points", header.point_count * 6), ("axes", header.triangle_count * 6),
            ("clips", header.clip_count * 8), ("lod_axes", header.lod_triangle_count * 6),
            ("lod_points", header.lod_point_count * 2)]


def tico_shape(data, header):
    counts = [header.point_count, header.triangle_count, header.keyframe_count, header.clip_count,
              header.lod_triangle_count, header.lod_point_count, header.extent]
    arrays = [array(data, header, field, size) for field, size in tico_shared_arrays(header)]
    vertices = [data[at + i * TRIANGLE_BYTES + v * 6:at + i * TRIANGLE_BYTES + v * 6 + 2]
                for at, count in ((header.triangles, header.triangle_count),
                                  (header.lod_triangles, header.lod_triangle_count))
                for i in range(count) for v in range(3)]
    return counts, arrays, vertices, all_poses(data, header)


def share_tico_bank(assets):
    sources = [assets.blob(name + ".bin") for name in TICO_FIGURES]
    headers = [figure_header(data) for data in sources]
    base, base_header = sources[0], headers[0]
    for name, data, header in zip(TICO_FIGURES[1:], sources[1:], headers[1:]):
        aliased = (header.lod_textures, header.lod_texture_count) == (header.textures, header.texture_count)
        if not aliased or tico_shape(data, header) != tico_shape(base, base_header):
            raise ValueError(f"{name} does not share its shape and poses with figure_tico")

    keys = key_count(base_header)
    shift = (len(sources) - 1) * FIGURE_BYTES
    bank = bytearray(len(sources) * FIGURE_BYTES) + base[FIGURE_BYTES:]
    for variant, (data, header) in enumerate(zip(sources, headers)):
        origin = variant * FIGURE_BYTES
        fields = {}
        for field in FIGURE_POINTERS:
            value = getattr(base_header, field)
            fields[field] = value + shift - origin if value else 0
        if variant:
            own = (("triangles", header.triangle_count * TRIANGLE_BYTES),
                   ("textures", header.texture_count * TEXTURE_BYTES),
                   ("lod_triangles", header.lod_triangle_count * TRIANGLE_BYTES))
            for field, size in own:
                bank += bytes(-len(bank) % 4)
                fields[field] = len(bank) - origin
                bank += array(data, header, field, size)
            fields["lod_textures"] = fields["textures"]
            bank += bytes(-len(bank) % 4)
            fields["keyframes"] = len(bank) - origin
            bank += bytes(keys * 4)
        for k in range(keys):
            descriptor = u32(base, base_header.keyframes + 4 * k) & ~1
            put_u32(bank, origin + fields["keyframes"] + 4 * k, (descriptor + shift - origin) | 1)
        FIGURE_FORMAT.pack_into(bank, origin, *header._replace(**fields))
    assets.set_blob("tico_shared_bank.bin", bank)

    for i, name in enumerate(TICO_FIGURES):
        if i:
            new = f"    .global {name}\n    .set {name}, figure_tico + {i * FIGURE_BYTES}"
        else:
            new = '    .global figure_tico\n    .balign 4\nfigure_tico:\n    .incbin "tico_shared_bank.bin"'
        old = f'    .global {name}\n    .balign 4\n{name}:\n    .incbin "{name}.bin"'
        assets.text = replace_once(assets.text, old + "\n", new + "\n")


def share_pose_descriptors(assets, pairs):
    pool_text = '\n    .section .rodata.assets, "a", %progbits\n'
    for group, (target, donor) in enumerate(pairs):
        blobs = {name: assets.blob(name + ".bin") for name in (target, donor)}
        headers = {name: figure_header(data) for name, data in blobs.items()}
        if headers[target].point_count != headers[donor].point_count:
            raise ValueError(f"{target} and {donor} have different point counts")
        donor_poses = {}
        for k in range(key_count(headers[donor])):
            at = descriptor_at(blobs[donor], 0, headers[donor], k)
            pose, end = decode_pose(blobs[donor], at, headers[donor].point_count)[:2]
            donor_poses.setdefault(pose, (at, end))
        common = set(all_poses(blobs[target], headers[target])) & donor_poses.keys()
        pooled = {pose: f"pose_shared_{group}_{i}" for i, pose in enumerate(sorted(common))}
        if not pooled:
            raise ValueError(f"{target} shares no poses with {donor}")

        for name in (target, donor):
            data = bytearray(blobs[name])
            header = headers[name]
            cuts = {}
            slots = {}
            for k in range(key_count(header)):
                at = descriptor_at(data, 0, header, k)
                pose, end = decode_pose(data, at, header.point_count)[:2]
                if pose in pooled:
                    cuts[at] = end - at
                    slots[header.keyframes + 4 * k] = pooled[pose]
                    put_u32(data, header.keyframes + 4 * k, 0)
            out, _, moved = remove_figure_arrays(name, data, list(cuts.items()), {})
            words = {moved(slot): f"{label} - {name} + 1" for slot, label in slots.items()}
            assets.replace_blob(name, out, words)
        bind = figure_header(assets.blob(donor + ".bin")).points
        for pose, label in pooled.items():
            at, end = donor_poses[pose]
            assets.set_blob(label + ".bin", blobs[donor][at:end])
            pool_text += (f'    .balign 4\n{label}:\n    .incbin "{label}.bin", 0, 4\n'
                          f'    .word {donor} + {bind} - {label}\n    .incbin "{label}.bin", 8\n')
    assets.text += pool_text


def cut_ranges(data, cuts):
    out = bytearray()
    cursor = 0
    for at, size in sorted(cuts):
        out += data[cursor:at]
        cursor = at + size
    return out + data[cursor:]


def static_textures(name, data):
    header = figure_header(data)
    at = header.textures
    records = [data[at + i * TEXTURE_BYTES:at + (i + 1) * TEXTURE_BYTES] for i in range(header.texture_count)]
    if header.keyframe_count or header.clip_count:
        raise ValueError(f"{name} is not a static figure")
    if header.lod_triangle_count and (header.lod_texture_count, header.lod_textures) != (
            header.texture_count, header.textures):
        raise ValueError(f"{name} has a separate LOD texture bank")
    if len(set(records)) != len(records):
        raise ValueError(f"{name} has duplicate textures")
    return header, records


def remap_triangle_textures(data, header, mapping):
    out = bytearray(data)
    triangles = ((header.triangles, header.triangle_count), (header.lod_triangles, header.lod_triangle_count))
    for at, count in triangles:
        for i in range(count):
            slot = at + i * TRIANGLE_BYTES + 18
            struct.pack_into("<H", out, slot, mapping[struct.unpack_from("<H", out, slot)[0]])
    return out


def share_texture_overlaps(assets, pairs):
    for number, names in enumerate(pairs):
        label = f"figure_texture_buoy_overlap_bank_{number}"
        blobs = [assets.blob(name + ".bin") for name in names]
        (first_header, first), (second_header, second) = [static_textures(n, b) for n, b in zip(names, blobs)]
        common = [record for record in first if record in second]
        records = [r for r in first if r not in common] + common + [r for r in second if r not in common]
        starts = (0, len(first) - len(common))
        for name, data, header, textures, start in zip(names, blobs, (first_header, second_header),
                                                       (first, second), starts):
            view = records[start:start + len(textures)]
            out = remap_triangle_textures(data, header, [view.index(record) for record in textures])
            size = len(textures) * TEXTURE_BYTES
            out = out[:header.textures] + out[header.textures + size:]
            for field, offset in FIGURE_POINTERS.items():
                value = u32(out, offset)
                if field not in ("textures", "lod_textures") and value >= header.textures + size:
                    put_u32(out, offset, value - size)
            pointer = f"{label} + {start * TEXTURE_BYTES} - {name}"
            words = {24: pointer, 48: pointer} if header.lod_triangle_count else {24: pointer}
            assets.replace_blob(name, out, words)
        assets.set_blob(label + ".bin", b"".join(records))
        assets.text += pool_section(label, ".rodata.assets.shared_textures")


def remove_figure_arrays(name, source, cuts, expressions):
    cuts = sorted(cuts)

    def moved(at):
        return at - sum(size for start, size in cuts if start + size <= at)

    out = cut_ranges(source, cuts)
    header = figure_header(source)
    patches = {}

    def patch(offset, value):
        if isinstance(value, str):
            patches[moved(offset)] = value
            value = 0
        put_u32(out, moved(offset), value)

    for field, offset in FIGURE_POINTERS.items():
        if offset in expressions:
            patch(offset, f"{expressions[offset]} - {name}")
        elif getattr(header, field):
            patch(offset, moved(getattr(header, field)))
    if header.keyframe_count & PACKED_KEYS:
        descriptors = set()
        for k in range(key_count(header)):
            slot = header.keyframes + 4 * k
            pointer = u32(source, slot) & ~1
            if pointer:
                patch(slot, moved(pointer) | 1)
                descriptors.add(pointer)
        for at in descriptors:
            target = at + s32(source, at + 4)
            if 8 in expressions and target == header.points:
                patch(at + 4, f"{expressions[8]} - ({name} + {moved(at)})")
            else:
                patch(at + 4, moved(target) - moved(at))
    return out, patches, moved


def split_incbin(statement, cuts, patches, moved):
    def original(offset):
        for at, size in sorted(cuts):
            if at <= offset:
                offset += size
        return offset

    start, end = statement.start, statement.start + statement.size
    edges = {start, end}
    for at, size in cuts:
        edges.update(edge for edge in (at, at + size) if start < edge < end)
    for offset in patches:
        if start <= original(offset) < end:
            edges.update((original(offset), original(offset) + 4))
    lines = []
    edges = sorted(edges)
    for first, last in zip(edges, edges[1:]):
        if any(at <= first < at + size for at, size in cuts):
            continue
        if moved(first) in patches:
            lines.append(f"    .word {patches[moved(first)]}")
        elif last > first:
            lines.append(f'    .incbin "{statement.name}", {moved(first)}, {last - first}')
    return lines


WHOLE_ARRAY_FIELDS = {"bind": ("points", "point_count"), "axes": ("axes", "triangle_count")}


def share_whole_arrays(assets, groups, contained):
    assembly = assets.assemble()
    plans = {}
    pools = []
    for number, (kind, names) in enumerate(groups):
        label = f"figure_whole_array_shared_{number}"
        field, count = WHOLE_ARRAY_FIELDS[kind]
        arrays = set()
        for name in names:
            data = assets.blob(name + ".bin")
            header = figure_header(data)
            at, size = getattr(header, field), getattr(header, count) * 6
            arrays.add(data[at:at + size])
            cuts, expressions = plans.setdefault(name, ([], {}))
            cuts.append((at, size))
            expressions[FIGURE_POINTERS[field]] = label
        if len(arrays) != 1:
            raise ValueError(f"{kind} arrays differ in {', '.join(names)}")
        pools.append((label, arrays.pop()))

    for name, donor in contained:
        data = assets.blob(name + ".bin")
        header = figure_header(data)
        origin = assembly.symbols[donor]
        donor_header = figure_header(assembly.data, origin)
        size = header.texture_count * TEXTURE_BYTES
        textures = data[header.textures:header.textures + size]
        bank_at = origin + donor_header.textures
        bank = assembly.data[bank_at:bank_at + donor_header.texture_count * TEXTURE_BYTES]
        starts = range(0, len(bank) - size + 1, TEXTURE_BYTES)
        matches = [at for at in starts if bank[at:at + size] == textures]
        if not matches:
            raise ValueError(f"{donor} does not contain the textures of {name}")
        word = assembly.words.get(origin + FIGURE_POINTERS["textures"])
        base = f"({word}) + {donor}" if word else f"{donor} + {donor_header.textures}"
        expression = f"{base} + {matches[0]}"
        cuts, expressions = plans.setdefault(name, ([], {}))
        cuts.append((header.textures, size))
        expressions[FIGURE_POINTERS["textures"]] = expression
        if header.lod_triangle_count:
            expressions[FIGURE_POINTERS["lod_textures"]] = expression

    spliced = {}
    shared_binds = {}
    for name, (cuts, expressions) in plans.items():
        source = assets.blob(name + ".bin")
        if 8 in expressions:
            shared_binds[rf"\b{name}\s*\+\s*{figure_header(source).points}\b"] = expressions[8]
        out, patches, moved = remove_figure_arrays(name, source, cuts, expressions)
        spliced[name + ".bin"] = (cuts, patches, moved)
        assets.set_blob(name + ".bin", out)

    lines = []
    for statement in assembly.statements:
        if statement.kind == "incbin" and statement.name in spliced:
            lines += split_incbin(statement, *spliced[statement.name])
        elif statement.kind == "word":
            expression = statement.expression
            for pattern, bind in shared_binds.items():
                expression = re.sub(pattern, bind, expression)
            lines.append(f"    .word {expression}")
        else:
            lines.append(statement.text)
    assets.text = "\n".join(lines) + "\n"
    for label, data in pools:
        assets.set_blob(label + ".bin", data)
        assets.text += pool_section(label)
