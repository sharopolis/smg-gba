enum {
    STINKBUG_SMALL_WAIT = 32,
    STINKBUG_SMALL_SEARCH,
    STINKBUG_SMALL_DASHSIGN,
    STINKBUG_SMALL_DASHSIGNEND,
    STINKBUG_SMALL_DASH,
    STINKBUG_SMALL_DASHEND,
    STINKBUG_SMALL_BACK,
    STINKBUG_SMALL_ATTACK,
    STINKBUG_SMALL_SPIN,
    STINKBUG_SMALL_PANIC,
    STINKBUG_SMALL_SHAKE,
    STINKBUG_SMALL_DOWN
};

static const FigureOps stinkbug_small_figure_ops;

static void stinkbug_small_think(Actor* a);
static void stinkbug_small_star_hit(Actor* a);
static void stinkbug_small_spawned(Actor* a, const StageObject* o);
