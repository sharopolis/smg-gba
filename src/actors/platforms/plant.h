#define PLANT_POINTS            104
#define PLANT_LEAVES_PER_FIGURE 10
#define PLANT_LEAVES_POINTS     260
#define PLANT_SLOTS             2
#define PLANT_LEAF_FIGURES      2
#define PLANT_LEAVES            (PLANT_LEAF_FIGURES * PLANT_LEAVES_PER_FIGURE)

typedef struct {
    int16_t stage_index;
    int16_t leaves;
    fx length;
    fx pose_grown;
    fx leaf_coord[PLANT_LEAVES];
    v3 leaf_dir[PLANT_LEAVES];
    int32_t leaf_size[PLANT_LEAVES];
    uint32_t drawn_tick;
    int16_t pose[PLANT_POINTS][3];
    int16_t leaf_pose[PLANT_LEAF_FIGURES][PLANT_LEAVES_POINTS][3];
    Figure leaf_figure[PLANT_LEAF_FIGURES], seed_figure;
} PlantSlot;

typedef struct {
    int16_t actor;
    int16_t timer;
    uint8_t mode;
    bool grabbed_top;
    fx coord;
    int32_t speed;
    v3 front, up, pos;
} PlantRide;

static EWRAM_BSS PlantSlot g_plants[PLANT_SLOTS];
static EWRAM_BSS uint8_t g_plant_grown[(STAGE_MAX_OBJECTS + 7) / 8];
static EWRAM_BSS PlantRide g_plant_ride;

static void plant_reset(void);
static int plant_rand(int lo, int hi);
static void plant_init(Actor* a, const StageObject* o);
static void plant_think(Actor* a);
static void plant_pose(const Actor* a, Figure* f);
static void plant_extra_figures(void);
static void plant_ride_mario_pose(Figure* figure);
static int plant_draw_cost(const Actor* a);
