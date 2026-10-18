typedef struct {
    uint8_t star_out[8];
    v3 star_from[8];
    int8_t kinopio_done;
    uint8_t chips[4];
    int16_t kinopio;
} HoneyMission;

static EWRAM_BSS HoneyMission g_honey;
static const LevelHooks honey_level;

static inline bool honey_stage(void);
static bool honey_handles_actor(const Actor* a);
static int honey_mission_stage(int stage, int mission);
static void honey_reset(void);
static void honey_star_appear_at(int id, v3 from);
static bool honey_star_busy(const Actor* a);
static void honey_actor_spawned(Actor* a, const StageObject* o);
static void honey_kind_think(Actor* a);
static void honey_kinopio_begin(Actor* a);
static void honey_kinopio_think(Actor* a);
static void honey_actors_pre(void);
static void honey_actors_post(void);
static bool cosmic_bound(void);
static void cosmic_reset(void);
static void cosmic_miss(void);
static void cosmic_tick(const KeyState* keys);
static void cosmic_apply_pose(void);
static void cosmic_camera(void);
static void cosmic_win(void);
static void cosmic_add_figure(void);
static void cosmic_draw_hud(void);
