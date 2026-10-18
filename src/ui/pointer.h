#define PTR_GIFT   0x20
#define PTR_SHOT   0x40
#define PTR_HOMING 0x80

static inline bool pointer_actor_hook(Actor* a);
static void pointer_reset(void);
static bool pointer_active(void);
static void pointer_screen(int* x, int* y);
static bool pointer_pressed(void);
static bool pointer_held(void);
static bool pointer_shoot_held(void);
static bool pointer_shoot_pressed(void);
static void pointer_set_hand(int hand);
static void pointer_world_ray(v3* origin, v3* dir);
static bool pointer_is_pointing(const Actor* a, fx radius3d);
static bool pointer_fireball_reflect(Actor* a, v3* direction);
static Actor* pointer_pull_selected(void);
static bool pointer_pull_camera_targets(v3* selected, v3* held);
static int pointer_pull_nearest(void);
static void pointer_filter_keys(KeyState* keys);
static bool pointer_gift_starbit(Actor* target);
static bool pointer_actor_think(Actor* a);
static void pointer_tick(void);
static void pointer_draw(void);
