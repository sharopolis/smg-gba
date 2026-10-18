typedef struct Actor {
    uint16_t type;
    uint8_t state, active;
    uint8_t flags;
    int16_t timer;
    int16_t stage_index;
    v3 position, velocity;
    v3 home;
    v3 up, front;
    v3 target;
    int16_t args[4];
    fx radius;
    fx anim_frame;
    int anim;
    Figure figure;
    int16_t (*pose)[3];
    int16_t health;
    int16_t value;
} Actor;

#define MAX_ACTORS 96


static void actor_modules_run(void (*const* steps)(void), unsigned count);
static void actor_scale_from_object(Actor* a, const StageObject* o);
static void actor_blow_away(Actor* v, Actor* by);
static Actor* actors_spawn_object(const Stage* stage, uint32_t i);
static void actors_load(const Stage* stage, int scenario);
static void actor_think(Actor* a);
static void actor_animate(Actor* a);
__attribute__((always_inline)) static inline bool actor_unclipped(const Actor* a);
static void actors_world_tick(void);
static void actors_tick(void);
static void actors_collect_figures(const CameraView* view);
