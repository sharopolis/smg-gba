import struct
import subprocess
from collections import defaultdict, namedtuple

from assembly import align4, header_figures, inside, merge_ranges
from figures import FIGURE_BYTES, PACKED_KEYS, decode_pose, descriptor_at, figure_header, key_count

Key = namedtuple("Key", "pose span mode anchor")
Model = namedtuple("Model", "name origin header keys table bind bind_at clips")
Anchor = namedtuple("Anchor", "pose at charge kind")
Plan = namedtuple("Plan", "rows row_of anchors choice kept")
MAX_POINTS = 224
MAX_SAMPLES = 128
MAX_NEW_ANCHORS = 32


def read_models(assembly, names):
    models = []
    for name in names:
        origin = assembly.symbols[name]
        header = figure_header(assembly.read(origin, FIGURE_BYTES))
        keys = [None] * key_count(header)
        if header.keyframe_count & PACKED_KEYS:
            for k in range(len(keys)):
                at = descriptor_at(assembly.data, origin, header, k)
                pose, end, mode, anchor = decode_pose(assembly.data, at, header.point_count)
                keys[k] = Key(pose, (at, end), mode, anchor)
        table = origin + header.keyframes
        bind_at = origin + header.points
        models.append(Model(name, origin, header, keys, (table, table + len(keys) * 4),
                            assembly.read(bind_at, header.point_count * 6), bind_at,
                            assembly.read(origin + header.clips, header.clip_count * 8)))
    return models


def groups_sharing_data(models):
    parent = list(range(len(models)))

    def root(i):
        while parent[i] != i:
            i = parent[i]
        return i

    users = defaultdict(list)
    for i, model in enumerate(models):
        for key in model.keys:
            if key:
                users[key.span].append(i)
        users[model.table].append(i)
    for rows in users.values():
        for i in rows[1:]:
            parent[root(i)] = root(rows[0])
    groups = defaultdict(list)
    for i, model in enumerate(models):
        if model.keys:
            groups[root(i)].append(model)
    return list(groups.values())


def encode(pose, base, points, mode):
    values = struct.unpack(f"<{points * 3}h", pose)
    bases = struct.unpack(f"<{points * 3}h", base)
    out = bytearray()
    for first in range(0, points, 32):
        count = min(32, points - first)
        deltas = [[(values[(first + i) * 3 + a] - bases[(first + i) * 3 + a] + 32768) % 65536 - 32768
                   for a in range(3)] for i in range(count)]
        low = [min(d[a] for d in deltas) for a in range(3)]
        high = [max(d[a] for d in deltas) for a in range(3)]
        step = 2 + (mode >> 1)
        scale = step if (all((d[a] - low[a]) % (1 << step) == 0 for d in deltas for a in range(3))
                         and any(high[a] > low[a] for a in range(3))) else 0
        widths = [((high[a] - low[a]) >> scale).bit_length() for a in range(3)]
        flags = widths[0] | widths[1] << 5 | widths[2] << 10 | (0x8000 if scale else 0)
        out += struct.pack("<3hH", *low, flags)
        bits = 0
        position = 0
        for d in deltas:
            for a, width in enumerate(widths):
                bits |= ((d[a] - low[a]) >> scale) << position
                position += width
        out += bits.to_bytes((position + 7) // 8, "little")
    return bytes(out)


def block_widths(assembly, key, points):
    blocks = (points + 31) // 32
    if key.mode not in (1, 3):
        return [0] * blocks
    widths = []
    at = key.span[0] + 8
    for first in range(0, points, 32):
        packed = struct.unpack_from("<H", assembly.data, at + 6)[0] & 0x7FFF
        widths.append(packed)
        size = sum(packed >> (5 * axis) & 31 for axis in range(3))
        at += 8 + (min(32, points - first) * size + 7) // 8
    return widths


def samples(count, limit):
    if count == 1:
        return [0]
    taken = min(count, limit)
    return [(i * (count - 1)) // (taken - 1) for i in range(taken)]


def anchor_costs(helper, points, rows, anchors, assembly):
    data = struct.pack("<3I", points, len(rows), len(anchors))
    data += b"".join(row.pose for row in rows) + b"".join(anchor.pose for anchor in anchors)
    data += b"".join(struct.pack("<I", row.mode) for row in rows)
    widths = [width for row in rows for width in block_widths(assembly, row, points)]
    data += struct.pack(f"<{len(widths)}I", *widths)
    result = subprocess.run([str(helper)], input=data, capture_output=True, check=True).stdout
    costs = [cost for (cost,) in struct.iter_unpack("<I", result)]
    return [costs[i * len(rows):(i + 1) * len(rows)] for i in range(len(anchors))]


def plan_group(group, helper, assembly):
    points = group[0].header.point_count
    if points > MAX_POINTS or any(not model.header.keyframe_count & PACKED_KEYS for model in group):
        return None
    spans = {key.span: key for model in group for key in model.keys}
    rows = list(spans.values())
    tables = {model.table for model in group}
    old_bytes = sum(end - start for start, end in spans) + sum(end - start for start, end in tables)
    row_of = {span: i for i, span in enumerate(spans)}

    free = []
    seen = set()
    for model in group:
        if model.bind not in seen:
            seen.add(model.bind)
            free.append(Anchor(model.bind, model.bind_at, 0, "bind"))
    for row in rows:
        if row.mode == 0 and row.pose not in seen:
            seen.add(row.pose)
            free.append(Anchor(row.pose, row.span[0] + 8, row.span[1] - row.span[0], "raw descriptor"))

    picks = set()
    for model in group:
        for first, count, _, _, _ in struct.iter_unpack("<HHBBH", model.clips):
            if count:
                for k in (first, first + count - 1, first + count // 2):
                    picks.add(row_of[model.keys[k].span])
    picks.update(samples(len(rows), MAX_SAMPLES))
    picks = sorted(picks)
    if len(picks) > MAX_SAMPLES:
        picks = [picks[i] for i in samples(len(picks), MAX_SAMPLES)]
    pose_bytes = align4(points * 6)
    candidates = [Anchor(rows[i].pose, None, pose_bytes, "new") for i in picks if rows[i].pose not in seen]
    anchors = free + candidates
    costs = anchor_costs(helper, points, rows, anchors, assembly)

    best = [end - start for start, end in spans]
    choice = [-1] * len(rows)

    def take(anchor):
        for i, cost in enumerate(costs[anchor]):
            if cost < best[i]:
                best[i] = cost
                choice[i] = anchor

    def saving(row_costs):
        return sum(max(old - new, 0) for old, new in zip(best, row_costs))

    for anchor in range(len(free)):
        take(anchor)
    picked = set()
    for _ in range(MAX_NEW_ANCHORS):
        scores = [-1 if i in picked else saving(costs[i]) - pose_bytes
                  for i in range(len(free), len(anchors))]
        if not scores or max(scores) <= 0:
            break
        anchor = max(range(len(scores)), key=lambda i: (scores[i], -i)) + len(free)
        picked.add(anchor)
        take(anchor)
    used = {anchor for anchor in choice if anchor >= 0}
    charges = sum(anchors[i].charge for i in used if anchors[i].kind != "new") + len(picked) * pose_bytes
    total = sum(best) + charges + sum(end - start for start, end in tables)
    if total >= old_bytes:
        return None
    kept = picked | {i for i in used if anchors[i].kind == "raw descriptor"}
    return Plan(rows, row_of, anchors, choice, sorted(kept))


class PoolWriter:
    def __init__(self, prefix):
        self.prefix = prefix
        self.data = bytearray()
        self.lines = []

    def start(self, label, data):
        at = align4(len(self.data))
        self.data += bytes(at - len(self.data)) + data
        self.lines += ["    .balign 4", f"{label}:"]
        return at

    def add_pose(self, label, pose):
        at = self.start(label, pose)
        self.lines.append(f'    .incbin "{self.prefix}.bin", {at}, {len(pose)}')

    def add_descriptor(self, label, descriptor, base):
        at = self.start(label, descriptor)
        self.lines += [f'    .incbin "{self.prefix}.bin", {at}, 4', f"    .word {base} - {label}",
                       f'    .incbin "{self.prefix}.bin", {at + 8}, {len(descriptor) - 8}']

    def add_table(self, label, descriptors, figure):
        self.start(label, bytes(len(descriptors) * 4))
        self.lines += [f"    .word {descriptor} - {figure} + 1" for descriptor in descriptors]


def reanchor_poses(assets, helper):
    assembly = assets.assemble()
    models = read_models(assembly, header_figures(assets.header))
    labels_at = defaultdict(list)
    for name, at in assembly.symbols.items():
        labels_at[at].append(name)
    extra_labels = {}

    def label_at(at):
        if labels_at[at]:
            return sorted(labels_at[at])[0]
        return extra_labels.setdefault(at, f"pose_anchor_retained_{at}")

    cuts = []
    patches = {}
    replacements = {}
    pool_lines = []
    for number, group in enumerate(groups_sharing_data(models)):
        plan = plan_group(group, helper, assembly)
        if plan is None:
            continue
        points = group[0].header.point_count
        rows, anchors = plan.rows, plan.anchors
        prefix = f"pose_anchor_{number}"
        pool = PoolWriter(prefix)
        anchor_labels = {}
        for i in plan.kept:
            label = f"{prefix}_raw_{i}"
            if anchors[i].kind == "raw descriptor":
                descriptor = struct.pack("<HHi", points, 0, 0) + anchors[i].pose
                pool.add_descriptor(label, descriptor, label_at(group[0].bind_at))
                anchor_labels[i] = label + " + 8"
            else:
                pool.add_pose(label, anchors[i].pose)
                anchor_labels[i] = label

        mode = 3 if any(row.mode == 3 for row in rows) else 1
        descriptors = []
        for r, row in enumerate(rows):
            label = f"{prefix}_descriptor_{r}"
            i = plan.choice[r]
            if i >= 0:
                body = encode(row.pose, anchors[i].pose, points, mode)
                descriptor = struct.pack("<HHi", points, mode, 0) + body
                base = anchor_labels.get(i) or label_at(anchors[i].at)
            else:
                descriptor = assembly.read(row.span[0], row.span[1] - row.span[0])
                base = label_at(row.anchor if row.mode in (1, 3) else group[0].bind_at)
            pool.add_descriptor(label, descriptor + bytes(-len(descriptor) % 4), base)
            descriptors.append(label)
            cuts.append(row.span)
            replacements[row.span[0]] = label

        tables = {}
        for m, model in enumerate(group):
            if model.origin not in tables:
                label = f"{prefix}_table_{m}"
                tables[model.origin] = label
                pool.add_table(label, [descriptors[plan.row_of[key.span]] for key in model.keys], model.name)
            patches[model.origin + 12] = f"{tables[model.origin]} - {model.name}"
            cuts.append(model.table)
            replacements[model.table[0]] = tables[model.origin]
        assets.set_blob(prefix + ".bin", pool.data)
        pool_lines += ['    .section .rodata.assets, "a", %progbits'] + pool.lines

    removed = merge_ranges(cuts)

    def is_removed(at):
        return inside(at, removed)

    lines = []
    for statement in assembly.statements:
        at = statement.address
        if statement.kind == "label" and is_removed(at):
            if at in replacements:
                lines.append(f"    .set {statement.name}, {replacements[at]}")
        elif statement.kind in ("other", "section", "alias", "label"):
            lines.append(statement.text)
        elif statement.kind == "align":
            if not is_removed(at):
                lines.append(statement.text)
        elif statement.kind == "word":
            if is_removed(at):
                continue
            if at in extra_labels:
                lines.append(extra_labels[at] + ":")
            lines.append(f"    .word {patches.get(at, statement.expression)}")
        else:
            end = at + statement.size
            edges = {at, end}
            edges.update(edge for low, high in removed for edge in (low, high) if at < edge < end)
            edges.update(edge for patch in patches for edge in (patch, patch + 4) if at < edge < end)
            edges.update(label for label in extra_labels if at < label < end)
            edges = sorted(edges)
            for first, last in zip(edges, edges[1:]):
                if is_removed(first):
                    continue
                if first in extra_labels:
                    lines.append(extra_labels[first] + ":")
                if first in patches:
                    lines.append(f"    .word {patches[first]}")
                else:
                    offset = statement.start + first - at
                    lines.append(f'    .incbin "{statement.name}", {offset}, {last - first}')
    assets.text = "\n".join(lines + pool_lines) + "\n"
