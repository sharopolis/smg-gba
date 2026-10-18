enum {
    TR_WAIT_START = 80,
    TR_DEMO_RUN,
    TR_DEMO_JUMP_START,
    TR_DEMO_JUMP,
    TR_DEMO_END,
    TR_WAIT,
    TR_RUN,
    TR_JUMP_START,
    TR_JUMP,
    TR_BRAKE,
    TR_FALL,
    TR_FALL_LAND,
    TR_FALL_GETUP,
    TR_COMEBACK_START,
    TR_COMEBACK,
    TR_TUMBLE,
    TR_GETUP,
    TR_CAUGHT,
    TR_CAUGHT_LAND,
    TR_CAUGHT_EVENT,
    TR_STAR_DEMO,
    TR_GIVE_UP
};

static v3 rabbit_contact_centre(const Actor* a);
static bool rabbit_unclipped(const Actor* a);
static bool rabbit_actor_init(Actor* a, const StageObject* o);
static void rabbit_star_piece(Actor* a);
static void rabbit_talk_ended(Actor* a);
static void rabbit_think(Actor* a);
static void trick_rabbit_think(Actor* a);
static int trick_rabbit_talk_pose(const Actor* a, int anim);
