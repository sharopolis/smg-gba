enum {
    KAMELLA_DORMANT = 64,
    KAMELLA_WAITAPPEAR,
    KAMELLA_START,
    KAMELLA_WAIT,
    KAMELLA_HIDESTART,
    KAMELLA_HIDEMOVE,
    KAMELLA_HIDEEND,
    KAMELLA_ATTACKWAIT,
    KAMELLA_ATTACK,
    KAMELLA_GUARD,
    KAMELLA_DAMAGE,
    KAMELLA_POWERUP,
    KAMELLA_DOWN,
    KAMELLA_SUMMONWAIT,
    KAMELLA_SUMMON
};

enum {
    KAMECK_BEAM_FOLLOW = 64,
    KAMECK_BEAM_SHOOT,
    KAMECK_BEAM_EXPLOSION,
    KAMECK_BEAM_TURTLE,
    KAMECK_BEAM_FIRE
};

typedef struct {
    int16_t boss, barrier, barrier_set, charged, children[2], held_shell;
    uint8_t round, pattern, hops, casts;
    bool demo;
} KamellaState;

static EWRAM_BSS KamellaState g_kamella;
static const FigureOps kamella_barrier_figure_ops;

static bool kamella_holding_shell(void);
static void kamella_reset(void);
static void kamella_init(Actor* a, const StageObject* o);
static void kameck_init(Actor* a, const StageObject* o);
static void jet_turtle_init(Actor* a, const StageObject* o);
static void kamella_damage_drop(void);
static bool kamella_carry_anim(void);
static bool kamella_camera_transparent_triangle(int triangle);
static void kamella_dispatch(Actor* a);
__attribute__((noinline, optimize("Os"))) static bool kamella_camera_points(v3* points, int* count, v3* up,
                                                                            v3* arena_front);
static void kameck_beam_figure(const Actor* a, Figure* f);
