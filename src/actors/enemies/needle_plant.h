enum { NEEDLEPLANT_WAIT = 64, NEEDLEPLANT_SHAKE };

static void needleplant_kill(Actor* a);
static void needleplant_think(Actor* a);
static void needleplant_init(Actor* a, const StageObject* o);
static void needleplant_blown_away(Actor* a, Actor* by);
