enum { WT_WAIT = ACTOR_STATE_WAIT, WT_START, WT_MOVE };
#define WATCHTOWER_LIFT_RADIUS FX(1300)

static void watch_tower_think(Actor* a){
    int sleep = actor_switch_key(a, SWITCH_SLEEP);
    fx step = 0;
    switch(a->state){
    case WT_WAIT:
        if(sleep < 0 || switch_on(sleep)){
            a->state = sleep < 0 ? WT_MOVE : WT_START;
            a->timer = 0;
            if(sleep >= 0) camera_shake(1);
        }
        break;
    case WT_START: {
        int32_t t = fx_min(a->timer, 180) * N14_ONE / 180;
        step = mul14(86, N14_ONE - (fx_cos(mul14(t, FXC(1.5708f) << 6) >> 6) << 6));
        if(a->timer >= 180) a->state = WT_MOVE;
        break;
    }
    case WT_MOVE: step = 86; break;
    }
    a->target.x += step;

    const StageObject* o = actor_stage_object(a);
    v3 up0 = o ? v3_from_i16(o->up) : a->up;
    a->up = v3_tangent14(v3_rotate_axis14(up0, a->front, a->target.x / 64), a->front, a->up);

    if(!(a->flags & 0x40)){
        a->flags |= 0x40;
        for(int k = 0; k < 4; k++){
            Actor* l = actor_spawn(OBJ_WATCHTOWERLIFT, a->position, up0, a->front);
            if(l){
                l->value = (int16_t)k;
                l->target.x = actor_index(a);
                l->target.y = a->stage_index;
            }
        }
    }
}

static void watch_tower_lift_think(Actor* l){
    Game* game = &g_game;
    int w = l->target.x;
    if(w < 0 || w >= game->actor_count || game->actors[w].type != OBJ_WATCHTOWERSTEP ||
       !game->actors[w].active || game->actors[w].stage_index != l->target.y){
        l->active = 0;
        return;
    }
    const Actor* a = &game->actors[w];
    const StageObject* o = actor_stage_object(a);
    v3 up0 = o ? v3_from_i16(o->up) : a->up;
    fx ang = a->target.x / 64 + l->value * (FX_TAU / 4);
    v3 radial = v3_rotate_axis14(up0, a->front, ang);
    l->position = v3_add(a->position, v3_scale14(radial, WATCHTOWER_LIFT_RADIUS));
    l->up = up0;
    l->front = a->front;
}
