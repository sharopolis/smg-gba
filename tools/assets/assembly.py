import ast
import re
import struct
from pathlib import Path

INCBIN = re.compile(r'\.incbin "([^"]+)"(?:,\s*(\d+)(?:,\s*(\d+))?)?')
SECTION = re.compile(r"\.section\s+([^,\s]+)")


def u16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def s32(data, offset):
    return struct.unpack_from("<i", data, offset)[0]


def put_u32(data, offset, value):
    struct.pack_into("<I", data, offset, value & 0xFFFFFFFF)


def align4(n):
    return (n + 3) & ~3


def merge_ranges(ranges):
    merged = []
    for low, high in sorted(ranges):
        if merged and low <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(high, merged[-1][1]))
        else:
            merged.append((low, high))
    return merged


def inside(at, ranges):
    return any(low <= at < high for low, high in ranges)


def evaluate(expression, symbols):
    def value(node):
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return node.value
        if isinstance(node, ast.Name):
            return symbols[node.id]
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            return -value(node.operand)
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
            return value(node.left) + value(node.right)
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Sub):
            return value(node.left) - value(node.right)
        raise ValueError(f"unsupported expression: {expression}")

    return value(ast.parse(expression, mode="eval").body)


def pool_section(label, section=".rodata.assets"):
    return (f'\n    .section {section}, "a", %progbits\n    .global {label}\n    .balign 4\n'
            f'{label}:\n    .incbin "{label}.bin"\n')


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"expected one {old.strip()!r} in assets.s")
    return text.replace(old, new)


def emit_body(name, length, words):
    lines = []
    cursor = 0
    for at, expression in sorted(words.items()):
        if at > cursor:
            lines.append(f'    .incbin "{name}.bin", {cursor}, {at - cursor}\n')
        lines.append(f"    .word {expression}\n")
        cursor = at + 4
    if cursor < length:
        lines.append(f'    .incbin "{name}.bin", {cursor}\n')
    return "".join(lines)


def find_body(text, name):
    start = text.index(name + ":\n") + len(name) + 2
    end = start
    cursor = 0
    words = {}
    for line in text[start:].splitlines(keepends=True):
        incbin = INCBIN.fullmatch(line.strip())
        if incbin and incbin[1] == name + ".bin":
            cursor = int(incbin[2] or 0) + int(incbin[3] or 0)
        elif line.startswith("    .word "):
            words[cursor] = line[len("    .word "):].strip()
            cursor += 4
        else:
            break
        end += len(line)
    return start, end, words


class Statement:
    def __init__(self, kind, text, section=None, address=0, size=0, name=None, start=0, expression=None):
        self.kind = kind
        self.text = text
        self.section = section
        self.address = address
        self.size = size
        self.name = name
        self.start = start
        self.expression = expression


class Assembly:
    def __init__(self, text, read_blob):
        self.statements = []
        sections = {}
        alignments = {}
        labels = {}
        aliases = []
        section = None
        for line in text.splitlines():
            stripped = line.strip()
            if not stripped or stripped.startswith("@"):
                self.statements.append(Statement("other", line))
                continue
            if stripped.startswith(".section"):
                section = SECTION.match(stripped)[1]
                sections.setdefault(section, bytearray())
                alignments.setdefault(section, 1)
                self.statements.append(Statement("section", line, section, len(sections[section])))
                continue
            data = sections[section]
            statement = Statement("other", line, section, len(data))
            incbin = INCBIN.fullmatch(stripped)
            if stripped.startswith(".global"):
                pass
            elif stripped.endswith(":"):
                statement.kind = "label"
                statement.name = stripped[:-1]
                if statement.name in labels:
                    raise ValueError(f"duplicate label {statement.name}")
                labels[statement.name] = (section, len(data))
            elif stripped.startswith(".set"):
                statement.kind = "alias"
                name, expression = stripped[4:].split(",", 1)
                aliases.append((name.strip(), expression.strip()))
            elif stripped.startswith(".balign"):
                statement.kind = "align"
                alignment = int(stripped.split()[1])
                alignments[section] = max(alignment, alignments[section])
                data += bytes(-len(data) % alignment)
            elif incbin:
                blob = read_blob(incbin[1])
                start = int(incbin[2] or 0)
                end = start + int(incbin[3]) if incbin[3] else len(blob)
                statement.kind = "incbin"
                statement.name = incbin[1]
                statement.start = start
                statement.size = end - start
                data += blob[start:end]
            elif stripped.startswith(".word "):
                statement.kind = "word"
                statement.expression = stripped[len(".word "):].strip()
                statement.size = 4
                data += bytes(4)
            else:
                raise ValueError(f"unsupported assembly: {stripped}")
            self.statements.append(statement)

        bases = {}
        total = 0
        for name, data in sections.items():
            total += -total % alignments[name]
            bases[name] = total
            total += len(data)
        self.symbols = {name: bases[section] + offset for name, (section, offset) in labels.items()}
        for name, expression in aliases:
            self.symbols[name] = evaluate(expression, self.symbols)
        self.words = {}
        for statement in self.statements:
            if statement.section is not None:
                statement.address += bases[statement.section]
            if statement.kind == "word":
                self.words[statement.address] = statement.expression
                value = evaluate(statement.expression, self.symbols)
                put_u32(sections[statement.section], statement.address - bases[statement.section], value)
        self.data = bytearray(total)
        for name, data in sections.items():
            self.data[bases[name]:bases[name] + len(data)] = data
        self.data = bytes(self.data)

    def read(self, address, size):
        return self.data[address:address + size]


class Assets:
    def __init__(self, folder):
        self.folder = Path(folder)
        self.text = (self.folder / "assets.s").read_text()
        self.header = (self.folder / "assets_gen.h").read_text()
        self.blobs = {}
        self.changed = set()

    def blob(self, filename):
        if filename not in self.blobs:
            self.blobs[filename] = (self.folder / filename).read_bytes()
        return self.blobs[filename]

    def set_blob(self, filename, data):
        self.blobs[filename] = bytes(data)
        self.changed.add(filename)

    def replace_blob(self, name, data, words):
        self.text = replace_once(self.text, f'    .incbin "{name}.bin"\n', emit_body(name, len(data), words))
        self.set_blob(name + ".bin", data)

    def assemble(self):
        return Assembly(self.text, self.blob)

    def save(self):
        for filename in sorted(self.changed):
            (self.folder / filename).write_bytes(self.blobs[filename])
        (self.folder / "assets.s").write_text(self.text)
        (self.folder / "assets_gen.h").write_text(self.header)


def header_list(header, name):
    match = re.search(r"^#define " + name + r"\s+\{([^}]+)\}", header, re.M)
    return match[1].replace(" ", "").split(",")


def header_figures(header):
    return re.findall(r"^extern const uint8_t (figure_\w+)\[\];", header, re.M)
