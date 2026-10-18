static EWRAM_BSS int16_t g_blue_chip_object;

static bool blue_chip_completion_cast(const Actor* a);
static bool blue_chip_completion_cast_shown(void);
static bool blue_chip_completion_group(const Actor* a);
static void blue_chip_completion_request(Actor* a);
static bool blue_chip_completion_paused(void);
static int blue_chip_counter_frame(void);
static bool gcapture_appear_tick(Actor* a);
static bool blue_chip_completion_tick(const KeyState* keys);
