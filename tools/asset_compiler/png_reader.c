#include "png_reader.h"
#include "common.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t *src;
    size_t len, pos;
    uint32_t bits;
    int nbits;
    ByteBuffer out;
} Inflate;

static int inflate_read_bit(Inflate *z) {
    if (!z->nbits) {
        if (z->pos >= z->len) die("png: truncated deflate stream");
        z->bits = z->src[z->pos++];
        z->nbits = 8;
    }
    int b = z->bits & 1;
    z->bits >>= 1;
    z->nbits--;
    return b;
}

static uint32_t inflate_read_bits(Inflate *z, int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; i++) v |= (uint32_t)inflate_read_bit(z) << i;
    return v;
}

typedef struct {
    uint16_t count[16];
    uint16_t symbol[320];
} Huffman;

static void huffman_build(Huffman *h, const uint8_t *lengths, int n) {
    uint16_t offs[16];
    memset(h->count, 0, sizeof h->count);
    for (int i = 0; i < n; i++) h->count[lengths[i]]++;
    h->count[0] = 0;
    offs[1] = 0;
    for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + h->count[i];
    for (int i = 0; i < n; i++)
        if (lengths[i]) h->symbol[offs[lengths[i]]++] = (uint16_t)i;
}

static int huffman_decode(Inflate *z, const Huffman *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= inflate_read_bit(z);
        int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    die("png: bad huffman code");
}

static const uint16_t len_base[] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                     35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t len_extra[] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                     3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t dist_base[] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                                      193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                                      6145, 8193, 12289, 16385, 24577 };
static const uint8_t dist_extra[] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                      6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static void inflate_block(Inflate *z, const Huffman *lit, const Huffman *dist) {
    for (;;) {
        int sym = huffman_decode(z, lit);
        if (sym < 256) {
            uint8_t c = (uint8_t)sym;
            buffer_append(&z->out, &c, 1);
        } else if (sym == 256) {
            return;
        } else {
            sym -= 257;
            if (sym >= 29) die("png: bad length symbol");
            size_t len = len_base[sym] + inflate_read_bits(z, len_extra[sym]);
            int ds = huffman_decode(z, dist);
            if (ds >= 30) die("png: bad distance symbol");
            size_t d = dist_base[ds] + inflate_read_bits(z, dist_extra[ds]);
            if (d > z->out.len) die("png: distance too far back");
            for (size_t i = 0; i < len; i++) {
                uint8_t c = z->out.data[z->out.len - d];
                buffer_append(&z->out, &c, 1);
            }
        }
    }
}

static ByteBuffer inflate_zlib(const uint8_t *src, size_t len) {
    Inflate z = { .src = src, .len = len };
    if (len < 2 || (src[0] & 15) != 8) die("png: unsupported zlib stream");
    z.pos = 2;
    int last;
    do {
        last = inflate_read_bit(&z);
        int type = (int)inflate_read_bits(&z, 2);
        if (type == 0) {
            z.nbits = 0;
            if (z.pos + 4 > z.len) die("png: truncated stored block");
            size_t n = z.src[z.pos] | z.src[z.pos + 1] << 8;
            z.pos += 4;
            if (z.pos + n > z.len) die("png: truncated stored block");
            buffer_append(&z.out, z.src + z.pos, n);
            z.pos += n;
        } else if (type == 1) {
            uint8_t lengths[320];
            int i = 0;
            for (; i < 144; i++) lengths[i] = 8;
            for (; i < 256; i++) lengths[i] = 9;
            for (; i < 280; i++) lengths[i] = 7;
            for (; i < 288; i++) lengths[i] = 8;
            Huffman lit, dist;
            huffman_build(&lit, lengths, 288);
            for (i = 0; i < 30; i++) lengths[i] = 5;
            huffman_build(&dist, lengths, 30);
            inflate_block(&z, &lit, &dist);
        } else if (type == 2) {
            static const uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
                                               11, 4, 12, 3, 13, 2, 14, 1, 15 };
            int hlit = (int)inflate_read_bits(&z, 5) + 257;
            int hdist = (int)inflate_read_bits(&z, 5) + 1;
            int hclen = (int)inflate_read_bits(&z, 4) + 4;
            uint8_t lengths[320] = { 0 };
            for (int i = 0; i < hclen; i++) lengths[order[i]] = (uint8_t)inflate_read_bits(&z, 3);
            Huffman clen;
            huffman_build(&clen, lengths, 19);
            memset(lengths, 0, sizeof lengths);
            for (int i = 0; i < hlit + hdist;) {
                int sym = huffman_decode(&z, &clen);
                if (sym < 16) {
                    lengths[i++] = (uint8_t)sym;
                } else {
                    int rep, val = 0;
                    if (sym == 16) {
                        if (!i) die("png: repeat with no previous length");
                        val = lengths[i - 1];
                        rep = 3 + (int)inflate_read_bits(&z, 2);
                    } else if (sym == 17) {
                        rep = 3 + (int)inflate_read_bits(&z, 3);
                    } else {
                        rep = 11 + (int)inflate_read_bits(&z, 7);
                    }
                    if (i + rep > hlit + hdist) die("png: too many code lengths");
                    while (rep--) lengths[i++] = (uint8_t)val;
                }
            }
            Huffman lit, dist;
            huffman_build(&lit, lengths, hlit);
            huffman_build(&dist, lengths + hlit, hdist);
            inflate_block(&z, &lit, &dist);
        } else {
            die("png: bad block type");
        }
    } while (!last);
    return z.out;
}

static uint32_t read_big_endian_u32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static int paeth_predictor(int a, int b, int c) {
    int p = a + b - c;
    int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

Image png_load(const char *path) {
    size_t len;
    uint8_t *file = read_file(path, &len);
    if (!file) die("cannot open %s", path);
    static const uint8_t sig[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
    if (len < 8 || memcmp(file, sig, 8)) die("%s: not a PNG", path);

    int w = 0, h = 0, depth = 0, ctype = 0, interlace = 0;
    uint8_t plte[256][4];
    memset(plte, 255, sizeof plte);
    ByteBuffer idat = { 0 };

    for (size_t pos = 8; pos + 8 <= len;) {
        uint32_t n = read_big_endian_u32(file + pos);
        const uint8_t *type = file + pos + 4, *data = file + pos + 8;
        if (pos + 12 + n > len) die("%s: truncated chunk", path);
        if (!memcmp(type, "IHDR", 4)) {
            w = (int)read_big_endian_u32(data);
            h = (int)read_big_endian_u32(data + 4);
            depth = data[8];
            ctype = data[9];
            interlace = data[12];
        } else if (!memcmp(type, "PLTE", 4)) {
            for (uint32_t i = 0; i < n / 3 && i < 256; i++) {
                plte[i][0] = data[i * 3];
                plte[i][1] = data[i * 3 + 1];
                plte[i][2] = data[i * 3 + 2];
            }
        } else if (!memcmp(type, "tRNS", 4) && ctype == 3) {
            for (uint32_t i = 0; i < n && i < 256; i++) plte[i][3] = data[i];
        } else if (!memcmp(type, "IDAT", 4)) {
            buffer_append(&idat, data, n);
        } else if (!memcmp(type, "IEND", 4)) {
            break;
        }
        pos += 12 + n;
    }
    if (depth != 8 || interlace) die("%s: only 8-bit non-interlaced PNGs are supported", path);

    int channels;
    switch (ctype) {
    case 0: channels = 1; break;
    case 2: channels = 3; break;
    case 3: channels = 1; break;
    case 4: channels = 2; break;
    case 6: channels = 4; break;
    default: die("%s: bad colour type %d", path, ctype);
    }

    ByteBuffer raw = inflate_zlib(idat.data, idat.len);
    size_t stride = (size_t)w * channels;
    if (raw.len < (stride + 1) * h) die("%s: not enough image data", path);

    uint8_t *px = xcalloc((size_t)h, stride);
    for (int y = 0; y < h; y++) {
        const uint8_t *in = raw.data + y * (stride + 1);
        uint8_t *cur = px + y * stride, *prev = y ? cur - stride : NULL;
        int filter = in[0];
        in++;
        for (size_t i = 0; i < stride; i++) {
            int a = i >= (size_t)channels ? cur[i - channels] : 0;
            int b = prev ? prev[i] : 0;
            int c = prev && i >= (size_t)channels ? prev[i - channels] : 0;
            int v = in[i];
            switch (filter) {
            case 0: break;
            case 1: v += a; break;
            case 2: v += b; break;
            case 3: v += (a + b) >> 1; break;
            case 4: v += paeth_predictor(a, b, c); break;
            default: die("%s: bad filter %d", path, filter);
            }
            cur[i] = (uint8_t)v;
        }
    }

    Image img = { w, h, xmalloc((size_t)w * h * 4) };
    for (int i = 0; i < w * h; i++) {
        const uint8_t *s = px + (size_t)i * channels;
        uint8_t *d = img.rgba + (size_t)i * 4;
        switch (ctype) {
        case 0: d[0] = d[1] = d[2] = s[0]; d[3] = 255; break;
        case 2: d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255; break;
        case 3: memcpy(d, plte[s[0]], 4); break;
        case 4: d[0] = d[1] = d[2] = s[0]; d[3] = s[1]; break;
        case 6: memcpy(d, s, 4); break;
        }
    }
    free(px);
    free(raw.data);
    free(idat.data);
    free(file);
    return img;
}

void image_free(Image *img) {
    free(img->rgba);
    img->rgba = NULL;
}
