enum { ICEKING_PART_DEAD = 180, ICEKING_PART_LIVE };

typedef struct {
    int16_t roll[4];
    v3 gravity_up, ground_normal;
    int32_t height, shadow;
    uint16_t demo_tick;
    uint8_t stage, owner, ready, opened, held, demo, first_pinch, pinch;
    uint8_t weapon[6], child[2], shock, broken;
} IceKingState;

static EWRAM_BSS IceKingState g_iceking;

__attribute__((noinline)) static v3 iceking_limit_bind(Actor* a, v3 from, v3 centre, fx radius, bool child);
static Actor* iceking_part(int slot, int type);
static void iceking_figure(const Actor* a, Figure* f);
static void iceking_init(Actor* a, const StageObject* object);
static void iceking_reset(void);
static bool iceking_unclipped(const Actor* a);
static void iceking_shock_think(Actor* p);
static void iceking_break_think(Actor* p);
static void iceking_think(Actor* a);
