enum {
    SY_WAIT = 64,
    SY_FADEOUT_START,
    SY_BLANK_START,
    SY_FADEIN_START,
    SY_TALK_START,
    SY_READY,
    SY_COUNTDOWN,
    SY_SWIM,
    SY_EMIT,
    SY_STAR,
    SY_END,
    SY_ALL_RINGS,
    SY_FADEOUT_RETRY,
    SY_BLANK_RETRY,
    SY_FADEIN_RETRY,
    SY_TALK_RETRY,
    SY_FORCE_KILL,
    SY_HIDE_SHORE,
    SY_SHORE,
    SY_WAIT_NORMAL,
    SY_TALK_NORMAL,
    SY_START
};

static bool missions_reveal_star_at(int object, v3 position);
static void syati_init(Actor* a, const StageObject* o);
static void syati_think(Actor* a);
static void prizering_think(Actor* b);
static bool syati_counting(void);
static void syati_draw_hud(void);
static void syati_talk_ended(Actor* a);
