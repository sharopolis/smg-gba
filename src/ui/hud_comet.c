#define COMET_TILE0    (HUD_STATIC_TILES + 128 + 192)
#define COMET_MTX_TIME 30

enum {
    TIME_LIMIT_OFF,
    TIME_LIMIT_APPEAR,
    TIME_LIMIT_COUNTDOWN,
    TIME_LIMIT_SCALE_UP,
    TIME_LIMIT_SCALE_KEEP,
    TIME_LIMIT_SCALE_DOWN,
    TIME_LIMIT_TIMEUP_READY
};
enum { TIME_LIMIT_ANIM_APPEAR, TIME_LIMIT_ANIM_SIGNAL, TIME_LIMIT_ANIM_FLASH };

typedef struct {
    uint8_t pc_on;
    uint8_t pc_appear;
    uint8_t pc_wait;
    uint8_t pc_rumble;
    int8_t pc_dy;
    int16_t pc_shown;

    uint8_t tl_nerve, tl_anim, tl_timing;
    int8_t tl_scale, tl_scale_dir;
    int16_t tl_step, tl_anim_frame;
    int32_t tl_prev_left;
} HudComet;

static HudComet g_comet EWRAM_BSS;

static const uint16_t comet_time_scale[9] = {256, 261, 275, 296, 320, 344, 365, 379, 384};

static void hud_comet_reset(void){
    g_comet = (HudComet){0};
    g_comet.tl_nerve = TIME_LIMIT_OFF;
}

static void comet_affine(int m, int pa, int pb, int pc, int pd){
    g_oam[m * 4 + 0].pad = (uint16_t)pa;
    g_oam[m * 4 + 1].pad = (uint16_t)pb;
    g_oam[m * 4 + 2].pad = (uint16_t)pc;
    g_oam[m * 4 + 3].pad = (uint16_t)pd;
}

static void comet_obj_affine(int px, int py, int w, int h, int tile, int pal, int obj, int m){
    int x = px - w, y = py - h;
    if(g_oam_count >= 128 || x <= -2 * w || x >= 240 || y <= -2 * h || y >= 160) return;
    OamEntry* e = &g_oam[g_oam_count++];
    e->attr0 = (uint16_t)((y & 0xFF) | 0x0300 | (obj >> 2) << 14);
    e->attr1 = (uint16_t)((x & 0x1FF) | m << 9 | (obj & 3) << 14);
    e->attr2 = (uint16_t)((512 + tile) | pal << 12);
}

static const uint8_t comet_obj_w[3][4] = {{8, 16, 32, 64}, {16, 32, 32, 64}, {8, 8, 16, 32}};
static const uint8_t comet_obj_h[3][4] = {{8, 16, 32, 64}, {8, 8, 16, 32}, {16, 32, 32, 64}};

static void comet_art_scaled(int art, int base, int cx, int cy, int dx, int dy, int scale, int m){
    const HudArt* a = &hud_arts[art];
    for(int i = 0; i < a->count; i++){
        const HudPiece* p = &hud_pieces[a->first + i];
        if(scale == 256){
            hud_piece(cx + dx + p->x, cy + dy + p->y, base + p->tile, a->bank, p->obj);
            continue;
        }
        int w = comet_obj_w[p->obj >> 2][p->obj & 3], h = comet_obj_h[p->obj >> 2][p->obj & 3];
        int ox = 2 * (dx + p->x) + w, oy = 2 * (dy + p->y) + h;
        comet_obj_affine(cx + ((ox * scale) >> 9), cy + ((oy * scale) >> 9), w, h, base + p->tile, a->bank,
                         p->obj, m);
    }
}

static void hud_purple_coins_tick(int count){
    HudComet* c = &g_comet;
    if(!c->pc_on){
        c->pc_on = 1;
        c->pc_appear = 0;
        c->pc_wait = 0;
        c->pc_rumble = 15;
    }
    if(count < c->pc_shown) c->pc_shown = (int16_t)count;

    if(c->pc_wait > 0) c->pc_wait--;
    else if(c->pc_shown < count && c->pc_appear >= HUD_PCOIN_APPEAR_FRAMES){
        c->pc_wait = HUD_PCOIN_COUNT_INTERVAL - 1;
        c->pc_shown++;
        c->pc_rumble = 0;
    }
    if(c->pc_appear < HUD_PCOIN_APPEAR_FRAMES) c->pc_appear++;
    c->pc_dy = c->pc_rumble < 15 ? hud_pcoin_rumble_dy[c->pc_rumble++] : 0;
}

static void hud_draw_purple_coins(void){
    const HudComet* c = &g_comet;
    if(!c->pc_on) return;
    int x = HUD_PCOIN_X - hud_slide_offset(c->pc_appear);
    hud_art(HUD_ART_PCOIN, x, HUD_PCOIN_Y, 0);
    int v = c->pc_shown > 999 ? 999 : c->pc_shown;
    int d[HUD_PCOIN_DIGITS] = {v / 100, v / 10 % 10, v % 10};
    int x16 = (x + HUD_PCOIN_DIGITS_DX) << 4;
    for(int i = 0; i < HUD_PCOIN_DIGITS; i++, x16 += HUD_DIGIT_ADV_X16)
        hud_art(HUD_ART_DIGIT0 + d[i], x16 >> 4, HUD_PCOIN_Y + HUD_PCOIN_DIGITS_DY + c->pc_dy, 0);
    hud_art(HUD_ART_PCOIN_100, x + HUD_PCOIN_100_DX, HUD_PCOIN_Y + HUD_PCOIN_100_DY, 0);
}

static int comet_time_timing(int left, int limit){
    for(int i = 0; i < 5; i++){
        int start = (int)hud_time_timing[i][0], from = start - (int)hud_time_timing[i][1];
        if(limit >= from && start >= limit) continue;
        if(from > left || left >= start) continue;
        return i;
    }
    return -1;
}

static void comet_time_nerve(HudComet* c, int nerve){
    c->tl_nerve = (uint8_t)nerve;
    c->tl_step = -1;
}

static void comet_time_anim(HudComet* c, int anim){
    c->tl_anim = (uint8_t)anim;
    c->tl_anim_frame = 0;
}

static bool comet_time_normal(HudComet* c, int left){
    if(c->tl_prev_left > 600 && left <= 600) comet_time_anim(c, TIME_LIMIT_ANIM_FLASH);
    c->tl_prev_left = left;
    if(left <= 0){
        comet_time_nerve(c, TIME_LIMIT_TIMEUP_READY);
        return true;
    }
    return false;
}

static bool hud_time_limit_running(void){
    return g_comet.tl_nerve >= TIME_LIMIT_COUNTDOWN && g_comet.tl_nerve != TIME_LIMIT_TIMEUP_READY;
}

static void hud_time_limit_tick(int frames_left, int frames_limit){
    HudComet* c = &g_comet;
    if(c->tl_nerve == TIME_LIMIT_OFF){
        comet_time_nerve(c, TIME_LIMIT_APPEAR);
        c->tl_scale = 8;
        c->tl_scale_dir = 1;
        c->tl_timing = 0;
        c->tl_prev_left = frames_left;
    }
    c->tl_step++;
    const uint32_t* t = hud_time_timing[c->tl_timing];
    switch(c->tl_nerve){
    case TIME_LIMIT_APPEAR:
        if(c->tl_step == 0) comet_time_anim(c, TIME_LIMIT_ANIM_APPEAR);
        else if(c->tl_anim_frame >= HUD_TIME_APPEAR_FRAMES) comet_time_nerve(c, TIME_LIMIT_COUNTDOWN);
        break;
    case TIME_LIMIT_COUNTDOWN: {
        if(comet_time_normal(c, frames_left)) break;
        int k = comet_time_timing(frames_left, frames_limit);
        if(k >= 0){
            c->tl_timing = (uint8_t)k;
            comet_time_nerve(c, TIME_LIMIT_SCALE_UP);
        }
        break;
    }
    case TIME_LIMIT_SCALE_UP:
        if(c->tl_step == 0){
            c->tl_scale_dir = 1;
            c->tl_scale = 0;
        }
        if(comet_time_normal(c, frames_left)) break;
        if(c->tl_scale == 8) comet_time_nerve(c, TIME_LIMIT_SCALE_KEEP);
        break;
    case TIME_LIMIT_SCALE_KEEP:
        if(c->tl_step == 0 && t[2]) comet_time_anim(c, TIME_LIMIT_ANIM_SIGNAL);
        if(comet_time_normal(c, frames_left)) break;
        if(c->tl_step > (int)t[1]) comet_time_nerve(c, t[3] ? TIME_LIMIT_SCALE_DOWN : TIME_LIMIT_COUNTDOWN);
        break;
    case TIME_LIMIT_SCALE_DOWN:
        if(c->tl_step == 0){
            c->tl_scale_dir = -1;
            c->tl_scale = 8;
        }
        if(comet_time_normal(c, frames_left)) break;
        if(c->tl_scale == 8) comet_time_nerve(c, TIME_LIMIT_COUNTDOWN);
        break;
    default: break;
    }

    if(c->tl_scale_dir > 0 && c->tl_scale < 8) c->tl_scale++;
    else if(c->tl_scale_dir < 0 && c->tl_scale > 0) c->tl_scale--;

    c->tl_anim_frame++;
    if(c->tl_anim == TIME_LIMIT_ANIM_APPEAR && c->tl_anim_frame > HUD_TIME_APPEAR_FRAMES)
        c->tl_anim_frame = HUD_TIME_APPEAR_FRAMES;
    if(c->tl_anim == TIME_LIMIT_ANIM_SIGNAL && c->tl_anim_frame > HUD_TIME_SIGNAL_FRAMES)
        c->tl_anim_frame = HUD_TIME_SIGNAL_FRAMES;
    if(c->tl_anim == TIME_LIMIT_ANIM_FLASH) c->tl_anim_frame %= HUD_TIME_FLASH_FRAMES;
}

static void hud_draw_time_limit(int frames_left){
    const HudComet* c = &g_comet;
    if(c->tl_nerve == TIME_LIMIT_OFF) return;

    int dy = 0, scale = comet_time_scale[c->tl_scale];
    bool visible = true;
    if(c->tl_anim == TIME_LIMIT_ANIM_APPEAR){
        const HudTimeAppear* ap = &hud_time_appear[c->tl_anim_frame];
        dy = ap->dy;
        scale = ap->scale;
        visible = ap->visible;
    } else if(c->tl_anim == TIME_LIMIT_ANIM_SIGNAL){
        visible = hud_time_signal_visible[c->tl_anim_frame];
    }

    const uint16_t* pal = hud_time_flash_pal[c->tl_anim == TIME_LIMIT_ANIM_FLASH ? c->tl_anim_frame : 0];
    for(int i = 0; i < HUD_TIME_SLOTS; i++) OBJ_PALETTE[HUD_TIME_BANK * 16 + hud_time_slot[i]] = pal[i];
    if(!visible) return;

    ui_text_copy_tiles(COMET_TILE0, hud_text, HUD_TIME_DATA / 32, HUD_TIME_TILES);
    int base = COMET_TILE0 - HUD_TIME_DATA / 32;
    if(scale != 256){
        int inv = (256 * 256 + scale / 2) / scale;
        comet_affine(COMET_MTX_TIME, inv, 0, 0, inv);
    }
    int cx = HUD_TIME_CX, cy = HUD_TIME_CY + dy;
    comet_art_scaled(HUD_ART_TIME_LABEL, base, cx, cy, HUD_TIME_LABEL_DX, HUD_TIME_LABEL_DY, scale,
                     COMET_MTX_TIME);
    if(frames_left < 0) frames_left = 0;
    int f = frames_left + 59, mm = 59, ss = 59;
    if(f < 60 * 60 * 60){
        mm = f / 3600;
        ss = f % 3600 / 60;
    }
    static const int8_t place_x[5] = {HUD_TIME_X0, HUD_TIME_X1, HUD_TIME_X2, HUD_TIME_X3, HUD_TIME_X4};
    int glyph[5] = {HUD_ART_TIME0 + mm / 10, HUD_ART_TIME0 + mm % 10, HUD_ART_TIME_COLON,
                    HUD_ART_TIME0 + ss / 10, HUD_ART_TIME0 + ss % 10};
    for(int i = 0; i < 5; i++)
        comet_art_scaled(glyph[i], base, cx, cy, place_x[i], HUD_TIME_GLYPH_DY, scale, COMET_MTX_TIME);
}
