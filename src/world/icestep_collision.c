static EWRAM_BSS int8_t g_icestep_actor[ICESTEP_SLOTS];
static EWRAM_BSS uint8_t g_icestep_alive, g_icestep_next;
static EWRAM_BSS bool g_icestep_level;
static EWRAM_BSS CollisionTriangle g_icestep_triangle;

static EWRAM_BSS uint32_t g_icestep_vertex_valid;
_Static_assert(ICESTEP_PRISMS == 24, "icestep_slot divides by 24");

static int icestep_slot(unsigned offset){ return (int)((offset * 2731u) >> 16); }
static bool icestep_stage(void){ return g_icestep_level; }
#define ICESTEP_POINTS_DIRTY (1u << 31)

static void icestep_cache_invalidate(void){ g_icestep_vertex_valid &= ICESTEP_POINTS_DIRTY; }
static void icestep_cache_end_logic(void){
    uint32_t dirty = g_icestep_vertex_valid & ICESTEP_POINTS_DIRTY;
    g_icestep_vertex_valid = 0;
    if(dirty) geometry_icestep_clear_point_marks();
}
static void icestep_reset(bool level_has_ice_steps){
    g_icestep_level = level_has_ice_steps;
    for(int i = 0; i < ICESTEP_SLOTS; i++) g_icestep_actor[i] = -1;
    g_icestep_alive = 0;
    g_icestep_next = 0;
    icestep_cache_invalidate();
}
static Actor* icestep_actor(int slot){
    int index = g_icestep_actor[slot];
    if(index < 0 || index >= g_game.actor_count) return NULL;
    Actor* a = &g_game.actors[index];
    return a->active && a->type == OBJ_ICESTEP ? a : NULL;
}
static int icestep_owner(int tri){
    if(!icestep_stage() || tri < ICESTEP_BASE || tri >= ICESTEP_BASE + ICESTEP_SLOTS * ICESTEP_PRISMS)
        return -1;
    return g_icestep_actor[icestep_slot((unsigned)(tri - ICESTEP_BASE))];
}

static const CollisionTriangle* icestep_tri(int index){
    unsigned offset = (unsigned)(index - ICESTEP_BASE);
    int slot = icestep_slot(offset);
    uint32_t slot_bit = 1u << slot;
    CollisionTriangle* records = geometry_icestep_triangle_scratch();
    if(g_icestep_vertex_valid & slot_bit) return records + offset;
    Actor* a = icestep_actor(slot);
    if(!a){
        g_icestep_triangle = (CollisionTriangle){0};
        return &g_icestep_triangle;
    }
    IceStepCollisionCache* cache = geometry_figure_scratch();
    v3 side = v3_cross14(a->up, a->front);
    int16_t scale[3] = {256, 256, 256};
    for(unsigned i = 0; i < sizeof icestep_vertices / sizeof icestep_vertices[0]; i++){
        v3 q = dyncol_point(icestep_vertices[i], V3(0, 0, 0), side, a->up, a->front, scale);
        cache->vertex[slot][i][0] = q.x;
        cache->vertex[slot][i][1] = q.y;
        cache->vertex[slot][i][2] = q.z;
    }
    g_icestep_vertex_valid |= ICESTEP_POINTS_DIRTY;
    for(int face = 0; face < ICESTEP_PRISMS; face++){
        CollisionTriangle* ct = records + slot * ICESTEP_PRISMS + face;
        v3 relative[3], v[3];
        for(int i = 0; i < 3; i++){
            relative[i] = v3_from_i16(cache->vertex[slot][icestep_faces[face][i]]);
            v[i] = v3_add(a->home, relative[i]);
            ct->v[i][0] = v[i].x >> g_collision.shift;
            ct->v[i][1] = v[i].y >> g_collision.shift;
            ct->v[i][2] = v[i].z >> g_collision.shift;
        }
        v3 n = v3_direction14(v3_cross(v3_sub(relative[1], relative[0]), v3_sub(relative[2], relative[0])),
                              a->up);
        ct->normal[0] = n.x;
        ct->normal[1] = n.y;
        ct->normal[2] = n.z;
        ct->attr = COL_ICE;
        ct->floor_code = (uint8_t)icestep_codes[face];
        ct->wall_code = (uint8_t)(icestep_codes[face] >> 8);
        ct->plane = v3_dot14(n, v[0]);
    }
    g_icestep_vertex_valid |= slot_bit;
    return records + offset;
}
static int icestep_gather(v3 lo, v3 hi, uint16_t* indices, int count, int max){
    if(!icestep_stage() || !g_icestep_alive) return count;
    v3 centre = V3((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, (lo.z + hi.z) / 2);
    v3 extent = V3((hi.x - lo.x + 1) / 2 + 2, (hi.y - lo.y + 1) / 2 + 2, (hi.z - lo.z + 1) / 2 + 2);

    bool narrow = (uint32_t)extent.x <= 16383 && (uint32_t)extent.y <= 16383 && (uint32_t)extent.z <= 16383;
    for(int slot = 0; slot < ICESTEP_SLOTS && count < max; slot++){
        Actor* a = icestep_actor(slot);
        if(!a || a->state == ICESTEP_HIT) continue;
        v3 axes[3] = {v3_cross14(a->up, a->front), a->up, a->front};
        v3 delta = v3_sub(centre, a->home);
        fx local[3], radius[3];
        for(int k = 0; k < 3; k++){
            local[k] = v3_dot14(delta, axes[k]);
            radius[k] =
                narrow ? ((extent.x * fx_abs(axes[k].x) + extent.y * fx_abs(axes[k].y) +
                           extent.z * fx_abs(axes[k].z) + 16383) >>
                          14) +
                             2
                       : (fx)(((int64_t)extent.x * fx_abs(axes[k].x) + (int64_t)extent.y * fx_abs(axes[k].y) +
                               (int64_t)extent.z * fx_abs(axes[k].z) + 16383) >>
                              14) +
                             2;
        }

        v3 qlo = V3(local[0] - radius[0], local[1] - radius[1], local[2] - radius[2]);
        v3 qhi = V3(local[0] + radius[0], local[1] + radius[1], local[2] + radius[2]);
        if(-170 > qhi.x || 170 < qlo.x || -17 > qhi.y || 16 < qlo.y || -147 > qhi.z || 147 < qlo.z) continue;
        for(int face = 0; face < ICESTEP_PRISMS && count < max; face++){
            const int16_t (*bounds)[3] = icestep_bounds[face];
            if(bounds[0][0] > qhi.x || bounds[1][0] < qlo.x || bounds[0][1] > qhi.y || bounds[1][1] < qlo.y ||
               bounds[0][2] > qhi.z || bounds[1][2] < qlo.z)
                continue;
            indices[count++] = (uint16_t)(ICESTEP_BASE + slot * ICESTEP_PRISMS + face);
        }
    }
    return count;
}
