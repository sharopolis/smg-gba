M32 = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF
M128 = (1 << 128) - 1
PCG_MULT = 0x2360ED051FC65DA44385DF649FCCF645
POOL_SIZE = 4
PB = 22
SIZE = 256
STARS = 420


def mix(x, y):
    r = (0xCA01F9DD * x - 0x4973F715 * y) & M32
    return r ^ (r >> 16)


class HashMix:
    def __init__(self):
        self.const = 0x43B0D7E5

    def __call__(self, v):
        v = (v ^ self.const) & M32
        self.const = (self.const * 0x931E8875) & M32
        v = (v * self.const) & M32
        return v ^ (v >> 16)


def seed_state(entropy, n_words):
    words = []
    while True:
        words.append(entropy & M32)
        entropy >>= 32
        if not entropy:
            break
    hashmix = HashMix()
    pool = [hashmix(words[i] if i < len(words) else 0) for i in range(POOL_SIZE)]
    for src in range(POOL_SIZE):
        for dst in range(POOL_SIZE):
            if src != dst:
                pool[dst] = mix(pool[dst], hashmix(pool[src]))
    for src in range(POOL_SIZE, len(words)):
        for dst in range(POOL_SIZE):
            pool[dst] = mix(pool[dst], hashmix(words[src]))
    out = []
    h = 0x8B51F9DD
    for i in range(n_words):
        v = (pool[i % POOL_SIZE] ^ h) & M32
        h = (h * 0x58F38DED) & M32
        v = (v * h) & M32
        out.append(v ^ (v >> 16))
    return out


class PCG64:
    def __init__(self, seed):
        s = seed_state(seed, 8)
        q = [s[2 * k] | (s[2 * k + 1] << 32) for k in range(4)]
        self.inc = (((q[2] << 64) | q[3]) << 1 | 1) & M128
        self.state = 0
        self.step()
        self.state = (self.state + ((q[0] << 64) | q[1])) & M128
        self.step()
        self.has32 = False
        self.u32 = 0

    def step(self):
        self.state = (self.state * PCG_MULT + self.inc) & M128

    def next64(self):
        self.step()
        st = self.state
        x = ((st >> 64) ^ st) & M64
        rot = st >> 122
        return ((x >> rot) | (x << ((64 - rot) & 63))) & M64

    def next32(self):
        if self.has32:
            self.has32 = False
            return self.u32
        v = self.next64()
        self.has32 = True
        self.u32 = v >> 32
        return v & M32

    def random(self):
        return (self.next64() >> 11) * (1.0 / 9007199254740992.0)

    def integer(self, low, high):
        rng = high - low - 1
        excl = rng + 1
        m = self.next32() * excl
        left = m & M32
        if left < excl:
            threshold = (M32 - rng) % excl
            while left < threshold:
                m = self.next32() * excl
                left = m & M32
        return low + (m >> 32)



def bicubic(x):
    if x < 0.0:
        x = -x
    if x < 1.0:
        return (1.5 * x - 2.5) * x * x + 1
    if x < 2.0:
        return (((x - 5) * x + 8) * x - 4) * -0.5
    return 0.0


def resample_coeffs(insize, outsize):
    scale = insize / outsize
    fscale = scale if scale > 1.0 else 1.0
    support = 2.0 * fscale
    out = []
    for xx in range(outsize):
        center = 0.0 + (xx + 0.5) * scale
        ss = 1.0 / fscale
        xmin = int(center - support + 0.5)
        if xmin < 0:
            xmin = 0
        xmax = int(center + support + 0.5)
        if xmax > insize:
            xmax = insize
        k = [bicubic((x + xmin - center + 0.5) * ss) for x in range(xmax - xmin)]
        ww = 0.0
        for w in k:
            ww += w
        if ww != 0.0:
            k = [w / ww for w in k]
        out.append((xmin, [fixed_point(w) for w in k]))
    return out


def fixed_point(w):
    if w < 0:
        return int(-0.5 + w * (1 << PB))
    return int(0.5 + w * (1 << PB))


def clamp_byte(v):
    if v < 0:
        return 0
    if v > 255:
        return 255
    return v


def clip_fixed(v):
    return clamp_byte(v >> PB)


def resize_l(rows, w, h, tw, th):
    half = 1 << (PB - 1)
    ch = resample_coeffs(w, tw)
    tmp = []
    for row in rows:
        line = []
        for xmin, k in ch:
            s = half
            for i, kk in enumerate(k):
                s += row[xmin + i] * kk
            line.append(clip_fixed(s))
        tmp.append(line)
    out = []
    for ymin, k in resample_coeffs(h, th):
        acc = [half] * tw
        for i, kk in enumerate(k):
            acc = [a + v * kk for a, v in zip(acc, tmp[ymin + i])]
        out.append([clip_fixed(a) for a in acc])
    return out


def noise(rng, size, scale, octaves):
    out = [[0.0] * size for _ in range(size)]
    amp = 1.0
    for o in range(octaves):
        n = max(2, int(size / scale * (1 << o)))
        g = [[rng.random() for _ in range(n + 1)] for _ in range(n + 1)]
        g[-1] = list(g[0])
        for row in g:
            row[-1] = row[0]
        px = [[int(v * 255) for v in row] for row in g]
        big = size + size // n
        im = resize_l(px, n + 1, n + 1, big, big)
        for y in range(size):
            orow = out[y]
            irow = im[y]
            for x in range(size):
                orow[x] += irow[x] / 255 * amp
        amp *= 0.5
    peak = max(max(row) for row in out)
    return [[v / peak for v in row] for row in out]


def colourise(v, lo, hi):
    out = []
    for a, b in zip(lo, hi):
        c = float(a) + (float(b) - float(a)) * v
        out.append(int(clamp_byte(c)))
    return out


def sky_space():
    rng = PCG64(7)
    n = noise(rng, SIZE, 64, 4)
    n2 = noise(rng, SIZE, 40, 3)
    px = bytearray(SIZE * SIZE * 3)
    for y in range(SIZE):
        for x in range(SIZE):
            base = colourise(n[y][x], (6, 8, 30), (30, 26, 80))
            v2 = n2[y][x]
            neb = colourise(v2, (0, 0, 0), (90, 40, 120)) if v2 > 0.55 else (0, 0, 0)
            p = (y * SIZE + x) * 3
            for c in range(3):
                t = base[c] + neb[c]
                px[p + c] = 255 if t > 255 else t
    for _ in range(STARS):
        x = rng.integer(0, SIZE)
        y = rng.integer(0, SIZE)
        b = rng.integer(150, 256)
        col = (b, b, min(255, b + 20))
        dots = ((0, 0),)
        if rng.random() < 0.12:
            dots = ((0, -1), (-1, 0), (0, 0), (1, 0), (0, 1))
        for dx, dy in dots:
            if 0 <= x + dx < SIZE and 0 <= y + dy < SIZE:
                p = ((y + dy) * SIZE + x + dx) * 3
                px[p:p + 3] = bytes(col)
    return px
