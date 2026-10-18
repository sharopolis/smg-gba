volatile uint32_t g_vblank_count;
static uint8_t g_display_full_res;
static uint8_t g_display_was_full_res;

static void frame_timer_restart(void){
    REG_TM2CNT_L = 0;
    REG_TM2CNT_H = 0;
    REG_TM2CNT_H = TM_ENABLE | TM_FREQ_1024;
}

static inline uint8_t* display_page(bool page){
    if(g_display_full_res) return (uint8_t*)VRAM + (page ? PAGE_SIZE : 0);
    return (uint8_t*)VRAM + (page ? 120 : 0);
}

static inline void display_show_page(bool page){
    if(g_display_full_res){
        REG_DISPCNT = DCNT_MODE4 | DCNT_BG2 | 0x1000 | 0x0040 | (page ? DCNT_PAGE : 0);
        REG_BG2PA = 256;
        REG_BG2PD = 256;
        REG_BG2X = 0;
    } else {
        REG_DISPCNT = DCNT_MODE4 | DCNT_BG2 | 0x1000 | 0x0040;
        REG_BG2PA = 128;
        REG_BG2PD = 128;
        REG_BG2X = page ? 120 << 8 : 0;
    }
}
