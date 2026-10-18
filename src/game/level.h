enum { UNCLIPPED_NO, UNCLIPPED_YES, UNCLIPPED_DEFAULT };
enum { STREAM_DEFAULT, STREAM_SKIP, STREAM_REQUIRED };

enum {
    LEVEL_FIXED_CAMERA = 1 << 0,
    LEVEL_FOLLOW_CAMERA = 1 << 1,
    LEVEL_OCEAN_BOWL = 1 << 2,
    LEVEL_RAISED_COUNTERS = 1 << 3,
    LEVEL_HUB_ROOM = 1 << 4,
    LEVEL_ICE_STEPS = 1 << 5,
    LEVEL_VOLCANO_DEMOS = 1 << 6,
    LEVEL_VALVE_DEMOS = 1 << 7,
    LEVEL_CAGES_BREAK_ON_ATTACK = 1 << 8,
    LEVEL_BEE_MUSHROOM_DEMOS = 1 << 9,
    LEVEL_CHIP_GROUPS_FROM_STAGE = 1 << 10,
};

typedef bool (*ActorTest)(const Actor* a);
typedef int (*StreamRule)(const StageObject* o);

typedef struct {
    int8_t pipe;
    uint8_t music;
} PipeMusic;

typedef struct {
    uint16_t first, count;
    uint16_t type;
    uint8_t scenarios, zone;
    const uint16_t* ids;
    uint8_t ids_step;
} ScenarioTail;

typedef struct {
    int galaxy_stage;
    uint8_t hidden_mission, hidden_scenario;
    uint8_t daredevil_mission;
    uint8_t intro_music_lead;
    uint16_t features;
    const PipeMusic* pipe_music;
    const ScenarioTail* scenario_tail;
    int (*mission_stage)(int stage, int mission);
    int (*music)(void);
    void (*reset)(void);
    void (*load_before_player)(void);
    void (*load_before_actors)(void);
    void (*before_player)(const KeyState* keys);
    void (*after_parts_update)(void);
    void (*after_map_parts)(void);
    void (*before_stream)(void);
    void (*before_actors)(void);
    void (*after_actors)(void);
    void (*after_camera)(void);
    void (*after_actor_cameras)(void);
    void (*after_player_pose)(void);
    void (*miss)(void);
    void (*star_get)(void);
    bool (*pull_back)(v3 position);
    bool (*pull_preview)(void);
    bool (*pull_target)(const Actor* a);
    bool (*camera_hold_height)(void);
    bool (*camera_blend_below)(void);
    bool (*camera_low_anchor)(const StageCamera* cp, int source, int chunk);
    bool (*camera_arena)(v3* points, int* count, v3* up, v3* front);
    void (*camera_arena_clear)(CollisionCandidates* candidates);
    bool (*camera_blend_checked)(int old_chunk, int new_chunk);
    bool (*camera_path_blocked)(v3 from, v3 to);
    bool (*launch_star_branch)(const StageObject* o);
    bool (*player_request)(const KeyState* keys);
    bool (*actor_init)(Actor* a, const StageObject* o);
    void (*actor_spawned)(Actor* a, const StageObject* o);
    bool (*actor_think)(Actor* a);
    int (*actor_unclipped)(const Actor* a);
    bool (*actor_bounds)(const Actor* a, v3* centre, fx* extent);
    void (*actor_figure)(const Actor* a, Figure* f);
    fx (*figure_margin)(const Figure* figure);
    bool (*power_star)(Actor* a);
    void (*star_appear)(int id, v3 from);
    StreamRule stream_rule;
    bool (*stream_important)(int type);
    ActorTest stream_keep;
    void (*add_figures)(void);
    bool (*player_figure)(Figure* figure);
    void (*draw_hud)(void);
} LevelHooks;

static EWRAM_BSS const LevelHooks* g_level;

static const LevelHooks* level_hooks(int stage);
static inline const LevelHooks* level_current(void);
static inline bool level_has(unsigned feature);
static void level_enter(int stage);
static void levels_reset(void);
static int level_mission_stage(int stage, int mission);
static int level_pipe_music(int pipe);
static uint32_t level_stream_count(const Stage* stage);
static void level_load_before_player(void);
static void level_load_before_actors(void);
static void level_before_player(const KeyState* keys);
static void level_after_parts_update(void);
static void level_after_map_parts(void);
static void level_before_stream(void);
static void level_before_actors(void);
static void level_after_actors(void);
static void level_after_camera(void);
static void level_after_actor_cameras(void);
static void level_after_player_pose(void);
static void level_miss(void);
static void level_star_get(void);
static bool level_pull_back(v3 position);
static bool level_pull_preview(void);
static bool level_pull_target(const Actor* a);
static bool level_camera_hold_height(void);
static bool level_camera_blend_below(void);
static bool level_camera_low_anchor(const StageCamera* cp, int source, int chunk);
static bool level_camera_arena(v3* points, int* count, v3* up, v3* front);
static void level_camera_arena_clear(CollisionCandidates* candidates);
static bool level_camera_blend_checked(int old_chunk, int new_chunk);
static bool level_camera_path_blocked(v3 from, v3 to);
static bool level_player_request(const KeyState* keys);
static bool level_actor_init(Actor* a, const StageObject* o);
static void level_actor_spawned(Actor* a, const StageObject* o);
static bool level_actor_think(Actor* a);
static int level_actor_unclipped(const Actor* a);
static bool level_actor_bounds(const Actor* a, v3* centre, fx* extent);
static void level_actor_figure(const Actor* a, Figure* f);
static fx level_figure_margin(const Figure* figure);
static bool level_power_star(Actor* a);
static void level_star_appear(int id, v3 from);
static StreamRule level_stream_rule(void);
static bool level_stream_important(int type);
static ActorTest level_stream_keep(void);
static void level_add_figures(void);
static bool level_player_figure(Figure* figure);
static void level_draw_hud(void);
