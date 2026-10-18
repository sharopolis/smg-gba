import struct

from common import Dump, jhash, u16, u32, yaz0_decompress


class Row(dict):
    def get(self, name, default=None):
        return dict.get(self, jhash(name), default)

    def __getitem__(self, name):
        if isinstance(name, int):
            return dict.__getitem__(self, name)
        return dict.__getitem__(self, jhash(name))


def bcsv_value(data, p, mask, shift, typ, stab):
    if typ in (0, 3):
        v = (u32(data, p) & mask) >> shift
        if mask == 0xFFFFFFFF and shift == 0 and v >= 0x80000000:
            v -= 0x100000000
    elif typ == 4:
        v = (u16(data, p) & mask) >> shift
        if mask == 0xFFFF and shift == 0 and v >= 0x8000:
            v -= 0x10000
    elif typ == 5:
        v = (data[p] & mask) >> shift
        if mask == 0xFF and shift == 0 and v >= 0x80:
            v -= 0x100
    elif typ == 2:
        v = round(struct.unpack_from(">f", data, p)[0], 5)
    elif typ == 1:
        v = data[p:p + 32].split(b"\0", 1)[0].decode("shift_jis", "replace")
    elif typ == 6:
        s = stab + u32(data, p)
        v = data[s:data.find(b"\0", s)].decode("shift_jis", "replace")
    else:
        v = None
    return v


def bcsv(data):
    data = yaz0_decompress(data)
    n, nf, doff, esize = struct.unpack_from(">iiiI", data, 0)
    fields = [struct.unpack_from(">IIHBB", data, 16 + i * 12) for i in range(nf)]
    stab = doff + n * esize
    rows = []
    for i in range(n):
        base = doff + i * esize
        row = Row()
        for h, mask, off, shift, typ in fields:
            row[h] = bcsv_value(data, base + off, mask, shift, typ, stab)
        rows.append(row)
    return rows


def arc_get(arc, path):
    want = path.replace("\\", "/").strip("/").lower()
    for p, blob in arc.files:
        low = p.lower()
        if low == want or low.split("/", 1)[-1] == want:
            return yaz0_decompress(blob)
    return None


class Tag:
    def __init__(self, group, tag, params):
        self.group, self.tag, self.params = group, tag, params

    def u8(self, i):
        return self.params[i] if i < len(self.params) else 0

    def u16(self, i):
        return u16(self.params, i * 2) if len(self.params) >= i * 2 + 2 else 0


def decode_parts(blob, off):
    out, p, n, run = [], off, len(blob), []
    while p + 1 < n:
        c = u16(blob, p)
        if c == 0:
            break
        if c == 0x1A:
            size = blob[p + 2]
            if run:
                out.append("".join(run))
                run = []
            out.append(Tag(blob[p + 3], u16(blob, p + 4), bytes(blob[p + 6:p + size])))
            p += max(size, 6)
            continue
        run.append(chr(c))
        p += 2
    if run:
        out.append("".join(run))
    return out


def decode_flat(blob, off):
    out, p, n = [], off, len(blob)
    while p + 1 < n:
        c = u16(blob, p)
        if c == 0:
            break
        if c == 0x1A:
            ln = blob[p + 2]
            out.append("{tag:%s}" % blob[p + 3:p + ln].hex())
            p += max(ln, 4)
            continue
        out.append(chr(c))
        p += 2
    return "".join(out)


class Message:
    def __init__(self, name, parts, talk_type, balloon_type, already_read):
        self.name, self.parts = name, parts
        self.talk_type, self.balloon_type, self.already_read = talk_type, balloon_type, already_read

    def is_empty(self):
        return not any(isinstance(p, str) and p.strip() for p in self.parts)


class FlowNode:
    def __init__(self, i, raw):
        self.i = i
        self.type, self.group, self.index, self.next, self.next_group = struct.unpack(">BBHHH", raw)


class MessageData:
    def __init__(self, arc):
        data = arc_get(arc, "message.bmg")
        ids = bcsv(arc_get(arc, "messageid.tbl"))
        p, sec = 0x20, {}
        for _ in range(u32(data, 12)):
            if p + 8 > len(data):
                break
            magic, size = struct.unpack_from(">4sI", data, p)
            sec[magic] = (p, size)
            p += size
        ip = sec[b"INF1"][0]
        count, esize = struct.unpack_from(">HH", data, ip + 8)
        dp, dsize = sec[b"DAT1"]
        dblob = data[dp + 8:dp + dsize]
        names = {r["Index"]: r["MessageId"] for r in ids}
        self.index_of = {r["MessageId"]: r["Index"] for r in ids}
        self.flat = {}
        for r in ids:
            idx = r["Index"]
            if idx is not None and 0 <= idx < count:
                self.flat[r["MessageId"]] = decode_flat(dblob, u32(data, ip + 16 + idx * esize))
        self.messages = []
        for i in range(count):
            e = ip + 16 + i * esize
            read = struct.unpack_from(">b", data, e + 11)[0]
            self.messages.append(Message(names.get(i, ""), decode_parts(dblob, u32(data, e)), data[e + 8],
                                         data[e + 9], read))
        fp = sec[b"FLW1"][0]
        ncount, bcount = struct.unpack_from(">HH", data, fp + 8)
        base = fp + 16
        self.nodes = [FlowNode(i, data[base + i * 8:base + i * 8 + 8]) for i in range(ncount)]
        tb = base + ncount * 8
        self.branch = [u16(data, tb + i * 2) for i in range(bcount)]

    def get(self, name):
        i = self.index_of.get(name)
        return None if i is None else self.messages[i]

    def find_node(self, name):
        i = self.index_of.get(name)
        for n in self.nodes:
            if n.type == 1 and n.index == i:
                return n
        return None

    def branch_node(self, slot):
        if slot >= len(self.branch) or self.branch[slot] == 0xFFFF:
            return None
        return self.nodes[self.branch[slot]]

    def next_node(self, n):
        if n.type == 1:
            return None if n.next == 0xFFFF else self.nodes[n.next]
        if n.type == 3:
            return self.branch_node(n.index)
        return None


class Dol:
    def __init__(self, data):
        self.d = data
        offs = struct.unpack_from(">18I", data, 0)
        addrs = struct.unpack_from(">18I", data, 0x48)
        sizes = struct.unpack_from(">18I", data, 0x90)
        self.secs = [(offs[i], addrs[i], sizes[i]) for i in range(18) if sizes[i]]

    def off(self, a):
        for o, ad, sz in self.secs:
            if ad <= a < ad + sz:
                return o + a - ad
        return None

    def u32s(self, a, n):
        return struct.unpack_from(">%dI" % n, self.d, self.off(a))

    def cstr(self, a):
        o = self.off(a)
        if o is None:
            return None
        return self.d[o:self.d.index(b"\0", o)].decode("shift_jis")


class Context:
    def __init__(self, orig):
        self.dump = Dump(orig)
        self.cache = {}

    def arc(self, rel):
        return self.dump.arc(rel)

    def memo(self, key, fn):
        if key not in self.cache:
            self.cache[key] = fn()
        return self.cache[key]

    def messages(self):
        return self.memo("messages", lambda: MessageData(self.arc("UsEnglish/MessageData/Message.arc")))
