static EWRAM_BSS MarioLifeUp g_lifeup;

static bool mario_lifeup_dark(void){
    int mission = level_current()->daredevil_mission;
    return mission && game_current_scenario() == mission;
}
static int mario_lifeup_max(void){ return g_mario_max_health; }
static bool mario_lifeup_meter_active(void){
    return g_mario_max_health > 3 || g_lifeup.phase != LIFEUP_NORMAL;
}
static void mario_lifeup_set_meter(int phase){
    g_lifeup.phase = phase;
    g_lifeup.step = 0;
}

static void mario_lifeup_health_changed(int previous){
    int hp = g_game.player.health;
    if(hp >= previous) return;
    if(hp <= 3) g_mario_max_health = 3;
    g_lifeup.count = (uint8_t)fx_clamp(hp, 0, 6);
    g_lifeup.previous = g_lifeup.count;
    if(previous >= 4 && hp > 0 && hp < 4){
        g_lifeup.break_count = (uint8_t)fx_clamp(previous, 4, 6);
        mario_lifeup_set_meter(LIFEUP_BREAK);
    } else if(hp >= 4) mario_lifeup_set_meter(LIFEUP_DAMAGE);
    else if(hp <= 0) mario_lifeup_set_meter(LIFEUP_NORMAL);
}
static void mario_lifeup_reset(void){
    g_mario_max_health = 3;
    g_lifeup.phase = LIFEUP_NORMAL;
    hud_lifeup_restore();
    int hp = fx_clamp(g_game.player.health, 0, 6);
    g_mario_max_health = mario_lifeup_dark() ? 1 : (hp > 3 ? 6 : 3);
    g_lifeup = (MarioLifeUp){.count = hp, .previous = hp, .image = 0xffff};
}
static void mario_lifeup_pickup(void){
    Player* p = &g_game.player;
    g_lifeup.sync_death = 1;

    if(mario_lifeup_dark() || p->health <= 0 || g_mario_held || p->action == MA_STAR_LAUNCH ||
       p->action == MA_STAR_GET || p->action == MA_DEATH || p->action == MA_TALK)
        return;
    if(g_mario_max_health > 3){
        bool partial = p->health < 6;
        p->health = 6;

        audio_play_effect(partial ? SFX_LIFEUP_INVALID_RECOVER : SFX_LIFEUP_INVALID, 4);
        g_lifeup.count = g_lifeup.previous = 6;
        mario_lifeup_set_meter(LIFEUP_RECOVER);
    } else g_lifeup.pending = 1;
}
static void mario_lifeup_sync_death(void){
    if(!g_stream.on) return;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        int id = a->stage_index;
        if(a->type != OBJ_LIFEUP || a->active || id < 0) continue;
        uint8_t* state = &g_object_state[id];
        if(!(*state & OBJECT_STATE_WAS_ACTIVE) || (*state & OBJECT_STATE_DEAD)) continue;
        switch_set(stage_object_switch(&g_game.stage.objects[id], SWITCH_DEAD), true);
        *state = OBJECT_STATE_GONE | OBJECT_STATE_DEAD;
        a->flags &= ~ACTOR_WAITING;
        a->stage_index = -1;
    }
}
static void mario_lifeup_meter_tick(void){
    MarioLifeUp* l = &g_lifeup;
    int hp = fx_clamp(g_game.player.health, 0, 6);
    if(!l->pending && l->phase != LIFEUP_POWER && l->phase != LIFEUP_MOVE && l->phase != LIFEUP_COUNTER &&
       hp != l->previous){
        if(hp < l->previous) mario_lifeup_health_changed(l->previous);
        else {
            l->count = (uint8_t)hp;
            if(hp > 3) mario_lifeup_set_meter(LIFEUP_RECOVER);
        }
        l->previous = (uint8_t)hp;
    }
    if(l->phase == LIFEUP_NORMAL) return;
    ++l->step;
    switch(l->phase){
    case LIFEUP_POWER:
        if(l->count < 6 && l->step % 5 == 0) ++l->count;
        if(l->step > 60) mario_lifeup_set_meter(LIFEUP_MOVE);
        break;
    case LIFEUP_MOVE:
        if(l->step > 25) mario_lifeup_set_meter(LIFEUP_COUNTER);
        break;
    case LIFEUP_COUNTER:
        if(l->step >= 13){
            l->count = l->previous = 6;
            mario_lifeup_set_meter(LIFEUP_NORMAL);
        }
        break;
    case LIFEUP_BREAK:
        if(l->step == 40){
            audio_queue_effect(SFX_LIFE_PLATE_DEC, 3);
            mario_lifeup_set_meter(LIFEUP_RETURN);
        }
        break;
    case LIFEUP_RETURN:
        if(l->step >= 11) mario_lifeup_set_meter(LIFEUP_NORMAL);
        break;
    case LIFEUP_DAMAGE:
        if(l->step >= 27) mario_lifeup_set_meter(LIFEUP_NORMAL);
        break;
    case LIFEUP_RECOVER:
        if(l->step >= 11) mario_lifeup_set_meter(LIFEUP_NORMAL);
        break;
    }
}
