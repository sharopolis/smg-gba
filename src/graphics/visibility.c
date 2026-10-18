static EWRAM_BSS FramePoint g_frame_points[MAX_POINTS];
static EWRAM_BSS uint32_t g_screen_positions[MAX_POINTS];

static EWRAM_BSS uint16_t g_visible_triangles[VISIBLE_CAP];
static EWRAM_BSS uint16_t g_point_list[MAX_POINTS];
static void* geometry_point_scratch(void){ return g_point_list; }

static EWRAM_BSS uint32_t g_rank_bits[(STAGE_MAX_RANKS / 3 + 31) / 32];
static EWRAM_BSS uint16_t g_visible_leaves[STAGE_MAX_LEAVES];

static EWRAM_BSS uint8_t g_point_marks[(DIRECT_POINTS + 3) & ~3];

static EWRAM_BSS uint8_t g_far_point_marks[(FAR_DIRECT_MARKS + 4) & ~3];
_Static_assert(STAGE_FAR_MAX_POINTS <= DIRECT_POINTS || STAGE_MAX_POINTS <= STAGE_DIRECT_POINTS,
               "far LOD points must fit g_point_marks");

static EWRAM_BSS uint8_t g_leaf_stamps[(STAGE_MAX_HOME_LEAVES + 4) & ~3];
static EWRAM_BSS int16_t g_leaf_delta[STAGE_MAX_HOME_LEAVES + 1];

static int g_far_reserve;
static int g_far_base;
_Static_assert(STAGE_FAR_MAX_POINTS <= STAGE_FAR_POINT_LIMIT &&
                   STAGE_DIRECT_POINTS + POINTS_SPLIT_RESERVE + STAGE_FAR_DIRECT_POINTS + 1 <= MAX_POINTS &&
                   POINTS_SPLIT_RESERVE + STAGE_FAR_POINT_LIMIT + 1 + 2048 <= MAX_POINTS,
               "point pool must hold the stage, split points and far LOD");

EWRAM_BSS uint16_t g_triangle_bin_heads[2 * BIN_COUNT];
static EWRAM_BSS Figure* g_figure_bin_heads[BIN_COUNT];
static uint8_t g_point_stamp;

IWRAM_CODE static inline uint32_t lowest_set_bit_index(uint32_t bits){
    static const uint8_t debruijn[32] = {0,  1,  28, 2,  29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4,  8,
                                         31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6,  11, 5,  10, 9};
    return debruijn[((bits & -bits) * 0x077CB531u) >> 27];
}

static int g_point_count, g_farthest_visible_bin;

IWRAM_CODE static int depth_bin_for_z(fx z){
    uint32_t i = (uint32_t)fx_max(z, 0) >> 6;
    return lut_zbin[i > 2047 ? 2047 : i];
}

_Static_assert(offsetof(CullJob, z_inner) == 172, "visibility_bvh.s assumes the CullJob layout");

static int collect_visible_chunks(const Stage* st, const CameraView* cv, v3 cam, int farthest_bin,
                                  int inner_bin){
    const m3* m = &cv->to_screen.m;
    v3 o = cv->to_screen.o;
    v3 rx = m->r[0], ry = m->r[1], rz = m->r[2];
    const v3 n[5] = {v3_sub(rx, rz), v3_sub(v3_neg(rx), rz), v3_sub(ry, rz), v3_sub(v3_neg(ry), rz), rz};
    const int32_t k[5] = {o.x - o.z, -o.x - o.z, o.y - o.z, -o.y - o.z, o.z};

    int s = st->point_shift;
    CullJob job = {st->chunks,
                   st->header->chunk_count,
                   {{0}},
                   lut_zdrop[farthest_bin],
                   {cam.x >> s, cam.y >> s, cam.z >> s},
                   g_visible_leaves,
                   0,
                   inner_bin ? lut_zdrop[inner_bin] : 0};
    for(int p = 0; p < 5; p++){
        int32_t* pl = job.planes[p];
        pl[0] = n[p].x << s;
        pl[1] = n[p].y << s;
        pl[2] = n[p].z << s;
        pl[3] = fx_abs(n[p].x) << s;
        pl[4] = fx_abs(n[p].y) << s;
        pl[5] = fx_abs(n[p].z) << s;
        pl[6] = k[p];
    }
    return geometry_cull_chunks(&job);
}

IWRAM_CODE static int view_nearest_axis(v3 dir){
    fx d[6] = {-dir.x, dir.x, -dir.y, dir.y, -dir.z, dir.z};
    int best = 0;
    for(int k = 1; k < 6; k++)
        if(d[k] >= d[best]) best = k;
    return best;
}

_Static_assert(offsetof(CollectJob, plist_end) == 60 && offsetof(CollectJob, home) == 64,
               "visibility_bvh.s assumes the CollectJob layout");
_Static_assert(offsetof(CollectJob, points) == 68 && offsetof(CollectJob, triangles) == 72,
               "assembly assumes the CollectJob pointer offsets");
_Static_assert(offsetof(CollectJob, axis_coord) == 80 && offsetof(CollectJob, axis_sign) == 84,
               "visibility_bvh.s assumes the CollectJob axis offsets");

__attribute__((noinline)) static void visibility_copy_overlay(const uint32_t* source, unsigned words){
    uint32_t* target = (uint32_t*)(uintptr_t)geometry_collect_leaves;
    for(unsigned i = 0; i < words; i += 64){
        unsigned n = words - i;
        if(n > 64) n = 64;
        dma3_copy32(target + i, source + i, n);
    }
}
__attribute__((noinline)) static void visibility_collect_indexed(CollectJob* job){
    if(job->home)
        visibility_copy_overlay(__visibility_overlay_start,
                                (unsigned)(__visibility_overlay_end - __visibility_overlay_start));
    else
        visibility_copy_overlay(
            __visibility_direct_overlay_start,
            (unsigned)(__visibility_direct_overlay_end - __visibility_direct_overlay_start));
    geometry_collect_leaves(job);
    const uint32_t* original =
        __iwram_lma + ((uintptr_t)geometry_collect_leaves - (uintptr_t)__iwram_start) / 4;
    unsigned words = ((uintptr_t)geometry_collect_leaves_end - (uintptr_t)geometry_collect_leaves) / 4;
    visibility_copy_overlay(original, words);
}

_Static_assert(offsetof(ClaimJob, slot_limit) == 44, "visibility_bvh.s assumes the ClaimJob layout");

__attribute__((noinline)) IWRAM_CODE static void reverse_visible(int from, int to){
    for(to--; from < to; from++, to--){
        uint16_t t = g_visible_triangles[from];
        g_visible_triangles[from] = g_visible_triangles[to];
        g_visible_triangles[to] = t;
    }
}

__attribute__((noinline)) static uint32_t rank_segment_next(
    const uint16_t* segs, uint32_t* seg, uint32_t pos, int* seg_out, int n){
    reverse_visible(*seg_out, n);
    *seg_out = n;
    uint32_t end;
    do end = ++*seg < segs[0] ? segs[*seg + 1] : UINT32_MAX;
    while(pos >= end);
    return end;
}

IWRAM_FN __attribute__((optimize("Os"))) static int rank_bits_to_triangles(
    const uint16_t* ranked, uint32_t lo, uint32_t hi, const uint16_t* segs){
    int n = 0, seg_out = 0;
    uint32_t seg = 1, seg_end = segs && segs[0] > 1 ? segs[2] : UINT32_MAX;
    for(uint32_t w = lo; w <= hi && lo <= hi; w++){
        uint32_t bits = g_rank_bits[w];
        if(!bits) continue;
        g_rank_bits[w] = 0;
        while(bits){
            uint32_t b = lowest_set_bit_index(bits), pos = w << 5 | b;
            bits &= bits - 1;
            if(pos >= seg_end) seg_end = rank_segment_next(segs, &seg, pos, &seg_out, n);
            if(n < VISIBLE_CAP) g_visible_triangles[n++] = ranked[pos];
        }
    }
    if(segs) reverse_visible(seg_out, n);
    return n;
}

__attribute__((target("arm"), noinline)) static int rank_bits_to_triangles_masked(
    const Stage* st, const uint16_t* ranked, uint32_t lo, uint32_t hi, const uint16_t* segs){
    int n = 0, seg_out = 0;
    uint32_t seg = 1, seg_end = segs && segs[0] > 1 ? segs[2] : UINT32_MAX;
    uint16_t scenario = (uint16_t)(1u << (g_game.scenario + 9));
    for(uint32_t w = lo; w <= hi && lo <= hi; w++){
        uint32_t bits = g_rank_bits[w];
        if(!bits) continue;
        g_rank_bits[w] = 0;
        while(bits){
            uint32_t b = lowest_set_bit_index(bits), pos = w << 5 | b;
            bits &= bits - 1;
            if(pos >= seg_end) seg_end = rank_segment_next(segs, &seg, pos, &seg_out, n);
            uint16_t tri = ranked[pos];
            uint16_t flags = st->triangles[tri].texture;
            if(!(flags & SURF_SCENARIO_MASK) || (flags & scenario))
                if(n < VISIBLE_CAP) g_visible_triangles[n++] = tri;
        }
    }
    if(segs) reverse_visible(seg_out, n);
    return n;
}

static int collect_visible_triangles(const Stage* st, int leaf_count, int axis, int32_t limit, v3 cam,
                                     int* n_plist, int* pool_used, uint8_t* marks, int pool_free){
    int s = st->point_shift;
    const StageHeader* h = st->header;
    const uint8_t* blob = (const uint8_t*)h;
    bool by_leaf = h->point_home != 0;
    const uint16_t* home = by_leaf ? (const uint16_t*)(blob + h->point_home) : NULL;
    bool indexed = (h->surface_features & STAGE_INDEXED_LEAVES) != 0;
    CollectJob job = {g_visible_leaves,
                      leaf_count,
                      st->leaf_triangle_offsets,
                      st->leaf_triangles,
                      st->leaf_draw_ranks + (axis >> 1) * h->leaf_triangle_count,
                      fx_min(limit >> s, 32767),
                      {cam.x >> s, cam.y >> s, cam.z >> s},
                      marks,
                      g_point_stamp,
                      g_rank_bits,
                      UINT32_MAX,
                      0,
                      g_point_list,
                      g_point_list + fx_max(pool_free - 48, 0),
                      home,
                      st->points,
                      st->triangles,
                      h->surface_features,
                      axis / 2 * 2,
                      axis & 1 ? -1 : 0};

    if(indexed){
        if((limit >> s) >= -32768) visibility_collect_indexed(&job);
    } else if((limit >> s) >= -32768) geometry_collect_leaves(&job);

    const uint16_t* ranked = st->rank_triangles + (uint32_t)(axis >> 1) * (h->rank_count / 3);
    const uint16_t* segs = axis & 1 ? st->rank_triangles + h->rank_count : NULL;
    int n = h->surface_features & STAGE_SCENARIO_SURFACES
                ? rank_bits_to_triangles_masked(st, ranked, job.lo, job.hi, segs)
                : rank_bits_to_triangles(ranked, job.lo, job.hi, segs);
    if(!by_leaf){
        *n_plist = *pool_used = (int)(job.plist - g_point_list);
        return n;
    }

    ClaimJob claim = {g_visible_triangles,
                      n,
                      st->triangles,
                      home,
                      (const uint16_t*)(blob + h->leaf_point_first),
                      g_leaf_stamps,
                      g_leaf_delta,
                      g_frame_points,
                      g_point_stamp,
                      g_point_list,
                      0,
                      (uint32_t)fx_max(pool_free - h->leaf_slot_max, 0)};
    n = geometry_claim_points(&claim);
    *n_plist = (int)(claim.plist - g_point_list);
    *pool_used = (int)claim.next_slot;
    return n;
}

_Static_assert(offsetof(TransformJob, home) == 68, "raster.s assumes the TransformJob layout");

static TransformJob geometry_transform_job(const int16_t (*src)[3], FramePoint* dst, uint32_t* scr,
                                           const uint16_t* list, int n, const xform* t, int shift){
    TransformJob job = {src, dst, scr, list, n, {{0}}, NULL, NULL};
    for(int r = 0; r < 3; r++){
        job.m[r][0] = t->m.r[r].x << shift;
        job.m[r][1] = t->m.r[r].y << shift;
        job.m[r][2] = t->m.r[r].z << shift;
        job.m[r][3] = ((const fx*)&t->o)[r];
    }
    return job;
}
