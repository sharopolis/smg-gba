#define REG_BLDALPHA REG16(0x04000052)

enum {
    MISSION_NORMAL,
    MISSION_COMET_RED,
    MISSION_COMET_GHOST,
    MISSION_COMET_PURPLE,
    MISSION_COMET_QUICK,
    MISSION_HIDDEN,
    MISSION_COMET_DARK
};

static const uint8_t mission_kinds[GALAXY_COUNT][6];
static const GameScene scenario_select_scene;

static void screen_fade_start(int frames, int dir, bool white);
static void scenario_select_open(int galaxy);
static void scenario_select_set_fade(Game* game);
