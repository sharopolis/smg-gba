enum { COCONUT_WAIT = 64, COCONUT_MOVE, COCONUT_GONE };
enum { COCONUT_SPUN = 0x10 };

static void coconut_think(Actor* a);
static void coconut_figure(const Actor* a, Figure* f);
static void coconut_init(Actor* a, const StageObject* o);
static bool coconut_idle(const Actor* a);
static void coconut_blown_away(Actor* a, Actor* by);
