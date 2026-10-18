enum {
    GALAXY_MAP_SELECT_START,
    GALAXY_MAP_SELECT,
    GALAXY_MAP_SELECT_CANCEL,
    GALAXY_MAP_CONFIRM_START,
    GALAXY_MAP_CONFIRM,
    GALAXY_MAP_CONFIRM_CANCEL,
    GALAXY_MAP_LAUNCH_APPEAR,
    GALAXY_MAP_LAUNCH_START,
    GALAXY_MAP_LAUNCH_JUMPOUT,
    GALAXY_MAP_LAUNCH_END
};

#define GALAXY_MAP_CONFIRM_FRAMES 45
#define DEG_TO_TURN16(d)          ((int32_t)((d) * 65536.0f / 360.0f))

typedef struct {
    uint8_t nerve, vram, from_dome, slot_art;
    int16_t step;
    int8_t pointed, selected;
    int8_t zoom[AD_MINI_COUNT];
    uint8_t zoom_in[AD_MINI_COUNT];
    int32_t orbit[AD_MINI_COUNT];
    int32_t spin[AD_MINI_COUNT];
    int32_t handle_rot, handle_speed;
    v3 handle_up, handle_front;
    v3 cam_start, cam_from_pos, cam_from_target, cam_from_up, cam_to_pos, cam_to_target, cam_to_up;
    v3 eye, target, up;
    v3 cf, cu, cl;
    int16_t fovy_x10;
    v3 mario_pos, mario_up, mario_front;
    fx mario_spin;
    v3 star_pos, star_up, star_front;
    uint8_t mario_hidden, star_shown;
    uint8_t ring_colour;
} GalaxyMap;

static EWRAM_BSS GalaxyMap g_galaxy_map;
static const GameScene galaxy_map_scene;

static inline fx sin_turn16(int32_t turns);
static inline fx cos_turn16(int32_t turns);
static int32_t step_ratio14(int step, int frames);
static int32_t ease_in_out14(int32_t t);
static int32_t ease_out14(int32_t t);
static v3 v3_lerp14(v3 a, v3 b, int32_t t);
static v3 v3_scale14_wide(v3 dir, int32_t length);
static void galaxy_map_set_nerve(int nerve);
static void galaxy_map_open(void);
static void galaxy_map_camera_commit(void);
static void galaxy_map_camera_basis(v3* front, v3* up);
static void galaxy_map_point_move(const KeyState* keys);
static void galaxy_map_galaxies_tick(bool moving);
static void galaxy_map_clear_return(void);
static bool galaxy_map_return_from_missions(void);
