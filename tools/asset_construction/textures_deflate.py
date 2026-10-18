import zlib

L_CODES = 286
D_CODES = 30
BL_CODES = 19
HEAP_SIZE = 2 * L_CODES + 1
MIN_MATCH = 3
MAX_MATCH = 258
MIN_LOOKAHEAD = MAX_MATCH + MIN_MATCH + 1
EXTRA_L = [0] * 8 + [1] * 4 + [2] * 4 + [3] * 4 + [4] * 4 + [5] * 4 + [0]
EXTRA_D = [0, 0, 0, 0] + [k // 2 for k in range(2, 28)]
EXTRA_BL = [0] * 16 + [2, 3, 7]
BL_ORDER = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15]
SLOW_CONFIG = {6: (8, 16, 128, 128), 9: (32, 258, 258, 4096)}


def code_tables():
    length_code = [0] * 256
    base_length = [0] * 29
    n = 0
    for code in range(28):
        base_length[code] = n
        for _ in range(1 << EXTRA_L[code]):
            length_code[n] = code
            n += 1
    length_code[255] = 28
    dist_code = [0] * 512
    base_dist = [0] * 30
    d = 0
    for code in range(16):
        base_dist[code] = d
        for _ in range(1 << EXTRA_D[code]):
            dist_code[d] = code
            d += 1
    d >>= 7
    for code in range(16, 30):
        base_dist[code] = d << 7
        for _ in range(1 << (EXTRA_D[code] - 7)):
            dist_code[256 + d] = code
            d += 1
    return length_code, base_length, dist_code, base_dist


LENGTH_CODE, BASE_LENGTH, DIST_CODE, BASE_DIST = code_tables()


def dist_code(dist):
    if dist < 256:
        return DIST_CODE[dist]
    return DIST_CODE[256 + (dist >> 7)]


def bit_reverse(code, n):
    r = 0
    for _ in range(n):
        r = (r << 1) | (code & 1)
        code >>= 1
    return r


def gen_codes(lens, max_code, bl_count):
    next_code = [0] * 16
    code = 0
    for bits in range(1, 16):
        code = (code + bl_count[bits - 1]) << 1
        next_code[bits] = code
    codes = [0] * len(lens)
    for n in range(max_code + 1):
        ln = lens[n]
        if ln:
            codes[n] = bit_reverse(next_code[ln], ln)
            next_code[ln] += 1
    return codes


def static_trees():
    lens = [8] * 144 + [9] * 112 + [7] * 24 + [8] * 8
    count = [0] * 16
    for ln in lens:
        count[ln] += 1
    return lens, gen_codes(lens, 287, count), [5] * 30, [bit_reverse(n, 5) for n in range(30)]


STATIC_LLEN, STATIC_LCODE, STATIC_DLEN, STATIC_DCODE = static_trees()


def build_tree(freq, max_length, extra, extra_base, stat_len):
    elems = len(freq)
    freq = freq + [0] * (elems + 1)
    size = len(freq)
    depth = [0] * size
    dad = [0] * size
    ln = [0] * size
    heap = [0] * HEAP_SIZE
    heap_len = 0
    max_code = -1
    opt = 0
    stat = 0
    for n in range(elems):
        if freq[n]:
            heap_len += 1
            heap[heap_len] = max_code = n
    while heap_len < 2:
        if max_code < 2:
            max_code += 1
            node = max_code
        else:
            node = 0
        heap_len += 1
        heap[heap_len] = node
        freq[node] = 1
        depth[node] = 0
        opt -= 1
        if stat_len:
            stat -= stat_len[node]

    def down(k):
        v = heap[k]
        j = k << 1
        while j <= heap_len:
            if j < heap_len:
                a = heap[j + 1]
                b = heap[j]
                if freq[a] < freq[b] or (freq[a] == freq[b] and depth[a] <= depth[b]):
                    j += 1
            m = heap[j]
            if freq[v] < freq[m] or (freq[v] == freq[m] and depth[v] <= depth[m]):
                break
            heap[k] = m
            k = j
            j <<= 1
        heap[k] = v

    for k in range(heap_len // 2, 0, -1):
        down(k)
    node = elems
    heap_max = HEAP_SIZE
    while True:
        n = heap[1]
        heap[1] = heap[heap_len]
        heap_len -= 1
        down(1)
        m = heap[1]
        heap_max -= 2
        heap[heap_max + 1] = n
        heap[heap_max] = m
        freq[node] = freq[n] + freq[m]
        depth[node] = max(depth[n], depth[m]) + 1
        dad[n] = node
        dad[m] = node
        heap[1] = node
        node += 1
        down(1)
        if heap_len < 2:
            break
    heap_max -= 1
    heap[heap_max] = heap[1]
    bl_count = [0] * 16
    ln[heap[heap_max]] = 0
    overflow = 0
    for h in range(heap_max + 1, HEAP_SIZE):
        n = heap[h]
        bits = ln[dad[n]] + 1
        if bits > max_length:
            bits = max_length
            overflow += 1
        ln[n] = bits
        if n > max_code:
            continue
        bl_count[bits] += 1
        xbits = extra[n - extra_base] if n >= extra_base else 0
        opt += freq[n] * (bits + xbits)
        if stat_len:
            stat += freq[n] * (stat_len[n] + xbits)
    if overflow:
        while overflow > 0:
            bits = max_length - 1
            while bl_count[bits] == 0:
                bits -= 1
            bl_count[bits] -= 1
            bl_count[bits + 1] += 2
            bl_count[max_length] -= 1
            overflow -= 2
        h = HEAP_SIZE
        for bits in range(max_length, 0, -1):
            n = bl_count[bits]
            while n:
                h -= 1
                m = heap[h]
                if m > max_code:
                    continue
                if ln[m] != bits:
                    opt += (bits - ln[m]) * freq[m]
                    ln[m] = bits
                n -= 1
    lens = ln[:elems]
    return max_code, lens, gen_codes(lens, max_code, bl_count), opt, stat


def length_runs(lens, max_code):
    out = []
    prevlen = -1
    nextlen = lens[0]
    count = 0
    if nextlen == 0:
        max_count, min_count = 138, 3
    else:
        max_count, min_count = 7, 4
    for n in range(max_code + 1):
        curlen = nextlen
        nextlen = lens[n + 1] if n < max_code else -1
        count += 1
        if count < max_count and curlen == nextlen:
            continue
        if count < min_count:
            out += [(curlen, 0)] * count
        elif curlen != 0:
            if curlen != prevlen:
                out.append((curlen, 0))
                count -= 1
            out.append((16, count - 3))
        elif count <= 10:
            out.append((17, count - 3))
        else:
            out.append((18, count - 11))
        count = 0
        prevlen = curlen
        if nextlen == 0:
            max_count, min_count = 138, 3
        elif curlen == nextlen:
            max_count, min_count = 6, 3
        else:
            max_count, min_count = 7, 4
    return out


class Trees:
    def __init__(self):
        self.out = bytearray()
        self.buf = 0
        self.nbits = 0
        self.reset()

    def reset(self):
        self.lfreq = [0] * L_CODES
        self.dfreq = [0] * D_CODES
        self.lfreq[256] = 1
        self.syms = []

    def put(self, value, n):
        self.buf |= value << self.nbits
        self.nbits += n
        while self.nbits >= 8:
            self.out.append(self.buf & 255)
            self.buf >>= 8
            self.nbits -= 8

    def windup(self):
        if self.nbits:
            self.out.append(self.buf & 255)
        self.buf = 0
        self.nbits = 0

    def lit(self, c):
        self.syms.append((0, c))
        self.lfreq[c] += 1

    def match(self, dist, lc):
        self.syms.append((dist, lc))
        self.lfreq[LENGTH_CODE[lc] + 257] += 1
        self.dfreq[dist_code(dist - 1)] += 1

    def emit(self, llen, lcode, dlen, dcode):
        put = self.put
        for dist, lc in self.syms:
            if dist == 0:
                put(lcode[lc], llen[lc])
                continue
            code = LENGTH_CODE[lc]
            put(lcode[code + 257], llen[code + 257])
            if EXTRA_L[code]:
                put(lc - BASE_LENGTH[code], EXTRA_L[code])
            dist -= 1
            code = dist_code(dist)
            put(dcode[code], dlen[code])
            if EXTRA_D[code]:
                put(dist - BASE_DIST[code], EXTRA_D[code])
        put(lcode[256], llen[256])

    def flush(self, buf, last):
        lmax, llen, lcode, lopt, lstat = build_tree(self.lfreq, 15, EXTRA_L, 257, STATIC_LLEN)
        dmax, dlen, dcode, dopt, dstat = build_tree(self.dfreq, 15, EXTRA_D, 0, STATIC_DLEN)
        runs = length_runs(llen, lmax) + length_runs(dlen, dmax)
        bfreq = [0] * BL_CODES
        for sym, _ in runs:
            bfreq[sym] += 1
        _, blen, bcode, bopt, _ = build_tree(bfreq, 7, EXTRA_BL, 0, None)
        max_bl = BL_CODES - 1
        while max_bl >= 3 and blen[BL_ORDER[max_bl]] == 0:
            max_bl -= 1
        opt = lopt + dopt + bopt + 3 * (max_bl + 1) + 14
        opt_lenb = (opt + 10) >> 3
        static_lenb = (lstat + dstat + 10) >> 3
        if static_lenb <= opt_lenb:
            opt_lenb = static_lenb
        if buf is not None and len(buf) + 4 <= opt_lenb:
            n = len(buf)
            self.put(last, 3)
            self.windup()
            self.out += bytes((n & 255, n >> 8, ~n & 255, (~n >> 8) & 255))
            self.out += buf
        elif static_lenb == opt_lenb:
            self.put(2 + last, 3)
            self.emit(STATIC_LLEN, STATIC_LCODE, STATIC_DLEN, STATIC_DCODE)
        else:
            self.put(4 + last, 3)
            self.put(lmax - 256, 5)
            self.put(dmax, 5)
            self.put(max_bl - 3, 4)
            for k in range(max_bl + 1):
                self.put(blen[BL_ORDER[k]], 3)
            for sym, rep in runs:
                self.put(bcode[sym], blen[sym])
                if sym >= 16:
                    self.put(rep, EXTRA_BL[sym])
            self.emit(llen, lcode, dlen, dcode)
        self.reset()
        if last:
            self.windup()


def zlib_wrap(level_flags, wbits, body, adler):
    head = ((8 + ((wbits - 8) << 4)) << 8) | (level_flags << 6)
    head += 31 - head % 31
    return bytes((head >> 8, head & 255)) + bytes(body) + (adler & 0xFFFFFFFF).to_bytes(4, "big")


def longest_match(win, prev, w_mask, s, cur, best, chain, nice, limit, lookahead, match_start):
    strend = s + MAX_MATCH
    while True:
        if (win[cur + best] == win[s + best] and win[cur + best - 1] == win[s + best - 1]
                and win[cur] == win[s] and win[cur + 1] == win[s + 1]):
            a = s + 3
            b = cur + 3
            while a < strend and win[a] == win[b]:
                a += 1
                b += 1
            ln = a - s
            if ln > best:
                match_start = cur
                best = ln
                if ln >= nice:
                    break
        cur = prev[cur & w_mask]
        if cur <= limit:
            break
        chain -= 1
        if chain == 0:
            break
    if best > lookahead:
        best = lookahead
    return best, match_start


def deflate_slow(data, level, wbits, filtered=False):
    good, lazy, nice_cfg, max_chain = SLOW_CONFIG[level]
    w_size = 1 << wbits
    w_mask = w_size - 1
    if len(data) > 2 * w_size - MIN_LOOKAHEAD:
        raise ValueError("input too large")
    hash_bits = 15
    hash_mask = (1 << hash_bits) - 1
    shift = (hash_bits + MIN_MATCH - 1) // MIN_MATCH
    sym_end = (1 << 14) - 1
    max_dist = w_size - MIN_LOOKAHEAD
    win = bytes(data) + bytes(MAX_MATCH + 8)
    prev = [0] * w_size
    head = [0] * (1 << hash_bits)
    trees = Trees()
    lookahead = len(data)
    ins_h = 0
    if lookahead >= MIN_MATCH:
        ins_h = ((win[0] << shift) ^ win[1]) & hash_mask
    strstart = 0
    block_start = 0
    match_length = MIN_MATCH - 1
    match_start = 0
    match_available = False

    def insert(s):
        nonlocal ins_h
        ins_h = ((ins_h << shift) ^ win[s + 2]) & hash_mask
        h = head[ins_h]
        prev[s & w_mask] = h
        head[ins_h] = s
        return h

    while lookahead:
        hash_head = insert(strstart) if lookahead >= MIN_MATCH else 0
        prev_length = match_length
        prev_match = match_start
        match_length = MIN_MATCH - 1
        if hash_head and prev_length < lazy and strstart - hash_head <= max_dist:
            chain = max_chain >> 2 if prev_length >= good else max_chain
            nice = nice_cfg if nice_cfg <= lookahead else lookahead
            limit = strstart - max_dist if strstart > max_dist else 0
            match_length, match_start = longest_match(win, prev, w_mask, strstart, hash_head, prev_length,
                                                      chain, nice, limit, lookahead, match_start)
            if match_length <= 5:
                if filtered or (match_length == MIN_MATCH and strstart - match_start > 4096):
                    match_length = MIN_MATCH - 1
        if prev_length >= MIN_MATCH and match_length <= prev_length:
            max_insert = strstart + lookahead - MIN_MATCH
            trees.match(strstart - 1 - prev_match, prev_length - MIN_MATCH)
            bflush = len(trees.syms) == sym_end
            lookahead -= prev_length - 1
            for _ in range(prev_length - 2):
                strstart += 1
                if strstart <= max_insert:
                    insert(strstart)
            match_available = False
            match_length = MIN_MATCH - 1
            strstart += 1
            if bflush:
                trees.flush(win[block_start:strstart], 0)
                block_start = strstart
        elif match_available:
            trees.lit(win[strstart - 1])
            if len(trees.syms) == sym_end:
                trees.flush(win[block_start:strstart], 0)
                block_start = strstart
            strstart += 1
            lookahead -= 1
        else:
            match_available = True
            strstart += 1
            lookahead -= 1
    if match_available:
        trees.lit(win[strstart - 1])
    trees.flush(win[block_start:strstart], 1)
    return zlib_wrap(3 if level > 6 else 2, wbits, trees.out, zlib.adler32(data))


def deflate_fast(data, wbits):
    w_size = 1 << wbits
    w_mask = w_size - 1
    window_size = 2 * w_size
    hash_bits = 15
    hash_size = 1 << hash_bits
    hash_mask = hash_size - 1
    hash_shift = (hash_bits + MIN_MATCH - 1) // MIN_MATCH
    sym_end = (1 << 14) - 1
    max_dist = w_size - MIN_LOOKAHEAD
    window = bytearray(window_size)
    prev = [0] * w_size
    head = [0] * hash_size
    trees = Trees()
    src_pos = 0
    src_len = len(data)
    strstart = 0
    block_start = 0
    lookahead = 0
    insert = 0
    ins_h = 0
    match_start = 0
    high_water = 0

    def flush(last):
        nonlocal block_start
        trees.flush(bytes(window[block_start:strstart]) if block_start >= 0 else None, last)
        block_start = strstart

    while True:
        if lookahead < MIN_LOOKAHEAD:
            while True:
                more = window_size - lookahead - strstart
                if strstart >= w_size + max_dist:
                    window[0:w_size - more] = window[w_size:window_size - more]
                    match_start -= w_size
                    strstart -= w_size
                    block_start -= w_size
                    if insert > strstart:
                        insert = strstart
                    head = [m - w_size if m >= w_size else 0 for m in head]
                    prev = [m - w_size if m >= w_size else 0 for m in prev]
                    more += w_size
                if src_pos >= src_len:
                    break
                n = src_len - src_pos
                if n > more:
                    n = more
                at = strstart + lookahead
                window[at:at + n] = data[src_pos:src_pos + n]
                src_pos += n
                lookahead += n
                if lookahead + insert >= MIN_MATCH:
                    s = strstart - insert
                    ins_h = window[s]
                    ins_h = ((ins_h << hash_shift) ^ window[s + 1]) & hash_mask
                    while insert:
                        ins_h = ((ins_h << hash_shift) ^ window[s + MIN_MATCH - 1]) & hash_mask
                        prev[s & w_mask] = head[ins_h]
                        head[ins_h] = s
                        s += 1
                        insert -= 1
                        if lookahead + insert < MIN_MATCH:
                            break
                if not (lookahead < MIN_LOOKAHEAD and src_pos < src_len):
                    break
            if high_water < window_size:
                curr = strstart + lookahead
                if high_water < curr:
                    init = min(window_size - curr, MAX_MATCH)
                    window[curr:curr + init] = bytes(init)
                    high_water = curr + init
                elif high_water < curr + MAX_MATCH:
                    init = min(curr + MAX_MATCH - high_water, window_size - high_water)
                    window[high_water:high_water + init] = bytes(init)
                    high_water += init
            if lookahead == 0:
                break
        hash_head = 0
        if lookahead >= MIN_MATCH:
            ins_h = ((ins_h << hash_shift) ^ window[strstart + MIN_MATCH - 1]) & hash_mask
            hash_head = head[ins_h]
            prev[strstart & w_mask] = hash_head
            head[ins_h] = strstart
        match_length = 0
        if hash_head != 0 and strstart - hash_head <= max_dist:
            nice = 8 if 8 <= lookahead else lookahead
            limit = strstart - max_dist if strstart > max_dist else 0
            match_length, match_start = longest_match(window, prev, w_mask, strstart, hash_head, 2, 4, nice,
                                                      limit, lookahead, match_start)
        if match_length >= MIN_MATCH:
            trees.match(strstart - match_start, match_length - MIN_MATCH)
            bflush = len(trees.syms) == sym_end
            lookahead -= match_length
            if match_length <= 4 and lookahead >= MIN_MATCH:
                match_length -= 1
                while True:
                    strstart += 1
                    ins_h = ((ins_h << hash_shift) ^ window[strstart + MIN_MATCH - 1]) & hash_mask
                    prev[strstart & w_mask] = head[ins_h]
                    head[ins_h] = strstart
                    match_length -= 1
                    if match_length == 0:
                        break
                strstart += 1
            else:
                strstart += match_length
                ins_h = window[strstart]
                ins_h = ((ins_h << hash_shift) ^ window[strstart + 1]) & hash_mask
        else:
            trees.lit(window[strstart])
            bflush = len(trees.syms) == sym_end
            lookahead -= 1
            strstart += 1
        if bflush:
            flush(0)
    flush(1)
    return zlib_wrap(0, wbits, trees.out, zlib.adler32(data))


class MediumDeflate:
    def __init__(self):
        self.w_size = 1 << 15
        self.w_mask = self.w_size - 1
        self.window_size = 2 * self.w_size
        self.max_dist = self.w_size - MIN_LOOKAHEAD
        self.win = bytearray(self.window_size + 4096)
        self.head = [0] * 65536
        self.prev = [0] * self.w_size
        self.sym_end = (1 << 15) - 1
        self.trees = Trees()
        self.strstart = 0
        self.block_start = 0
        self.lookahead = 0
        self.insert = 0
        self.match_start = 0
        self.high_water = 0
        self.inp = b""
        self.inpos = 0
        self.adler = 1

    def hash_at(self, p):
        w = self.win
        v = w[p] | (w[p + 1] << 8) | (w[p + 2] << 16) | (w[p + 3] << 24)
        return ((v * 2654435761) & 0xFFFFFFFF) >> 16

    def quick_insert(self, s):
        h = self.hash_at(s)
        hd = self.head[h]
        if hd != s:
            self.prev[s & self.w_mask] = hd
            self.head[h] = s
        return hd

    def insert_string(self, s, count):
        for idx in range(s, s + count):
            self.quick_insert(idx & 0xFFFF)

    def fill_window(self):
        ws = self.w_size
        while True:
            more = self.window_size - self.lookahead - self.strstart
            if self.strstart >= ws + self.max_dist:
                self.win[0:ws] = self.win[ws:2 * ws]
                if self.match_start >= ws:
                    self.match_start -= ws
                else:
                    self.match_start = 0
                self.strstart -= ws
                self.block_start -= ws
                if self.insert > self.strstart:
                    self.insert = self.strstart
                self.head = [m - ws if m >= ws else 0 for m in self.head]
                self.prev = [m - ws if m >= ws else 0 for m in self.prev]
                more += ws
            avail = len(self.inp) - self.inpos
            if avail == 0:
                break
            n = avail if avail < more else more
            at = self.strstart + self.lookahead
            self.win[at:at + n] = self.inp[self.inpos:self.inpos + n]
            self.inpos += n
            self.lookahead += n
            if self.lookahead + self.insert >= MIN_MATCH:
                s = self.strstart - self.insert
                if s >= 1:
                    self.quick_insert(s - 1)
                count = self.insert
                if self.lookahead == 1:
                    count -= 1
                if count > 0:
                    self.insert_string(s, count)
                    self.insert -= count
            if not (self.lookahead < MIN_LOOKAHEAD and self.inpos < len(self.inp)):
                break
        if self.high_water < self.window_size:
            curr = self.strstart + self.lookahead
            if self.high_water < curr:
                init = min(self.window_size - curr, MAX_MATCH)
                self.win[curr:curr + init] = bytes(init)
                self.high_water = curr + init
            elif self.high_water < curr + MAX_MATCH:
                init = min(curr + MAX_MATCH - self.high_water, self.window_size - self.high_water)
                self.win[self.high_water:self.high_water + init] = bytes(init)
                self.high_water += init

    def compare256(self, a, b):
        w = self.win
        if w[a:a + 256] == w[b:b + 256]:
            return 256
        n = 0
        while w[a + n] == w[b + n]:
            n += 1
        return n

    def longest_match(self, cur):
        w = self.win
        s = self.strstart
        prev = self.prev
        wmask = self.w_mask
        lookahead = self.lookahead
        best = 2
        offset = 1
        width = 2
        chain = 128
        limit = s - self.max_dist if s > self.max_dist else 0
        while True:
            if cur >= s:
                return best
            while not (w[cur + offset:cur + offset + width] == w[s + offset:s + offset + width]
                       and w[cur:cur + width] == w[s:s + width]):
                chain -= 1
                if chain == 0:
                    return best
                cur = prev[cur & wmask]
                if cur <= limit:
                    return best
            ln = self.compare256(s + 2, cur + 2) + 2
            if ln > best:
                self.match_start = cur
                if ln >= lookahead:
                    return lookahead
                best = ln
                if best >= 128:
                    return best
                offset = best - 1
                width = 2
                if best >= 4:
                    offset -= 2
                    width = 4
                    if best >= 8:
                        offset -= 4
                        width = 8
            chain -= 1
            if chain == 0:
                return best
            cur = prev[cur & wmask]
            if cur <= limit:
                return best

    def find(self):
        s = self.strstart
        hash_head = self.quick_insert(s)
        dist = s - hash_head
        if 0 < dist <= self.max_dist and hash_head != 0:
            ln = self.longest_match(hash_head) & 0xFFFF
            ms = self.match_start & 0xFFFF
            if ln < 4 or ms >= s:
                ln = 1
            return [ms, ln, s, s]
        return [0, 1, s, s]

    def insert_match(self, m):
        ln = m[1]
        ss = m[2]
        org = m[3]
        if self.lookahead <= ln + 4:
            return
        if ln < 4:
            ss += 1
            ln -= 1
            if ln > 0 and ss >= org:
                if ss + ln - 1 >= org:
                    self.insert_string(ss, ln)
                else:
                    self.insert_string(ss, org - ss + 1)
            return
        if ln <= 256 and self.lookahead >= 4:
            ln -= 1
            ss += 1
            if ss >= org:
                if ss + ln - 1 >= org:
                    self.insert_string(ss, ln)
                else:
                    self.insert_string(ss, org - ss + 1)
            elif org < ss + ln:
                self.insert_string(org, ss + ln - org)
        else:
            ss += ln
            if ss >= 1:
                self.quick_insert(ss - 1)

    def fizzle(self, c, n):
        w = self.win
        if c[1] <= 1 or c[1] > 1 + n[0] or c[1] > 1 + n[2]:
            return
        if w[n[0] - c[1] + 1] != w[n[2] - c[1] + 1]:
            return
        cc = list(c)
        nn = list(n)
        limit = n[2] - self.max_dist if n[2] > self.max_dist else 0
        mp = nn[0] - 1
        op = nn[2] - 1
        changed = 0
        while w[mp] == w[op]:
            if cc[1] < 1 or nn[2] <= limit or nn[1] >= 256 or nn[0] <= 1:
                break
            nn[2] -= 1
            nn[0] -= 1
            nn[1] += 1
            cc[1] -= 1
            mp -= 1
            op -= 1
            changed += 1
        if changed and cc[1] <= 1 and nn[1] != 2:
            nn[3] += 1
            c[:] = cc
            n[:] = nn

    def emit_match(self, m):
        ms, ln, ss, _ = m
        t = self.trees
        if ln < 4:
            for k in range(ln):
                t.lit(self.win[ss + k])
            self.lookahead -= ln
            return ln > 0 and len(t.syms) >= self.sym_end
        t.match(ss - ms, ln - MIN_MATCH)
        self.lookahead -= ln
        return len(t.syms) == self.sym_end

    def flush_block(self, last):
        bs = self.block_start
        buf = bytes(self.win[bs:self.strstart]) if bs >= 0 else None
        self.trees.flush(buf, last)
        self.block_start = self.strstart

    def run(self, finish):
        nxt = [0, 0, 0, 0]
        while True:
            if self.lookahead < MIN_LOOKAHEAD:
                self.fill_window()
                if self.lookahead < MIN_LOOKAHEAD and not finish:
                    return
                if self.lookahead == 0:
                    break
                nxt[1] = 0
            if nxt[1] > 0:
                cur = list(nxt)
                nxt[1] = 0
            elif self.lookahead >= 4:
                cur = self.find()
            else:
                cur = [0, 1, self.strstart, self.strstart]
            self.insert_match(cur)
            if self.lookahead > MIN_LOOKAHEAD and cur[2] + cur[1] < self.window_size - MIN_LOOKAHEAD:
                self.strstart = cur[2] + cur[1]
                nxt = self.find()
                if nxt[1] >= 4:
                    self.fizzle(cur, nxt)
                self.strstart = cur[2]
            else:
                nxt = [0, 0, 0, 0]
            bflush = self.emit_match(cur)
            self.strstart += cur[1]
            if bflush:
                self.flush_block(0)
        self.insert = self.strstart if self.strstart < 2 else 2
        self.flush_block(1)

    def compress(self, chunk):
        self.inp = chunk
        self.inpos = 0
        self.adler = zlib.adler32(chunk, self.adler)
        if chunk or self.lookahead:
            self.run(False)


def deflate_medium(chunks):
    z = MediumDeflate()
    for chunk in chunks:
        z.compress(chunk)
    z.inp = b""
    z.inpos = 0
    z.run(True)
    return zlib_wrap(2, 15, z.trees.out, z.adler)
