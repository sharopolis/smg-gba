enum { THROWING_ICE_IDLE = 112, THROWING_ICE_THROW, THROWING_ICE_FALL };

static void throwing_ice_star_hit(Actor* a);
static bool throwing_ice_pool_init(Actor* host);
static void throwing_ice_emit(Actor* a, v3 from, v3 target, v3 path_up);
__attribute__((noinline)) static void throwing_ice_think(Actor* a);
static void throwing_ice_figure(const Actor* a, Figure* f);
