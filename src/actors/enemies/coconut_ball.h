enum { COCONUT_BALL_THROW = 64, COCONUT_BALL_TO_HOST, COCONUT_BALL_REBOUND };

static Actor* coconut_ball_throw(Actor* host, v3 from, v3 at, int32_t speed);
static void coconut_ball_think(Actor* a);
static void coconut_ball_figure(const Actor* a, Figure* f);
