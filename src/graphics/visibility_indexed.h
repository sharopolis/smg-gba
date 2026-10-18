#ifndef GALAXY_VISIBILITY_INDEXED_H
#define GALAXY_VISIBILITY_INDEXED_H
#include "../assets/asset_format.h"
#define VISIBILITY_LEAF_MAX 16u

static inline __attribute__((always_inline)) void visibility_decode_records(
    const uint16_t* ids, unsigned count, const StageTriangle* triangles, const int16_t (*points)[3],
    LeafTriangle* out){
    for(unsigned i = 0; i < count; i++){
        const StageTriangle* t = triangles + ids[i];
        const int16_t* a = points[t->v[0].point];
        for(unsigned k = 0; k < 3; k++){
            out[i].points[k] = t->v[k].point;
            out[i].a[k] = a[k];
            out[i].normal[k] = (int16_t)(t->normal[k] * 2);
        }
        out[i].pad = 0;
    }
}
#endif
