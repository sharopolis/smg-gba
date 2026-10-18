enum {
    BLOCK_CAMERA_INPUT = 1 << 0,
    BLOCK_POINTER_INPUT = 1 << 1,
    BLOCK_POINTER_UPDATE = 1 << 2,
    ALLOW_RAIL_FLIGHT_INPUT = 1 << 3,
    BLOCK_PAUSE = 1 << 4,
    BLOCK_HUD = 1 << 5,
    BLOCK_COLLECTIBLES = 1 << 6,
    BLOCK_DEMO_START = 1 << 7,
    BLOCK_SCENE_START = 1 << 8,
    BLOCK_MARIO_ANIM = 1 << 9,
    BLOCK_CAMERA_FRAMING = 1 << 10,
    BLOCK_GROUND_ANIM = 1 << 11,
    BLOCK_GALAXY_CAMERA = 1 << 12,
    BLOCK_PLAYER = 1 << 13,
    BLOCK_INPUT = 1 << 14,
    BLOCK_INPUT_EXCEPT_START = 1 << 15,
    BLOCK_CAMERA_PLAYER_FOCUS = 1 << 16,
    BLOCK_EVENT_PLAYER_FOCUS = 1 << 17
};

static bool cutscene_tick(const KeyState* keys);
__attribute__((always_inline)) static inline bool cutscene_blocks(unsigned query);
static bool cutscene_draw_hud(void);
static Actor* cutscene_actor(void);
static bool cutscene_cast(const Actor* demo, const Actor* a);
static bool cutscene_camera(Camera* cam);
static bool cutscene_camera_target(v3* pos, v3* up, v3* front, v3* move);
