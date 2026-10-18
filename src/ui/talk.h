#define OVL_PAL       15
#define OVL_TILE0     (HUD_STATIC_TILES + 128)
#define OVL_OBJ_32x16 (1 << 2 | 2)

enum {
    OVL_REGION_BALLOON,
    OVL_REGION_BEAK,
    OVL_REGION_ABTN0,
    OVL_REGION_ABTN1,
    OVL_REGION_SHORT_BEAK,
    OVL_REGION_PROMPT,
    OVL_REGION_POINTER,
    OVL_REGION_HAND,
    OVL_REGION_COUNT
};

static void ovl_reset(void);
static void ovl_palette(int which);
static void ovl_upload(int region, int img);
static void ovl_draw(int region, int x, int y);
static void talk_reset(void);
static bool talk_active(void);
static bool talk_actor_normal_talking(const Actor* a);
static void talk_reset_actor(Actor* a);
static void talk_forward_actor(Actor* a);
static void talk_actor_think(Actor* a);
static bool talk_force_actor(Actor* a);
static bool talk_tick(const KeyState* keys);
static void talk_tico_delight(Actor* tico);
static void talk_camera_begin(void);
static void talk_camera(void);
static void talk_draw(void);
