#define PACKUN_PETIT_SWOON_START     40
#define PACKUN_PETIT_SWOON           41
#define PACKUN_PETIT_SWOON_TO_THREAT 42

enum {
    PACKUN_PETIT_WAIT = 16,
    PACKUN_PETIT_FIND,
    PACKUN_PETIT_THREAT,
    PACKUN_PETIT_ATTACK,
    PACKUN_PETIT_HIT,
    PACKUN_PETIT_PRESS,
    PACKUN_PETIT_BLOW
};

static const FigureOps packun_petit_figure_ops;

static bool packun_petit_swoon_think(Actor* a);
static void packun_petit_think(Actor* a);
static void packun_petit_star_hit(Actor* a);
static void packun_petit_spawned(Actor* a, const StageObject* o);
