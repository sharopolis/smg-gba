enum { HR_WAIT = ACTOR_STATE_WAIT, HR_BREAK };

static void collectibles_pop_column(Actor* a, int kind, int count){
    for(int k = 0; k < count; k++){
        Actor* it =
            actor_spawn(kind, v3_add(a->position, v3_scale14(a->up, FX(80 + 40 * k))), a->up, a->front);
        if(!it) break;
        it->flags |= 2;
        if(!collectible_fall_enabled(count)) it->flags |= DROP_NO_FALL;
        it->value = (int16_t)count;
        if(kind == OBJ_STARBIT)
            it->figure.model = (const FigureHeader*)starbit_figures[(actor_index(a) + k) % 6];
    }
}

static void hipdrop_rock_think(Actor* a){
    if(a->state == HR_WAIT){
        if(g_mario_link.hipdrop == actor_index(a)){
            a->state = HR_BREAK;
            a->timer = 0;
            audio_play_effect(SFX_ENEMY_DEFEAT, 3);
            switch_set(actor_switch_key(a, SWITCH_B), true);
            int16_t item = actor_stage_arg(a, 0);
            if(item == 0) collectibles_pop_column(a, OBJ_COIN, 1);
            if(item == 1)
                collectibles_pop_column(a, OBJ_STARBIT,
                                        actor_stage_arg(a, 1) > 0 ? actor_stage_arg(a, 1) : 6);
            actor_show_model(a, false);
        }
    } else if(a->timer >= 30){
        a->active = 0;
    }
}
