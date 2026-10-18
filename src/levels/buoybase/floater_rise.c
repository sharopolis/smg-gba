static bool floater_rise_active(void){
    return buoy_stage() && g_buoy.rise_step >= 0 && g_buoy.rise_step < BUOY_RISE_STEPS;
}
static v3 floater_rise_cast_position(void){
    int type = g_buoy.rise_step < 30   ? OBJ_BUOYFIXATION
               : g_buoy.rise_step < 90 ? OBJ_BUOYCHAIN
                                       : OBJ_BUOYFLOATERU;
    for(uint32_t i = 0; i < g_game.stage.header->object_count; i++){
        const StageObject* o = &g_game.stage.objects[i];
        if(o->type != type) continue;
        if(type == OBJ_BUOYFLOATERU && o->rail >= 0 && g_buoy.rise_step >= 90)
            return rail_position_at(o->rail, fx_min(FX((g_buoy.rise_step - 90) * 10), rail_length(o->rail)));
        return V3(o->position[0], o->position[1], o->position[2]);
    }
    return g_game.player.figure.position;
}

static void floater_rise_camera_target(void){
    GalaxyCamera* c = &g_gcam;
    if(!c->event_active || c->event_first != BUOY_RISE_CAMERA) return;
    v3 pos = floater_rise_cast_position();
    if(g_buoy.rise_step != 30 && g_buoy.rise_step != 90)
        c->event_target.last_move = v3_add(c->event_target.last_move, v3_sub(pos, c->event_target.pos));
    c->event_target.pos = pos;
}

static void floater_rise_events(void){
    if(g_buoy.rise_step < 0 && switch_on(3024)){
        g_buoy.rise_step = 0;
        camera_event_start(BUOY_RISE_CAMERA, 3, floater_rise_cast_position(), V3(0, 0, N14_ONE),
                           V3(0, N14_ONE, 0));
    } else if(g_buoy.rise_step >= 0 && g_buoy.rise_step < BUOY_RISE_STEPS){
        if(++g_buoy.rise_step == BUOY_RISE_STEPS){
            switch_set(3005, true);
            if(g_gcam.event_first == BUOY_RISE_CAMERA) camera_event_cancel();
        } else floater_rise_camera_target();
    }
    if(g_buoy.rise_step == 90) audio_play_effect(SFX_BUOY_OJ_HUGE_FLOATER_MOVE_ST, 2);
    if(g_buoy.rise_step == 389) audio_play_effect(SFX_BUOY_OJ_HUGE_FLOATER_MOVE_ED, 2);
}

static bool floater_rise_tick(const KeyState* keys){
    if(!floater_rise_active()) return false;
    oceanbowl_tick();
    buoy_events_tick();
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(!a->active) continue;
        if(a->type != OBJ_BUOYFIXATION && a->type != OBJ_BUOYCHAIN && a->type != OBJ_BUOYCHAINLINK &&
           a->type != OBJ_BUOYCHAINFIX && a->type != OBJ_BUOYFLOATERU)
            continue;
        actor_think(a);
        actor_animate(a);
    }
    return true;
}
