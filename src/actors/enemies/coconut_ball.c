static Actor* coconut_ball_throw(Actor* host, v3 from, v3 at, int32_t speed){
    Actor* b = actor_spawn_reuse(OBJ_COCONUTBALL, from, host->up, host->front);
    if(!b) return NULL;
    b->value = (int16_t)actor_index(host);
    b->health = 0;
    b->velocity = v3_scale14(v3_direction14(v3_sub(at, from), host->front), speed);
    actor_set_state(b, COCONUT_BALL_THROW, -1);
    return b;
}

static void coconut_ball_return(Actor* a, const Actor* host){
    v3 up = a->up, player = g_game.player.figure.position;
    v3 host_from_player = v3_tangent14(v3_sub(host->position, player), up, host->front);
    v3 ball_from_player = v3_tangent14(v3_sub(a->position, player), up, a->front);
    bool right = v3_dot14(up, v3_cross14(host_from_player, ball_from_player)) > 0;
    bool front = v3_dot14(v3_neg(host_from_player), ball_from_player) < R14(-.95f);
    v3 destination = v3_add(actor_centre(host), v3_scale14(up, FX(100)));
    if(!front){
        v3 side = v3_direction14(v3_cross14(up, host_from_player), a->front);
        destination = v3_add(destination, v3_scale14(side, right ? FX(150) : -FX(150)));
    }
    v3 delta = v3_sub(destination, a->position), horizontal = v3_reject14(delta, up);
    fx height = v3_dot14(delta, up), distance = v3_length_exact(horizontal);
    int32_t time = fx_max((int32_t)((int64_t)distance * 256 / 42), 1);
    v3 dir = v3_direction14(horizontal, a->front), side = v3_direction14(v3_cross14(up, dir), a->front);
    int32_t vertical = (int32_t)((int64_t)height * 65536 / time) + (int32_t)((int64_t)V16(2.2f) * time / 512);
    a->velocity = v3_add(v3_scale14(dir, V16(42)), v3_scale14(up, vertical));
    a->home = v3_scale14(up, -V16(2.2f));
    if(!front){
        int32_t lateral = right ? V16(1.2f) : -V16(1.2f);
        a->velocity = v3_add(a->velocity, v3_scale14(side, (int32_t)((int64_t)lateral * time / 512)));
        a->home = v3_sub(a->home, v3_scale14(side, lateral));
    }
}

static void coconut_ball_think(Actor* a){
    Actor* host = &g_game.actors[a->value];

    if(a->state == COCONUT_BALL_TO_HOST && a->timer >= 2) a->velocity = v3_add(a->velocity, a->home);
    a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
    a->target.x += DEG(12);
    if(a->state == COCONUT_BALL_REBOUND)
        a->velocity = v3_sub(a->velocity, v3_scale14(a->up, V16(2.2f * 1.0f)));
    if(a->state != COCONUT_BALL_TO_HOST) a->up = host->up;

    RayHit hit;
    bool gone =
        a->timer > 600 || !host->active || v3_length_exact(actor_to_mario(a)) > FX(5000) ||
        (a->state != COCONUT_BALL_TO_HOST && a->timer > 10 &&
         collision_ray(a->position,
                       v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8)),
                       &hit, 0));
    if(gone){
        actor_defeat(a, false);
        return;
    }
    if(a->state == COCONUT_BALL_TO_HOST){
        v3 d = v3_sub(actor_centre(host), a->position);
        fx r = host->radius + FX(40);
        if(v3_length_squared64(d) < (uint64_t)((int64_t)r * r)){
            if(host->type == OBJ_OTAKING) otaking_ball_arrives(host, a);
            else otarock_ball_arrives(host, a);
        }
        return;
    }

    if(g_mario.attack_timer > 0 && actor_mario_within(a, FX(400)) && a->state != COCONUT_BALL_TO_HOST){
        a->health++;
        coconut_ball_return(a, host);
        actor_set_state(a, COCONUT_BALL_TO_HOST, -1);
        audio_play_effect(SFX_ENEMY_HIT, 3);
        return;
    }
    if(actor_touches_mario(a, 0) && a->state == COCONUT_BALL_THROW){
        mario_damage(a->position, 0);
        v3 away = v3_tangent14(v3_neg(actor_to_mario(a)), a->up, a->front);
        a->velocity = v3_add(v3_scale14(a->up, V16(15.0f)), v3_scale14(away, V16(5.0f)));
        actor_set_state(a, COCONUT_BALL_REBOUND, -1);
    }
}

static void coconut_ball_figure(const Actor* a, Figure* f){ f->local_pitch = a->target.x; }
