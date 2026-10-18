typedef struct {
    int16_t entry, out_index, timer, hide_timer;
    uint8_t phase, input_off;
    v3 from_pos, from_front, from_up, entry_front;
    v3 exit_pos, exit_up, exit_front;
    int16_t exit_h, exit_v;
} PipeTravel;

static EWRAM_BSS PipeTravel g_pipe;

static void earthenpipe_reset(void);
static void earthenpipe_init(Actor* a, const StageObject* o);
static void earthenpipe_think(Actor* a);
static void earthenpipe_global_tick(void);
static void earthenpipe_figure(const Actor* a, Figure* f);
