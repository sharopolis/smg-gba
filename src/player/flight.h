enum { FLIGHT_OFF, FLIGHT_CAPTURE, FLIGHT_SHOOT_START, FLIGHT_SHOOT };

#define FLIGHT_BIND_TIME 90

enum { STAR_WAIT = 0, STAR_COOLDOWN = 1, STAR_BOUND = 2 };

typedef struct {
    int phase, step;
    int16_t star;
    int16_t star_object;
    bool sling, starter;
    uint8_t gravity_turn;
    int flight_time;

    int path;
    int rail;
    v3 start;
    v3 rail_offset;
    v3 par_y, par_z;
    fx par_a, par_b, par_h;
    fx speed;
    v3 init_up, init_front, init_side;

    v3 pos, vel;
    v3 pos16, vel16;
    v3 from;
    v3 head, front;
    v3 head0, front0;
    fx roll, roll_total, flip;
    int t_loop, t_end, t_fall;
    int landing;
    int camera, cameras;
} Flight;

static EWRAM_BSS Flight g_flight;

__attribute__((pure, noinline)) static bool flight_default_branch(const StageObject* o);
static bool flight_prefetch_near(v3 pos);
static bool flight_resident_wait_clock(const Actor* a);
static bool flight_star_keep(const Actor* a);
static void flight_parabola_params(float h, float d, float* a, float* b);
static bool flight_takeover_accepts(const Actor* a);
static void flight_star_spawned(Actor* a, const StageObject* o);
static void flight_star_think(Actor* a);
static void flight_apply_pose(Figure* figure);
static void flight_bind(Actor* a, bool capture);
static void flight_tick(void);
static bool flight_start_scenario(int object);
static bool flight_land(void);
static void flight_water_landed(void);
static inline bool flight_input_locked(void);
static inline bool flight_free_sling_power_jump(void);
static __attribute__((noinline)) v3 flight_sling_gravity(v3 previous_up, v3 sampled_up, v3* jump_vec);
static __attribute__((noinline)) int32_t flight_sling_gravity_scale(v3 up, v3* jump_vec);
static void flight_reset(void);
