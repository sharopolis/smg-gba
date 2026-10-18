enum {
    KARIKARI_FALL = 64,
    KARIKARI_LAND,
    KARIKARI_WAIT,
    KARIKARI_PREPURSUE,
    KARIKARI_PURSUE,
    KARIKARI_WATCH,
    KARIKARI_CLING,
    KARIKARI_BLOWOUT
};

static void karikari_blow_out(Actor* a);
static void karikari_think(Actor* a);
static void karikari_init(Actor* a, const StageObject* o);
static void karikari_blown_away(Actor* a, Actor* by);
