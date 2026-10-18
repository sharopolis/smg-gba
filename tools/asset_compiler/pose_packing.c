#include "pose_packing.h"

#include <stdio.h>
#include <string.h>

#define POSE_GRID_SHIFT 3

static void append_bit(ByteBuffer *b, unsigned *bit, unsigned value) {
    if (!*bit) buffer_append(b, "", 1);
    if (value) b->data[b->len - 1] |= (uint8_t)(1u << *bit);
    *bit = (*bit + 1) & 7u;
}

static ByteBuffer pack_delta_blocks(const int16_t *pose, const int16_t *bind, unsigned points) {
    ByteBuffer b = { 0 };
    for (unsigned first = 0; first < points; first += FIGURE_POSE_BLOCK) {
        unsigned n = points - first;
        if (n > FIGURE_POSE_BLOCK) n = FIGURE_POSE_BLOCK;
        int16_t delta[FIGURE_POSE_BLOCK][3];
        int low[3] = { 32767, 32767, 32767 }, high[3] = { -32768, -32768, -32768 };
        unsigned width[3] = { 0 };
        for (unsigned i = 0; i < n; i++) {
            for (unsigned a = 0; a < 3; a++) {
                int d = (int16_t)((uint16_t)pose[(first + i) * 3 + a] - (uint16_t)bind[(first + i) * 3 + a]);
                delta[i][a] = (int16_t)d;
                if (d < low[a]) low[a] = d;
                if (d > high[a]) high[a] = d;
            }
        }
        unsigned scale = POSE_GRID_SHIFT, varies = 0;
        for (unsigned i = 0; i < n; i++) {
            for (unsigned a = 0; a < 3; a++) {
                if ((delta[i][a] - low[a]) & ((1 << POSE_GRID_SHIFT) - 1)) scale = 0;
                if (delta[i][a] != low[a]) varies = 1;
            }
        }
        if (!varies) scale = 0;
        for (unsigned a = 0; a < 3; a++) {
            unsigned range = (unsigned)(high[a] - low[a]) >> scale;
            while (range) {
                width[a]++;
                range >>= 1;
            }
        }
        unsigned flags = width[0] | width[1] << 5 | width[2] << 10 | (scale ? FIGURE_POSE_SCALE4 : 0);
        int16_t header[4] = { (int16_t)low[0], (int16_t)low[1], (int16_t)low[2], (int16_t)flags };
        buffer_append(&b, header, sizeof header);
        unsigned bit = 0;
        for (unsigned i = 0; i < n; i++) {
            for (unsigned a = 0; a < 3; a++) {
                unsigned value = (unsigned)(delta[i][a] - low[a]) >> scale;
                for (unsigned j = 0; j < width[a]; j++) append_bit(&b, &bit, (value >> j) & 1u);
            }
        }
    }
    return b;
}

static int16_t round_to_grid(int x) {
    int grid = 1 << POSE_GRID_SHIFT, half = grid / 2;
    int r = x >= 0 ? (x + half) / grid * grid : -((-x + half - 1) / grid * grid);
    return (int16_t)(r > 32768 - grid ? 32768 - grid : r < -32768 ? -32768 : r);
}

void quantize_pose_values(int16_t *values, size_t count, unsigned points) {
    if (points > FIGURE_PACKED_MAX_POINTS) return;
    for (size_t i = 0; i < count; i++) values[i] = round_to_grid(values[i]);
}

void hold_one_tick_keys(ByteBuffer *keys, unsigned points, const AnimClip *clips, unsigned clip_count,
                        const char *name) {
    if (points > FIGURE_PACKED_MAX_POINTS) return;
    int16_t *values = (int16_t *)(void *)keys->data;
    size_t pose_values = (size_t)points * 3, held = 0;
    quantize_pose_values(values, keys->len / 2, points);
    if (!strncmp(name, "mario", 5) || !strcmp(name, "ghostmario")) return;
    for (unsigned c = 0; c < clip_count; c++) {
        if (clips[c].ticks_per_key != 1 || clips[c].key_count < 3) continue;
        for (unsigned f = 1; f + 1 < clips[c].key_count; f += 2) {
            int16_t *key = values + (size_t)(clips[c].first_key + f) * pose_values;
            memcpy(key, key - pose_values, pose_values * 2);
            held++;
        }
    }
    if (held) printf("  %s: %zu one-tick keys held\n", name, held);
}

uint32_t append_figure_keys(ByteBuffer *out, FigureHeader *h, const ByteBuffer *keys, const int16_t *bind,
                            const char *name) {
    if (h->keyframe_count > FIGURE_KEY_COUNT) die("%s: too many pose keys", name);
    if ((!strcmp(name, "mario") || !strcmp(name, "mariobee")) && h->point_count > 160)
        die("%s: Mario blending holds at most 160 points", name);
    if (h->point_count > FIGURE_PACKED_MAX_POINTS) return buffer_append_aligned(out, keys->data, keys->len);

    ByteBuffer packed = { 0 };
    buffer_append(&packed, NULL, 4u * h->keyframe_count);
    uint32_t *positions = xcalloc(h->keyframe_count, sizeof(uint32_t));
    uint32_t *hashes = xcalloc(h->keyframe_count, sizeof(uint32_t));
    unsigned delta_keys = 0, repeated_keys = 0;
    size_t pose_bytes = h->point_count * 6u;
    for (unsigned k = 0; k < h->keyframe_count; k++) {
        const int16_t *pose = (const int16_t *)(const void *)(keys->data + k * pose_bytes);
        const uint8_t *bytes = (const uint8_t *)(const void *)pose;
        uint32_t hash = 2166136261u;
        for (size_t i = 0; i < pose_bytes; i++) hash = (hash ^ bytes[i]) * 16777619u;
        hashes[k] = hash;
        unsigned prior;
        for (prior = 0; prior < k; prior++)
            if (hashes[prior] == hash && !memcmp(pose, keys->data + prior * pose_bytes, pose_bytes)) break;
        if (prior < k) {
            positions[k] = positions[prior];
            repeated_keys++;
            continue;
        }
        ByteBuffer delta = pack_delta_blocks(pose, bind, h->point_count);
        PackedPoseHeader ph = { h->point_count, FIGURE_POSE_RAW, 0 };
        if (delta.len < pose_bytes) {
            ph.mode = FIGURE_POSE_DELTA32_SCALE8;
            delta_keys++;
        }
        positions[k] = buffer_append_aligned(&packed, &ph, sizeof ph);
        if (ph.mode == FIGURE_POSE_RAW) buffer_append(&packed, pose, pose_bytes);
        else buffer_append(&packed, delta.data, delta.len);
        buffer_align(&packed, 4);
        free(delta.data);
    }
    uint32_t offset;
    if (packed.len < keys->len) {
        offset = buffer_align(out, 4);
        for (unsigned k = 0; k < h->keyframe_count; k++) {
            uint32_t at = positions[k];
            ((uint32_t *)(void *)packed.data)[k] = (offset + at) | FIGURE_POSE_TAG;
            PackedPoseHeader *descriptor = (PackedPoseHeader *)(void *)(packed.data + at);
            descriptor->bind_relative = (int32_t)h->points - (int32_t)(offset + at);
        }
        buffer_append(out, packed.data, packed.len);
        printf("  poses: %zu -> %zu bytes (%u delta keys, %u repeated keys)\n", keys->len, packed.len,
               delta_keys, repeated_keys);
        h->keyframe_count |= FIGURE_PACKED_KEYS;
    } else {
        offset = buffer_append_aligned(out, keys->data, keys->len);
    }
    free(hashes);
    free(positions);
    free(packed.data);
    return offset;
}
