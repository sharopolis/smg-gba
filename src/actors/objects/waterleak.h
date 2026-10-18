enum { WL_WAIT = 190, WL_FREEZE, WL_ICE_DEAD, WL_ICE_APPEAR, WL_ICE_BREAK };

static const WaterLeakSpec* waterleak_find_spec(int id);
static bool waterleak_init_spec(Actor* a, const StageObject* o, const WaterLeakSpec* spec);
static void waterleak_init(Actor* a, const StageObject* o);
static bool waterleak_unclipped(const Actor* a);
static void waterleak_think(Actor* a);
static bool waterleak_collision_one(int i, v3 centre);
static void waterleak_collision(void);
static void waterleak_pipe_figure(const Actor* a, Figure* f);
