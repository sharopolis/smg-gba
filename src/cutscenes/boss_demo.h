typedef struct {
    int16_t owner;
    int16_t star;
    int16_t star_music;
    uint8_t flags;
    int16_t star_step;
    v3 cam_eye;
    v3 peep;
} BossDemo;

#define BOSS_AFTER_BGM MUSIC_MBGM_STAR_EXIST

static EWRAM_BSS BossDemo g_boss_demo;
static const fx boss_camera_zooms[CAMERA_ZOOM_COUNT];

static void boss_demo_reset(void);
static bool boss_demo_active(void);
static Actor* boss_demo_actor(void);
static void boss_demo_begin(const Actor* owner, bool camera);
static void boss_demo_end(v3 pos, v3 up, v3 front);
static bool boss_demo_camera(Camera* cam);
static void boss_demo_set_camera(v3 eye, v3 watch, v3 up, int32_t fovy_tan4096);
static bool boss_area_camera(Camera* cam);
static bool star_appear_busy(const Actor* a);
static bool star_appear_done(void);
static bool star_appear_think(Actor* a);
static void star_appear_figure(const Actor* a, Figure* f);
static bool boss_power_star_appear(const Actor* boss, v3 from, fx radius, bool star_camera, int music);
static bool boss_demo_cast(const Actor* demo, const Actor* a);
