static bool flippanel_same_group(const StageObject* a, const StageObject* b){
    return a->zone == b->zone && a->args[6] >= 0 && a->args[6] == b->args[6];
}

static bool flippanel_available(const StageObject* panel){
    for(uint32_t i = 0; i < g_game.stage.header->object_count; i++){
        const StageObject* o = &g_game.stage.objects[i];
        if(o->type == OBJ_FLIPPANELOBSERVER && flippanel_same_group(panel, o)){
            int appear = stage_object_switch(o, SWITCH_APPEAR);
            return appear < 0 || switch_on(appear);
        }
    }
    return true;
}

static void flippanel_init(Actor* a, const StageObject* o){
    a->value = -1;
    a->target = V3(0, 0, 0);
    int scale = o->scale[1] / 4;
    a->figure.scale = V3(scale, scale, scale);
    unsigned state = stream_part_state(a->stage_index);
    actor_set_state(a, FP_WAIT,
                    state & FP_COMPLETE ? ANIM_FLIPPANEL_PANELEND
                    : state & FP_BLUE   ? ANIM_FLIPPANEL_PANELA
                                        : ANIM_FLIPPANEL_PANELB);
    mappart_place_kcl(a, kcl_flippanel + 10, 2);
}

static void flippanel_think(Actor* a){
    mappart_place_kcl(a, kcl_flippanel + 10, 2);
    unsigned state = stream_part_state(a->stage_index);
    if(state & FP_COMPLETE){
        a->anim = ANIM_FLIPPANEL_PANELEND;
        return;
    }
    Player* p = game_player();
    bool on = a->value >= 0 && mover_carries_mario(a->value) && v3_dot14(p->up, a->up) > R14(0.99f);
    if(a->target.x){
        if(p->on_ground && !on) a->target.x = 0;
        return;
    }
    if(on){
        if(++a->target.y < 3) return;
        state ^= FP_BLUE;
        stream_part_state_set(a->stage_index, state);
        a->target.x = 1;
        a->target.y = 0;
        actor_set_state(a, FP_WAIT, state & FP_BLUE ? ANIM_FLIPPANEL_PANELA : ANIM_FLIPPANEL_PANELB);
    } else a->target.y = 0;
}

static void flippanelobserver_think(Actor* a){
    const StageObject* self = actor_stage_object(a);
    if(!self) return;
    if(a->state == FP_OBSERVE_COMPLETE){
        if(a->timer > actor_arg_or(a, 0, 0) + 40){
            switch_set(actor_switch_key(a, SWITCH_A), true);
            a->active = 0;
        }
        return;
    }
    if((g_game.tick & 7) != 0) return;
    int total = 0, blue = 0;
    for(uint32_t i = 0; i < g_game.stage.header->object_count; i++){
        const StageObject* o = &g_game.stage.objects[i];
        if(o->type != OBJ_FLIPPANEL || !flippanel_same_group(self, o)) continue;
        total++;
        blue += (stream_part_state(i) & FP_BLUE) != 0;
    }
    if(!total || blue != total) return;
    for(uint32_t i = 0; i < g_game.stage.header->object_count; i++){
        const StageObject* o = &g_game.stage.objects[i];
        if(o->type == OBJ_FLIPPANEL && flippanel_same_group(self, o))
            stream_part_state_set(i, stream_part_state(i) | FP_COMPLETE);
    }
    actor_set_state(a, FP_OBSERVE_COMPLETE, -1);
}

static void flippanelobserver_init(Actor* a, const StageObject* o){ actor_set_state(a, FP_WAIT, -1); }
