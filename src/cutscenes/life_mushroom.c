static bool life_mushroom_paused(void){ return g_lifeup.scene_ticks || g_lifeup.paused_frame; }
static bool life_mushroom_anim_paused(void){ return g_lifeup.scene_ticks != 0; }
static bool life_mushroom_can_start(void){
    Player* p = &g_game.player;
    return g_game.scene == SCENE_PLAY && p->health > 0 && !g_mario_held && p->action != MA_DEATH &&
           p->action != MA_STAR_GET && p->action != MA_STAR_LAUNCH && p->action != MA_TALK &&
           !cutscene_blocks(BLOCK_DEMO_START | BLOCK_SCENE_START);
}

static bool life_mushroom_tick(const KeyState* keys){
    MarioLifeUp* l = &g_lifeup;
    l->paused_frame = 0;
    if(l->sync_death){
        l->sync_death = 0;
        mario_lifeup_sync_death();
    }
    if(l->pending && !l->scene_ticks && life_mushroom_can_start()){
        l->pending = 0;
        l->count = l->previous = (uint8_t)fx_clamp(g_game.player.health, 0, 3);
        l->break_count = l->count;
        g_mario_max_health = 6;
        g_game.player.health = 6;
        l->scene_ticks = 90;
        mario_lifeup_set_meter(LIFEUP_POWER);
        audio_play_effect(SFX_LIFEUP_GET, 4);
    }
    if(!l->scene_ticks) return false;
    l->paused_frame = 1;

    g_game.player.velocity = V3(0, 0, 0);
    mario_ride_carry();
    --l->scene_ticks;
    return true;
}
