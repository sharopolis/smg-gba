enum {
    METBO_NONACTIVE = 200,
    METBO_WAIT,
    METBO_WALK,
    METBO_SEARCH,
    METBO_CHASESTART,
    METBO_CHASE,
    METBO_GUARD,
    METBO_PRESS,
    METBO_BLOW,
    METBO_ATTACK,
    METBO_REST,
    METBO_SWOONSTART,
    METBO_SWOONLAND,
    METBO_SWOONSPIN,
    METBO_SWOONWAIT,
    METBO_RECOVER,
    METBO_DPDSWOON
};

enum {
    METBO_MSG_LOCKON,
    METBO_MSG_REFLECT,
    METBO_MSG_HIPDROP,
    METBO_MSG_TRAMPLE,
    METBO_MSG_SPIN,
    METBO_MSG_INVINCIBLE,
    METBO_MSG_EXPLOSION,
    METBO_MSG_INHALE
};

static bool metbo_message(Actor* a, int message, v3 sender);
static void metbo_init(Actor* a, const StageObject* o);
__attribute__((noinline)) static bool metbo_unclipped(const Actor* a);
static void metbo_think(Actor* a);
static void metbo_star_hit(Actor* a);
static void metbo_animate(Actor* a);
