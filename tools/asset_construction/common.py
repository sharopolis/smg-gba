import argparse
import ctypes
import hashlib
import json
import lzma
import os
import struct
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))


def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig", required=True)
    ap.add_argument("--data", required=True)
    ap.add_argument("--out", required=True)
    return ap.parse_args()


def build_c(name, bin_dir, shared=True):
    src = os.path.join(HERE, name + ".c")
    out = os.path.join(bin_dir, name + (".so" if shared else ""))
    if os.path.exists(out) and os.path.getmtime(out) >= os.path.getmtime(src):
        return out
    os.makedirs(bin_dir, exist_ok=True)
    cmd = os.environ.get("HOSTCC", "cc").split() + ["-std=c11", "-O2", "-ffp-contract=off"]
    if shared:
        cmd += ["-fPIC", "-shared"]
    tmp = "%s.%d" % (out, os.getpid())
    subprocess.run(cmd + ["-o", tmp, src, "-lm"], check=True)
    os.replace(tmp, out)
    return out


def load_lib(name, bin_dir):
    return ctypes.CDLL(build_c(name, bin_dir))


def u16(b, o):
    return struct.unpack_from(">H", b, o)[0]


def s16(b, o):
    return struct.unpack_from(">h", b, o)[0]


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def s32(b, o):
    return struct.unpack_from(">i", b, o)[0]


def f32_at(b, o):
    return struct.unpack_from(">f", b, o)[0]


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def cstr(b, o):
    return b[o:b.index(b"\0", o)].decode("shift_jis", "replace")


def jhash(s):
    if isinstance(s, str):
        s = s.encode("shift_jis", "replace")
    h = 0
    for c in s:
        if c >= 0x80:
            c -= 0x100
        h = (h * 31 + c) & 0xFFFFFFFF
    return h


def name_table(b, o):
    return [cstr(b, o + u16(b, o + 6 + i * 4)) for i in range(u16(b, o))]


def yaz0_decompress(src):
    if src[:4] != b"Yaz0":
        return bytes(src)
    size = struct.unpack_from(">I", src, 4)[0]
    dst = bytearray(size)
    sp, dp = 16, 0
    n = len(src)
    while dp < size and sp < n:
        code = src[sp]
        sp += 1
        for bit in range(8):
            if dp >= size:
                break
            if code & (0x80 >> bit):
                dst[dp] = src[sp]
                dp += 1
                sp += 1
            else:
                b1 = src[sp]
                b2 = src[sp + 1]
                sp += 2
                dist = ((b1 & 0x0F) << 8 | b2) + 1
                cnt = b1 >> 4
                if cnt == 0:
                    cnt = src[sp] + 0x12
                    sp += 1
                else:
                    cnt += 2
                s = dp - dist
                if dist >= cnt:
                    dst[dp:dp + cnt] = dst[s:s + cnt]
                    dp += cnt
                else:
                    for _ in range(cnt):
                        dst[dp] = dst[s]
                        dp += 1
                        s += 1
    return bytes(dst)


class RARC:
    def __init__(self, data):
        data = yaz0_decompress(data)
        if data[:4] != b"RARC":
            raise ValueError("not a RARC archive")
        self.data = data
        _, data_off = struct.unpack_from(">II", data, 8)
        data_base = data_off + 0x20
        info = 0x20
        num_nodes, node_off, _, entry_off, _, str_off = struct.unpack_from(">IIIIII", data, info)
        node_off += info
        entry_off += info
        str_off += info

        def name_at(o):
            e = data.index(b"\0", str_off + o)
            return data[str_off + o:e].decode("shift_jis", "replace")

        nodes = []
        for i in range(num_nodes):
            _, name_ofs, _, count, first = struct.unpack_from(">IIHHI", data, node_off + i * 16)
            nodes.append((name_at(name_ofs), count, first))
        self.files = []

        def walk(index, prefix):
            name, count, first = nodes[index]
            path = prefix + name + "/"
            for e in range(first, first + count):
                _, _, flags_name, ofs, size = struct.unpack_from(">HHIII", data, entry_off + e * 20)
                flags = flags_name >> 24
                ename = name_at(flags_name & 0xFFFFFF)
                if ename in (".", ".."):
                    continue
                if flags & 0x02:
                    walk(ofs, path)
                else:
                    self.files.append((path + ename, data[data_base + ofs:data_base + ofs + size]))

        walk(0, "")

    def find(self, name):
        low = name.lower()
        for path, blob in self.files:
            if path.lower() == low or path.lower().endswith("/" + low):
                return yaz0_decompress(blob)
        raise KeyError(name)


class Dump:
    def __init__(self, root):
        self.root = root
        self._arcs = {}

    def path(self, rel):
        return os.path.join(self.root, rel)

    def read(self, rel):
        with open(self.path(rel), "rb") as f:
            return f.read()

    def arc(self, rel):
        a = self._arcs.get(rel)
        if a is None:
            a = self._arcs[rel] = RARC(self.read(rel))
        return a

    def load(self, spec):
        if ":" in spec:
            rel, inner = spec.split(":", 1)
            return self.arc(rel).find(inner)
        return yaz0_decompress(self.read(spec))


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def unzigzag(u):
    return (u >> 1) ^ -(u & 1)


class IntReader:
    def __init__(self, data, pos=0):
        self.data = data
        self.pos = pos

    def uvarint(self):
        data, pos = self.data, self.pos
        u = shift = 0
        while True:
            b = data[pos]
            pos += 1
            u |= (b & 0x7F) << shift
            if b < 0x80:
                break
            shift += 7
        self.pos = pos
        return u

    def int(self):
        return unzigzag(self.uvarint())

    def ints(self, n):
        return [self.int() for _ in range(n)]


def read_pak(path):
    with open(path, "rb") as f:
        raw = lzma.decompress(f.read())
    n = struct.unpack_from("<I", raw)[0]
    index = json.loads(raw[4:4 + n])
    base = 4 + n
    return {name: raw[base + o:base + o + size] for name, o, size in index}


def write_out(out_root, rel, data):
    path = os.path.join(out_root, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def fsum(values):
    it = iter(values)
    for first in it:
        break
    else:
        return 0
    total = 0 + first
    if not isinstance(total, float):
        for x in it:
            total = total + x
            if isinstance(total, float):
                break
        else:
            return total
    comp = 0.0
    for x in it:
        x = float(x)
        t = total + x
        if abs(total) >= abs(x):
            comp += (total - t) + x
        else:
            comp += (x - t) + total
        total = t
    if comp and comp - comp == 0.0:
        total += comp
    return total


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
