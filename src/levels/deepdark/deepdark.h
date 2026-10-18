static const LevelHooks deep_level;

static bool deep_actor_init(Actor* a, const StageObject* o);
static bool deep_actor_think(Actor* a);
static void deep_actors_pre(void);
__attribute__((noinline)) static bool deep_kind_unclipped(const Actor* a);
