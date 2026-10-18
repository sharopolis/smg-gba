static void coconut_set_front(Actor* a, v3 delta, int32_t parallel){
    if(!(delta.x | delta.y | delta.z)) return;
    int64_t cx = (int64_t)delta.y * a->up.z - (int64_t)delta.z * a->up.y;
    int64_t cy = (int64_t)delta.z * a->up.x - (int64_t)delta.x * a->up.z;
    int64_t cz = (int64_t)delta.x * a->up.y - (int64_t)delta.y * a->up.x;
    if(cx >= -parallel && cx <= parallel && cy >= -parallel && cy <= parallel && cz >= -parallel &&
       cz <= parallel)
        a->front = v3_direction14(delta, a->front);
    else {
        int32_t dot8 = (int32_t)(((int64_t)delta.x * a->up.x + (int64_t)delta.y * a->up.y +
                                  (int64_t)delta.z * a->up.z) >>
                                 6);
        v3 precise = v3_sub(V3(delta.x * 256, delta.y * 256, delta.z * 256), v3_scale14(a->up, dot8));
        a->front = v3_direction14(precise, a->front);
    }
}

static void coconut_kick(Actor* a, int32_t speed, int32_t lift, bool spin){
    GravitySample gravity = gravity_sample(g_game.stage.header, actor_centre(a));
    if(gravity.index >= 0) a->up = gravity.up;
    coconut_set_front(a, v3_sub(actor_centre(a), mario_centre()), 163);

    a->target.y = speed;
    a->target.z = -lift;
    a->velocity = v3_add(v3_scale14(a->front, speed), v3_scale14(a->up, lift));
    a->flags = (a->flags & ~ACTOR_ON_GROUND) | (spin ? COCONUT_SPUN : 0);
    actor_set_state(a, COCONUT_MOVE, -1);
}

static bool coconut_walk_kick(Actor* a){
    v3 velocity = g_mario.jump_vec;
    if(a->state == COCONUT_MOVE && a->timer < 15) return false;
    if(!(velocity.x | velocity.y | velocity.z)) return false;
    if(v3_length_squared64(velocity) < v3_length_squared64(a->velocity)) return false;
    GravitySample gravity = gravity_sample(g_game.stage.header, actor_centre(a));
    v3 up = gravity.index >= 0 ? gravity.up : a->up;
    if(a->target.z < 0) return false;
    v3 delta = v3_sub(actor_centre(a), mario_centre());
    if(!(delta.x | delta.y | delta.z)) return false;
    v3 to = v3_direction14(delta, a->front), motion = v3_direction14(velocity, a->front);
    if(v3_dot14(to, motion) < R14(0.7071067812f)) return false;
    a->up = up;
    coconut_set_front(a, velocity, 41943);
    int32_t along = v3_dot14(a->front, velocity);
    int32_t speed = fx_max(V16(18.0f), along * 11 / 10), lift = along * 7 / 10;
    a->target.y = speed;
    a->target.z = -lift;
    a->velocity = v3_add(v3_scale14(a->front, speed), v3_scale14(up, lift));
    a->flags &= ~ACTOR_ON_GROUND;
    actor_set_state(a, COCONUT_MOVE, -1);
    return true;
}

static void coconut_move(Actor* a){
    GravitySample g = gravity_sample(g_game.stage.header, actor_centre(a));
    if(g.index >= 0) a->up = g.up;
    bool ground = (a->flags & ACTOR_ON_GROUND) && a->target.z > 0;
    if(ground){
        a->target.z = a->target.z > V16(2.5f) ? -a->target.z / 2 : 0;
        a->target.y = mul14(a->target.y, R14(0.925f));
        a->flags &= ~COCONUT_SPUN;
    }
    coconut_set_front(a, a->front, 2684354);
    a->target.z = fx_min(V16(25.0f), a->target.z + ((a->flags & COCONUT_SPUN) ? V16(0.4f) : V16(1.0f)));
    a->velocity = v3_sub(v3_scale14(a->front, a->target.y), v3_scale14(a->up, a->target.z));

    a->flags &= ~ACTOR_ON_GROUND;
    actor_move(a, 0, N14_ONE, N14_ONE);
}

static void coconut_think(Actor* a){
    if(a->state == COCONUT_GONE){
        a->flags |= ACTOR_HIDDEN;
        if(a->timer > 180 && !actor_mario_within(a, FX(1500))){
            a->position = a->home;
            a->velocity = V3(0, 0, 0);
            a->target.y = a->target.z = 0;
            a->flags &= ~(ACTOR_HIDDEN | COCONUT_SPUN);
            actor_set_state(a, COCONUT_WAIT, -1);
        }
        return;
    }
    if(a->state == COCONUT_MOVE){
        coconut_move(a);
        bool spun = a->flags & COCONUT_SPUN;
        v3 h = v3_reject14(a->velocity, a->up);
        a->target.x += v3_length_exact(h) * FX_ONE / (55 << 8);
        bool wall = actor_bounce_wall(a, FX(70), R14(0.8f));
        if(wall){
            a->front = v3_direction14(v3_reject14(a->velocity, a->up), a->front);
            a->target.y = mul14(a->target.y, R14(0.8f));
        }
        static const uint16_t victims[] = {OBJ_KURIBO,           OBJ_KURIBOMINI, OBJ_TAKOBO,
                                           OBJ_KARIKARI,         OBJ_OTAROCK,    OBJ_PETARI,
                                           OBJ_HAMMERHEADPACKUN, OBJ_COCONUT,    OBJ_NEEDLEPLANT};
        Actor* hit = spun ? actor_overlapping_kind(a, actor_centre(a), a->radius, victims, 9) : NULL;
        if(hit) actor_blow_away(hit, a);
        if(spun && (wall || hit)){
            actor_set_state(a, COCONUT_GONE, -1);
            audio_play_effect(SFX_ENEMY_HIT, 2);
            return;
        }
        if((a->flags & ACTOR_ON_GROUND) && v3_length_squared64(h) < (uint64_t)V16(1.5f) * V16(1.5f)){
            a->velocity = V3(0, 0, 0);
            a->target.y = a->target.z = 0;
            a->flags &= ~COCONUT_SPUN;
            actor_set_state(a, COCONUT_WAIT, -1);
        }
    } else if(actor_arg_or(a, 0, 0) > 0){
        actor_move(a, V16(1.0f), R14(0.925f), N14_ONE);
    }
    switch(actor_mario_contact(a, FX(60))){
    case CONTACT_SPIN:
        coconut_kick(a, V16(35.0f), V16(20.0f), true);
        audio_play_effect(SFX_ENEMY_HIT, 2);
        break;
    case CONTACT_STAR:
        coconut_kick(a, V16(25.0f), V16(15.0f), false);
        audio_play_effect(SFX_ENEMY_HIT, 2);
        break;
    case CONTACT_HIPDROP:
        coconut_kick(a, V16(25.0f), V16(15.0f), false);
        mario_stomp_bounce(false);
        break;
    case CONTACT_TRAMPLE:
        coconut_kick(a, V16(10.0f), V16(5.0f), false);
        mario_stomp_bounce(g_game.keys.held & KEY_A);
        break;
    case CONTACT_TOUCH: {
        coconut_walk_kick(a);
        break;
    }
    }
}

static void coconut_figure(const Actor* a, Figure* f){
    f->local_pitch = a->target.x;
    f->position = v3_add(f->position, v3_scale14(a->up, FX(55)));
}

static void coconut_init(Actor* a, const StageObject* o){
    a->target.y = a->target.z = 0;
    actor_set_state(a, COCONUT_WAIT, -1);
}

static bool coconut_idle(const Actor* a){ return a->state == COCONUT_WAIT; }

static void coconut_blown_away(Actor* a, Actor* by){
    a->velocity = v3_add(a->velocity, v3_scale14(by->velocity, R14(0.5f)));
    by->velocity = v3_scale14(by->velocity, R14(0.5f));
    a->front = v3_direction14(v3_reject14(a->velocity, a->up), a->front);
    by->front = v3_direction14(v3_reject14(by->velocity, by->up), by->front);
    a->target.y = v3_length_exact(v3_reject14(a->velocity, a->up));
    a->target.z = -v3_dot14(a->velocity, a->up);
    by->target.y = v3_length_exact(v3_reject14(by->velocity, by->up));
    by->target.z = -v3_dot14(by->velocity, by->up);
    a->flags &= ~ACTOR_ON_GROUND;
    actor_set_state(a, COCONUT_MOVE, -1);
}
