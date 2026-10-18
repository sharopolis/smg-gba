_Static_assert(HUD_LIFEUP_TILES <= 64, "life meter must fit 64 tiles");
static void hud_lifeup_restore(void){
    if(!g_lifeup.tile_dirty) return;
    if(mario_lifeup_meter_active() &&
       (g_game.scene == SCENE_PLAY || g_game.scene == SCENE_PAUSE || g_game.scene == SCENE_STAR_GET))
        return;
    hud_copy_tiles(HUD_LIFEUP_TILE, hud_tiles + HUD_LIFEUP_TILE * 32, 64);
    const uint16_t* palette = (const uint16_t*)hud_palette;
    for(int i = 0; i < 16; i++) OBJ_PALETTE[8 * 16 + i] = palette[8 * 16 + i];
    g_lifeup.tile_dirty = 0;
    g_lifeup.image = 0xffff;
}
static void hud_lifeup_draw(void){
    MarioLifeUp* l = &g_lifeup;
    unsigned frame = LU_COUNT_BASE + fx_clamp(g_game.player.health, 0, 6);
    int dx = 0, dy = 0;
    if(l->phase == LIFEUP_POWER || l->phase == LIFEUP_MOVE){
        frame = l->phase == LIFEUP_POWER ? LU_POWER_BASE + (l->break_count - 1) * 62 + fx_min(l->step, 61)
                                         : LU_RECOVERY_BASE + l->count;
        if(l->phase == LIFEUP_POWER){
            const Player* p = &g_game.player;
            const Camera* c = &g_game.camera;
            v3 d = v3_sub(v3_add(p->figure.position, v3_scale14(p->up, 120)), c->position);
            int z = v3_dot14(d, c->front);
            if(z > 50){
                v3 left = v3_cross14(c->up, c->front);
                l->follow_x = (int16_t)(120 - v3_dot14(d, left) * 193 / z + 13 - (HUD_METER_X + 16));
                l->follow_y = (int16_t)(80 - v3_dot14(d, c->up) * 193 / z - 13 - (HUD_METER_Y + 16));
            }
            dx = l->follow_x;
            dy = l->follow_y;
        } else {
            fx angle = FX_PI * fx_min(l->step, 25) / 50;
            int weight = fx_cos(angle) << 6;
            dx = l->follow_x * weight / N14_ONE;
            dy = l->follow_y * weight / N14_ONE;
        }
    } else if(l->phase == LIFEUP_COUNTER) frame = LU_COUNTER_BASE + fx_min(l->step, 12);
    else if(l->phase == LIFEUP_BREAK)
        frame = LU_BREAK_BASE + ((l->break_count - 4) * 3 + (l->count - 1)) * 40 + fx_min(l->step, 39);
    else if(l->phase == LIFEUP_RETURN) frame = LU_RETURN_BASE + (l->count - 1) * 11 + fx_min(l->step, 10);
    else if(l->phase == LIFEUP_DAMAGE) frame = LU_DAMAGE_BASE + (l->count - 4) * 27 + fx_min(l->step, 26);
    else if(l->phase == LIFEUP_RECOVER) frame = LU_RECOVER_BASE + (l->count - 4) * 11 + fx_min(l->step, 10);
    if(frame >= HUD_LIFEUP_FRAMES) return;
    unsigned image = lifeup_meter_index[frame];
    if(!l->tile_dirty || l->image != image){
        const uint16_t* refs = (const uint16_t*)(lifeup_meter + image * HUD_LIFEUP_POOL_RECORD_BYTES);
        const uint16_t* palette =
            (const uint16_t*)(lifeup_meter + HUD_LIFEUP_POOL_PALETTE_OFFSET + refs[0] * 32);
        for(int i = 0; i < 16; i++) OBJ_PALETTE[8 * 16 + i] = palette[i];

        const uint32_t* tiles = (const uint32_t*)(lifeup_meter + HUD_LIFEUP_POOL_TILE_OFFSET);
        volatile uint32_t* dst = (volatile uint32_t*)(OBJ_TILES + HUD_LIFEUP_TILE * 16);
        for(unsigned tile = 0; tile < HUD_LIFEUP_TILES; tile++){
            const uint32_t* src = tiles + refs[1 + tile] * 8;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            dst[4] = src[4];
            dst[5] = src[5];
            dst[6] = src[6];
            dst[7] = src[7];
            dst += 8;
        }
        l->tile_dirty = 1;
        l->image = (uint16_t)image;
    }
    hud_art(HUD_ART_LIFE, HUD_LIFE_X + dx, HUD_LIFE_Y + dy, 0);
    hud_piece(HUD_METER_X - 16 + dx, HUD_METER_Y - 16 + dy, HUD_LIFEUP_TILE, 8, 3);
}
