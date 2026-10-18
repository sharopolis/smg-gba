#define ACTOR_THINK_RANGE FX(4200)

enum { AS_IDLE, AS_ACTIVE, AS_STUNNED, AS_DEFEATED, AS_COLLECTED, AS_CHASE, AS_ATTACK, AS_WAIT };
enum { FIREBALL_THROW = 64, FIREBALL_ARC, FIREBALL_REFLECT };

typedef struct {
    bool (*draw)(uint8_t* framebuffer, Figure* figure, const CameraView* view);
    bool (*view_bounds)(const Figure* figure, const CameraView* view);
    bool (*wide_visible)(const Figure* figure, const CameraView* view, v3 position, fx depth);
    bool (*both_sides)(const Figure* figure);
} FigureOps;

enum {
    KIND_CAMERA_VECTOR = 1 << 0,
    KIND_NO_POINTER = 1 << 1,
    KIND_EXTENT_IN_RANGE = 1 << 2,
    KIND_ALWAYS_NEAR = 1 << 3,
    KIND_NEAR_FIRST = 1 << 4,
    KIND_YAW_FROM_VALUE = 1 << 5,
    KIND_YAW_FROM_VELOCITY = 1 << 6,
    KIND_OWN_THINK = 1 << 7,
    KIND_RESTS = 1 << 8,
    KIND_ENEMY = 1 << 9,
    KIND_OWN_SCALE = 1 << 10,
    KIND_SPIN_SYNCED = 1 << 11,
};

enum {
    NPC_NO_TURN = 1 << 0,
    NPC_SLOW_TURN = 1 << 1,
    NPC_FAR_REACH = 1 << 2,
    NPC_KIND_ANIMS = 1 << 3,
    NPC_OWN_CAMERA = 1 << 4,
    NPC_KEEPS_NODE = 1 << 5,
    NPC_SHORT_KEEPS_NODE = 1 << 6,
};

typedef struct {
    int8_t anim;
    uint8_t flags;
    int (*pose)(const Actor* a, int anim);
    bool (*busy)(Actor* a);
    void (*event)(Actor* a, int arg);
    void (*ended)(Actor* a);
    int (*node)(const Actor* a, int node);
    int (*count)(const Actor* a);
    bool (*held)(const Actor* a);
} TalkKind;

typedef struct {
    const uint8_t* figure;
    fx scale;
    fx radius;
    fx height;
    uint8_t spin;
    uint16_t flags;
    int8_t reaction_anim;
    const FigureOps* figure_ops;
    const TalkKind* talk;
    void (*init)(Actor* a, const StageObject* o);
    void (*spawned)(Actor* a, const StageObject* o);
    bool (*present)(const StageObject* o);
    void (*think)(Actor* a);
    bool (*idle)(const Actor* a);
    void (*animate)(Actor* a);
    void (*blown_away)(Actor* a, Actor* by);
    void (*star_piece)(Actor* a);
    void (*star_piece_gift)(Actor* a);
    void (*adjust_figure)(const Actor* a, Figure* f);
    void (*pose_figure)(const Actor* a, Figure* f);
    void (*orient_figure)(const Actor* a, Figure* f);
    void (*prepare_draw)(const Actor* a, Figure* f);
    int (*draw_cost)(const Actor* a);
    fx (*draw_extent)(const Actor* a);
    bool (*hidden)(const Actor* a);
    fx (*pointer_radius)(const Actor* a);
    bool (*pointer_target)(const Actor* a);
    bool (*pointer_preferred)(const Actor* a);
    bool (*pointer_swipe)(const Actor* a);
    bool (*pointer_blocks_shot)(const Actor* a);
    bool (*pull_target)(const Actor* a);
} ActorKind;

typedef struct {
    const uint8_t* model;
    const FigureOps* ops;
} ExtraFigureKind;

static const uint8_t* const tico_figures[6];
static const uint8_t* const starbit_figures[6];
static const ActorKind actor_kinds[OBJ_TYPE_COUNT];
static const ExtraFigureKind extra_figure_kinds[1];
