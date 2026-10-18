#define ICESTEP_BASE  0x7200
#define ICESTEP_SLOTS 20

enum { ICESTEP_WAIT = 80, ICESTEP_END, ICESTEP_HIT };

typedef struct {
    int16_t vertex[ICESTEP_SLOTS][14][3];
    int16_t normal[ICESTEP_SLOTS][ICESTEP_PRISMS][3];
    uint32_t normal_valid[ICESTEP_SLOTS];
    uint32_t triangle_valid[ICESTEP_SLOTS];
} IceStepCollisionCache;

static EWRAM_BSS int8_t g_icestep_actor[ICESTEP_SLOTS];
static EWRAM_BSS uint8_t g_icestep_alive, g_icestep_next;
static EWRAM_BSS CollisionTriangle g_icestep_triangle;

static bool icestep_stage(void);
static void icestep_cache_invalidate(void);
static void icestep_cache_end_logic(void);
static void icestep_reset(bool level_has_ice_steps);
static Actor* icestep_actor(int slot);
static int icestep_owner(int tri);
static const CollisionTriangle* icestep_tri(int index);
static int icestep_gather(v3 lo, v3 hi, uint16_t* indices, int count, int max);
