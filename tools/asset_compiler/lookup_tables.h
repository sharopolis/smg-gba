#ifndef ASSETC_LOOKUP_TABLES_H
#define ASSETC_LOOKUP_TABLES_H

#include "common.h"

typedef struct {
    ByteBuffer sin_cos;
    ByteBuffer recip8;
    ByteBuffer recip_clip;
    ByteBuffer recip_sprite;
    ByteBuffer recip_area;
    ByteBuffer project;
    ByteBuffer delta;
    ByteBuffer sqrt;
    ByteBuffer atan2;
    ByteBuffer zbin;
    ByteBuffer zdrop;
    ByteBuffer pow;
} Tables;

Tables tables_build(void);

#endif
