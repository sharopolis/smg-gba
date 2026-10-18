typedef struct {
    v3 horizontal;
    int32_t vertical;
    uint16_t timer, after;
    uint8_t phase, bounce, lock, wall;
} MarioFireState;

static EWRAM_BSS MarioFireState g_fire;

static void mario_fire_reset(void);
static bool mario_fire_hit(v3 normal, bool wall);
static void mario_fire_close(void);
static bool mario_fire_tick(const KeyState* keys);
