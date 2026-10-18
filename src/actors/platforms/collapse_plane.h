enum { CP_WAIT = 64, CP_COLLAPSE, CP_END };

static void collapse_init(Actor* a, const StageObject* o);
static void collapse_think(Actor* a);
static void collapse_figure(const Actor* a, Figure* f);
