#define BEE_AIR_WALK_TIME 180

typedef struct {
    bool on;
    int fly_time;
    int power;
    int32_t drift;
    int revive;
    bool flying;
    bool flapping;
    int wall;
    int wall_cooldown;
    v3 wall_up;
} BeeState;

static EWRAM_BSS BeeState g_bee;

static inline bool bee_active(void);
static int bee_fly_time(void);
static void bee_start(void);
static void bee_end(void);
static int bee_anim(int anim);
static bool bee_update_gravity(void);
static bool bee_ground_anim(void);
static v3 bee_air_physics(const KeyState* keys);
static inline bool bee_owns_air(void);
static void bee_reset(void);
