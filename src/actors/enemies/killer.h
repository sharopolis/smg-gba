enum { KILLERGUNNER_WAIT = 64, KILLERGUNNER_CHARGE, KILLERGUNNER_SHOOT };
enum {
    HOMINGKILLER_WAIT = 64,
    HOMINGKILLER_OUT,
    KILLER_APPEAR = 64,
    KILLER_MOVE,
    KILLER_CHASE,
    KILLER_TARGET,
    KILLER_CHASESTART,
    KILLER_MOVESTART
};

static void killergunner_think(Actor* a);
static void homingkiller_think(Actor* a);
static void killer_think(Actor* a);
static void killergunner_init(Actor* a, const StageObject* o);
static void homingkiller_init(Actor* a, const StageObject* o);
