enum { WORM_PLANET_WAIT = 64, WORM_PLANET_EATING, WORM_PLANET_REST };
enum { CATERPILLAR_WRIGGLE = 64, CATERPILLAR_REST, CATERPILLAR_END };

static Actor* actor_find_type(int type);
static void worm_planet_init(Actor* a, const StageObject* o);
static void worm_planet_think(Actor* a);
static void caterpillar_think(Actor* a);
static void worm_hill_think(Actor* a);
