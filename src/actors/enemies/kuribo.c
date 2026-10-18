static const KuriboKind kuribo_kind = {ANIM_KURIBO_WAIT,
                                       ANIM_KURIBO_WALK,
                                       ANIM_KURIBO_RUN,
                                       ANIM_KURIBO_TURN,
                                       ANIM_KURIBO_JUMPSTART,
                                       ANIM_KURIBO_JUMP,
                                       ANIM_KURIBO_LAND,
                                       ANIM_KURIBO_HIT,
                                       ANIM_KURIBO_FLATDOWN,
                                       ANIM_KURIBO_HIPDROPDOWN,
                                       ANIM_KURIBO_BLOW,
                                       ANIM_KURIBO_STAGGERSTART,
                                       ANIM_KURIBO_STAGGER,
                                       ANIM_KURIBO_STAGGEREND,
                                       V16(0.2f),
                                       V16(0.4f),
                                       false};
static const KuriboKind kuribomini_kind = {ANIM_KURIBOMINI_WAIT,
                                           ANIM_KURIBOMINI_WALK,
                                           ANIM_KURIBOMINI_RUN,
                                           ANIM_KURIBOMINI_TURN,
                                           ANIM_KURIBOMINI_JUMPSTART,
                                           ANIM_KURIBOMINI_JUMP,
                                           ANIM_KURIBOMINI_LAND,
                                           ANIM_KURIBOMINI_HIT,
                                           ANIM_KURIBOMINI_FLATDOWN,
                                           ANIM_KURIBOMINI_FLATDOWN,
                                           ANIM_KURIBOMINI_BLOWDOWN,
                                           -1,
                                           -1,
                                           -1,
                                           V16(0.1f),
                                           V16(0.2f),
                                           true};

static void kuribo_blow_down(Actor* a, const KuriboKind* k, int32_t h, int32_t v, bool bits){
    v3 away = v3_tangent14(v3_neg(actor_to_mario(a)), a->up, v3_neg(a->front));
    a->velocity = v3_add(v3_scale14(away, h), v3_scale14(a->up, v));
    a->flags &= ~ACTOR_ON_GROUND;
    actor_set_state(a, KURIBO_BLOWDOWN, k->blow);
    if(bits) collectibles_pop(OBJ_STARBIT, actor_centre(a), a->up, 3);
    else collectibles_pop(OBJ_COIN, actor_centre(a), a->up, 1);
    audio_play_effect(SFX_ENEMY_DEFEAT, 3);
}

static const KuriboKind* kuribo_kind_of(const Actor* a){
    return a->type == OBJ_KURIBOMINI ? &kuribomini_kind : &kuribo_kind;
}

static void kuribo_init(Actor* a, const StageObject* o){
    actor_set_state(a, KURIBO_WAIT, kuribo_kind_of(a)->wait);
}

static void kuribo_blown_away(Actor* a, Actor* by){
    if(a->state >= KURIBO_FLAT) return;
    const KuriboKind* k = kuribo_kind_of(a);
    kuribo_blow_down(a, k, k->mini ? V16(22.0f) : V16(36.0f), V16(30.0f), !k->mini);
}

static void kuribo_think(Actor* a){
    const KuriboKind* k = kuribo_kind_of(a);
    bool sight = actor_sees_mario(a, FX(1000), DEG(70), DEG(30));
    switch(a->state){
    case KURIBO_WAIT:
        if(sight){
            actor_set_state(a, KURIBO_FIND, k->turn);
            break;
        }
        if(!k->mini && (a->flags & ACTOR_ON_GROUND) && !actor_mario_within(a, FX(3000))){
            actor_set_state(a, KURIBO_SLEEP, k->wait);
            break;
        }
        if(a->timer > 120){
            fx ang = (fx)((g_game.tick * 2654435761u >> 8) % FX_TAU);
            v3 side = v3_rotate_axis14(a->front, a->up, ang);
            a->target = v3_add(a->home, v3_scale14(side, FX(100 + (int)(g_game.tick * 7 % 400))));
            actor_set_state(a, KURIBO_WALK, k->walk);
        }
        break;
    case KURIBO_WALK: {
        if(sight){
            actor_set_state(a, KURIBO_FIND, k->turn);
            break;
        }
        v3 to = v3_sub(a->target, a->position);
        fx ang = actor_turn_toward(a, to, DEG(3));
        if(ang < DEG(8)) actor_push_forward(a, k->wander);
        if((a->timer & 7) == 0 && !actor_floor_ahead(a, FX(150))){
            a->velocity = V3(0, 0, 0);
            actor_set_state(a, KURIBO_WAIT, k->wait);
        } else if(a->timer > 120 || v3_length_exact(v3_reject14(to, a->up)) < FX(20)){
            actor_set_state(a, KURIBO_WAIT, k->wait);
        }
        break;
    }
    case KURIBO_SLEEP:
        if(actor_mario_within(a, FX(3000))) actor_set_state(a, KURIBO_WAIT, k->wait);
        return;
    case KURIBO_FIND:
        actor_turn_toward(a, actor_to_mario(a), DEG(5));
        if(a->timer >= 30) actor_set_state(a, KURIBO_JUMPSTART, k->jumpstart);
        break;
    case KURIBO_JUMPSTART:
        if(actor_anim_done(a)){
            actor_set_state(a, KURIBO_JUMP, k->jump);
            a->velocity = v3_add(a->velocity, v3_scale14(a->up, V16(20.0f)));
            a->flags &= ~ACTOR_ON_GROUND;
        }
        break;
    case KURIBO_JUMP:
        if((a->flags & ACTOR_ON_GROUND) && a->timer > 5) actor_set_state(a, KURIBO_LAND, k->land);
        break;
    case KURIBO_LAND:
        if(actor_anim_done(a)) actor_set_state(a, KURIBO_CHASE, k->run);
        break;
    case KURIBO_CHASE:
        if(sight) actor_turn_toward(a, actor_to_mario(a), DEG(2));
        actor_push_forward(a, k->chase);
        if(((a->timer & 7) == 0 && !actor_floor_ahead(a, FX(150))) || a->timer > 300 ||
           (a->timer > 130 && !sight)){
            if(!actor_floor_ahead(a, FX(150))) a->velocity = V3(0, 0, 0);
            actor_set_state(a, KURIBO_CHASE_END, k->wait);
        }
        break;
    case KURIBO_CHASE_END:
        if(a->timer > 30){
            a->home = a->position;
            actor_set_state(a, KURIBO_WAIT, k->wait);
        }
        break;
    case KURIBO_HIT:
        actor_turn_toward(a, actor_to_mario(a), DEG(5));
        if(a->timer > 60) actor_set_state(a, KURIBO_WAIT, k->wait);
        break;
    case KURIBO_STAGGER: {
        if(a->anim == k->stagger_start && actor_anim_done(a)){
            a->anim = k->stagger;
            a->anim_frame = 0;
        }

        int32_t t = a->timer < 60 ? N14_ONE : a->timer < 120 ? (120 - a->timer) * N14_ONE / 60 : 0;
        a->front = v3_tangent14(v3_rotate_axis14(a->front, a->up, fx_mul(DEG(40), t >> 6)), a->up, a->front);
        v3 side = v3_cross14(a->up, a->front);
        actor_push_forward(a, mul14(V16(0.35f), t));
        a->velocity =
            v3_add(a->velocity, v3_scale14(side, mul14(mul14(fx_sin(DEG(3) * a->timer) << 6, V16(0.5f)), t)));
        if(actor_bounce_wall(a, FX(80), R14(0.9f))){
        }
        if(a->timer > 180) actor_set_state(a, KURIBO_STAGGER_END, k->stagger_end);
        break;
    }
    case KURIBO_STAGGER_END:
        if(actor_anim_done(a)){
            a->home = a->position;
            actor_set_state(a, KURIBO_WAIT, k->wait);
        }
        break;
    case KURIBO_FLAT:
        if(a->timer > 20){
            actor_defeat(a, true);
            return;
        }
        break;
    case KURIBO_HIPDROP:
        if(a->timer > 40){
            actor_defeat(a, true);
            return;
        }
        break;
    case KURIBO_BLOWDOWN:
        a->front = v3_tangent14(v3_rotate_axis14(a->front, a->up, DEG(30)), a->up, a->front);
        if(a->timer > 30){
            actor_defeat(a, false);
            return;
        }
        break;
    }
    actor_follow_gravity(a, R14(0.1f));
    actor_move(a, V16(1.5f), R14(0.93f), R14(0.99f));

    if(a->state == KURIBO_FLAT || a->state == KURIBO_HIPDROP || a->state == KURIBO_BLOWDOWN) return;
    bool staggered = a->state == KURIBO_STAGGER || (a->state == KURIBO_STAGGER_END && a->timer < 15);
    bool kickable = a->state == KURIBO_STAGGER && a->timer >= 15;
    switch(actor_mario_contact(a, FX(60))){
    case CONTACT_STAR: kuribo_blow_down(a, k, V16(36.0f), V16(30.0f), true); break;
    case CONTACT_TRAMPLE:
        if(staggered) break;
        mario_stomp_bounce(g_game.keys.held & KEY_A);
        actor_set_state(a, KURIBO_FLAT, k->flat);
        a->velocity = V3(0, 0, 0);
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
        break;
    case CONTACT_HIPDROP:
        actor_set_state(a, KURIBO_HIPDROP, k->hipdrop);
        a->velocity = V3(0, 0, 0);
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
        break;
    case CONTACT_SPIN:
        if(a->state == KURIBO_STAGGER && !kickable) break;
        mario_spin_hop();
        if(k->mini){
            kuribo_blow_down(a, k, V16(22.0f), V16(25.0f), false);
        } else if(kickable){
            kuribo_blow_down(a, k, V16(36.0f), V16(30.0f), true);
        } else {
            v3 away = v3_tangent14(v3_neg(actor_to_mario(a)), a->up, v3_neg(a->front));
            v3 round = v3_cross14(a->up, away);
            a->velocity = v3_add(v3_add(v3_scale14(away, V16(10.0f)), v3_scale14(a->up, V16(10.0f))),
                                 v3_scale14(round, V16(10.0f)));
            a->flags &= ~ACTOR_ON_GROUND;
            actor_set_state(a, KURIBO_STAGGER, k->stagger_start);
            audio_play_effect(SFX_ENEMY_HIT, 2);
        }
        break;
    case CONTACT_TOUCH:
        if(kickable && g_mario.ratio > R14(0.3f)){
            kuribo_blow_down(a, k, V16(36.0f), V16(30.0f), true);
        } else if(!staggered && a->state != KURIBO_HIT && game_player()->action != MA_HIPDROP){
            actor_hurt_mario(a, 1);
            actor_set_state(a, KURIBO_HIT, k->hit);
        }
        break;
    }
}
