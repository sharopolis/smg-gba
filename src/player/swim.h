typedef struct {
    int32_t speed, buoyancy;
    int16_t stroke, spin, oxygen, oxygen_delay, drown, cold, age, buoyancy_delay;
    int16_t pitch;
    int16_t pitch_input, turn_input;
    uint8_t active, dive, surface_full, surface_hold, spin_phase, body_immediate;
    int16_t body_front[3], body_up[3];
    bool rise_armed;
} SwimPhysics;

static EWRAM_BSS SwimPhysics g_swim;

static void mario_swim_reset(void);
static int mario_swim_life(void);
static void mario_swim_recover(void);
__attribute__((noinline)) static void mario_swim_body_tick(void);
static void mario_swim_apply_pose(Figure* figure);
static v3 mario_swim_camera_front(v3 front, v3 up);
static bool mario_swim_tick(const KeyState* keys);
