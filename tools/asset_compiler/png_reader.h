#ifndef ASSETC_PNG_READER_H
#define ASSETC_PNG_READER_H

#include <stdint.h>

typedef struct {
    int w, h;
    uint8_t *rgba;
} Image;

Image png_load(const char *path);
void image_free(Image *img);

#endif
