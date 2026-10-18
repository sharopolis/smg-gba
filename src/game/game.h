typedef struct {
    uint8_t name_art;
    int stage;
    int mission_count;
    uint8_t mission_arts[6];
    uint8_t star_ids[6];
    uint8_t starts[6];
    uint8_t plate_art;
} GalaxyInfo;

enum {
    GALAXY_HONEYHIVE,
    GALAXY_GOODEGG,
    GALAXY_BEACHBOWL,
    GALAXY_FREEZEFLAME,
    GALAXY_DEEPDARK,
    GALAXY_GUSTYGARDEN,
    GALAXY_BUOYBASE,
    GALAXY_COUNT
};

static const GalaxyInfo galaxies[GALAXY_COUNT];

static int galaxy_for_stage(int stage);
static bool game_star_collected(int stage, int star_id);
static int game_stage_music(void);
static int game_current_scenario(void);
static void game_load_stage(int index, int start, int scenario);
static void game_reload_keep_health(int index, int start, int scenario);
static void game_open_missions(int galaxy);
static void game_collect_star(int star_id);
