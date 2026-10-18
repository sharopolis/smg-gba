#ifndef ASSETC_POSE_PACKING_H
#define ASSETC_POSE_PACKING_H

#include "../../src/assets/asset_format.h"
#include "common.h"

void quantize_pose_values(int16_t *values, size_t count, unsigned points);
void hold_one_tick_keys(ByteBuffer *keys, unsigned points, const AnimClip *clips, unsigned clip_count,
                        const char *name);
uint32_t append_figure_keys(ByteBuffer *out, FigureHeader *h, const ByteBuffer *keys, const int16_t *bind,
                            const char *name);

#endif
