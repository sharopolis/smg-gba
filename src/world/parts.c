static uint32_t g_parts_hidden;

static inline const StagePart* stage_parts(const StageHeader* h){
    return h->part_count ? (const StagePart*)((const uint8_t*)h + h->parts) : NULL;
}

static void parts_update(void){
    const StageHeader* h = g_game.stage.header;
    const StagePart* parts = stage_parts(h);
    uint32_t hidden = 0;
    for(uint32_t i = 0; i < h->part_count && i < 32; i++){
        bool on = switch_on(parts[i].sw);
        if(parts[i].mode == PART_SHOW_WHEN_ON ? !on : on) hidden |= 1u << i;
    }
    g_parts_hidden = hidden;
    g_col_parts_hidden = hidden;
    level_after_parts_update();
}

static int parts_filter_leaves(const StageHeader* h, uint16_t* leaves, int n){
    if(!g_parts_hidden || !h->part_count) return n;
    const StagePart* parts = stage_parts(h);
    int out = 0;
    for(int k = 0; k < n; k++){
        uint32_t leaf = leaves[k];
        bool keep = true;
        for(uint32_t i = 0; i < h->part_count && i < 32; i++){
            if(!(g_parts_hidden >> i & 1)) continue;
            if(leaf - parts[i].leaf_first < parts[i].leaf_count){
                keep = false;
                break;
            }
        }
        if(keep) leaves[out++] = (uint16_t)leaf;
    }
    return out;
}

static int occluder_filter_leaves(uint16_t* leaves, int n, v3 camera){
    const StageOccluder* o = g_stage_occluder;
    if(!o) return n;
    uint32_t i = (uint32_t)((camera.x - o->x0) >> 8), j = (uint32_t)((camera.y - o->y0) >> 8);
    if(i >= o->n || j >= o->n) return n;
    const int8_t* grid = (const int8_t*)(o + 1);
    int cells = o->n * o->n, k = (int)(j * o->n + i);
    unsigned hide = 0;
    if(grid[k] != 127 && camera.z > o->base + 32 * grid[k]) hide |= 1;
    if(grid[cells + k] != -128 && camera.z < o->base + 32 * grid[cells + k]) hide |= 2;
    if(!hide) return n;
    const StageOccluderRange* ranges =
        (const StageOccluderRange*)((const uint8_t*)(o + 1) + ((2 * cells + 3) & ~3));
    int out = 0;
    for(int q = 0; q < n; q++){
        uint32_t leaf = leaves[q];
        bool keep = true;
        for(int r = 0; r < o->range_count; r++)
            if((ranges[r].cls & hide) && leaf - ranges[r].first < ranges[r].count){
                keep = false;
                break;
            }
        if(keep) leaves[out++] = (uint16_t)leaf;
    }
    return out;
}
