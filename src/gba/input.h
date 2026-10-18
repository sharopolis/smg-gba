typedef struct {
    uint16_t held;
    uint16_t pressed;
    uint16_t pending;
    bool l_was_held;
    fx l_hold_time;
} KeyState;

volatile KeyState g_keys;
volatile uint32_t g_input_latch_enabled;

static inline uint16_t input_read_keys(void);
static void input_finish_frame(KeyState* k, uint16_t now, fx dt);
