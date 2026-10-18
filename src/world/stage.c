static const uint8_t* const stage_blobs[STAGE_COUNT] = STAGE_BLOBS;
static const uint8_t* const stage_far_blobs[STAGE_COUNT] = STAGE_FAR_BLOBS;
static const uint8_t* const stage_occluders[STAGE_COUNT] = STAGE_OCCLUDERS;

static EWRAM_BSS const StageOccluder* g_stage_occluder;

static inline int stage_water_texture(const StageHeader* h, int texture){
    return h->texture_water ? ((const uint16_t*)((const uint8_t*)h + h->texture_water))[texture] : texture;
}

static EWRAM_BSS Stage g_far_stage;
static bool g_has_far_stage;

static EWRAM_BSS uint8_t g_texture_average[STAGE_MAX_TEXTURES];
static EWRAM_BSS uint8_t g_far_texture_average[STAGE_MAX_TEXTURES];

static uint8_t palette_nearest(int r, int g, int b){
    int best = 1, best_d = 1 << 30;
    for(int i = 1; i < 256; i++){
        uint16_t c = game_palette[i];
        int dr = (c & 31) - r, dg = (c >> 5 & 31) - g, db = (c >> 10 & 31) - b;
        int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
        if(d < best_d){
            best_d = d;
            best = i;
        }
    }
    return (uint8_t)best;
}

#define TEXCACHE_LOW_SLOTS 18
static EWRAM_BSS uint8_t g_texture_cache_slot[STAGE_MAX_TEXTURES];
EWRAM_BSS TextureReference g_texture_cache_reference[TEXCACHE_SLOTS];
static inline uint8_t* texture_cache_address(unsigned slot){
    return (uint8_t*)VRAM + (slot < TEXCACHE_LOW_SLOTS
                                 ? 0x4B00 + slot * TEX_TEXELS
                                 : PAGE_SIZE + (slot - TEXCACHE_LOW_SLOTS) * TEX_TEXELS);
}
_Static_assert(PAGE_SIZE + (TEXCACHE_SLOTS - TEXCACHE_LOW_SLOTS) * TEX_TEXELS <= 0x13300,
               "texture cache must end below the second pose tail");

static void stage_copy_texture(unsigned slot, const TextureReference* texture){
    dma3_copy32(texture_cache_address(slot), (const void*)(uintptr_t)texture->mip[0], TEX_TEXELS / 4);
    g_texture_cache_reference[slot].mip[0] = (uint32_t)texture_cache_address(slot);
    g_texture_cache_reference[slot].mip[1] = texture->mip[1];
}

static void stage_cache_textures(const Stage* stage){
    int n = (int)stage->header->texture_count;
    if(n > STAGE_MAX_TEXTURES) n = STAGE_MAX_TEXTURES;

    _Static_assert(sizeof g_game.actors >= STAGE_MAX_TEXTURES * 4,
                   "texture weights must fit the actor array");
    uint32_t* weight = (uint32_t*)(void*)g_game.actors;
    for(int t = 0; t < n; t++){
        weight[t] = 0;
        g_texture_cache_slot[t] = 0;
    }

    for(uint32_t i = 0; i < stage->header->triangle_count; i++){
        const StageTriangle* st = &stage->triangles[i];
        const int16_t* a = stage->points[st->v[0].point];
        const int16_t* b = stage->points[st->v[1].point];
        const int16_t* c = stage->points[st->v[2].point];
        int32_t ext[3];
        for(int k = 0; k < 3; k++){
            int32_t lo = a[k], hi = a[k];
            if(b[k] < lo) lo = b[k];
            if(b[k] > hi) hi = b[k];
            if(c[k] < lo) lo = c[k];
            if(c[k] > hi) hi = c[k];
            ext[k] = (hi - lo) >> 4;
        }
        uint32_t area = (uint32_t)(ext[0] * ext[1] + ext[1] * ext[2] + ext[0] * ext[2]);
        int texture = (int)stage_triangle_texture(st);
        if(texture < n) weight[texture] += area;
        int water = stage_water_texture(stage->header, texture);
        if(water < n && water != texture) weight[water] += area / 4;
    }
    for(int slot = 0; slot < TEXCACHE_SLOTS; slot++){
        int best = -1;
        for(int t = 0; t < n; t++)
            if(weight[t] && (best < 0 || weight[t] > weight[best])) best = t;
        if(best < 0) break;
        weight[best] = 0;
        stage_copy_texture(slot, &stage->textures[best]);
        g_texture_cache_slot[best] = (uint8_t)(slot + 1);
    }
}

static void stage_restore_texture_cache(void){
    const Stage* stage = &g_game.stage;
    unsigned n = stage->header->texture_count;
    if(n > STAGE_MAX_TEXTURES) n = STAGE_MAX_TEXTURES;
    for(unsigned t = 0; t < n; t++){
        unsigned slot = g_texture_cache_slot[t];
        if(!slot) continue;
        const TextureReference* r = &g_texture_cache_reference[slot - 1];
        if(r->mip[0] != r->mip[1]) stage_copy_texture(slot - 1, &stage->textures[t]);
        else {
            volatile uint32_t zero = 0;
            dma3_fill32(texture_cache_address(slot - 1), &zero, TEX_TEXELS / 4);
        }
    }
}

static const uint32_t* stage_far_records(const Stage* stage){
    return (const uint32_t*)(uintptr_t)((const uint32_t*)stage->textures)[-2];
}
static const TextureReference* stage_far_raw_texture(unsigned index){
    uint32_t record = stage_far_records(&g_far_stage)[index];
    if(record & 0x80000000u) return NULL;
    return &g_far_stage.textures[index];
}

static __attribute__((noinline)) uint8_t stage_raw_texture_average(const uint8_t* texels){
    int r = 0, g = 0, b = 0, count = 0;
    for(int i = 0; i < TEX_TEXELS; i += 3){
        uint8_t idx = texels[i];
        if(!idx) continue;
        uint16_t c = game_palette[idx];
        r += c & 31;
        g += c >> 5 & 31;
        b += c >> 10 & 31;
        count++;
    }
    return count ? palette_nearest(r / count, g / count, b / count) : 0;
}

static void stage_compute_texture_averages(const Stage* stage, uint8_t* average){
    int n = (int)stage->header->texture_count;
    if(n > STAGE_MAX_TEXTURES) n = STAGE_MAX_TEXTURES;
    for(int t = 0; t < n; t++){
        const uint8_t* texels;
        if(stage->header->surface_features & STAGE_PACKED_FAR_TEXTURES){
            uint32_t record = stage_far_records(stage)[t];

            if(record & 0x80000000u){
                average[t] = (uint8_t)record;
                continue;
            }
            texels = (const uint8_t*)stage->header + record;
        } else {
            texels = (const uint8_t*)(uintptr_t)stage->textures[t].mip[0];
        }
        average[t] = stage_raw_texture_average(texels);
    }
}

static void stage_load_blob(Stage* stage, const uint8_t* blob){
    const StageHeader* header = (const StageHeader*)blob;
    stage->header = header;
    stage->points = (const int16_t (*)[3])(blob + header->points);
    stage->triangles = (const StageTriangle*)(blob + header->triangles);
    stage->textures = (const TextureReference*)(blob + header->textures);
    stage->chunks = (const StageChunk*)(blob + header->chunks);
    stage->leaf_triangle_offsets = (const uint16_t*)(blob + header->leaf_triangle_offsets);
    stage->leaf_triangles = (const LeafTriangle*)(blob + header->leaf_triangles);
    stage->leaf_draw_ranks = (const uint16_t*)(blob + header->leaf_draw_ranks);
    stage->rank_triangles = (const uint16_t*)(blob + header->rank_triangles);
    stage->sky = blob + header->sky;
    stage->gravities = (const Gravity*)(blob + header->gravities);
    stage->objects = (const StageObject*)(blob + header->objects);
    stage->starts = (const StageStart*)(blob + header->starts);
    stage->rails = (const StageRail*)(blob + header->rails);
    stage->rail_points = (const StageRailPoint*)(blob + header->rail_points);
    stage->rail_parts = (const StageRailPart*)(blob + header->rail_parts);
    stage->areas = (const StageArea*)(blob + header->areas);
    stage->cameras = (const StageCamera*)(blob + header->cameras);
    stage->point_shift = header->point_shift;
    collision_load(header);
    stage_compute_texture_averages(stage, g_texture_average);
    stage_cache_textures(stage);
}

static void stage_load_far(int index){
    g_stage_occluder = (const StageOccluder*)stage_occluders[index];
    const uint8_t* blob = stage_far_blobs[index];
    g_has_far_stage = blob != NULL;
    if(!blob) return;
    const StageHeader* header = (const StageHeader*)blob;
    Stage* stage = &g_far_stage;
    stage->header = header;
    stage->points = (const int16_t (*)[3])(blob + header->points);
    stage->triangles = (const StageTriangle*)(blob + header->triangles);
    stage->textures = (const TextureReference*)(blob + header->textures);
    stage->chunks = (const StageChunk*)(blob + header->chunks);
    stage->leaf_triangle_offsets = (const uint16_t*)(blob + header->leaf_triangle_offsets);
    stage->leaf_triangles = (const LeafTriangle*)(blob + header->leaf_triangles);
    stage->leaf_draw_ranks = (const uint16_t*)(blob + header->leaf_draw_ranks);
    stage->rank_triangles = (const uint16_t*)(blob + header->rank_triangles);
    stage->point_shift = header->point_shift;
    stage_compute_texture_averages(stage, g_far_texture_average);
}

static bool stage_water_level_at(v3 position, fx* level){
    const StageHeader* header = g_game.stage.header;
    if(!header->has_water) return false;
    if(header->has_water_region){
        v2 d = V2(position.x - header->water_centre[0], position.y - header->water_centre[1]);
        if(v2_length_squared(d) > header->water_radius_squared || position.z < header->water_min_z)
            return false;
    }
    *level = header->water_level;
    return true;
}
