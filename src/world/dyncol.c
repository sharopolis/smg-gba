static EWRAM_BSS CollisionTriangle g_dyncol_tris[DYNCOL_MAX];
static EWRAM_BSS DynColGroup g_dyncol_groups[DYNCOL_GROUPS];
static int g_dyncol_tri_count, g_dyncol_group_count;

#define COL_TRI(i) col_tri(i)

static EWRAM_BSS uint32_t g_dyncol_key[DYNCOL_GROUPS][2];
static EWRAM_BSS uint16_t g_dyncol_key_first[DYNCOL_GROUPS], g_dyncol_key_count[DYNCOL_GROUPS];

static EWRAM_BSS uint8_t g_dyncol_pending[DYNCOL_GROUPS];
static EWRAM_BSS uint8_t g_dyncol_pending_group[DYNCOL_GROUPS];
static EWRAM_BSS uint8_t g_dyncol_tri_pending[DYNCOL_MAX];

static void dyncol_mark_part(int slot){
    const DynColGroup* d = &g_dyncol_groups[slot];
    uint8_t v = g_dyncol_pending[slot] ? (uint8_t)(slot + 1) : 0;
    for(int k = 0; k < d->count; k++) g_dyncol_tri_pending[d->first + k] = v;
}
static int g_dyncol_defer_count;
static bool g_dyncol_defer_on;

static void dyncol_defer_reset(void){
    for(int g = g_dyncol_group_count; g < DYNCOL_GROUPS; g++) g_dyncol_pending[g] = 0;
    g_dyncol_defer_count = 0;
}

static EWRAM_BSS uint32_t g_dyncol_generation;
static EWRAM_BSS fx g_dyncol_near_slack;
static bool g_dyncol_full;
static void dyncol_clear(void){
    g_dyncol_generation++;
    g_dyncol_full = false;
    icestep_cache_invalidate();
    for(int g = g_dyncol_group_count; g < DYNCOL_GROUPS; g++) g_dyncol_key[g][0] = g_dyncol_key[g][1] = 0;
    dyncol_defer_reset();
    g_dyncol_tri_count = 0;
    g_dyncol_group_count = 0;
}

static inline uint32_t dyncol_rotl(uint32_t x, int r){ return x << r | x >> (32 - r); }

static inline uint32_t dyncol_mulk(uint32_t x, uint32_t k){
    __asm__("" : "+r"(k));
    return x * k;
}

static inline void dyncol_hash_word(uint32_t h[2], uint32_t w){
    h[0] = dyncol_mulk(dyncol_rotl(h[0] + dyncol_mulk(w, 0x85EBCA77u), 13), 0x9E3779B1u);
    h[1] = dyncol_mulk(dyncol_rotl(h[1] + dyncol_mulk(w, 0xC2B2AE3Du), 17), 0x27D4EB2Fu);
}
static inline void dyncol_hash_final(uint32_t h[2]){
    for(int k = 0; k < 2; k++){
        uint32_t x = h[k];
        x ^= x >> 15;
        x = dyncol_mulk(x, 0x85EBCA77u);
        x ^= x >> 13;
        x = dyncol_mulk(x, 0xC2B2AE3Du);
        x ^= x >> 16;
        h[k] = x | 1;
    }
}

__attribute__((noinline)) static bool dyncol_segment_meets(const DynColGroup* d, v3 a, v3 b){
    int64_t en = 0, ed = 1, xn = 1, xd = 1;
    for(int k = 0; k < 3; k++){
        int32_t A = (&a.x)[k], D = (&b.x)[k] - A, L = (&d->lo.x)[k] - 16, H = (&d->hi.x)[k] + 16;
        if(D == 0){
            if(A < L || A > H) return false;
            continue;
        }
        int64_t n0 = D > 0 ? (int64_t)L - A : (int64_t)A - H, n1 = D > 0 ? (int64_t)H - A : (int64_t)A - L;
        int64_t dd = D > 0 ? D : -(int64_t)D;
        if(n0 * ed > en * dd){
            en = n0;
            ed = dd;
        }
        if(n1 * xd < xn * dd){
            xn = n1;
            xd = dd;
        }
        if(en * xd > xn * ed) return false;
    }
    return true;
}

static inline bool dyncol_sphere_meets(const DynColGroup* d, v3 c, fx radius){
    fx r = radius + 16;
    return !(c.x < d->lo.x - r || c.x > d->hi.x + r || c.y < d->lo.y - r || c.y > d->hi.y + r ||
             c.z < d->lo.z - r || c.z > d->hi.z + r);
}

static inline const DynColGroup* dyncol_deferred_part(uint32_t k){
    return k < DYNCOL_MAX && g_dyncol_tri_pending[k] ? &g_dyncol_groups[g_dyncol_tri_pending[k] - 1] : NULL;
}

__attribute__((noinline)) static int dyncol_gather(v3 lo, v3 hi, uint16_t* index, int count, int max){
    for(int g = 0; g < g_dyncol_group_count; g++){
        const DynColGroup* d = &g_dyncol_groups[g];
        if(d->lo.x > hi.x || d->hi.x < lo.x || d->lo.y > hi.y || d->hi.y < lo.y || d->lo.z > hi.z ||
           d->hi.z < lo.z)
            continue;
        for(int k = 0; k < d->count && count < max; k++)
            index[count++] = (uint16_t)(DYNCOL_BASE + d->first + k);
    }
    return icestep_stage() ? icestep_gather(lo, hi, index, count, max) : count;
}

static inline v3 dyncol_point(const int16_t p[3], v3 pos, v3 side, v3 up, v3 front, const int16_t scale[3]){
    fx x = (p[0] * scale[0]) >> 8, y = (p[1] * scale[1]) >> 8, z = (p[2] * scale[2]) >> 8;
    return v3_add(pos, v3_add(v3_add(v3_scale14(side, x), v3_scale14(up, y)), v3_scale14(front, z)));
}

_Static_assert(sizeof(DynColVertexCache) == 16, "DynColVertexCache must be 16 bytes");

static struct {
    const HoneyColMesh* mesh;
    int reach;
} g_dyncol_reach[32];
static int dyncol_mesh_reach(const HoneyColMesh* m){
    unsigned slot = ((uintptr_t)m >> 4 ^ (uintptr_t)m >> 9) & 31;
    if(g_dyncol_reach[slot].mesh == m) return g_dyncol_reach[slot].reach;
    int r = 0;
    for(int g = 0; g < m->group_count; g++)
        for(int k = 0; k < 3; k++){
            int lo = m->groups[g].min[k], hi = m->groups[g].max[k];
            if(lo < 0) lo = -lo;
            if(hi < 0) hi = -hi;
            if(lo > r) r = lo;
            if(hi > r) r = hi;
        }
    g_dyncol_reach[slot].mesh = m;
    g_dyncol_reach[slot].reach = r;
    return r;
}

static bool dyncol_mesh_beyond(const HoneyColMesh* m, v3 pos, int smax, v3 centre, fx near){
    fx b = (fx)(((int64_t)dyncol_mesh_reach(m) * smax) >> 8) + 1;
    fx r = 3 * b + ((3 * b) >> 9) + 8 + near + g_dyncol_near_slack;
    v3 d = v3_sub(centre, pos);
    return fx_abs(d.x) > r || fx_abs(d.y) > r || fx_abs(d.z) > r;
}

_Static_assert(sizeof(DynColReciprocalCache) == 8, "DynColReciprocalCache must be 8 bytes");
_Static_assert(58 * sizeof(DynColVertexCache) + 8 * sizeof(DynColReciprocalCache) <=
                   64 * sizeof(DynColVertexCache),
               "BeeFlower caches must fit 64 scratch slots");
IWRAM_FN static v3 dyncol_beeflower_direction14(v3 a, v3 fallback, DynColReciprocalCache* cache){
    uint32_t m = (uint32_t)fx_max(fx_max(fx_abs(a.x), fx_abs(a.y)), fx_abs(a.z));
    if(m == 0) return fallback;
    int down = 0, up = 0;
    while(m >= 32768){
        m >>= 1;
        down++;
    }
    while(m < 16384){
        m <<= 1;
        up++;
    }
    a = V3((a.x >> down) << up, (a.y >> down) << up, (a.z >> down) << up);
    uint32_t squared = (uint32_t)(a.x * a.x + a.y * a.y) + (uint32_t)(a.z * a.z);
    DynColReciprocalCache* entry = &cache[((squared >> 20) ^ (squared >> 23)) & 7u];
    uint32_t r;
    if(entry->squared == squared) r = entry->reciprocal;
    else {
        uint32_t len = isqrt32(squared);
        r = recip30_arm(len);
        entry->squared = squared;
        entry->reciprocal = r;
    }
    return V3((a.x * (int32_t)r) >> 16, (a.y * (int32_t)r) >> 16, (a.z * (int32_t)r) >> 16);
}

__attribute__((noinline)) static void dyncol_vertex_miss(
    const DynColTriangleJob* j, DynColVertexCache* e, uint32_t vertex){
    e->point = dyncol_point(j->m->v[vertex], j->pos, j->side, j->up, j->front, j->scale);
    e->vertex = vertex;
}

IWRAM_FN static int dyncol_build_triangles(const DynColTriangleJob* j){
    const HoneyColGroup* hg = j->hg;
    const uint16_t (*t)[4] = j->m->t + hg->first;
    const uint16_t* codes = j->codes ? j->codes + hg->first : NULL;
    const int16_t (*normals)[3] = j->normals ? j->normals + hg->first : NULL;
    DynColVertexCache* cache = j->cache;
    DynColReciprocalCache* bee = j->bee;
    int s = j->s;
    uint16_t extra_attr = j->extra_attr;
    int count = hg->count, built = 0;
    CollisionTriangle* ct = j->out;
    for(int k = 0; k < count; k++, t++){
        v3 q[3];
        for(int i = 0; i < 3; i++){
            uint32_t vertex = (*t)[i];
            DynColVertexCache* e = &cache[vertex & 63];
            if(e->vertex != vertex) dyncol_vertex_miss(j, e, vertex);
            q[i] = e->point;
        }

        v3 n;
        if(normals){
            n = v3_from_i16(normals[k]);
            if(!(n.x | n.y | n.z)) continue;
        } else {
            n = v3_cross(v3_sub(q[1], q[0]), v3_sub(q[2], q[0]));
            if(!(n.x | n.y | n.z)) continue;
            n = bee ? dyncol_beeflower_direction14(n, j->up, bee) : v3_direction14(n, j->up);
        }
        for(int i = 0; i < 3; i++){
            ct->v[i][0] = (int16_t)(q[i].x >> s);
            ct->v[i][1] = (int16_t)(q[i].y >> s);
            ct->v[i][2] = (int16_t)(q[i].z >> s);
        }
        ct->normal[0] = (int16_t)n.x;
        ct->normal[1] = (int16_t)n.y;
        ct->normal[2] = (int16_t)n.z;
        ct->attr = (uint16_t)((*t)[3] | extra_attr);
        uint16_t source = codes ? codes[k] : 0;
        ct->floor_code = (uint8_t)source;
        ct->wall_code = (uint8_t)(source >> 8);
        ct->plane = v3_dot14(n, q[0]);
        ct++;
        built++;
    }
    return built;
}

IWRAM_FN static int dyncol_count_triangles(const DynColTriangleJob* j){
    const HoneyColGroup* hg = j->hg;
    const uint16_t (*t)[4] = j->m->t + hg->first;
    DynColVertexCache* cache = j->cache;
    int count = hg->count, built = 0;
    for(int k = 0; k < count; k++, t++){
        v3 q[3];
        for(int i = 0; i < 3; i++){
            uint32_t vertex = (*t)[i];
            DynColVertexCache* e = &cache[vertex & 63];
            if(e->vertex != vertex) dyncol_vertex_miss(j, e, vertex);
            q[i] = e->point;
        }
        v3 n = v3_cross(v3_sub(q[1], q[0]), v3_sub(q[2], q[0]));
        built += (n.x | n.y | n.z) != 0;
    }
    return built;
}

typedef struct {
    const HoneyColMesh* m;
    const uint16_t* codes;
    v3 pos, side, up, front;
    int16_t scale[3];
    uint16_t extra_attr;
    uint8_t bee, shift;
} DynColDeferred;
#define DYNCOL_DEFER_MAX 24
static EWRAM_BSS DynColDeferred g_dyncol_defer[DYNCOL_DEFER_MAX];
static EWRAM_BSS DynColVertexCache g_dyncol_defer_cache[64];
static EWRAM_BSS DynColReciprocalCache g_dyncol_defer_recip[8];

static int dyncol_defer_inputs(int* index, const HoneyColMesh* m, const uint16_t* codes, bool bee, v3 pos,
                               v3 side, v3 up, v3 front, const int16_t scale[3], uint16_t extra_attr){
    if(*index >= 0) return *index;
    if(g_dyncol_defer_count >= DYNCOL_DEFER_MAX) return -1;
    DynColDeferred* r = &g_dyncol_defer[g_dyncol_defer_count];
    r->m = m;
    r->codes = codes;
    r->bee = bee;
    r->pos = pos;
    r->side = side;
    r->up = up;
    r->front = front;
    for(int k = 0; k < 3; k++) r->scale[k] = scale[k];
    r->extra_attr = extra_attr;
    r->shift = (uint8_t)g_collision.shift;
    return *index = g_dyncol_defer_count++;
}

__attribute__((noinline)) static void dyncol_build_deferred(int slot){
    const DynColDeferred* r = &g_dyncol_defer[g_dyncol_pending[slot] - 1];
    DynColGroup* d = &g_dyncol_groups[slot];
    g_dyncol_pending[slot] = 0;
    for(int i = 0; i < 64; i++) g_dyncol_defer_cache[i].vertex = 0xFFFFFFFFu;
    for(int i = 0; i < 8; i++) g_dyncol_defer_recip[i].squared = 0;
    DynColTriangleJob job = {r->m,
                             &r->m->groups[g_dyncol_pending_group[slot]],
                             g_dyncol_defer_cache,
                             r->pos,
                             r->side,
                             r->up,
                             r->front,
                             r->scale,
                             r->shift,
                             r->extra_attr,
                             r->codes,
                             NULL,
                             r->bee ? g_dyncol_defer_recip : NULL,
                             &g_dyncol_tris[d->first]};
    dyncol_build_triangles(&job);
    dyncol_mark_part(slot);
}

#define DYNCOL_BUILD_NAME dyncol_add_codes
#define DYNCOL_BUILD_ATTR
#include "world/dyncol_build.inc"
#undef DYNCOL_BUILD_ATTR
#undef DYNCOL_BUILD_NAME

#define DYNCOL_BUILD_NAME dyncol_add_beeflower
#define DYNCOL_BUILD_ATTR __attribute__((noinline))
#define DYNCOL_BUILD_BEE_RECIP
#include "world/dyncol_build.inc"
#undef DYNCOL_BUILD_BEE_RECIP
#undef DYNCOL_BUILD_ATTR
#undef DYNCOL_BUILD_NAME

static bool dyncol_add(const HoneyColMesh* m, v3 pos, v3 up, v3 front, const int16_t scale[3], int owner,
                       uint16_t attr, v3 centre, fx near){
    return dyncol_add_codes(m, pos, up, front, scale, owner, attr, centre, near, NULL);
}

static int dyncol_owner_code(int tri){
    if(tri < DYNCOL_BASE) return -1;
    int ice = icestep_owner(tri);
    if(ice >= 0) return ice;
    int k = tri - DYNCOL_BASE;
    for(int g = 0; g < g_dyncol_group_count; g++)
        if(k >= g_dyncol_groups[g].first && k < g_dyncol_groups[g].first + g_dyncol_groups[g].count)
            return g_dyncol_groups[g].owner;
    return -1;
}

static int dyncol_owner(int tri){
    int code = dyncol_owner_code(tri);
    return code < 0 ? code : code & 255;
}
static int dyncol_part(int tri){
    int code = dyncol_owner_code(tri);
    return code < 0 ? 0 : code >> 8;
}
