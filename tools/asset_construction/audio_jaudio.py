import random
import struct

from common import cstr, u16, u32, yaz0_decompress

AFC_COEF = [(0, 0), (2048, 0), (0, 2048), (1024, 1024), (4096, -2048), (3584, -1536), (3072, -1024),
            (4608, -2560), (4200, -2248), (4800, -2300), (5120, -3072), (2048, -2048), (1024, -1024),
            (-1024, 1024), (-1024, 0), (-2048, 0)]


class Wave:
    __slots__ = ("aw", "wave_id", "fmt", "base_key", "rate", "offset", "size", "loop", "loop_start",
                 "loop_end", "nsamples")


class WaveSystem:
    def __init__(self, data, base):
        winf = base + u32(data, base + 0x10)
        wbct = base + u32(data, base + 0x14)
        ngroups = u32(data, winf + 4)
        ncgroups = u32(data, wbct + 8)
        self.by_id = {}
        for g in range(ngroups):
            go = base + u32(data, winf + 8 + 4 * g)
            aw = cstr(data, go)
            cnt = u32(data, go + 0x70)
            ctrl = None
            if g < ncgroups:
                scne = base + u32(data, wbct + 0xC + 4 * g)
                ctrl = base + u32(data, scne + 0xC)
            for j in range(cnt):
                wo = base + u32(data, go + 0x74 + 4 * j)
                w = Wave()
                w.aw = aw
                w.fmt, w.base_key = data[wo + 1], data[wo + 2]
                w.rate = int(round(struct.unpack_from(">f", data, wo + 4)[0]))
                fields = struct.unpack_from(">6I", data, wo + 8)
                w.offset, w.size, loop_flag, w.loop_start, w.loop_end, w.nsamples = fields
                w.loop = loop_flag != 0
                if ctrl is not None and j < u32(data, ctrl + 4):
                    w.wave_id = u16(data, base + u32(data, ctrl + 8 + 4 * j) + 2)
                else:
                    w.wave_id = j
                self.by_id[w.wave_id] = w


class Velo:
    __slots__ = ("high_vel", "wave_id", "volume", "pitch")


def read_velos(data, p, n):
    out = []
    for _ in range(n):
        v = Velo()
        v.high_vel = data[p]
        v.wave_id = u32(data, p + 4) & 0xFFFF
        v.volume, v.pitch = struct.unpack_from(">ff", data, p + 8)
        out.append(v)
        p += 16
    return out


class Osc:
    __slots__ = ("target", "rate", "table", "rel_table", "same_rel", "scale", "offset")


def read_env(data, p):
    pts = []
    for _ in range(64):
        m, t, v = struct.unpack_from(">hhh", data, p)
        pts.append((m, t, v))
        p += 6
        if m in (0xD, 0xE, 0xF):
            break
    return pts


class Inst:
    def __init__(self, kind):
        self.kind = kind
        self.volume = 1.0
        self.pitch = 1.0
        self.regions = []
        self.perc = {}
        self.perc_release = {}
        self.oscs = []

    def volume_osc(self):
        for o in self.oscs:
            if o.target == 0:
                return o
        return None

    def lookup(self, key, vel):
        if self.kind == "Inst":
            for hk, velos in self.regions:
                if key <= hk:
                    for v in velos:
                        if vel <= v.high_vel:
                            return v, self.volume, self.pitch
                    return None
            return None
        p = self.perc.get(key)
        if p is None:
            return None
        vol, pitch, velos = p
        for v in velos:
            if vel <= v.high_vel:
                return v, vol, pitch
        return None


class Bank:
    def __init__(self, data, base, ws_no):
        self.ws_no = ws_no
        size = u32(data, base + 4)
        self.bank_no = u32(data, base + 8)
        self.insts = {}
        if u32(data, base + 0xC) != 1:
            return
        p, lst, envt, osct = base + 0x20, None, None, None
        while p < base + size:
            cid, cs = struct.unpack_from(">4sI", data, p)
            if cid == b"LIST":
                lst = p
            elif cid == b"ENVT":
                envt = p + 8
            elif cid == b"OSCT":
                osct = p + 8
            if cs == 0 and cid == b"\0\0\0\0":
                break
            p = (p + 0xB + cs) & ~3
        if lst is None:
            return
        oscs = []
        if osct is not None and envt is not None:
            for k in range(u32(data, osct)):
                q = osct + 4 + 0x1C * k
                o = Osc()
                o.target = data[q + 4]
                o.rate, = struct.unpack_from(">f", data, q + 8)
                ta, tr = u32(data, q + 0xC), u32(data, q + 0x10)
                o.scale, o.offset = struct.unpack_from(">ff", data, q + 0x14)
                o.table = read_env(data, envt + ta)
                o.rel_table = read_env(data, envt + tr) if tr != ta else o.table
                o.same_rel = tr == ta
                oscs.append(o)
        for prog in range(u32(data, lst + 8)):
            off = u32(data, lst + 0xC + 4 * prog)
            if not off:
                continue
            q = base + off
            magic = data[q:q + 4]
            q += 4
            if magic == b"Inst":
                inst = Inst("Inst")
                nosc = u32(data, q)
                for k in range(nosc):
                    osc_index = u32(data, q + 4 + 4 * k)
                    if osc_index < len(oscs):
                        inst.oscs.append(oscs[osc_index])
                q += 4 + 4 * nosc
                q += 4 + 4 * u32(data, q)
                nkr = u32(data, q)
                q += 4
                for _ in range(nkr):
                    hk = data[q]
                    nv = u32(data, q + 4)
                    inst.regions.append((hk, read_velos(data, q + 8, nv)))
                    q += 8 + 16 * nv
                inst.volume, inst.pitch = struct.unpack_from(">ff", data, q)
                self.insts[prog] = inst
            elif magic == b"Perc":
                inst = Inst("Perc")
                cnt = u32(data, q)
                q += 4
                for k in range(cnt):
                    po = u32(data, q + 4 * k)
                    if not po:
                        continue
                    r = base + po
                    vol, pitch = struct.unpack_from(">ff", data, r + 4)
                    inst.perc_release[k] = u16(data, r + 0xE)
                    r += 16
                    r += 4 + 4 * u32(data, r)
                    nv = u32(data, r)
                    inst.perc[k] = (vol, pitch, read_velos(data, r + 4, nv))
                self.insts[prog] = inst


class AudioArchive:
    def __init__(self, data):
        if data[:4] != b"AA_<":
            raise ValueError("not a BAA")
        self.ws_by_no = {}
        self.bank_by_no = {}
        self.bst = self.bsc = None
        p = 4
        d = data
        while p < len(d):
            cmd = d[p:p + 4]
            if cmd == b">_AA":
                break
            if cmd == b"ws  ":
                no, off, _ = struct.unpack_from(">3I", d, p + 4)
                p += 16
                self.ws_by_no[no] = WaveSystem(d, off)
            elif cmd == b"bnk ":
                no, off = struct.unpack_from(">2I", d, p + 4)
                p += 12
                b = Bank(d, off, no)
                self.bank_by_no[b.bank_no] = b
            elif cmd in (b"bst ", b"bstn", b"bsc "):
                s, e = struct.unpack_from(">2I", d, p + 4)
                p += 12
                if cmd == b"bst ":
                    self.bst = d[s:e]
                elif cmd == b"bsc ":
                    self.bsc = d[s:e]
            elif cmd == b"bms ":
                p += 16
            elif cmd in (b"bl_<", b"vbnk"):
                p += 12
            elif cmd in (b"bsft", b"bmsa", b"dsqb", b">_bl", b"sect"):
                p += 4 if cmd == b">_bl" else 8
            else:
                raise ValueError("unknown BAA command")
        bsc = self.bsc
        self.seq_offsets = []
        for g in range(u16(bsc, 2)):
            t = u32(bsc, 8 + 4 * g)
            self.seq_offsets.append([u32(bsc, t + 4 + 4 * i) for i in range(u32(bsc, t))])


def decode_afc(raw, bits, nsamples):
    fsz = 9 if bits == 4 else 5
    out = []
    h1 = h2 = 0
    for f in range(len(raw) // fsz):
        fr = raw[f * fsz:f * fsz + fsz]
        hdr = fr[0]
        scale = 1 << (hdr >> 4)
        c1, c2 = AFC_COEF[hdr & 15]
        nib = []
        if bits == 4:
            for b in fr[1:]:
                nib.append(b >> 4)
                nib.append(b & 15)
            nib = [n - 16 if n >= 8 else n for n in nib]
            shift = 11
        else:
            for b in fr[1:]:
                for k in range(4):
                    nib.append((b >> (6 - 2 * k)) & 3)
            nib = [n - 4 if n >= 2 else n for n in nib]
            shift = 13
        for n in nib:
            s = ((n * scale) << shift) + c1 * h1 + c2 * h2
            s >>= 11
            if s > 32767:
                s = 32767
            elif s < -32768:
                s = -32768
            out.append(s)
            h2 = h1
            h1 = s
    return out[:nsamples] if nsamples else out


def decode_wave(raw, fmt, nsamples):
    if fmt == 0:
        return decode_afc(raw, 4, nsamples)
    if fmt == 1:
        return decode_afc(raw, 2, nsamples)
    if fmt == 2:
        pcm = [(b - 256 if b >= 128 else b) << 8 for b in raw]
    elif fmt == 3:
        n = len(raw) // 2
        pcm = list(struct.unpack(">%dh" % n, raw[:2 * n]))
    else:
        raise ValueError("unknown wave format")
    return pcm[:nsamples] if nsamples else pcm


CMDS = {
    0xB1: ("noteon", 3, 0), 0xB2: ("noteoff", 1, 0), 0xB3: ("note", 4, 0x40), 0xB4: ("setlastnote", 1, 0),
    0xB8: ("paramE", 2, 0), 0xB9: ("paramI", 2, 4), 0xBA: ("paramEI", 3, 0x10), 0xBB: ("paramII", 3, 0x14),
    0xC1: ("opentrack", 2, 8), 0xC2: ("closetrack", 1, 0), 0xC3: ("call", 1, 2), 0xC4: ("callf", 2, 8),
    0xC5: ("ret", 0, 0), 0xC6: ("retf", 1, 0), 0xC7: ("jmp", 1, 2), 0xC8: ("jmpf", 2, 8),
    0xC9: ("jmptable", 2, 0xB), 0xCA: ("calltable", 2, 0xB), 0xCB: ("loops", 1, 1), 0xCC: ("loope", 0, 0),
    0xD0: ("readport", 2, 0), 0xD1: ("writeport", 2, 0xC), 0xD2: ("checkportin", 1, 0),
    0xD3: ("checkportout", 1, 0), 0xD4: ("parentwriteport", 2, 0xC), 0xD5: ("childwriteport", 2, 0xC),
    0xD6: ("parentreadport", 2, 0), 0xD7: ("childreadport", 2, 0), 0xD8: ("regload", 2, 4),
    0xD9: ("reg", 3, 0x30), 0xDA: ("reg", 3, 0x10), 0xDB: ("reguni", 2, 0), 0xDC: ("regtblload", 4, 0xE0),
    0xE0: ("tempo", 1, 1), 0xE1: ("bankprg", 1, 1), 0xE2: ("bank", 1, 0), 0xE3: ("prg", 1, 0),
    0xE7: ("envscaleset", 2, 4), 0xE8: ("envset", 2, 8), 0xE9: ("simpleadsr", 5, 0x155),
    0xEA: ("busconnect", 2, 4), 0xEB: ("iircutoff", 1, 0), 0xEC: ("iirset", 4, 0x55), 0xED: ("firset", 1, 2),
    0xF0: ("wait", 0, 0), 0xF1: ("waitbyte", 1, 0), 0xF3: ("setinttable", 1, 2), 0xF4: ("setinterrupt", 1, 1),
    0xF5: ("disinterrupt", 1, 1), 0xF6: ("reti", 0, 0), 0xF7: ("clri", 0, 0), 0xF8: ("inttimer", 2, 4),
    0xF9: ("synccpu", 1, 1), 0xFD: ("printf", 0, 0), 0xFE: ("nop", 0, 0), 0xFF: ("finish", 0, 0),
}
REG_F = 3


class Reader:
    def __init__(self, data, pc):
        self.d, self.pc = data, pc

    def u8(self):
        v = self.d[self.pc]
        self.pc += 1
        return v

    def u16(self):
        v = u16(self.d, self.pc)
        self.pc += 2
        return v

    def u24(self):
        b = self.d[self.pc:self.pc + 3]
        self.pc += 3
        return (b[0] << 16) | (b[1] << 8) | b[2]

    def midi(self):
        b = self.u8()
        if not b & 0x80:
            return b
        v = b & 0x7F
        for _ in range(3):
            v <<= 7
            b = self.u8()
            v |= b & 0x7F
            if not b & 0x80:
                return v
        return 0


def decode_one(rd, readreg):
    op = rd.u8()
    if op < 0x80:
        flags = rd.u8()
        vel = rd.u8()
        gate = rd.midi() if (flags & 7) == 0 else None
        return "note_on", [op, flags, vel, gate]
    hi = op & 0xF0
    if hi == 0x80:
        return "note_off", [op & 7]
    width = 0
    if hi == 0x90:
        n = (op & 7) + 1
        mask = rd.u8()
        b = 3
        for _ in range(n):
            if mask & 0x80:
                width |= b
            mask = (mask << 1) & 0xFF
            b <<= 2
        op = rd.u8()
    if op == 0xB0:
        return "ext%02x" % rd.u8(), []
    info = CMDS.get(op)
    if info is None:
        return "unk%02x" % op, []
    name, nargs, widths = info
    rt = widths | width
    args = []
    for _ in range(nargs):
        t = rt & 3
        if t == 0:
            a = rd.u8()
        elif t == 1:
            a = rd.u16()
        elif t == 2:
            a = rd.u24()
        else:
            a = readreg(rd.u8())
        args.append(a)
        rt = (rt & 0xFFFF) >> 2
    if name == "printf":
        s = b""
        while True:
            c = rd.u8()
            if c == 0:
                break
            s += bytes([c])
        for _ in range(s.count(b"%")):
            rd.u8()
        args = [s]
    elif name == "wait":
        args = [rd.midi()]
    return name, args


class Note:
    __slots__ = ("time", "bank", "prog", "key", "vel", "gate", "vol", "pitch_oct", "adsr")


class Track:
    def __init__(self, pc, parent=None, start_time=0.0):
        self.pc = pc
        self.parent = parent
        self.regs = {}
        self.ports = [0] * 16
        self.bank = 0
        self.prog = 0
        self.transpose = 0
        self.bendsense = 12
        self.tempo = parent.tempo if parent else 120
        self.timebase = parent.timebase if parent else 48
        self.time = start_time
        self.stack = []
        self.loops = []
        self.params = {0: 1.0, 1: 0.0}
        self.done = False
        self.held = {}
        self.children = {}
        self.adsr = None

    def tick_s(self):
        r = self
        while r.parent is not None:
            r = r.parent
        return 60.0 / (max(1, r.tempo) * max(1, r.timebase))

    def readreg(self, r):
        if r < 0x40:
            return self.regs.get(r, 0)
        if r < 0x50:
            return self.ports[r - 0x40]
        return {0x62: self.timebase, 0x63: self.transpose, 0x64: self.bendsense, 0x67: self.bank,
                0x68: self.prog}.get(r, 0)

    def writereg(self, r, v):
        v &= 0xFFFF
        self.regs[REG_F] = v
        if r < 0x40:
            self.regs[r] = v
        elif r < 0x50:
            self.ports[r - 0x40] = v
        elif r == 0x62:
            self.timebase = v
        elif r == 0x63:
            self.transpose = v - 0x10000 if v & 0x8000 else v
        elif r == 0x64:
            self.bendsense = v
        elif r == 0x67:
            self.bank = v & 0xFF
        elif r == 0x68:
            self.prog = v & 0xFF


class Sim:
    def __init__(self, data, start, rng, max_time, ports, max_steps=20000):
        self.data = data
        self.rng = rng
        self.max_time = max_time
        self.max_steps = max_steps
        self.notes = []
        root = Track(start)
        if ports:
            for k, v in ports.items():
                root.ports[k] = v
        self.tracks = [root]
        self.steps = 0

    def cond(self, t, c):
        f = t.regs.get(REG_F, 0)
        return {0: True, 1: f == 0, 2: f != 0, 3: f == 1, 4: f >= 0x8000, 5: f < 0x8000}.get(c, False)

    def note(self, t, key, vel, gate):
        n = Note()
        n.time, n.bank, n.prog = t.time, t.bank, t.prog
        n.key = (key + t.transpose) & 0xFF
        n.vel, n.gate = vel, gate
        n.vol = t.params.get(0, 1.0)
        n.pitch_oct = t.params.get(1, 0.0) * t.bendsense / 3.0
        n.adsr = t.adsr
        self.notes.append(n)
        return n

    def run(self):
        while self.steps < self.max_steps:
            live = [t for t in self.tracks if not t.done]
            if not live:
                break
            t = min(live, key=lambda x: x.time)
            if t.time > self.max_time:
                break
            self.step(t)
        return self.notes

    def step(self, t):
        self.steps += 1
        rd = Reader(self.data, t.pc)
        try:
            name, a = decode_one(rd, t.readreg)
        except (IndexError, struct.error):
            t.done = True
            return
        t.pc = rd.pc
        if name == "note_on":
            key, flags, vel, gate = a
            n = self.note(t, key, vel, gate)
            if (flags & 7) == 0:
                if gate:
                    n.gate = gate * t.tick_s()
                    t.time += n.gate
                else:
                    n.gate = None
            else:
                t.held[flags & 7] = n
        elif name == "note_off":
            n = t.held.pop(a[0], None)
            if n is not None:
                n.gate = t.time - n.time
        elif name == "noteon":
            t.held[a[0] & 7] = self.note(t, a[1], a[2], None)
        elif name == "noteoff":
            n = t.held.pop(a[0] & 7, None)
            if n is not None:
                n.gate = t.time - n.time
        elif name == "note":
            n = self.note(t, a[1], a[2], a[3] * t.tick_s())
            t.time += n.gate
        elif name in ("wait", "waitbyte"):
            t.time += a[0] * t.tick_s()
        elif name == "tempo":
            t.tempo = a[0]
        elif name == "bankprg":
            t.bank, t.prog = a[0] >> 8, a[0] & 0xFF
        elif name == "bank":
            t.bank = a[0] & 0xFF
        elif name == "prg":
            t.prog = a[0] & 0xFF
        elif name in ("paramE", "paramEI"):
            v = a[1] & 0xFF
            s = (v << 8) | ((v << 1) if not v & 0x80 else 0)
            s = s - 0x10000 if s & 0x8000 else s
            t.params[a[0]] = s / 32767.0
        elif name in ("paramI", "paramII"):
            v = a[1] & 0xFFFF
            t.params[a[0]] = (v - 0x10000 if v & 0x8000 else v) / 32767.0
        elif name == "opentrack":
            c = Track(a[1], t, t.time)
            c.bank, c.prog = t.bank, t.prog
            t.children[a[0]] = c
            self.tracks.append(c)
        elif name == "closetrack":
            c = t.children.get(a[0])
            if c:
                c.done = True
        elif name == "call":
            t.stack.append(t.pc)
            t.pc = a[0]
        elif name == "callf":
            if self.cond(t, a[0]):
                t.stack.append(t.pc)
                t.pc = a[1]
        elif name in ("ret", "reti"):
            if t.stack:
                t.pc = t.stack.pop()
            else:
                t.done = True
        elif name == "retf":
            if self.cond(t, a[0]):
                if t.stack:
                    t.pc = t.stack.pop()
                else:
                    t.done = True
        elif name == "jmp":
            t.pc = a[0]
        elif name == "jmpf":
            if self.cond(t, a[0]):
                t.pc = a[1]
        elif name in ("jmptable", "calltable"):
            idx = a[0] if isinstance(a[0], int) else 0
            p = a[1] + idx * 3
            tgt = (self.data[p] << 16) | (self.data[p + 1] << 8) | self.data[p + 2]
            if name == "calltable":
                t.stack.append(t.pc)
            t.pc = tgt
        elif name == "loops":
            t.loops.append([t.pc, a[0]])
        elif name == "loope":
            if t.loops:
                lp = t.loops[-1]
                lp[1] -= 1
                if lp[1] > 0:
                    t.pc = lp[0]
                else:
                    t.loops.pop()
        elif name == "readport":
            t.writereg(a[1], t.ports[a[0] & 15])
        elif name == "writeport":
            t.ports[a[0] & 15] = a[1] & 0xFFFF
        elif name == "parentwriteport":
            if t.parent:
                t.parent.ports[a[0] & 15] = a[1] & 0xFFFF
        elif name == "childwriteport":
            c = t.children.get(a[0] >> 4)
            if c:
                c.ports[a[0] & 15] = a[1] & 0xFFFF
        elif name == "parentreadport":
            t.writereg(a[1], t.parent.ports[a[0] & 15] if t.parent else 0)
        elif name == "childreadport":
            c = t.children.get(a[0] >> 4)
            t.writereg(a[1], c.ports[a[0] & 15] if c else 0)
        elif name in ("checkportin", "checkportout"):
            t.regs[REG_F] = 0
        elif name == "regload":
            t.writereg(a[0], a[1])
        elif name == "reg":
            op, r, v = a
            cur = t.readreg(r)
            if op == 0:
                pass
            elif op == 1:
                v = cur + v
            elif op in (2, 3):
                v = cur - v
                if op == 3:
                    r = REG_F
            elif op == 4:
                v = cur * v
                r = 0x21
            elif op == 5:
                v &= cur
            elif op == 6:
                v |= cur
            elif op == 7:
                v ^= cur
            elif op == 8:
                v = self.rng.randrange(v) if (self.rng and v) else 0
            elif op == 9:
                v = cur << v
            elif op == 10:
                v = cur >> v
            else:
                return
            t.writereg(r, v & 0xFFFF)
        elif name == "reguni":
            op, r = a
            cur = t.readreg(r)
            t.writereg(r, (-cur) & 0xFFFF if op == 0xB else cur)
        elif name == "regtblload":
            op, r, tbl, idx = a
            idx = idx if isinstance(idx, int) else 0
            if op == 0xC:
                v = self.data[tbl + idx]
            elif op == 0xD:
                v = u16(self.data, tbl + idx * 2)
            elif op == 0xE:
                p = tbl + idx * 3
                v = (self.data[p] << 16) | (self.data[p + 1] << 8) | self.data[p + 2]
            elif op == 0xF:
                v = u32(self.data, tbl + idx * 4)
            else:
                v = 0
            t.writereg(r, v)
        elif name == "simpleadsr":
            t.adsr = tuple(a)
        elif name == "finish" or name.startswith("unk"):
            t.done = True


def rarc_file_by_id(data, file_id):
    d = yaz0_decompress(data)
    count, entries = struct.unpack_from(">2I", d, 0x28)
    base = 0x20 + u32(d, 0x0C)
    for i in range(count):
        fid, _, typ, _, _, off, size = struct.unpack_from(">HHBBHII", d, 0x20 + entries + 0x14 * i)
        if typ & 1 and fid == file_id:
            return yaz0_decompress(d[base + off:base + off + size])
    raise KeyError(file_id)


ENV_RATE = 400.0
ENV_INC = 48000.0 / 32000.0
CURVES = [
    [1.0, .9375, .875, .8125, .75, .6875, .625, .5625, .5, .4375, .375, .3125, .25, .1875, .125, .0625, 0.0],
    [1.0, .968246, .935414, .901388, .866025, .829156, .790569, .75, .707107, .661438, .612372, .559017, .5,
     .433013, .353553, .25, 0.0],
    [1.0, .878906, .765625, .660156, .5625, .472656, .390625, .316406, .25, .191406, .140625, .097656, .0625,
     .035156, .015625, .003906, 0.0],
    [1.0, .970489, .781274, .546281, .399792, .289315, .212104, .157476, .112613, .08179, .057985, .043642,
     .030824, .023713, .015259, .009156, 0.0],
]


class OscState:
    def __init__(self, osc):
        self.o = osc
        self.el = self.cur = self.start = self.target = 0.0
        self.idx = 0
        self.curve = 0
        self.dr = 0
        if osc is None or not osc.table:
            self.mode, self.cur = 2, 1.0
        else:
            self.target = osc.table[0][2] / 32768.0
            self.curve = osc.table[0][0] & 3
            self.mode = 1

    def value(self):
        o = self.o
        return self.cur * (o.scale if o else 1.0) + (o.offset if o else 0.0)

    def cv(self, t):
        f = 16.0 * self.el / t if t else 16.0
        i = min(int(f), 15)
        fr = f - i
        tab = CURVES[self.curve & 3]
        w = (1 - fr) * tab[i] + fr * tab[i + 1]
        self.cur = self.start * w + self.target * (1 - w)

    def update(self):
        if self.mode == 4:
            x = self.dr & 0x3FFF
            if self.el < x:
                self.cv(x)
            else:
                self.cur = self.target
                self.mode = 0
            return
        if self.mode in (0, 2):
            return
        tbl = self.o.table if self.mode == 1 else self.o.rel_table
        guard = 0
        while self.el >= tbl[self.idx][1] and guard < 64:
            guard += 1
            self.el -= tbl[self.idx][1]
            self.cur = self.target
            self.idx += 1
            if self.idx >= len(tbl):
                self.mode = 0
                return
            self.start = self.cur
            m, _, v = tbl[self.idx]
            if m == 0xF:
                self.mode = 0
                return
            if m == 0xE:
                self.mode = 2
                return
            if m == 0xD:
                self.idx = v
                continue
            self.curve = m & 3
            self.target = v / 32768.0
        self.cv(tbl[self.idx][1])

    def inc(self):
        if self.mode in (0, 2):
            return
        self.el += ENV_INC * (self.o.rate if self.o else 1.0)
        self.update()

    def release(self, direct_release):
        o = self.o
        if direct_release:
            self.dr = direct_release
            self.el = 0.0
            self.start = self.cur
            self.target = 0.0
            self.idx = 0
            self.curve = (direct_release >> 14) & 3
            self.mode = 4
            self.update()
            return
        if o is None or not o.rel_table:
            self.dr = 10
            self.el, self.start, self.target, self.curve, self.mode = 0.0, self.cur, 0.0, 0, 4
            self.update()
            return
        if not o.same_rel:
            self.el = 0.0
            self.start = self.cur
            self.target = o.rel_table[0][2] / 32768.0
            self.idx = 0
            self.curve = o.rel_table[0][0] & 3
        self.mode = 3
        self.update()


def envelope(osc, gate_s, max_s, direct_release=0):
    st = OscState(osc)
    out = []
    rel_at = int(round(gate_s * ENV_RATE)) if gate_s else None
    for k in range(int(max_s * ENV_RATE)):
        if rel_at is not None and k == rel_at:
            st.release(direct_release)
        out.append(max(0.0, st.value()))
        if st.mode == 0 and k > 0:
            break
        st.inc()
    return out


def sid_parts(sid):
    return sid >> 24, (sid >> 16) & 0xFF, sid & 0xFFFF


class Resolved:
    __slots__ = ("wave", "ratio", "vol", "note", "osc", "direct_release")


class AudioSystem:
    def __init__(self, dump, pow2):
        self.dump = dump
        self.arc = AudioArchive(yaz0_decompress(dump.read("AudioRes/SMR.szs")))
        self.pow2 = pow2
        self.adsr = False
        self.aw_cache = {}
        self.pcm_cache = {}
        self.jaiseq = None

    def bgm_data(self, sid):
        s, g, i = sid_parts(sid)
        bst = self.arc.bst
        root = u32(bst, 12)
        sec = u32(bst, root + 4 + 4 * s)
        grp = u32(bst, sec + 4 + 4 * g)
        item = u32(bst, grp + 8 + 4 * i) & 0xFFFFFF
        res_id = u16(bst, item + 2)
        if self.jaiseq is None:
            self.jaiseq = self.dump.read("AudioRes/Seqs/JaiSeq.arc")
        return rarc_file_by_id(self.jaiseq, res_id)

    def run(self, sid, rng, ports):
        s, g, i = sid_parts(sid)
        if s == 1:
            return Sim(self.bgm_data(sid), 0, rng, 20.0, ports).run()
        return Sim(self.arc.bsc, self.arc.seq_offsets[g][i], rng, 6.0, ports).run()

    def pow2f(self, x):
        if x == 0.0:
            return 1.0
        return self.pow2[x.hex()]

    def resolve_note(self, n):
        b = self.arc.bank_by_no.get(n.bank)
        if b is None:
            return None
        inst = b.insts.get(n.prog)
        if inst is None:
            return None
        r = inst.lookup(n.key, n.vel)
        if r is None:
            return None
        v, ivol, ipitch = r
        ws = self.arc.ws_by_no.get(b.ws_no)
        w = ws.by_id.get(v.wave_id) if ws else None
        if w is None:
            return None
        res = Resolved()
        ratio = v.pitch * ipitch * self.pow2f(n.pitch_oct)
        if inst.kind == "Inst":
            ratio *= self.pow2f((n.key - w.base_key) / 12.0)
            res.osc, res.direct_release = inst.volume_osc(), 0
        else:
            res.osc, res.direct_release = None, inst.perc_release.get(n.key & 0x7F, 0)
        if self.adsr and n.adsr:
            a, d, s, lvl, dr = [v - 0x10000 if v & 0x8000 else v for v in n.adsr[:4]] + [n.adsr[4]]
            o = Osc()
            o.target, o.rate, o.scale, o.offset = 0, 1.0, 1.0, 0.0
            o.table = [(0, a, 32767), (0, d, 32767), (0, s, lvl), (0xE, 0, 0)]
            o.rel_table = None
            o.same_rel = False
            res.osc, res.direct_release = o, dr
        res.wave, res.ratio, res.note = w, ratio, n
        res.vol = v.volume * ivol * n.vol * n.vol * (n.vel / 127.0)
        return res

    def variants(self, sid, ports, want):
        out, seen = [], set()
        for k in range(49):
            rng = None if k == 0 else random.Random(k)
            notes = self.run(sid, rng, ports)
            res = [r for r in (self.resolve_note(n) for n in notes) if r]
            sig = tuple((r.wave.aw, r.wave.wave_id, round(r.note.time, 3)) for r in res)
            if sig not in seen:
                seen.add(sig)
                out.append(res)
                if len(out) > want:
                    break
        return out

    def wave_pcm(self, w):
        key = (w.aw, w.offset, w.size, w.fmt, w.nsamples)
        p = self.pcm_cache.get(key)
        if p is None:
            if w.aw not in self.aw_cache:
                self.aw_cache[w.aw] = self.dump.read("AudioRes/Waves/" + w.aw)
            raw = self.aw_cache[w.aw][w.offset:w.offset + w.size]
            p = self.pcm_cache[key] = decode_wave(raw, w.fmt, w.nsamples)
        return p
