enum { TAKOBO_WAIT = 64, TAKOBO_MOVE, TAKOBO_ATTACK, TAKOBO_HITREACT, TAKOBO_PRESS, TAKOBO_PUNCH };

static void takobo_init(Actor* a, const StageObject* o);
static void takobo_blown_away(Actor* a, Actor* by);
static void takobo_think(Actor* a);
