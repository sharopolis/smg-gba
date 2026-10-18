enum { AW_WAIT = 64, AW_ROTATE, AW_LOCK };

static void arrowswitch_init(Actor* a, const StageObject* o){
    a->args[0] = o->args[0];
    a->args[1] = o->args[1] > 0;
    a->value = 0;
    a->target = V3(0, 0, 0);
    a->radius = FX(100);
    actor_set_state(a, AW_WAIT, -1);
}

static int arrowswitch_step(const Actor* a){ return (a->args[0] == 1 || a->args[0] == 2) ? 1 : 2; }

static void arrowswitch_think(Actor* a){
    static const int16_t target_deg[4] = {0, 90, 180, -90};
    if(a->state == AW_WAIT){
        v3 tip =
            v3_add(a->position, v3_add(v3_scale14(a->up, FX(50)),
                                       v3_scale14(v3_rotate_axis14(a->front, a->up, a->target.x), FX(250))));
        v3 d = v3_sub(tip, mario_centre());
        fx r = FX(100) + C_BODY_RADIUS + FX(60);
        if(g_mario.attack_timer > 0 && v3_length_squared64(d) < (uint64_t)r * r){
            v3 side = v3_cross14(a->up, a->front);
            bool plus = v3_dot14(side, v3_sub(g_game.player.figure.position, a->position)) < 0;
            int step = arrowswitch_step(a);
            a->value = (int16_t)((a->value + (plus ? step : -step) + 4) % 4);
            a->target.y = plus ? DEG(6) : -DEG(6);
            actor_set_state(a, AW_ROTATE, -1);
            audio_play_effect(SFX_HIPDROP_LAND, 2);
        }
    } else if(a->state == AW_ROTATE){
        fx goal = DEG(target_deg[a->value]);
        a->target.x += a->target.y;
        fx diff = goal - a->target.x;
        while(diff > FX_PI) diff -= FX_TAU;
        while(diff < -FX_PI) diff += FX_TAU;
        if((a->target.y > 0 && diff <= 0) || (a->target.y < 0 && diff >= 0)){
            a->target.x = goal;
            switch_set(actor_switch_key(a, SWITCH_A), a->value != 0);
            actor_set_state(a, a->args[1] ? AW_LOCK : AW_WAIT, -1);
        }
    }
    actor_block_mario(a, FX(130), FX(84));
}
