enum { MP_WAIT = 64, MP_SCALE, MP_BREAK, MP_DONE };

static int marble_star_object(void);
static void marble_planet_think(Actor* a);
static void marble_electron_think(Actor* e);
static void marble_core_think(Actor* c);
static void marble_electron_figure(const Actor* a, Figure* f);
