#define BURROWS_HILLS_KEPT 8

static int actor_tick_random(const Actor* a, int lo, int hi);
static void molehill_trail(Actor* owner, v3* reserve, int16_t* seq, int16_t* ramp, fx every, int keep);
static void molehill_think(Actor* h);
static void mogucchi_init(Actor* a, const StageObject* o);
static void mogucchi_think(Actor* a);
