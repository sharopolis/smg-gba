enum {
    HAMMERHEAD_WAIT = 64,
    HAMMERHEAD_SEARCH,
    HAMMERHEAD_ATTACK,
    HAMMERHEAD_REST,
    HAMMERHEAD_RECOVER,
    HAMMERHEAD_DAMAGE,
    HAMMERHEAD_SWOON,
    HAMMERHEAD_RISE,
    HAMMERHEAD_CROW,
    HAMMERHEAD_KILL
};

static const FigureOps hammerhead_packun_figure_ops;

static void hammerhead_think(Actor* a);
static fx hammerhead_draw_extent(const Actor* a);
static void hammerhead_init(Actor* a, const StageObject* o);
static void hammerhead_blown_away(Actor* a, Actor* by);
