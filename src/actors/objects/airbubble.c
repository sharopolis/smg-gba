static void airbubble_init(Actor* a, const StageObject* o){
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    a->radius = a->radius * o->scale[0] / 256;
    if(a->type == OBJ_AIRBUBBLE){
        a->value = 360;
        actor_set_state(a, AIRBUBBLE_WAIT, ANIM_AIRBUBBLE_MOVE);
    } else {
        a->value = -1;
        actor_set_state(a, AIRBUBBLE_GENERATOR_WAIT, -1);
        mappart_place_kcl(a, kcl_airbubblegenerator, KCL_AIRBUBBLEGENERATOR_COUNT);
    }
}
static void airbubble_kill(Actor* a){
    if(actor_has_switch(a, SWITCH_DEAD)) switch_set(actor_switch_key(a, SWITCH_DEAD), true);
    a->active = 0;
}
static void airbubble_think(Actor* a){
    int step = a->timer - 1;
    switch(a->state){
    case AIRBUBBLE_WAIT:
        if(step >= 120){
            a->timer = 1;
            step = 0;
        }
        a->position = a->home;
        a->position.z += fx_sin((fx)((int64_t)(step % 120) * FX_TAU / 120)) / 256;
        break;
    case AIRBUBBLE_MOVE: {
        a->up = gravity_sample(g_game.stage.header, a->position).up;
        if(step == 0) a->velocity = v3_scale14(a->up, V16(7.0f));
        a->target = v3_rotate_axis14(a->target, v3_neg(a->up), FXC(1.5f * TAU_F / 360));

        a->target = v3_tangent14(a->target, a->up, V3(N14_ONE, 0, 0));
        a->velocity =
            v3_add(a->velocity, v3_add(v3_scale14(a->target, V16(.1f)), v3_scale14(a->up, V16(.3f))));
        a->velocity = v3_scale14(a->velocity, R14(.85f));
        a->position = v3_add(a->position, V3((a->velocity.x + 128) >> 8, (a->velocity.y + 128) >> 8,
                                             (a->velocity.z + 128) >> 8));
        if(step > a->value){
            a->flags |= ACTOR_HIDDEN;
            audio_play_effect(SFX_AIRBUBBLE_BREAK, 2);
            actor_set_state(a, AIRBUBBLE_KILL_WAIT, -1);
        }
        break;
    }
    case AIRBUBBLE_BREAK:
        audio_play_effect(SFX_AIRBUBBLE_BREAK, 2);
        for(int i = 0; i < 8; i++) mario_swim_recover();
        airbubble_kill(a);
        return;
    case AIRBUBBLE_KILL_WAIT:
        if(step > 90){
            airbubble_kill(a);
            return;
        }
        break;
    }
    if(g_game.player.action == MA_DEATH) return;
    bool touch = actor_touches_mario(a, 0);
    v3 centre = v3_add(g_game.player.figure.position, v3_scale14(g_game.player.gravity_up, C_BODY_HEIGHT));
    bool spin = g_mario.attack_timer > 0 && a->state != AIRBUBBLE_KILL_WAIT &&
                v3_length_squared64(v3_sub(centre, a->position)) < (int64_t)250 * 250;
    if(touch && a->state == AIRBUBBLE_KILL_WAIT){
        for(int i = 0; i < 8; i++) mario_swim_recover();
        airbubble_kill(a);
    } else if(touch || spin) actor_set_state(a, AIRBUBBLE_BREAK, -1);
}
static void airbubblegenerator_think(Actor* a){
    mappart_place_kcl(a, kcl_airbubblegenerator, KCL_AIRBUBBLEGENERATOR_COUNT);
    int step = a->timer - 1;
    if(a->state == AIRBUBBLE_GENERATOR_WAIT){
        int wait = fx_max(actor_arg_or(a, 0, 180), 1);
        if(step % wait == wait - 1)
            actor_set_state(a, AIRBUBBLE_GENERATOR_GENERATE, ANIM_AIRBUBBLEGENERATOR_GENERATE);
    } else {
        if(step == 0) audio_play_effect(SFX_AIRBUBBLE_APPEAR, 2);
        if(step > 6){
            Actor* b = actor_spawn_reuse(OBJ_AIRBUBBLE, v3_add(a->position, v3_scale14(a->up, 120)), a->up,
                                         a->front);
            if(b){
                b->value = actor_arg_or(a, 1, 360);
                if(b->value <= 0) b->value = 360;
                b->target = V3(0, N14_ONE, 0);
                actor_set_state(b, AIRBUBBLE_MOVE, ANIM_AIRBUBBLE_MOVE);
            }
            actor_set_state(a, AIRBUBBLE_GENERATOR_WAIT, -1);
        }
    }
}
