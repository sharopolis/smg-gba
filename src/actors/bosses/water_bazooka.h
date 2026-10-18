enum {
    WATER_BAZOOKA_WAIT = 64,
    WATER_BAZOOKA_AIM,
    WATER_BAZOOKA_AIMEND,
    WATER_BAZOOKA_SHOT,
    WATER_BAZOOKA_SHOTNM,
    WATER_BAZOOKA_TIRE,
    WATER_BAZOOKA_LAUGH,
    WATER_BAZOOKA_CRACK,
    WATER_BAZOOKA_ANGER,
    WATER_BAZOOKA_BREAKWAIT,
    WATER_BAZOOKA_BREAKSIGN,
    WATER_BAZOOKA_EXPLODE,
    WATER_BAZOOKA_BREAKEND,
    WATER_BAZOOKA_PANIC,
    WATER_BAZOOKA_STORM
};

static void water_bazooka_think(Actor* a);
static void mogucchi_shooter_think(Actor* m);
static void water_bazooka_capsule_think(Actor* c);
