static float throwing_ice_path_sqrt(float x){
    float root = float_sqrt_approx(x);
    for(int i = 0; i < 3; i++) root = 0.5f * (root + x / root);

    uint32_t xb = float_bits(x), bits = float_bits(root);
    if((xb & 0x7f800000) == 0 || (xb & 0x7f800000) == 0x7f800000) return root;
    for(int i = 0; i < 3; i++){
        uint32_t mant = (bits & 0x7fffff) | 0x800000;
        int shift = (int)((xb >> 23) & 255) - 2 * (int)((bits >> 23) & 255) + 152;
        uint64_t value = (uint64_t)((xb & 0x7fffff) | 0x800000) << shift;
        uint64_t upper = (uint64_t)(mant * 2 + 1) * (mant * 2 + 1);
        if(value > upper || (value == upper && (bits & 1))){
            bits++;
            continue;
        }
        uint64_t lower;
        if(mant == 0x800000){
            lower = (uint64_t)(mant * 4 - 1) * (mant * 4 - 1);
            value <<= 2;
        } else lower = (uint64_t)(mant * 2 - 1) * (mant * 2 - 1);
        if(value < lower || (value == lower && (bits & 1))){
            bits--;
            continue;
        }
        break;
    }
    return float_from_bits(bits);
}
static void throwing_ice_path_params(float end, float* accel, float* velocity){
    float max = 200.0f + (end > 0.0f ? end : 0.0f);
    float discrim = max * (max - end);
    if(discrim <= 0.0f){
        *accel = -max;
        *velocity = end + max;
        return;
    }
    if(end > -0.0001f && end < 0.0001f){
        *accel = -4.0f * max;
        *velocity = 4.0f * max;
        return;
    }
    float root = throwing_ice_path_sqrt(discrim);
    float t1 = (max + root) / end, t2 = (max - root) / end;
    float t = (t1 >= 0.0f && t1 <= 1.0f) ? t1 : (t2 >= 0.0f && t2 <= 1.0f) ? t2 : 1.0f;
    *accel = -max / (t * t);
    *velocity = -2.0f * t * (*accel);
}
static inline int32_t throwing_ice_path_coordinate(int32_t start, int32_t up, int32_t horizontal,
                                                   float height, float rate){
    return float_to_int_saturated(start + up * (height / 16384.0f) + horizontal * rate);
}
static void throwing_ice_kill(Actor* a, bool sound){
    if(sound) audio_play_effect(SFX_ICEKING_STONE_BREAK, 3);
    a->state = THROWING_ICE_IDLE;
    a->timer = 0;
    a->flags |= ACTOR_HIDDEN;
    a->velocity = V3(0, 0, 0);
    a->health = 0;
}
static void throwing_ice_star_hit(Actor* a){
    if(a->state == THROWING_ICE_THROW) throwing_ice_kill(a, true);
}
static float throwing_ice_arg_float(const Actor* a, int i){
    return float_from_bits((uint16_t)a->args[2 * i] | ((uint32_t)(uint16_t)a->args[2 * i + 1] << 16));
}
static void throwing_ice_set_float(Actor* a, int i, float value){
    uint32_t bits = float_bits(value);
    a->args[2 * i] = (int16_t)bits;
    a->args[2 * i + 1] = (int16_t)(bits >> 16);
}
static bool throwing_ice_pool_init(Actor* host){
    int count = 0, owner = actor_index(host);
    for(int i = 0; i < g_game.actor_count; i++)
        if(g_game.actors[i].active && g_game.actors[i].type == OBJ_ICEKINGWEAPON &&
           g_game.actors[i].value == owner)
            count++;
    while(count < 6){
        Actor* a = actor_spawn_reuse(OBJ_ICEKINGWEAPON, host->position, host->up, host->front);
        if(!a && stream_evict(host->position))
            a = actor_spawn_reuse(OBJ_ICEKINGWEAPON, host->position, host->up, host->front);
        if(!a) return false;
        a->value = owner;
        throwing_ice_kill(a, false);
        count++;
    }
    return true;
}
static void throwing_ice_emit(Actor* a, v3 from, v3 target, v3 path_up){
    a->position = a->home = from;
    a->up = path_up;
    v3 delta = v3_sub(target, from);
    float end = ((float)delta.x * path_up.x + (float)delta.y * path_up.y + (float)delta.z * path_up.z) *
                (1.0f / 16384.0f);
    a->target = V3(float_to_int_saturated(delta.x - path_up.x * (end / 16384.0f)),
                   float_to_int_saturated(delta.y - path_up.y * (end / 16384.0f)),
                   float_to_int_saturated(delta.z - path_up.z * (end / 16384.0f)));
    GravitySample sample = gravity_sample(g_game.stage.header, from);
    v3 gravity_up = sample.index < 0 ? path_up : sample.up;
    v3 direction = v3_direction14(v3_reject14(delta, gravity_up), V3(0, 0, 0));
    a->front = v3_cross14(direction, v3_neg(gravity_up));
    float accel, velocity;
    throwing_ice_path_params(end, &accel, &velocity);
    throwing_ice_set_float(a, 0, accel);
    throwing_ice_set_float(a, 1, velocity);
    a->flags &= ~(ACTOR_HIDDEN | ACTOR_ON_GROUND);
    a->health = 0;
    a->velocity = V3(0, 0, 0);
    actor_set_state(a, THROWING_ICE_THROW, -1);
}
static v3 throwing_ice_path_position(const Actor* a, int step){
    float rate = step >= 101 ? 1.0f : (float)step / 101.0f;
    float height = rate * (throwing_ice_arg_float(a, 1) + throwing_ice_arg_float(a, 0) * rate);
    return V3(throwing_ice_path_coordinate(a->home.x, a->up.x, a->target.x, height, rate),
              throwing_ice_path_coordinate(a->home.y, a->up.y, a->target.y, height, rate),
              throwing_ice_path_coordinate(a->home.z, a->up.z, a->target.z, height, rate));
}

__attribute__((noinline)) static void throwing_ice_bind(Actor* a, v3 desired){
    CollisionCandidates candidates;
    collision_gather_around(a->position, desired, 78 + 1, &candidates);
    bool bound = false;
    RayHit ray;
    if(collision_raycast(&candidates, a->position, desired, &ray, 0)){
        desired = v3_add(ray.point, v3_scale14(ray.normal, 78));
        bound = true;
    }
    for(int i = 0; i < candidates.count; i++){
        SphereContact hit;
        if(!collectible_sphere_triangle(candidates.index[i], desired, 78, &hit) || hit.depth < 0) continue;
        desired = v3_add(desired, v3_scale14(hit.normal, hit.depth + 1));
        bound = true;
    }
    a->position = desired;
    if(bound) a->flags |= ACTOR_ON_GROUND;
    else a->flags &= ~ACTOR_ON_GROUND;
}
static bool throwing_ice_damage_mario(v3 position){
    Player* p = &g_game.player;
    int health = p->health, action = p->action;
    mario_damage(position, 1);
    return p->health != health || p->action != action;
}
__attribute__((noinline)) static void throwing_ice_think(Actor* a){
    if(a->value < 0 || a->value >= g_game.actor_count || !g_game.actors[a->value].active ||
       g_game.actors[a->value].type != OBJ_ICEKING){
        a->active = 0;
        return;
    }
    if(a->state == THROWING_ICE_IDLE){
        a->timer = 0;
        return;
    }
    int step = a->timer - 1;
    v3 desired;
    if(a->state == THROWING_ICE_THROW){
        desired = throwing_ice_path_position(a, step);
        v3 delta = v3_sub(desired, a->position);
        a->velocity = V3(delta.x * 256, delta.y * 256, delta.z * 256);
    } else {
        GravitySample sample = gravity_sample(g_game.stage.header, a->position);
        v3 up = sample.index < 0 ? a->up : sample.up;
        v3 velocity = v3_sub(a->velocity, v3_scale14(up, V16(2)));
        int32_t along = v3_dot14(velocity, up);
        v3 horizontal = v3_scale14(v3_reject14(velocity, up), R14(.98f));
        if(along > 0) along = mul14(along, R14(.98f));
        a->velocity = v3_add(horizontal, v3_scale14(up, along));
        v3 move;
        for(int i = 0; i < 3; i++){
            int32_t fraction = (&a->velocity.x)[i] + a->args[i + 1];
            (&move.x)[i] = fraction >> 8;
            a->args[i + 1] = (int16_t)(fraction & 255);
        }
        desired = v3_add(a->position, move);
    }
    a->health++;
    if((a->flags & ACTOR_ON_GROUND) || (a->state == THROWING_ICE_FALL && step > 60)){
        throwing_ice_kill(a, true);
        return;
    }
    if(actor_touches_mario(a, 0)){
        bool ice = mario_power_mode() == 3;
        if(ice ? throwing_ice_damage_mario(a->position) : mario_freeze_hit()){
            audio_play_effect(ice ? SFX_ICE_PROJECTILE_DAMAGE : SFX_FREEZE_PROJECTILE_HIT, 4);
            throwing_ice_kill(a, false);
            return;
        }
    }

    Player* p = &g_game.player;
    if(a->state == THROWING_ICE_THROW && actor_touches_mario(a, 0) &&
       (g_mario_star_power || p->action == MA_HIPDROP ||
        (g_mario.jumping && vertical_speed(g_mario.jump_vec, p->up) < 0 &&
         v3_dot14(v3_sub(mario_centre(), a->position), p->up) > 60))){
        throwing_ice_kill(a, true);
        return;
    }
    if(a->state == THROWING_ICE_THROW && step > 100){
        actor_set_state(a, THROWING_ICE_FALL, -1);
        memset(a->args, 0, sizeof a->args);
    }
    throwing_ice_bind(a, desired);
}
static void throwing_ice_figure(const Actor* a, Figure* f){
    v3 axis = a->front;
    fx angle = (int)a->health * FXC(.25f);
    f->basis_up = v3_rotate_axis14(V3(0, 0, N14_ONE), axis, angle);
    f->basis_front = v3_rotate_axis14(V3(0, N14_ONE, 0), axis, angle);
}
