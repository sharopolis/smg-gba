enum {
    ICECHILD_WAIT = 140,
    ICECHILD_WALK,
    ICECHILD_CHASE_START,
    ICECHILD_CHASE_DASH,
    ICECHILD_CHASE_TURN,
    ICECHILD_DAMAGE,
    ICECHILD_SHRINK,
    ICECHILD_RUNAWAY,
    ICECHILD_START_DIVING,
    ICECHILD_DIVING,
    ICECHILD_SINK,
    ICECHILD_FLOAT,
    ICECHILD_IGNITION,
    ICECHILD_IGNITION_FORCE,
    ICECHILD_ATTACK_SUCCESS,
    ICECHILD_BIND_POINTER,
    ICECHILD_BIND_COLD_POINTER,
    ICECHILD_DOWN,
    ICECHILD_FLAT_DOWN,
    ICECHILD_READY_RESTART
};
static bool icechild_stompable(const Actor* a){
    return a->state == ICECHILD_RUNAWAY || a->state == ICECHILD_START_DIVING || a->state == ICECHILD_DIVING ||
           a->state == ICECHILD_BIND_COLD_POINTER;
}
static bool icechild_hittable(const Actor* a){
    return a->state == ICECHILD_WAIT || a->state == ICECHILD_WALK || a->state == ICECHILD_SHRINK ||
           a->state == ICECHILD_CHASE_START || a->state == ICECHILD_CHASE_DASH ||
           a->state == ICECHILD_CHASE_TURN || a->state == ICECHILD_IGNITION ||
           a->state == ICECHILD_IGNITION_FORCE || a->state == ICECHILD_ATTACK_SUCCESS ||
           a->state == ICECHILD_BIND_POINTER;
}
static bool icechild_can_chase(const Actor* a){
    return actor_mario_within(a, 900) && v3_length_squared64(v3_sub(a->position, a->home)) <= 3000u * 3000u;
}
static int icechild_ground_height(Actor* a){
    RayHit h;
    if(!collision_ray(a->position, v3_sub(a->position, v3_scale14(a->up, 1500)), &h, 0)) return -1;
    return v3_dot14(v3_sub(a->position, h.point), a->up);
}
static void icechild_hover(Actor* a, int height, int down, int up, int range){
    int shadow = icechild_ground_height(a);
    if(shadow < 0) return;
    int delta = fx_clamp(shadow - height, -range, range);
    int force = (delta >= 0 ? down : up) * delta / range;
    a->velocity = v3_sub(a->velocity, v3_scale14(a->up, force));
}
static void icechild_accel(Actor* a, v3 direction, int accel, int cosine){
    v3 dir = v3_reject14(direction, a->up);
    if(v3_length_squared64(dir) > 1){
        dir = v3_direction14(dir, a->front);
        if(cosine <= -N14_ONE) a->front = dir;
        else if(v3_dot14(a->front, dir) >= cosine) a->front = dir;
        else {
            fx limit = fx_atan2(isqrt32((uint32_t)N14_ONE * N14_ONE - (uint32_t)cosine * cosine), cosine);
            a->front = v3_tangent14(
                v3_rotate_axis14(a->front, a->up,
                                 v3_dot14(v3_cross14(a->front, dir), a->up) >= 0 ? limit : -limit),
                a->up, dir);
        }
    }
    a->velocity = v3_add(a->velocity, v3_scale14(a->front, accel));
}
static void icechild_rebound(Actor* a, int ground, int wall){
    if(a->flags & ACTOR_ON_GROUND){
        int v = v3_dot14(a->velocity, a->up);
        if(v < 0) a->velocity = v3_sub(a->velocity, v3_scale14(a->up, mul14(v, N14_ONE + ground)));
    }
    if(wall) actor_bounce_wall(a, 70, wall);
}
__attribute__((noinline)) static void icechild_bind(Actor* a){
    v3 delta;
    for(int i = 0; i < 3; i++){
        int f = (&a->velocity.x)[i] + a->args[i + 1];
        (&delta.x)[i] = f >> 8;
        a->args[i + 1] = f & 255;
    }
    v3 desired = v3_add(a->position, delta);
    CollisionCandidates candidates;
    collision_gather_around(a->position, desired, 46, &candidates);
    RayHit ray;
    a->flags &= ~ACTOR_ON_GROUND;
    if(collision_raycast(&candidates, a->position, desired, &ray, 0))
        desired = v3_add(ray.point, v3_scale14(ray.normal, 45));
    for(int i = 0; i < candidates.count; i++){
        SphereContact hit;
        if(!collectible_sphere_triangle(candidates.index[i], desired, 45, &hit) || hit.depth < 0) continue;
        desired = v3_add(desired, v3_scale14(hit.normal, hit.depth + 1));
        if(v3_dot14(hit.normal, a->up) > R14(.5f)) a->flags |= ACTOR_ON_GROUND;
    }
    a->position = iceking_limit_bind(a, a->position, desired, 45, true);
}
static void icechild_appear(Actor* a){
    a->position = a->home;
    a->velocity = a->target = V3(0, 0, 0);
    a->flags &= ~(ACTOR_HIDDEN | ACTOR_ON_GROUND);
    memset(a->args, 0, sizeof a->args);
    a->health = 1;
    a->radius = 55;
    actor_set_state(a, ICECHILD_WAIT, ANIM_ICEMERAMERA_WAIT);
}
static void icechild_kill(Actor* a, bool coin){
    if(coin) collectibles_pop(OBJ_COIN, a->position, a->up, 1);
    a->active = 0;
}
static void iceking_children_appear(void){
    for(int i = 0; i < 2; i++){
        Actor* p = iceking_part(g_iceking.child[i], OBJ_ICECHILD);
        if(p) icechild_appear(p);
    }
}
static void iceking_children_kill(void){
    for(int i = 0; i < 2; i++){
        Actor* p = iceking_part(g_iceking.child[i], OBJ_ICECHILD);
        if(p && p->state != ICEKING_PART_DEAD) icechild_kill(p, true);
        else if(p) p->active = 0;
    }
}
static void icechild_star_hit(Actor* a){
    if(icechild_hittable(a)) actor_set_state(a, ICECHILD_RUNAWAY, ANIM_ICEMERAMERA_DAMAGE);
}
static bool icechild_point_bound(const Actor* a){
    return a->state == ICECHILD_BIND_POINTER || a->state == ICECHILD_BIND_COLD_POINTER;
}
static bool icechild_try_point_bind(Actor* a){
    if(!pointer_held() || !pointer_is_pointing(a, 55)) return false;
    if(a->state == ICECHILD_RUNAWAY || a->state == ICECHILD_START_DIVING){
        actor_set_state(a, ICECHILD_BIND_COLD_POINTER, -1);
        a->args[0] = 0;
        return true;
    }
    if(a->state == ICECHILD_WAIT || a->state == ICECHILD_WALK || a->state == ICECHILD_CHASE_START ||
       a->state == ICECHILD_CHASE_DASH || a->state == ICECHILD_CHASE_TURN || a->state == ICECHILD_IGNITION ||
       a->state == ICECHILD_IGNITION_FORCE || a->state == ICECHILD_ATTACK_SUCCESS){
        actor_set_state(a, ICECHILD_BIND_POINTER, -1);
        a->args[0] = 0;
        return true;
    }
    return false;
}
static v3 icechild_attack_centre(const Actor* a){
    int frame = a->anim_frame >> 8, count = icechild_pose_count[a->anim];
    bool loop = a->anim == ANIM_ICEMERAMERA_WAIT || a->anim == ANIM_ICEMERAMERA_CHASEDASH ||
                a->anim == ANIM_ICEMERAMERA_CHASETURN;
    frame = loop ? frame % (count - 1) : fx_min(frame, count - 1);
    const int16_t* p = icechild_attack_tracks[a->anim][frame];
    v3 right = v3_cross14(a->front, a->up);
    return v3_add(a->position, v3_add(v3_scale14(right, -p[0]),
                                      v3_add(v3_scale14(a->up, p[1]), v3_scale14(a->front, p[2]))));
}
static void icechild_contacts(Actor* a){
    if(g_mario_held || g_game.player.action == MA_DEATH) return;
    v3 delta = v3_sub(mario_centre(), a->position);
    if(g_mario.attack_timer > 0 && icechild_hittable(a) && v3_length_squared64(delta) <= 300u * 300u){
        a->front = v3_direction14(v3_reject14(v3_neg(delta), a->up), a->front);
        a->velocity = v3_scale14(a->front, V16(35));
        actor_set_state(a, ICECHILD_DAMAGE, ANIM_ICEMERAMERA_DAMAGE);
        return;
    }
    Player* p = &g_game.player;
    bool body = actor_touches_mario(a, 0);
    if(body && g_mario_star_power){
        a->velocity = V3(0, 0, 0);
        actor_set_state(a, ICECHILD_DOWN, ANIM_ICEMERAMERA_DOWN);
        return;
    }
    if(body && icechild_stompable(a) &&
       (p->action == MA_HIPDROP || (g_mario.jumping && vertical_speed(g_mario.jump_vec, p->up) < 0))){
        actor_set_state(a, ICECHILD_FLAT_DOWN, ANIM_ICEMERAMERA_FLATDOWN);
        return;
    }
    bool attack = a->state == ICECHILD_WAIT || a->state == ICECHILD_WALK || a->state == ICECHILD_CHASE_DASH ||
                  a->state == ICECHILD_CHASE_TURN || a->state == ICECHILD_BIND_POINTER;
    v3 attack_delta = v3_sub(mario_centre(), icechild_attack_centre(a));
    if(attack && v3_length_squared64(attack_delta) <= (70u + C_BODY_RADIUS) * (70u + C_BODY_RADIUS) &&
       mario_freeze_hit()){
        actor_set_state(a, ICECHILD_ATTACK_SUCCESS, ANIM_ICEMERAMERA_ATTACK);
        return;
    }
    if(!body) return;

    int penetration = 55 + C_BODY_RADIUS - v3_length_exact(delta);
    if(penetration > 0){
        v3 move = v3_scale14(v3_tangent14(delta, p->up, p->front), penetration * 11 / 10);
        p->figure.position = v3_add(p->figure.position, move);
        g_mario.position16 = v3_add(g_mario.position16, V3(move.x * 256, move.y * 256, move.z * 256));
    }
}
__attribute__((noinline)) static void icechild_think(Actor* a){
    if(a->state == ICEKING_PART_DEAD){
        a->timer = 0;
        return;
    }
    if(a->value != g_iceking.owner || g_iceking.stage != g_game.stage_index){
        a->active = 0;
        return;
    }
    GravitySample gravity = gravity_sample(g_game.stage.header, a->position);
    if(gravity.index >= 0) a->up = gravity.up;
    if(!icechild_point_bound(a)) icechild_try_point_bind(a);
    int step = a->timer - 1, state = a->state;
    bool chase = icechild_can_chase(a);
    switch(state){
    case ICECHILD_BIND_POINTER:
    case ICECHILD_BIND_COLD_POINTER:
        a->anim_frame = fx_max(0, a->anim_frame - FX_ONE);
        a->velocity = V3(0, 0, 0);
        if(pointer_held() && pointer_is_pointing(a, 55)) a->args[0] = 0;
        else if(++a->args[0] > 5)
            actor_set_state(a, state == ICECHILD_BIND_COLD_POINTER ? ICECHILD_RUNAWAY : ICECHILD_WAIT, -1);
        icechild_bind(a);
        if(!g_iceking.demo) icechild_contacts(a);
        return;
    case ICECHILD_WAIT:
    case ICECHILD_WALK:
    case ICECHILD_SHRINK:
        if(!step && state != ICECHILD_WALK){
            a->anim = ANIM_ICEMERAMERA_WAIT;
            a->anim_frame = 0;
            a->health = 1;
        }
        icechild_hover(a, 130, V16(.5f), V16(.1f), 15);
        if(state == ICECHILD_WALK) icechild_accel(a, v3_sub(a->target, a->position), V16(.05f), R14(.995f));
        a->velocity = v3_scale14(a->velocity, R14(.98f));
        icechild_rebound(a, 0, 0);
        if(state == ICECHILD_SHRINK){
            if(step > 20)
                actor_set_state(a, chase ? ICECHILD_CHASE_START : ICECHILD_WAIT,
                                chase ? ANIM_ICEMERAMERA_CHASESTART : ANIM_ICEMERAMERA_WAIT);
        } else if(chase) actor_set_state(a, ICECHILD_CHASE_START, ANIM_ICEMERAMERA_CHASESTART);
        else if(state == ICECHILD_WAIT && step > 120){
            fx angle = (g_game.tick * 2654435761u >> 8) % FX_TAU;
            v3 random = v3_rotate_axis14(a->front, a->up, angle);
            a->target = v3_add(a->home, v3_scale14(random, 200));
            actor_set_state(a, ICECHILD_WALK, -1);
        } else if(state == ICECHILD_WALK &&
                  (step > 300 || v3_length_exact(v3_reject14(v3_sub(a->target, a->position), a->up)) <= 100))
            actor_set_state(a, ICECHILD_WAIT, ANIM_ICEMERAMERA_WAIT);
        break;
    case ICECHILD_CHASE_START:
    case ICECHILD_CHASE_DASH:
    case ICECHILD_CHASE_TURN:
        if(state == ICECHILD_CHASE_START){
            icechild_hover(a, 300, V16(.5f), V16(.2f), 15);
            icechild_accel(a, actor_to_mario(a), V16(.05f), R14(.99f));
            a->velocity = v3_scale14(a->velocity, R14(.97f));
        } else if(state == ICECHILD_CHASE_DASH){
            if(!step) a->target = actor_to_mario(a);
            icechild_hover(a, step < 30 ? 150 : 190, V16(.5f), step < 30 ? V16(.1f) : V16(.2f),
                           step < 30 ? 15 : 20);
            icechild_accel(a, a->target, V16(.175f), R14(.98f));
            RayHit wall;
            v3 dir = v3_tangent14(a->target, a->up, a->front);
            if(icechild_ground_height(a) < 250 &&
               collision_ray(a->position, v3_add(a->position, v3_scale14(dir, 200)), &wall, 0))
                a->velocity = v3_add(a->velocity, v3_scale14(a->up, V16(.9f)));
            a->velocity = v3_scale14(a->velocity, R14(.98f));
        } else {
            icechild_hover(a, 190, V16(.3f), V16(.3f), 20);
            icechild_accel(a, actor_to_mario(a), V16(.05f), R14(.99f));
            a->velocity = v3_scale14(a->velocity, R14(.97f));
        }
        icechild_rebound(a, 0, 0);
        if(state != ICECHILD_CHASE_DASH && (!chase || step > 240))
            actor_set_state(a, ICECHILD_WAIT, ANIM_ICEMERAMERA_WAIT);
        else if(state == ICECHILD_CHASE_START && step > 60)
            actor_set_state(a, ICECHILD_CHASE_DASH, ANIM_ICEMERAMERA_CHASEDASH);
        else if(state == ICECHILD_CHASE_DASH && step > 45)
            actor_set_state(a, ICECHILD_CHASE_TURN, ANIM_ICEMERAMERA_CHASETURN);
        else if(state == ICECHILD_CHASE_TURN && step > 50)
            actor_set_state(a, ICECHILD_CHASE_DASH, ANIM_ICEMERAMERA_CHASEDASH);
        break;
    case ICECHILD_DAMAGE:
        if(step >= 3) a->health = 2;
        icechild_hover(a, 140, 0, V16(1.2f), 15);
        a->velocity = v3_scale14(a->velocity, R14(.9f));
        icechild_rebound(a, 0, 0);
        if(step > 30) actor_set_state(a, ICECHILD_RUNAWAY, -1);
        break;
    case ICECHILD_RUNAWAY:
        if(!step){
            a->health = 3;
            a->args[0] = 0;
        }
        if(water_query(a->position, a->up, NULL)){
            a->target = a->position;
            a->velocity = V3(0, 0, 0);
            actor_set_state(a, ICECHILD_SINK, ANIM_ICEMERAMERA_WAIT);
            break;
        }
        if(step > 70 && (a->flags & ACTOR_ON_GROUND)){
            if(v3_length_exact(v3_reject14(a->velocity, a->up)) < V16(3)) a->args[0]--;
            if(a->args[0] <= 0){
                v3 away =
                    actor_mario_within(a, 1000) ? v3_neg(actor_to_mario(a)) : v3_sub(a->home, a->position);
                fx angle = DEG((int)(g_game.tick % 51) - 25);
                away = v3_rotate_axis14(v3_direction14(away, a->front), a->up, angle);
                icechild_accel(a, away, V16(20), -N14_ONE);
                a->velocity = v3_add(a->velocity, v3_scale14(a->up, V16(45) + (g_game.tick % 6) * 256));
                a->args[0] = 30 + (g_game.tick % 11);
            }
        }
        if(!(a->flags & ACTOR_ON_GROUND)) a->velocity = v3_sub(a->velocity, v3_scale14(a->up, V16(2)));
        a->velocity = v3_scale14(a->velocity, (a->flags & ACTOR_ON_GROUND) ? R14(.8f) : R14(.98f));
        icechild_rebound(a, R14(.5f), R14(.3f));

        if(!actor_mario_within(a, 1500) &&
           v3_length_squared64(v3_sub(a->home, g_game.player.figure.position)) > 1500u * 1500u &&
           v3_dot14(v3_sub(a->position, g_game.camera.position), g_game.camera.front) < -100 &&
           v3_dot14(v3_sub(a->home, g_game.camera.position), g_game.camera.front) < -100)
            icechild_appear(a);
        break;
    case ICECHILD_SINK:
        a->flags |= ACTOR_HIDDEN;
        a->health = 3;
        a->velocity = V3(0, 0, 0);
        if(step > 50) actor_set_state(a, ICECHILD_FLOAT, ANIM_ICEMERAMERA_FLOAT);
        return;
    case ICECHILD_FLOAT:
        if(!step){
            a->flags &= ~ACTOR_HIDDEN;
            a->health = 1;
            a->velocity = v3_scale14(a->up, V16(50));
        }
        icechild_hover(a, 100, V16(.9f), V16(.1f), 50);
        a->velocity = v3_scale14(a->velocity, R14(.94f));
        icechild_rebound(a, 0, 0);
        if(step > 30) actor_set_state(a, ICECHILD_IGNITION, -1);
        break;
    case ICECHILD_IGNITION:
        if(step == 15){
            a->anim = ANIM_ICEMERAMERA_FLOATIGNITION;
            a->anim_frame = 0;
            a->velocity = v3_add(a->velocity, v3_scale14(a->up, V16(25)));
        }
        icechild_hover(a, 100, V16(.5f), V16(.1f), 50);
        a->velocity = v3_scale14(a->velocity, R14(.98f));
        icechild_rebound(a, 0, 0);
        if(step > 20)
            actor_set_state(a, chase ? ICECHILD_CHASE_DASH : ICECHILD_WAIT,
                            chase ? ANIM_ICEMERAMERA_CHASEDASH : ANIM_ICEMERAMERA_WAIT);
        break;
    case ICECHILD_ATTACK_SUCCESS:
        if(!step) a->velocity = V3(0, 0, 0);
        icechild_hover(a, 50, V16(.5f), V16(.1f), 15);
        a->velocity = v3_scale14(a->velocity, R14(.98f));
        icechild_rebound(a, 0, 0);
        if(step > 180) actor_set_state(a, ICECHILD_WAIT, ANIM_ICEMERAMERA_WAIT);
        break;
    case ICECHILD_DOWN:
        if(!step){
            a->health = 3;
            a->velocity = v3_scale14(a->up, V16(25));
        }
        if(!(a->flags & ACTOR_ON_GROUND)) a->velocity = v3_sub(a->velocity, v3_scale14(a->up, V16(.5f)));
        a->velocity = v3_scale14(a->velocity, R14(.99f));
        icechild_rebound(a, R14(.9f), R14(.9f));
        if(step > 0){
            icechild_kill(a, true);
            return;
        }
        break;
    case ICECHILD_FLAT_DOWN:
        if(!step){
            a->health = 3;
            a->velocity = V3(0, 0, 0);
        }
        if(step > 0){
            icechild_kill(a, true);
            return;
        }
        break;
    default: return;
    }
    icechild_bind(a);
    if(!g_iceking.demo) icechild_contacts(a);
}
