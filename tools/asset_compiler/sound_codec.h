#ifndef ASSETC_SOUND_CODEC_H
#define ASSETC_SOUND_CODEC_H

#include "common.h"

float *sound_resample(const int8_t *pcm, size_t n, size_t loop, int out_rate, int in_rate, size_t *out_n,
                      size_t *out_loop);

double sound_encode(ByteBuffer *b, const float *x, size_t n, size_t loop);

void sound_decode(const uint8_t *blob, int8_t *out);

#endif
