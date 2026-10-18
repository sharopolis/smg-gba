typedef struct {
    int16_t rise_step, hatch_step, bind;
    uint8_t hatch_open;
} BuoyBaseState;

enum { BUOY_RISE_CAMERA = 28, BUOY_RISE_STEPS = 390 };

static EWRAM_BSS BuoyBaseState g_buoy;
static const FigureOps buoy_hatch_figure_ops;
static const FigureOps buoy_floater_figure_ops;
static const LevelHooks buoy_level;

static bool buoy_stage(void);
static bool buoy_cage_star_owner(const Actor* a);
static bool buoy_cage_star_busy(const Actor* a);
static void buoy_cage_star_init(Actor* a, const StageObject* o);
static void buoy_cage_star_tick(Actor* a);
__attribute__((noinline)) static bool buoy_floater_unclipped(const Actor* a);
__attribute__((noinline)) static fx buoy_figure_radius(const Figure* figure);
static void buoy_reset(void);
static bool buoy_no_yaw_kind(int type);
static bool buoy_actor_init(Actor* a, const StageObject* o);
static void buoy_parts_hidden(void);
static bool buoy_rise_camera_switch(int old_chunk, int new_chunk);
static bool buoy_camera_path_blocked(v3 from, v3 to);
static void buoy_events_tick(void);
static void buoy_actors_pre(void);
static void buoy_actors_post(void);
static bool chasing_killer_hit_target(Actor* p);
static bool chasing_killer_ray_hit_cage(int tri);
static bool chasing_killer_cage_target(Actor* a, v3* aim);
static bool chasing_killer_can_turn(Actor* a, v3 aim);
static void chasing_killer_ground_aim(Actor* a, v3 gravity_up, v3* aim);
static bool buoy_bubble_unclipped(const Actor* a);
static bool buoy_actor_think(Actor* a);
static bool floater_rise_active(void);
static void floater_rise_events(void);
static bool floater_rise_tick(const KeyState* keys);
