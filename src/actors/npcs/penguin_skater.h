enum {
    PS_REACTION = 64,
    PS_TALK,
    PS_WAIT,
    PS_DEMO,
    PS_AWAY,
    PS_TURN,
    PS_PROVOKE,
    PS_CAUGHT,
    PS_FADEOUT,
    PS_FADEIN,
    PS_GIVEUP,
    PS_SITDOWN,
    PS_SWITCH
};

static void penguin_init(Actor* a, const StageObject* o);
static bool penguin_unclipped(const Actor* a);
static bool penguin_talk_busy(Actor* a);
static int penguin_talk_pose(const Actor* a, int anim);
static bool penguin_talk_held(const Actor* a);
static void penguin_talk_ended(Actor* a);
static void penguin_think(Actor* a);
