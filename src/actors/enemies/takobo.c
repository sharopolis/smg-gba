enum { TAKOBO_FLAG_BACKWARD = 0x10 };

static void takobo_init(Actor* a, const StageObject* o){
    int axis = actor_arg_or(a, 1, 2);
    v3 side = v3_cross14(a->up, a->front);
    v3 dir = axis == 0 || axis == 1 ? side : a->front;
    if(axis == 1 || axis == 3) dir = v3_neg(dir);
    a->target = dir;
    actor_set_state(a, TAKOBO_WAIT, ANIM_TAKOBO_WAIT);
}

static void takobo_think(Actor* a){
    int32_t speed = actor_arg_or(a, 2, 5);
    fx range = FX(actor_arg_or(a, 3, 500));
    switch(a->state){
    case TAKOBO_WAIT:
        if(a->timer > 60 && speed > 0){
            actor_set_state(a, TAKOBO_MOVE, ANIM_TAKOBO_WAIT);
            a->velocity = a->position;
        }
        break;
    case TAKOBO_MOVE: {
        v3 end = v3_add(a->home, v3_scale14(a->target, (a->flags & TAKOBO_FLAG_BACKWARD) ? -range : range));
        int32_t dist = v3_length_exact(v3_sub(end, a->velocity));
        int32_t frames = fx_max(1, dist * 13 / (10 * speed));
        int32_t t = fx_min(a->timer * N14_ONE / frames, N14_ONE);
        int32_t e = mul14(mul14(t, t), 3 * N14_ONE - 2 * t);
        a->position = v3_add(a->velocity, v3_scale14(v3_sub(end, a->velocity), e));
        if(actor_mario_within(a, FX(800))){
            fx ang = actor_turn_toward(a, actor_to_mario(a), FXC(0.05f));
            if(ang < FXC(0.1f) && actor_mario_within(a, FX(250))){
                actor_set_state(a, TAKOBO_ATTACK, ANIM_TAKOBO_ATTACK);
                break;
            }
        }
        if(t >= N14_ONE){
            a->flags ^= TAKOBO_FLAG_BACKWARD;
            a->velocity = a->position;
            a->timer = 0;
        }
        break;
    }
    case TAKOBO_ATTACK: {
        if(a->timer >= 85 && a->timer <= 90){
            Actor head = *a;
            head.position = v3_add(a->position, v3_scale14(a->front, FX(90)));
            head.radius = FX(80);
            if(actor_touches_mario(&head, 0)) actor_hurt_mario(a, 1);
        }
        if(actor_anim_done(a)) actor_set_state(a, TAKOBO_WAIT, ANIM_TAKOBO_WAIT);
        break;
    }
    case TAKOBO_HITREACT:
        if(actor_anim_done(a)) actor_set_state(a, TAKOBO_WAIT, ANIM_TAKOBO_WAIT);
        break;
    case TAKOBO_PRESS:
        if(a->timer > 30){
            collectibles_pop(OBJ_COIN, v3_add(a->position, v3_scale14(a->up, FX(80))), a->up, 1);
            actor_defeat(a, false);
        }
        return;
    case TAKOBO_PUNCH:
        a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
        a->velocity = v3_sub(a->velocity, v3_scale14(a->up, V16(1.5f)));
        if(a->timer == 21) collectibles_pop(OBJ_STARBIT, actor_centre(a), a->up, 3);
        if(a->timer >= 22) actor_defeat(a, false);
        return;
    }
    if(a->state == TAKOBO_WAIT || a->state == TAKOBO_ATTACK) actor_follow_gravity(a, R14(0.2f));
    switch(actor_mario_contact(a, FX(60))){
    case CONTACT_TRAMPLE: mario_stomp_bounce(g_game.keys.held & KEY_A);

    case CONTACT_HIPDROP:
        actor_set_state(a, TAKOBO_PRESS, ANIM_TAKOBO_PRESS);
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
        break;
    case CONTACT_STAR:
    case CONTACT_SPIN: {
        v3 away = v3_tangent14(v3_neg(actor_to_mario(a)), a->up, v3_neg(a->front));
        a->velocity = v3_add(v3_scale14(away, V16(20.0f)), v3_scale14(a->up, V16(35.0f)));
        actor_set_state(a, TAKOBO_PUNCH, ANIM_TAKOBO_HITPUNCH);
        mario_spin_hop();
        audio_play_effect(SFX_ENEMY_HIT, 3);
        break;
    }
    case CONTACT_TOUCH: {
        if(game_player()->action == MA_HIPDROP) break;

        v3 delta = v3_sub(mario_centre(), v3_add(a->position, v3_scale14(a->up, a->radius)));
        bool above = v3_dot14(v3_direction14(delta, V3(0, 0, 0)), a->up) > R14(0.25f);
        if((a->state == TAKOBO_MOVE || a->state == TAKOBO_ATTACK) && !above){
            int health = game_player()->health;
            actor_hurt_mario(a, 1);
            if(game_player()->health < health){
                actor_set_state(a, TAKOBO_HITREACT, ANIM_TAKOBO_HITREACTION);
                break;
            }
        }

        actor_block_mario(a, a->radius, a->radius * 2);
        break;
    }
    }
}

static void takobo_blown_away(Actor* a, Actor* by){
    if(a->state == TAKOBO_PUNCH || a->state == TAKOBO_PRESS) return;
    v3 away = v3_tangent14(v3_sub(a->position, by->position), a->up, a->front);
    a->velocity = v3_add(v3_scale14(away, V16(20.0f)), v3_scale14(a->up, V16(35.0f)));
    actor_set_state(a, TAKOBO_PUNCH, ANIM_TAKOBO_HITPUNCH);
}
