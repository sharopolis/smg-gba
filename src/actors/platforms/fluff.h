static EWRAM_BSS uint16_t g_fluff_cooldown;

static void fluff_reset(void);
static bool fluff_has_rider(void);
static bool fluff_init(Actor* a, const StageObject* o);
static void fluff_think(Actor* a);
static int fluff_rider_slot(void);
static void fluff_figure(const Actor* a, Figure* f);
