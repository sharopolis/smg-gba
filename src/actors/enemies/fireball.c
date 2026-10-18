static void fireball_kill(Actor* a){
    audio_play_effect(SFX_FIRE_BALL_BREAK, 3);
    a->active = 0;
}
static void fireball_star_hit(Actor* a){
    if(a->state == FIREBALL_THROW || a->state == FIREBALL_REFLECT) fireball_kill(a);
}

static Actor* fireball_throw(Actor* host, v3 from, int32_t speed, fx angle){
    GravitySample gravity = gravity_sample(g_game.stage.header, from);
    v3 up = gravity.index < 0 ? host->up : gravity.up;
    v3 target = v3_add(g_game.player.figure.position, v3_scale14(up, FX(120)));
    v3 direction = v3_direction14(v3_sub(target, from), host->front);
    direction = v3_rotate_axis14(direction, up, angle);
    Actor* a = actor_spawn_reuse(OBJ_FIREBALL, from, up, direction);
    if(!a) return NULL;
    a->value = (int16_t)actor_index(host);
    a->front = direction;
    a->velocity = v3_scale14(direction, speed);
    actor_set_state(a, FIREBALL_THROW, ANIM_FIREBALL_SPIN);
    return a;
}

__attribute__((noinline)) static void fireball_bind(Actor* a){
    v3 motion;
    for(int axis = 0; axis < 3; axis++){
        int32_t fraction = (&a->velocity.x)[axis] + a->args[axis + 1];
        (&motion.x)[axis] = fraction >> 8;
        a->args[axis + 1] = (int16_t)(fraction & 255);
    }
    v3 from = a->position, to = v3_add(from, motion);
    CollisionCandidates candidates;
    collision_gather_around(from, to, FX(60) + 1, &candidates);
    int tri = -1;
    RayHit ray;
    if(collision_raycast(&candidates, from, to, &ray, 0)){
        to = v3_add(ray.point, v3_scale14(ray.normal, FX(60)));
        tri = ray.tri;
    }
    for(int i = 0; i < candidates.count; i++){
        SphereContact hit;
        if(!collectible_sphere_triangle(candidates.index[i], to, FX(60), &hit) || hit.depth < 0) continue;
        to = v3_add(to, v3_scale14(hit.normal, hit.depth + 1));
        tri = hit.tri;
    }
    a->position = to;
    a->args[0] = (int16_t)tri;
    if(tri >= 0) a->flags |= ACTOR_ON_GROUND;
    else a->flags &= ~ACTOR_ON_GROUND;
}

static bool fireball_try_kill(Actor* a){
    if((a->flags & ACTOR_ON_GROUND) || !actor_mario_within(a, FX(5000))){
        fireball_kill(a);
        return true;
    }
    return false;
}

static void fireball_think(Actor* a){
    int step = a->timer - 1;
    v3 swipe;

    if(a->state == FIREBALL_THROW && a->timer > 32) a->timer = 33;
    bool reflect = pointer_fireball_reflect(a, &swipe);
    if(a->state == FIREBALL_THROW && step > 30 && reflect){
        swipe = v3_reject14(swipe, a->up);
        uint32_t seed = (g_game.tick ^ ((uint32_t)actor_index(a) << 16)) * 1664525u + 1013904223u;
        swipe = v3_add(swipe, v3_scale14(a->up, (int32_t)(seed >> 18)));
        a->velocity = v3_scale14(v3_direction14(swipe, a->up), V16(100));
        actor_set_state(a, FIREBALL_REFLECT, -1);
    } else {
        if(a->state == FIREBALL_REFLECT){
            if(step == 0) audio_play_effect(SFX_FIRE_BUBBLE_REFLECT, 3);
            a->velocity = v3_scale14(a->velocity, R14(.96f));
        }
        if(fireball_try_kill(a)) return;
        if(a->state == FIREBALL_REFLECT && step == 60){
            fireball_kill(a);
            return;
        }
    }

    if(actor_touches_mario(a, 0) && mario_receive_enemy_attack_fire(a->position)){
        fireball_kill(a);
        return;
    }
    if(fireball_attack_enemies(a)){
        fireball_kill(a);
        return;
    }
    fireball_bind(a);
    a->front = v3_direction14(a->velocity, a->front);
}

static void firebubble_think(Actor* a){
    a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
    a->velocity = v3_sub(a->velocity, v3_scale14(a->up, V16(1.0f)));
    a->target.x += DEG(8);
    RayHit hit;
    if(a->timer > 300 ||
       (a->timer > 10 &&
        collision_ray(a->position,
                      v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8)),
                      &hit, 0))){
        actor_defeat(a, false);
        return;
    }
    if(actor_mario_contact(a, 0) != CONTACT_NONE){
        actor_hurt_mario(a, 1);
        actor_defeat(a, false);
    }
}

static void fireball_figure(const Actor* a, Figure* f){
    if(a->state == FIREBALL_ARC) f->local_pitch = a->target.x;
    else {
        v3 right = v3_direction14(v3_cross14(a->up, a->front), V3(N14_ONE, 0, 0));
        f->basis_up = v3_cross14(a->front, right);
        f->basis_front = a->front;
    }
}

static void fireball_pose(const Actor* a, Figure* f){
    const FigureHeader* model = a->figure.model;
    if(a->state == FIREBALL_ARC) f->pose = model_rest_pose(model);
    else if(model->clip_count) f->pose = figure_clip_pose(model, a->anim, a->anim_frame);
}

static bool fireball_hidden(const Actor* a){
    return a->state == FIREBALL_THROW &&
           v3_length_squared64(v3_sub(a->position, g_game.camera.position)) < (uint64_t)FX(200) * FX(200);
}

static fx fireball_pointer_radius(const Actor* a){ return a->state != FIREBALL_ARC ? FX(150) : a->radius; }

static bool fireball_pointer_preferred(const Actor* a){ return a->state != FIREBALL_ARC; }

static bool fireball_pointer_swipe(const Actor* a){ return a->state == FIREBALL_THROW && a->timer > 31; }

static void fireball_actor_think(Actor* a){
    if(a->state == FIREBALL_ARC) firebubble_think(a);
    else fireball_think(a);
}
