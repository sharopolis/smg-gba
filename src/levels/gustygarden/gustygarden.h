static const LevelHooks gusty_level;

static bool gusty_star_busy(const Actor* a);
static bool gusty_star_unclipped(const Actor* a);
static void gusty_star_appear(int id, v3 from);
static bool dig_planet_init(Actor* a, const StageObject* o);
static void dig_planet_think(Actor* a);
static bool dig_planet_unclipped(const Actor* a);
static bool dig_planet_no_yaw(const Actor* a);
static fx dig_planet_yaw(const Actor* a);
static fx dig_planet_extent(const Actor* a);
static bool dig_planet_camera(void);
