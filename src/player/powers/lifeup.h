enum {
    LIFEUP_NORMAL,
    LIFEUP_POWER,
    LIFEUP_MOVE,
    LIFEUP_COUNTER,
    LIFEUP_BREAK,
    LIFEUP_RETURN,
    LIFEUP_DAMAGE,
    LIFEUP_RECOVER
};

typedef struct {
    int16_t follow_x, follow_y;
    uint16_t step, scene_ticks, image;
    uint8_t pending, paused_frame, phase, count, previous, break_count, tile_dirty, sync_death;
} MarioLifeUp;

static EWRAM_BSS MarioLifeUp g_lifeup;

static bool mario_lifeup_dark(void);
static int mario_lifeup_max(void);
static bool mario_lifeup_meter_active(void);
static void mario_lifeup_set_meter(int phase);
static void mario_lifeup_health_changed(int previous);
static void mario_lifeup_reset(void);
static void mario_lifeup_pickup(void);
static void mario_lifeup_sync_death(void);
static void mario_lifeup_meter_tick(void);
