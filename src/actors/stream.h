#define STREAM_KEEP FX(6400)

enum { OBJECT_STATE_OUT = 1, OBJECT_STATE_GONE = 2, OBJECT_STATE_WAS_ACTIVE = 4, OBJECT_STATE_DEAD = 8 };

typedef struct {
    bool on;
    uint8_t scenario_bit;
    uint16_t sweep_count;
    uint32_t next;
} ObjectStream;

static EWRAM_BSS ObjectStream g_stream;
static EWRAM_BSS uint8_t g_object_state[STAGE_MAX_OBJECTS + 4];

static bool stream_launch_ready(int object);
static void stream_launch_ready_set(int object, bool ready);
static bool stream_object_maybe_out(int object);
static bool stream_object_collected(int object);
static unsigned stream_part_state(int object);
static void stream_part_state_set(int object, unsigned state);
static inline bool stream_near(v3 a, v3 b, fx r);
static Actor* actors_stream_free_slot(void);
static bool stream_important(int type);
static bool stream_evict_preserving(v3 mario, v3 burst_origin);
static bool stream_evict(v3 mario);
static void stream_spawn_near(const Stage* stage, v3 mario, uint32_t first, uint32_t count);
static bool actors_stream_start(const Stage* stage, int scenario);
static void actors_stream_tick(void);
