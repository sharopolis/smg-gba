import ctypes
import json
import os
import struct
import sys
from array import array

from audio_jaudio import AudioSystem, Sim, envelope
from common import Dump, IntReader, f32, load_lib, parse_args, read_pak, write_out


def load_dsp(out_root):
    lib = load_lib("audio_dsp", os.path.join(out_root, ".bin"))
    ptr, i32, dbl, lng, u64 = ctypes.c_void_p, ctypes.c_int, ctypes.c_double, ctypes.c_long, ctypes.c_uint64
    lib.rs_count.argtypes = [lng, i32]
    lib.rs_count.restype = lng
    lib.rs_run.argtypes = [ptr, lng, i32, i32, ptr]
    lib.rs_run.restype = lng
    lib.render_part.argtypes = [ptr, lng, dbl, i32, lng, lng, lng, ptr, lng, dbl, dbl, ptr, lng, i32]
    lib.render_finish.argtypes = [ptr, lng, i32, ptr]
    lib.render_finish.restype = lng
    lib.peak_abs.argtypes = [ptr, lng]
    lib.peak_abs.restype = dbl
    lib.scale.argtypes = [ptr, lng, dbl]
    lib.add_at.argtypes = [ptr, ptr, lng]
    lib.f32_to_f64.argtypes = [ptr, ptr, lng]
    lib.fade_linspace.argtypes = [ptr, lng]
    lib.pcg_seed.argtypes = [u64, u64, u64, u64]
    lib.quantise.argtypes = [ptr, lng, ptr, lng, ptr, ptr, lng]
    return lib


def addr(a, offset=0):
    return a.buffer_info()[0] + offset * a.itemsize


def fx(h):
    return float.fromhex(h)


class Builder:
    def __init__(self, dump, data, out_root):
        self.dump = dump
        self.dsp = load_dsp(out_root)
        self.consts = data["consts"]
        self.snd = AudioSystem(dump, {k: fx(v) for k, v in data["pow2"].items()})
        self.streams = {}

    def resample(self, pcm, nch, rate):
        n = len(pcm) // nch
        cnt = self.dsp.rs_count(n, rate)
        out = array("f", bytes(4 * cnt))
        self.dsp.rs_run(addr(pcm), n, nch, rate, addr(out))
        res = array("d", bytes(8 * cnt))
        self.dsp.f32_to_f64(addr(out), addr(res), cnt)
        return res

    def notes(self, part):
        ports = {int(k): v for k, v in part.get("ports", {}).items()} or None
        want = part.get("v", 0)
        vs = self.snd.variants(part["sid"], ports, want)
        var = vs[min(want, len(vs) - 1)]
        hold = part.get("hold")
        if hold is not None:
            hold = fx(hold)
            var = [r for r in var if r.note.time < hold]
            for r in var:
                left = hold - r.note.time
                if r.note.gate is None or r.note.gate > left:
                    r.note.gate = left
        if part.get("vel", 0):
            var = [r for r in var if r.note.vel > 1]
        if part.get("vel0", 0):
            var = [r for r in var if r.note.vel > 0]
        shift = part.get("shift")
        if shift is not None:
            for r in var:
                r.note.time -= fx(shift)
        return var

    def render(self, resolved, rate, max_len, trim, flavor=0):
        if not resolved:
            return array("h"), rate or 32000
        rate = rate or max(r.wave.rate for r in resolved)
        parts = []
        for r in resolved:
            w = r.wave
            src = self.snd.wave_pcm(w)
            if flavor & 1:
                step = f32(f32(f32(r.ratio) * f32(float(w.rate))) / f32(float(rate)))
            else:
                step = r.ratio * w.rate / float(rate)
            gain = envelope(r.osc, r.note.gate, max_len, r.direct_release)
            if flavor & 2 and len(gain) > 1:
                gain = gain[:-1]
            if flavor & 8:
                gain = [f32(v) for v in gain]
            env_len = len(gain) / 400.0
            loop = w.loop and w.loop_end > w.loop_start
            if loop:
                out_len = int(min(env_len, max_len) * rate)
            else:
                out_len = int(min(len(src) / step, env_len * rate, max_len * rate))
            if out_len > 0:
                parts.append((int(round(r.note.time * rate)), out_len, src, step, loop, w, gain, r.vol))
        if not parts:
            return array("h"), rate
        total = min(max(p[0] + p[1] for p in parts), int(max_len * rate))
        mix = array("d", bytes(8 * total))
        for o, out_len, src, step, loop, w, gain, vol in parts:
            if o >= total:
                continue
            s = array("h", src)
            g = array("d", gain)
            self.dsp.render_part(addr(s), len(s), step, 1 if loop else 0, w.loop_start, w.loop_end, out_len,
                                 addr(g), len(g), float(rate), vol, addr(mix, o), total - o,
                                 1 if flavor & 8 else 0)
        out = array("h", bytes(2 * total))
        n = self.dsp.render_finish(addr(mix), total, 1 if trim else 0, addr(out))
        return out[:n], rate

    def src_se(self, s):
        resolved = []
        self.snd.adsr = bool(s.get("fl", 0) & 4)
        for part in s["notes"]:
            resolved += self.notes(part)
        self.snd.adsr = False
        pcm, rate = self.render(resolved, s.get("rate"), fx(s["max_len"]), s.get("trim", 1), s.get("fl", 0))
        if s.get("norm") is not None and len(pcm):
            peak = float(max(abs(v) for v in pcm))
            if peak > 0:
                gain = (fx(s["norm"]) * 32767) / peak
                pcm = array("h", [int(max(-32768.0, min(32767.0, v * gain))) for v in pcm])
        if s.get("direct"):
            return array("d", [v / 32768.0 for v in pcm])
        return self.resample(pcm, 1, rate)

    def src_bgm(self, s):
        seconds = fx(s["sec"])
        notes = Sim(self.snd.bgm_data(s["sid"]), 0, None, seconds, None, 5000000).run()
        resolved = [r for r in (self.snd.resolve_note(n) for n in notes) if r]
        pcm, rate = self.render(resolved, s["rate"], seconds, 0)
        n = int(seconds * rate)
        x = list(pcm[:n]) + [0] * max(0, n - len(pcm))
        k = 32767.0 / (float(max(abs(v) for v in x)) or 1.0)
        return self.resample(array("h", [round(v * k) for v in x]), 1, rate)

    def stream(self, rel):
        if rel not in self.streams:
            data = self.dump.read(rel)
            fmt, _, ch, _, rate, total = struct.unpack_from(">HHHHII", data, 8)
            if fmt != 1:
                raise ValueError("unsupported stream format")
            chans = [array("h") for _ in range(ch)]
            p = 0x40
            while p + 0x20 <= len(data) and data[p:p + 4] == b"BLCK":
                bs = struct.unpack_from(">I", data, p + 4)[0]
                p += 0x20
                for c in range(ch):
                    blk = data[p + c * bs:p + (c + 1) * bs]
                    a = array("h", blk + b"\0" * (bs - len(blk)))
                    if sys.byteorder == "little":
                        a.byteswap()
                    chans[c].extend(a)
                p += ch * bs
            self.streams[rel] = ([c[:total] for c in chans], rate)
        return self.streams[rel]

    def src_ast(self, s):
        chans, rate = self.stream(s["file"])
        if s.get("pair") is not None:
            chans = chans[2 * s["pair"]:2 * s["pair"] + 2]
        nch = len(chans)
        inter = array("h", bytes(2 * len(chans[0]) * nch))
        for c in range(nch):
            inter[c::nch] = chans[c]
        return self.resample(inter, nch, rate)

    def float_ops(self, x, ops):
        for op in ops:
            k = op[0]
            if k == "mul":
                self.dsp.scale(addr(x), len(x), fx(op[1]))
            elif k == "peak":
                p = self.dsp.peak_abs(addr(x), len(x))
                lim = fx(op[1])
                if p > lim:
                    self.dsp.scale(addr(x), len(x), lim / p)
            elif k == "nrm":
                p = self.dsp.peak_abs(addr(x), len(x))
                if p > 0:
                    self.dsp.scale(addr(x), len(x), fx(op[1]) / p)
            elif k == "fade":
                f = min(len(x), op[1])
                self.dsp.fade_linspace(addr(x, len(x) - f), f)
            elif k == "seg":
                y = array("d")
                for a, b in op[1]:
                    y.extend(x[a:b])
                x = y
        return x

    def build(self, rec, resid, total):
        tracks = []
        for t in rec["tracks"]:
            s = t["src"]
            if s["k"] == "se":
                x = self.src_se(s)
            elif s["k"] == "bgm":
                x = self.src_bgm(s)
            else:
                x = self.src_ast(s)
            tracks.append((t.get("at", 0), self.float_ops(x, t.get("ops", []))))
        if len(tracks) == 1 and tracks[0][0] == 0:
            mix = tracks[0][1]
        else:
            mix = array("d", bytes(8 * max(a + len(x) for a, x in tracks)))
            for a, x in tracks:
                self.dsp.add_at(addr(mix, a), addr(x), len(x))
        mix = self.float_ops(mix, rec.get("post", []))
        n = len(mix)
        final = rec.get("final", [])
        keep = n
        guide = n
        for op in final:
            if op[0] == "t" and keep > 4:
                keep &= ~3
            elif op[0] == "trim8" and keep > op[1]:
                keep = op[1]
                guide = min(guide, op[1] - op[2])
        guide = min(guide, keep)
        st, inc = self.consts["pcg"]
        self.dsp.pcg_seed(st >> 64, st & 0xFFFFFFFFFFFFFFFF, inc >> 64, inc & 0xFFFFFFFFFFFFFFFF)
        q = array("b", bytes(n))
        early = [(p, v) for p, v in resid if p < guide]
        rp = array("l", [p for p, _ in early] or [0])
        rv = array("i", [v for _, v in early] or [0])
        self.dsp.quantise(addr(mix), n, addr(q), guide, addr(rp), addr(rv), len(early))
        out = list(q)
        for op in final:
            k = op[0]
            if k == "t":
                if len(out) > 4:
                    out = out[:len(out) & ~3]
            elif k == "p":
                out += [0] * (-len(out) % 4)
            elif k == "pad":
                out += [0] * op[1]
            elif k == "trim8":
                m, f = op[1], op[2]
                if len(out) > m:
                    y = [float(v) for v in out[:m]]
                    if f == 1:
                        w = [1.0]
                    else:
                        step = (0.0 - 1.0) / (f - 1)
                        w = [i * step + 1.0 for i in range(f - 1)] + [0.0]
                    for i in range(f):
                        y[m - f + i] *= w[i]
                    out = [round(v) for v in y]
        out = (out + [0] * max(0, total - len(out)))[:total]
        for p, v in resid:
            if p >= guide:
                out[p] += v
        return bytes((v + 256) & 0xFF for v in out)


def load_data(data_dir):
    pak = read_pak(os.path.join(data_dir, "audio", "audio.pak"))
    data = json.loads(pak["recipes.json"])
    resid = {}
    r = IntReader(pak["residuals.bin"])
    for name in data["order"]:
        n = r.uvarint()
        total = r.uvarint()
        items = []
        pos = 0
        for _ in range(n):
            pos += r.uvarint()
            items.append((pos, r.int()))
        resid[name] = (items, total)
    return data, resid


def main():
    args = parse_args()
    data, resid = load_data(args.data)
    rest = Builder(Dump(args.orig), data, args.out)
    for name in data["order"]:
        items, total = resid[name]
        write_out(args.out, "assets/audio/" + name + ".pcm8", rest.build(data["recipes"][name], items, total))
    print("audio: %d files" % len(data["order"]))


if __name__ == "__main__":
    main()
