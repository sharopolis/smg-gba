enum {
    COCOSAMBO_HIDDEN = 64,
    COCOSAMBO_APPEAR,
    COCOSAMBO_WAIT,
    COCOSAMBO_ATTACK,
    COCOSAMBO_INTERVAL,
    COCOSAMBO_HIDE,
    COCOSAMBO_HEADLESS,
    COCOSAMBO_RECOVER,
    COCOSAMBO_DEAD
};
enum {
    COCOSAMBO_HEAD_ON = 64,
    COCOSAMBO_HEAD_FALL,
    COCOSAMBO_HEAD_LAND,
    COCOSAMBO_HEAD_SWOON,
    COCOSAMBO_HEAD_SWOONEND,
    COCOSAMBO_HEAD_PRESS,
    COCOSAMBO_HEAD_BLOW
};

static int cocosambo_anim_body(int st){
    switch(st){
    case COCOSAMBO_APPEAR: return ANIM_COCOSAMBOBODY_APPEAR;
    case COCOSAMBO_ATTACK: return ANIM_COCOSAMBOBODY_ATTACK;
    case COCOSAMBO_HIDE: return ANIM_COCOSAMBOBODY_HIDE;
    case COCOSAMBO_HIDDEN: return ANIM_COCOSAMBOBODY_HIDEWAIT;
    case COCOSAMBO_HEADLESS: return ANIM_COCOSAMBOBODY_HIT;
    case COCOSAMBO_RECOVER: return ANIM_COCOSAMBOBODY_RECOVER;
    default: return ANIM_COCOSAMBOBODY_WAIT;
    }
}

static Actor* cocosambo_head(Actor* a){
    Actor* h = &g_game.actors[a->value];
    return a->value > 0 && h->active && h->type == OBJ_COCOSAMBOHEAD ? h : NULL;
}

static void cocosambo_set(Actor* a, int st){
    actor_set_state(a, st, cocosambo_anim_body(st));
    Actor* h = cocosambo_head(a);
    if(h && h->state == COCOSAMBO_HEAD_ON){
        static const int16_t head_anim[] = {ANIM_COCOSAMBOHEAD_HIDE, ANIM_COCOSAMBOHEAD_APPEAR,
                                            ANIM_COCOSAMBOHEAD_WAIT, ANIM_COCOSAMBOHEAD_ATTACK,
                                            ANIM_COCOSAMBOHEAD_WAIT, ANIM_COCOSAMBOHEAD_HIDE};
        if(st - COCOSAMBO_HIDDEN < 6){
            h->anim = head_anim[st - COCOSAMBO_HIDDEN];
            h->anim_frame = 0;
        }
        if(st == COCOSAMBO_RECOVER){
            h->anim = ANIM_COCOSAMBOHEAD_RECOVER;
            h->anim_frame = 0;
        }
    }
}

static void cocosambo_think(Actor* a){
    fx scale = a->figure.scale.x;
    if(a->value == 0){
        Actor* h = actor_spawn_reuse(OBJ_COCOSAMBOHEAD, a->position, a->up, a->front);
        if(h){
            h->value = (int16_t)actor_index(a);
            h->figure.scale = a->figure.scale;
            h->radius = FX(100) * scale / 64;
            actor_set_state(h, COCOSAMBO_HEAD_ON, ANIM_COCOSAMBOHEAD_HIDE);
            a->value = (int16_t)actor_index(h);
        }
        cocosambo_set(a, COCOSAMBO_HIDDEN);
    }
    fx search = FX(actor_arg_or(a, 0, 1800));
    switch(a->state){
    case COCOSAMBO_HIDDEN:
        if(actor_mario_within(a, search)) cocosambo_set(a, COCOSAMBO_APPEAR);
        break;
    case COCOSAMBO_APPEAR:
        actor_turn_toward(a, actor_to_mario(a), DEG(1));
        if(actor_anim_done(a)) cocosambo_set(a, COCOSAMBO_WAIT);
        break;
    case COCOSAMBO_WAIT:
        actor_turn_toward(a, actor_to_mario(a), DEG(1));
        if(actor_mario_within(a, FX(700))) cocosambo_set(a, COCOSAMBO_ATTACK);
        else if(!actor_mario_within(a, search + FX(100))) cocosambo_set(a, COCOSAMBO_HIDE);
        break;
    case COCOSAMBO_ATTACK:
        if(a->anim_frame < FX(30)) actor_turn_toward(a, actor_to_mario(a), DEG(2));
        if(actor_anim_done(a)) cocosambo_set(a, COCOSAMBO_INTERVAL);
        break;
    case COCOSAMBO_INTERVAL:
        if(a->timer > 60) cocosambo_set(a, COCOSAMBO_WAIT);
        break;
    case COCOSAMBO_HIDE:
        if(actor_anim_done(a)) cocosambo_set(a, COCOSAMBO_HIDDEN);
        break;
    case COCOSAMBO_HEADLESS:
        if(a->timer > 90 && (!cocosambo_head(a) || cocosambo_head(a)->state == COCOSAMBO_HEAD_ON))
            cocosambo_set(a, COCOSAMBO_RECOVER);
        break;
    case COCOSAMBO_RECOVER:
        if(actor_anim_done(a)) cocosambo_set(a, COCOSAMBO_APPEAR);
        break;
    case COCOSAMBO_DEAD:
        actor_set_switch_dead(a, FX(3000));
        actor_defeat(a, false);
        return;
    }
    if(a->state != COCOSAMBO_HIDDEN && a->state != COCOSAMBO_HEADLESS){
        Actor body = *a;
        body.radius = FX(80) * scale / 64 + FX(40);
        if(actor_mario_contact(&body, 0) != CONTACT_NONE) actor_hurt_mario(a, 1);
    }
}

static void cocosambo_head_think(Actor* a){
    Actor* body = &g_game.actors[a->value];
    if(!body->active || body->type != OBJ_COCOSAMBO){
        a->active = 0;
        return;
    }
    fx scale = body->figure.scale.x;
    switch(a->state){
    case COCOSAMBO_HEAD_ON: {
        a->position = v3_add(body->position, v3_scale14(body->up, FX(300) * scale / 64));
        a->up = body->up;
        a->front = body->front;
        a->anim_frame = body->anim_frame;
        a->flags = (a->flags & ~ACTOR_HIDDEN) |
                   (body->state == COCOSAMBO_HIDDEN || body->state == COCOSAMBO_HEADLESS ? ACTOR_HIDDEN : 0);
        if(body->state == COCOSAMBO_HIDDEN || body->state == COCOSAMBO_HEADLESS) return;

        static const uint16_t knock[] = {OBJ_COCONUT};
        Actor* nut = actor_overlapping_kind(a, actor_centre(a), a->radius, knock, 1);
        for(int k = 1; !nut && k <= 3; k++)
            nut = actor_overlapping_kind(
                a, v3_add(body->position, v3_scale14(body->up, FX(80) * k * scale / 64)), FX(80) * scale / 64,
                knock, 1);
        if(nut && nut->state == COCONUT_MOVE && (nut->flags & COCONUT_SPUN)){
            actor_set_state(nut, COCONUT_GONE, -1);
            a->velocity = V3(0, 0, 0);
            actor_set_state(a, COCOSAMBO_HEAD_FALL, ANIM_COCOSAMBOHEAD_FALL);
            cocosambo_set(body, COCOSAMBO_HEADLESS);
            audio_play_effect(SFX_ENEMY_HIT, 3);
            return;
        }
        if(actor_mario_contact(a, 0) != CONTACT_NONE) actor_hurt_mario(a, 1);
        return;
    }
    case COCOSAMBO_HEAD_FALL: {
        int32_t t = fx_min(a->timer * N14_ONE / 45, N14_ONE);
        fx h = mul14(FX(300) * scale / 64, N14_ONE - mul14(t, t));
        a->position =
            v3_add(v3_add(body->position, v3_scale14(body->front, FX(250))), v3_scale14(body->up, h));
        if(t >= N14_ONE) actor_set_state(a, COCOSAMBO_HEAD_LAND, ANIM_COCOSAMBOHEAD_LAND);
        return;
    }
    case COCOSAMBO_HEAD_LAND:
        if(actor_anim_done(a)) actor_set_state(a, COCOSAMBO_HEAD_SWOON, ANIM_COCOSAMBOHEAD_SWOON);
        break;
    case COCOSAMBO_HEAD_SWOON:
        if(a->timer > 180) actor_set_state(a, COCOSAMBO_HEAD_SWOONEND, ANIM_COCOSAMBOHEAD_SWOONEND);
        break;
    case COCOSAMBO_HEAD_SWOONEND:
        if(actor_anim_done(a)) actor_set_state(a, COCOSAMBO_HEAD_ON, ANIM_COCOSAMBOHEAD_WAIT);
        break;
    case COCOSAMBO_HEAD_PRESS:
        if(a->timer > 35){
            cocosambo_set(body, COCOSAMBO_DEAD);
            actor_defeat(a, false);
        }
        return;
    case COCOSAMBO_HEAD_BLOW:
        a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
        a->velocity = v3_sub(a->velocity, v3_scale14(a->up, V16(1.0f)));
        if(a->timer > 20){
            cocosambo_set(body, COCOSAMBO_DEAD);
            actor_defeat(a, false);
        }
        return;
    }
    bool weak = a->state == COCOSAMBO_HEAD_LAND || a->state == COCOSAMBO_HEAD_SWOON ||
                (a->state == COCOSAMBO_HEAD_SWOONEND && a->timer < 60);
    int c = actor_mario_contact(a, FX(60));
    if(!weak){
        if(c != CONTACT_NONE && c != CONTACT_STAR) actor_hurt_mario(a, 1);
        return;
    }
    if(c == CONTACT_TRAMPLE || c == CONTACT_HIPDROP){
        if(c == CONTACT_TRAMPLE) mario_stomp_bounce(g_game.keys.held & KEY_A);
        actor_set_state(a, COCOSAMBO_HEAD_PRESS, ANIM_COCOSAMBOHEAD_PRESSY);
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
    } else if(c == CONTACT_SPIN || c == CONTACT_STAR){
        a->velocity =
            v3_add(v3_scale14(a->up, V16(20.0f)),
                   v3_scale14(v3_tangent14(v3_neg(actor_to_mario(a)), a->up, a->front), V16(100.0f) / 4));
        actor_set_state(a, COCOSAMBO_HEAD_BLOW, ANIM_COCOSAMBOHEAD_BLOW);
        mario_spin_hop();
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
    }
}
