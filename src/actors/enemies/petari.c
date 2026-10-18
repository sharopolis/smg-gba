static void petari_run(Actor* a, v3 dir, int32_t accel, int32_t friction14, fx turn){
    actor_turn_toward(a, dir, turn);
    actor_push_forward(a, accel);
    actor_follow_gravity(a, R14(0.2f));
    actor_move(a, V16(1.3f), friction14, friction14);
}

static void petari_think(Actor* a){
    int32_t d = actor_mario_distance(a);
    switch(a->state){
    case PETARI_LURK:
        a->flags |= ACTOR_HIDDEN;
        if(d < FX(1500)){
            a->flags &= ~ACTOR_HIDDEN;
            actor_set_state(a, PETARI_WAIT, ANIM_PETARI_WAIT);
        }
        return;
    case PETARI_WAIT:
        actor_move(a, V16(1.3f), R14(0.92f), R14(0.92f));
        if(d > FX(700)) actor_set_state(a, PETARI_APPROACH, ANIM_PETARI_WAIT);
        else if(d < FX(500)) actor_set_state(a, PETARI_ESCAPE, ANIM_PETARI_WAIT);
        break;
    case PETARI_APPROACH:
        petari_run(a, actor_to_mario(a), V16(0.5f), R14(0.92f), DEG(3));
        if(d < FX(600)) actor_set_state(a, PETARI_WAIT, ANIM_PETARI_WAIT);
        break;
    case PETARI_ESCAPE: {
        if(a->timer % 90 == 0) a->value = (int16_t)((g_game.tick * 37) % 181) - 90;
        v3 away = v3_rotate_axis14(v3_tangent14(v3_neg(actor_to_mario(a)), a->up, a->front), a->up,
                                   DEG(a->value) / 2);
        if(!actor_floor_ahead(a, FX(300))) away = v3_neg(a->front);
        int32_t h = fx_max(d / 100, 1);
        int32_t boost = fx_min(N14_ONE, 4 * N14_ONE / (h * h));
        petari_run(a, away, mul14(V16(0.45f), N14_ONE + boost / 2), R14(0.97f), DEG(10));
        if(d > FX(710)) actor_set_state(a, PETARI_WAIT, ANIM_PETARI_WAIT);
        break;
    }
    case PETARI_SWOONSTART:
        actor_move(a, V16(1.3f), R14(0.9f), R14(0.97f));
        if(actor_anim_done(a)) actor_set_state(a, PETARI_SWOON, ANIM_PETARI_SWOON);
        break;
    case PETARI_SWOON:
        actor_move(a, V16(1.3f), R14(0.9f), R14(0.97f));
        if(a->timer > 180) actor_set_state(a, PETARI_SWOONEND, ANIM_PETARI_HIDE);
        break;
    case PETARI_SWOONEND:
        if(actor_anim_done(a)) actor_set_state(a, PETARI_LAND, ANIM_PETARI_WAIT);
        break;
    case PETARI_LAND:
        petari_run(a, v3_neg(actor_to_mario(a)), V16(1.0f), R14(0.97f), DEG(10));
        if(a->timer > 45) actor_set_state(a, PETARI_ESCAPE, ANIM_PETARI_WAIT);
        break;
    case PETARI_BURST: {
        int total = actor_arg_or(a, 1, 20);
        bool end = actor_anim_done(a);
        if(a->timer % 5 == 0 && a->health < total){
            collectibles_pop(OBJ_STARBIT, v3_add(a->position, v3_scale14(a->up, FX(150))), a->up, 2);
            a->health += 2;
        }
        if(end){
            if(a->health < total)
                collectibles_pop(OBJ_STARBIT, v3_add(a->position, v3_scale14(a->up, FX(150))), a->up,
                                 fx_min(total - a->health, 5));
            actor_defeat(a, false);
        }
        return;
    }
    }
    int c = actor_mario_contact(a, FX(40));
    if(a->state == PETARI_SWOON || a->state == PETARI_SWOONSTART){
        if(c == CONTACT_SPIN || c == CONTACT_STAR || c == CONTACT_TOUCH){
            if(c == CONTACT_SPIN) mario_spin_hop();
            a->health = 0;
            actor_set_state(a, PETARI_BURST, ANIM_PETARI_DAMAGE);
            audio_play_effect(SFX_ENEMY_DEFEAT, 3);
        } else if(c == CONTACT_TRAMPLE || c == CONTACT_HIPDROP){
            if(c == CONTACT_TRAMPLE) mario_stomp_bounce(g_game.keys.held & KEY_A);
            a->health = 0;
            actor_set_state(a, PETARI_BURST, ANIM_PETARI_PRESS);
            audio_play_effect(SFX_ENEMY_DEFEAT, 3);
        }
    } else if(c == CONTACT_SPIN || c == CONTACT_STAR){
        v3 away = v3_tangent14(v3_neg(actor_to_mario(a)), a->up, a->front);
        a->velocity = v3_scale14(away, V16(5.0f));
        mario_spin_hop();
        actor_set_state(a, PETARI_SWOONSTART, ANIM_PETARI_APPEARANCE);
        audio_play_effect(SFX_ENEMY_HIT, 2);
    }
}

static void petari_init(Actor* a, const StageObject* o){
    actor_set_state(a, PETARI_LURK, ANIM_PETARI_WAIT);
    a->flags |= ACTOR_HIDDEN;
}

static void petari_blown_away(Actor* a, Actor* by){
    if(a->state >= PETARI_SWOONSTART || a->state == PETARI_LURK) return;
    v3 away = v3_tangent14(v3_sub(a->position, by->position), a->up, a->front);
    a->velocity = v3_scale14(away, V16(5.0f));
    actor_set_state(a, PETARI_SWOONSTART, ANIM_PETARI_APPEARANCE);
}
