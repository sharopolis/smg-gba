#define DYNCOL_BASE   0x7000
#define DYNCOL_MAX    160
#define DYNCOL_GROUPS 32

typedef struct {
    int16_t min[3], max[3];
    uint16_t first, count;
} HoneyColGroup;

typedef struct {
    const int16_t (*v)[3];
    const uint16_t (*t)[4];
    const HoneyColGroup* groups;
    uint8_t group_count;
} HoneyColMesh;

typedef struct {
    v3 lo, hi;
    uint16_t first, count;
    int16_t owner;
} DynColGroup;

typedef struct {
    v3 point;
    uint32_t vertex;
} DynColVertexCache;

typedef struct {
    uint32_t squared, reciprocal;
} DynColReciprocalCache;

typedef struct {
    const HoneyColMesh* m;
    const HoneyColGroup* hg;
    DynColVertexCache* cache;
    v3 pos, side, up, front;
    const int16_t* scale;
    int s;
    uint16_t extra_attr;
    const uint16_t* codes;
    const int16_t (*normals)[3];
    DynColReciprocalCache* bee;
    CollisionTriangle* out;
} DynColTriangleJob;

static EWRAM_BSS CollisionTriangle g_dyncol_tris[DYNCOL_MAX];
static EWRAM_BSS DynColGroup g_dyncol_groups[DYNCOL_GROUPS];
static int g_dyncol_tri_count, g_dyncol_group_count;
static EWRAM_BSS uint32_t g_dyncol_key[DYNCOL_GROUPS][2];
static EWRAM_BSS uint16_t g_dyncol_key_first[DYNCOL_GROUPS], g_dyncol_key_count[DYNCOL_GROUPS];
static EWRAM_BSS uint8_t g_dyncol_pending[DYNCOL_GROUPS];
static EWRAM_BSS uint8_t g_dyncol_pending_group[DYNCOL_GROUPS];
static EWRAM_BSS uint8_t g_dyncol_tri_pending[DYNCOL_MAX];
static bool g_dyncol_defer_on;
static EWRAM_BSS uint32_t g_dyncol_generation;
static EWRAM_BSS fx g_dyncol_near_slack;
static bool g_dyncol_full;

static void dyncol_mark_part(int slot);
static void dyncol_clear(void);
static inline void dyncol_hash_word(uint32_t h[2], uint32_t w);
static inline void dyncol_hash_final(uint32_t h[2]);
__attribute__((noinline)) static bool dyncol_segment_meets(const DynColGroup* d, v3 a, v3 b);
static inline bool dyncol_sphere_meets(const DynColGroup* d, v3 c, fx radius);
static inline const DynColGroup* dyncol_deferred_part(uint32_t k);
__attribute__((noinline)) static int dyncol_gather(v3 lo, v3 hi, uint16_t* index, int count, int max);
static inline v3 dyncol_point(const int16_t p[3], v3 pos, v3 side, v3 up, v3 front, const int16_t scale[3]);
static bool dyncol_mesh_beyond(const HoneyColMesh* m, v3 pos, int smax, v3 centre, fx near);
IWRAM_FN static int dyncol_build_triangles(const DynColTriangleJob* j);
IWRAM_FN static int dyncol_count_triangles(const DynColTriangleJob* j);
static int dyncol_defer_inputs(int* index, const HoneyColMesh* m, const uint16_t* codes, bool bee, v3 pos,
                               v3 side, v3 up, v3 front, const int16_t scale[3], uint16_t extra_attr);
__attribute__((noinline)) static void dyncol_build_deferred(int slot);
static bool dyncol_add(const HoneyColMesh* m, v3 pos, v3 up, v3 front, const int16_t scale[3], int owner,
                       uint16_t attr, v3 centre, fx near);
static int dyncol_owner(int tri);
static int dyncol_part(int tri);
