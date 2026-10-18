#ifndef CANDLE_EFFECT_RASTER_H
#define CANDLE_EFFECT_RASTER_H
#include <stdint.h>
#include <stdbool.h>
typedef struct {
    const uint8_t* rgba;
    const uint8_t* original_bti;
    uint16_t width, height, original_bytes;
} FFCXTexture;
typedef struct {
    uint32_t half_x, half_y;
    uint8_t prm[4], env[4];
    uint8_t alpha, texture;
} FFCXFrame;
typedef struct {
    const uint8_t* original_resource;
    const FFCXFrame* frame;
    int32_t offset[3];
    uint16_t id, frame_count, max_emitter_frame;
    uint8_t additive, continuous;
} FFCXLayer;
#endif
