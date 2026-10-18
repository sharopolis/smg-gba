enum {
    BEGOMAN_BABY_LAUNCH = 180,
    BEGOMAN_BABY_WAIT,
    BEGOMAN_BABY_SIGN,
    BEGOMAN_BABY_PURSUE,
    BEGOMAN_BABY_TURN,
    BEGOMAN_BABY_BRAKE,
    BEGOMAN_BABY_TRAMPLE,
    BEGOMAN_BABY_HIT,
    BEGOMAN_BABY_PROVOKE,
    BEGOMAN_BABY_TIRED,
    BEGOMAN_BABY_BLOW,
    BEGOMAN_BABY_WATER,
    BEGOMAN_BABY_NOCALC,
    BEGOMAN_BABY_KEEP,
    BEGOMAN_BABY_BIND,
    BEGOMAN_BABY_AFTER_LAUNCH,
    BEGOMAN_BABY_AFTER_GROUND
};

static void baby_attack_permit_reset(void);
static Actor* actor_at_slot(int slot, int type);
static int mario_sphere_contact(v3 centre, v3 up, fx radius, bool spin);
static void jumpguarder_init(Actor* a, const StageObject* o);
static void jumpguarder_pre_update(void);
static void begoman_baby_kill(Actor* b, bool items);
static void begoman_baby_bind(Actor* b, bool rebound);
static bool jumpguarder_dispatch(Actor* a);
static bool __attribute__((noinline)) jumpguarder_unclipped(const Actor* a);
