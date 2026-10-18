#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BLOCK 32
#define UNUSABLE UINT_MAX

static unsigned read_word(void) {
    unsigned value = 0;
    for (unsigned i = 0; i < 4; i++) {
        int c = getchar();
        if (c < 0) exit(2);
        value |= (unsigned)c << (8 * i);
    }
    return value;
}

static void write_word(unsigned value) {
    for (unsigned i = 0; i < 4; i++) putchar((int)((value >> (8 * i)) & 255));
}

static int *read_coordinates(size_t count) {
    int *values = malloc(count * sizeof *values);
    if (!values) exit(3);
    for (size_t i = 0; i < count; i++) {
        int low = getchar(), high = getchar();
        if (low < 0 || high < 0) exit(2);
        values[i] = (int16_t)((unsigned)low | (unsigned)high << 8);
    }
    return values;
}

static void count_refills(unsigned points, const unsigned width[3], unsigned reads[3]) {
    unsigned available = 0;
    reads[0] = reads[1] = reads[2] = 0;
    for (unsigned i = 0; i < points; i++) {
        for (unsigned axis = 0; axis < 3; axis++) {
            if (available < width[axis]) {
                available += 32 - width[axis];
                reads[axis]++;
            } else {
                available -= width[axis];
            }
        }
    }
}

static unsigned encoded_size(const int *pose, const int *base, unsigned n, unsigned mode,
                             const unsigned *widths) {
    unsigned bytes = 8;
    for (unsigned first = 0; first < n; first += BLOCK) {
        unsigned count = n - first < BLOCK ? n - first : BLOCK;
        int delta[BLOCK][3], low[3] = { 32767, 32767, 32767 }, high[3] = { -32768, -32768, -32768 };
        for (unsigned i = 0; i < count; i++) {
            for (unsigned axis = 0; axis < 3; axis++) {
                size_t at = (size_t)(first + i) * 3 + axis;
                int d = (int16_t)(uint16_t)((unsigned)pose[at] - (unsigned)base[at]);
                delta[i][axis] = d;
                if (d < low[axis]) low[axis] = d;
                if (d > high[axis]) high[axis] = d;
            }
        }
        unsigned step = mode == 3 ? 3 : 2, scale = step, varies = 0;
        for (unsigned i = 0; i < count; i++) {
            for (unsigned axis = 0; axis < 3; axis++) {
                if ((delta[i][axis] - low[axis]) & ((1 << step) - 1)) scale = 0;
                if (delta[i][axis] != low[axis]) varies = 1;
            }
        }
        if (!varies) scale = 0;
        unsigned bits = 0, new_width[3], old_width[3], old_reads[3], new_reads[3];
        unsigned packed = widths[first / BLOCK];
        for (unsigned axis = 0; axis < 3; axis++) {
            unsigned width = 0, range = (unsigned)(high[axis] - low[axis]) >> scale;
            while (range) {
                width++;
                range >>= 1;
            }
            new_width[axis] = width;
            old_width[axis] = (packed >> (5 * axis)) & 31;
            bits += width;
            if ((old_width[axis] == 0 || old_width[axis] == 16) && width != old_width[axis]) return UNUSABLE;
        }
        count_refills(count, old_width, old_reads);
        count_refills(count, new_width, new_reads);
        for (unsigned axis = 0; axis < 3; axis++)
            if (new_reads[axis] > old_reads[axis]) return UNUSABLE;
        bytes += 8 + (count * bits + 7) / 8;
    }
    return (bytes + 3) & ~3u;
}

int main(void) {
    unsigned n = read_word(), rows = read_word(), anchors = read_word();
    if (!n || n > 224 || !rows || rows > 32767 || !anchors || anchors > 512) return 4;
    size_t coordinates = (size_t)n * 3;
    unsigned blocks = (n + BLOCK - 1) / BLOCK;
    int *poses = read_coordinates(coordinates * rows), *bases = read_coordinates(coordinates * anchors);
    unsigned *modes = malloc(rows * sizeof *modes), *widths = malloc((size_t)rows * blocks * sizeof *widths);
    if (!modes || !widths) return 3;
    for (unsigned r = 0; r < rows; r++) modes[r] = read_word();
    for (size_t i = 0; i < (size_t)rows * blocks; i++) widths[i] = read_word();
    if (getchar() != EOF) return 5;
    for (unsigned a = 0; a < anchors; a++) {
        for (unsigned r = 0; r < rows; r++) {
            if (modes[r] != 1 && modes[r] != 3) write_word(UNUSABLE);
            else write_word(encoded_size(poses + coordinates * r, bases + coordinates * a, n, modes[r],
                                         widths + (size_t)r * blocks));
        }
    }
    free(widths);
    free(modes);
    free(bases);
    free(poses);
    return ferror(stdout) ? 6 : 0;
}
