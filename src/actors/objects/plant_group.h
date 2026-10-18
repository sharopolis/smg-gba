enum { PG_IDLE = 64, PG_OUT, PM_WAIT = 64, PM_SHAKE };

static void plantgroup_think(Actor* a);
static void plantmember_think(Actor* a);
static void plantgroup_init(Actor* a, const StageObject* o);
