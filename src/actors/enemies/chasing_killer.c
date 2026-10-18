static __attribute__((noinline)) v3 chasing_killer_turn_limited(v3 from, v3 aim, v3 axis, int cosine,
                                                                int sine){
    v3 want = v3_direction14(aim, from);
    if(v3_dot14(from, want) > cosine) return want;
    v3 reject = v3_reject14(want, from);
    if(v3_length_squared64(reject) < (uint64_t)16 * 16){
        v3 side = v3_cross14(from, axis);
        if(v3_length_squared64(side) < (uint64_t)16 * 16)
            side = v3_tangent14(V3(N14_ONE, 0, 0), from, V3(0, N14_ONE, 0));
        side = v3_direction14(side, V3(0, N14_ONE, 0));
        return v3_direction14(v3_add(from, v3_scale14(side, 328)), from);
    }
    reject = v3_direction14(reject, from);
    return v3_direction14(v3_add(v3_scale14(from, cosine), v3_scale14(reject, sine)), from);
}
static __attribute__((noinline)) v3 chasing_killer_steer(v3 from, v3 aim, v3 up, int turn_radius){
    int radius = fx_clamp(turn_radius, 0, 10000);
    int cosine = ((10000 - radius) * N14_ONE + 5000) / 10000;
    int sine = radius == 2 ? 328
               : radius == 3
                   ? 401
                   : (int)(isqrt64((uint64_t)radius * (20000 - radius) * N14_ONE * N14_ONE) / 10000);
    return chasing_killer_turn_limited(from, aim, v3_neg(up), cosine, sine);
}

static void chasing_killer_think(Actor* a){
    Player* p = game_player();
    if(a->state == KILLER_APPEAR){
        int32_t t = fx_min(a->timer * N14_ONE / 20, N14_ONE);
        a->position = v3_add(a->home, v3_scale14(a->target, mul14(FX(200), t)));
        if(a->timer > 20 && a->timer < 42){
            int rumble = a->timer - 20;
            fx offset = (22 - rumble) * mul14(FX(18), fx_sin(DEG(100) * (rumble)) << 6) / 22;
            a->position = v3_add(a->position, v3_scale14(a->target, offset));
        }
        if(a->timer == 20) audio_play_effect(SFX_BUOYROCKET_KILLER_STANDBY_END, 2);
        a->front = a->target;
        a->anim_frame = -FX_ONE;
        if(a->timer >= 72){
            a->target = a->up;
            a->velocity = V3(0, 0, 0);
            actor_set_state(a, KILLER_MOVESTART, ANIM_HOMINGKILLER_START);
            audio_play_effect(SFX_BUOY_EM_KILLER_FIRING, 2);
        }
        return;
    }
    bool moving = a->state == KILLER_MOVE || a->state == KILLER_MOVESTART;
    bool chasing = a->state == KILLER_CHASESTART || a->state == KILLER_CHASE || a->state == KILLER_TARGET;
    if(!moving && !chasing) return;

    if(a->state == KILLER_CHASESTART && a->timer == 1) audio_play_effect(SFX_BUOYROCKET_KILLER_JET, 2);
    bool ignore_gravity = actor_arg_or(a, 2, 0) > 0 || actor_arg_or(a, 3, 0) > 0;
    v3 g = gravity_sample(g_game.stage.header, a->position).up;
    v3 aim = v3_sub(v3_add(p->figure.position, v3_scale14(p->up, FX(200))), a->position);
    bool blast = false;
    if(chasing){
        if(!actor_mario_within(a, FX(3000)) || water_query(a->position, g, NULL)) blast = true;
        chasing_killer_ground_aim(a, g, &aim);
        if(chasing_killer_can_turn(a, aim))
            a->front = chasing_killer_steer(a->front, aim, g, actor_arg_or(a, 1, 3));
    } else {
        if(++a->velocity.x > 600 || (ignore_gravity && water_query(a->position, g, NULL))) blast = true;
        if(!ignore_gravity){
            v3 cross = v3_cross14(a->front, g);
            if(fx_abs(cross.x) > R14(.01f) || fx_abs(cross.y) > R14(.01f) || fx_abs(cross.z) > R14(.01f))
                a->front = chasing_killer_steer(a->front, v3_tangent14(a->front, g, a->front), g, 5);
        }
    }

    if(!ignore_gravity || chasing){
        a->target = chasing ? chasing_killer_turn_limited(a->target, g, a->front, 16374, 572) : g;
    }
    a->up = v3_tangent14(a->target, a->front, a->up);
    v3 step = v3_scale14(a->front, FX(12));
    RayHit hit;

    if(a->velocity.y >= 35 && collision_ray(a->position, v3_add(a->position, step), &hit, 0)){
        chasing_killer_ray_hit_cage(hit.tri);
        blast = true;
    }
    a->position = v3_add(a->position, step);
    if(chasing_killer_hit_target(a)) blast = true;
    if(actor_mario_contact(a, 0) != CONTACT_NONE){
        actor_hurt_mario(a, 1);
        blast = true;
    }
    if(blast){
        a->state = AS_DEFEATED;
        a->timer = 0;
        audio_play_effect(SFX_BUOYROCKET_KILLER_EXPLOSION, 3);
        return;
    }
    if(moving){
        ++a->velocity.y;
        v3 to_player = v3_sub(p->figure.position, a->position);
        v3 direction = v3_direction14(to_player, V3(0, 0, 0));

        int sight = actor_arg_or(a, 3, 0) > 0 ? -N14_ONE : 0;
        if(a->velocity.y >= 35 && actor_arg_or(a, 2, 0) <= 0 && v3_length_squared64(direction) &&
           v3_dot14(direction, a->front) > sight && actor_mario_within(a, FX(actor_arg_or(a, 0, 2000)))){
            actor_set_state(a, KILLER_CHASESTART, ANIM_HOMINGKILLER_CHASESTART);
        } else if(a->state == KILLER_MOVESTART && a->timer >= 34)
            actor_set_state(a, KILLER_MOVE, ANIM_HOMINGKILLER_MOVE);
    } else {
        if(a->state == KILLER_CHASESTART && a->timer >= 58)
            actor_set_state(a, KILLER_CHASE, ANIM_HOMINGKILLER_CHASE);

        if(a->state == KILLER_CHASE || a->state == KILLER_TARGET){
            v3 sensed;
            if(chasing_killer_cage_target(a, &sensed)){
                if(a->state != KILLER_TARGET) actor_set_state(a, KILLER_TARGET, ANIM_HOMINGKILLER_CHASE);
            } else if(a->state == KILLER_TARGET) actor_set_state(a, KILLER_CHASE, ANIM_HOMINGKILLER_CHASE);
        }
    }
}
