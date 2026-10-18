typedef struct {
    v3 position;
    v3 x, y, z;
    int32_t scale;
} MoverFrame;

typedef struct {
    const int16_t (*tris)[9];
    int count, capacity;
    int key;
    uint32_t used_tick;
    MoverFrame frame;
    bool placed;
} MoverSet;

static EWRAM_BSS MoverSet g_movers[COL_DYN_SETS];

static int movers_alloc(int key, const int16_t (*tris)[9], int count);
static inline v3 mover_to_world(const MoverFrame* f, v3 local);
static bool mover_carries_mario(int s);
static void movers_place(int s, const MoverFrame* f);
static void movers_enable(int s, bool on);
static MoverFrame mover_frame_of(v3 position, v3 up, v3 front, int32_t scale14);
