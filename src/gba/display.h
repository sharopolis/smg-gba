static uint8_t g_display_full_res;
static uint8_t g_display_was_full_res;

static void frame_timer_restart(void);
static inline uint8_t* display_page(bool page);
static inline void display_show_page(bool page);
