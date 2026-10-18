typedef struct {
    uint8_t alpha;
    int8_t color;
    uint16_t depth;
    uint8_t blue;
    uint8_t count;
    uint16_t base;
    uint8_t side;
} OceanBowlState;

static bool g_oceanbowl_split;
static EWRAM_BSS OceanBowlState g_oceanbowl;

static void oceanbowl_reset(void);
static void oceanbowl_tick(void);
static void oceanbowl_begin(void);
__attribute__((noinline)) static void oceanbowl_queue(const CameraView* view);
static void oceanbowl_queue_disc(const CameraView* view);
__attribute__((noinline)) static void oceanbowl_draw_surface(void);
