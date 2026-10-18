#ifndef METBO_H

#define METBO_H

#endif

enum {
    METBO_CUE_FIND,
    METBO_CUE_STOMPED,
    METBO_CUE_BLOW,
    METBO_CUE_CRASH,
    METBO_CUE_RECOVER,
    METBO_CUE_TRAMPLE,
    METBO_CUE_EXPLODE,
    METBO_CUE_SPIN,
    METBO_CUE_BURST
};
static inline bool metbo_is_down(int s){ return s == METBO_PRESS || s == METBO_BLOW; }
static inline bool metbo_is_swooning(int s){ return s >= METBO_SWOONSTART && s <= METBO_SWOONWAIT; }
static inline bool metbo_is_roaming(int s){ return s >= METBO_WAIT && s <= METBO_CHASE; }
static const uint8_t metbo_cue_sfx[] = {SFX_METBO_FIND,    SFX_METBO_STOMPED, SFX_METBO_BLOW,
                                        SFX_METBO_CRASH,   SFX_METBO_RECOVER, SFX_METBO_TRAMPLE,
                                        SFX_METBO_EXPLODE, SFX_METBO_SPIN,    SFX_METBO_BURST};
static void metbo_play_cue(int event){ audio_play_effect(metbo_cue_sfx[event], 3); }
static void metbo_set(Actor* a, int state, int animation){
    if(a->state == METBO_CHASE && state != METBO_CHASE) a->home = a->position;
    actor_set_state(a, state, animation);
}
static bool metbo_anim_stopped(const Actor* a){
    return (a->anim_frame >> 8) >= anim_length_of(a->figure.model, a->anim) - 1;
}
static v3 metbo_ground_normal(const Actor* a){ return V3(a->args[0], a->args[1], a->args[2]); }
static void metbo_drop_star_bits(Actor* a){
    if(a->health > 0){
        collectibles_pop(OBJ_STARBIT, a->position, a->up, a->health);
        a->health = 0;
        metbo_play_cue(METBO_CUE_BURST);
    }
}
static void metbo_kill(Actor* a){
    metbo_play_cue(METBO_CUE_EXPLODE);
    actor_set_switch_dead(a, 0);
    a->active = 0;
}

static __attribute__((noinline)) bool metbo_sphere_triangle(int tri, v3 centre, SphereContact* out){
    const fx radius = 100;
    const CollisionTriangle* t = col_tri(tri);
    v3 n = v3_from_i16(t->normal), a = col_vertex(t, 0, centre);
    fx dist = -v3_dot14(a, n);
    if(dist > radius || dist < 0) return false;
    v3 b = col_vertex(t, 1, centre), c = col_vertex(t, 2, centre);

    if(fx_min(a.x, fx_min(b.x, c.x)) > radius || fx_max(a.x, fx_max(b.x, c.x)) < -radius ||
       fx_min(a.y, fx_min(b.y, c.y)) > radius || fx_max(a.y, fx_max(b.y, c.y)) < -radius ||
       fx_min(a.z, fx_min(b.z, c.z)) > radius || fx_max(a.z, fx_max(b.z, c.z)) < -radius)
        return false;
    v3 q = closest_point_to_origin(a, b, c, v3_scale14(n, -dist));
    uint64_t d2 = v3_length_squared64(q);
    if(d2 > (uint64_t)radius * radius) return false;
    fx d = (fx)isqrt64(d2);
    out->normal = d > 2 && dist > 0 ? v3_direction14(v3_neg(q), n) : n;
    out->depth = radius - d;
    out->point = v3_add(centre, q);
    out->attr = t->attr;
    out->tri = (int16_t)tri;
    return true;
}
static void metbo_bind(Actor* a){
    v3 old = a->position;
    a->position = v3_add(old, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
    a->flags &= ~ACTOR_ON_GROUND;
    a->value = 0;
    CollisionCandidates cand;
    collision_gather_around(v3_add(old, v3_scale14(a->up, 100)), v3_add(a->position, v3_scale14(a->up, 100)),
                            101, &cand);
    for(int pass = 0; pass < 2; pass++){
        bool touched = false;
        for(int j = 0; j < cand.count; j++){
            SphereContact hit;
            if(!metbo_sphere_triangle(cand.index[j], v3_add(a->position, v3_scale14(a->up, 100)), &hit))
                continue;
            a->value |= 1;
            touched = true;
            if(hit.attr & COL_DEATH){
                metbo_kill(a);
                return;
            }
            a->position = v3_add(a->position, v3_scale14(hit.normal, hit.depth));
            int ny = v3_dot14(hit.normal, a->up), speed = v3_dot14(a->velocity, hit.normal);
            if(ny > R14(.5)){
                a->flags |= ACTOR_ON_GROUND;
                a->args[0] = hit.normal.x;
                a->args[1] = hit.normal.y;
                a->args[2] = hit.normal.z;
            }
            if(speed < 0) a->velocity = v3_sub(a->velocity, v3_scale14(hit.normal, speed));
        }
        if(!touched) break;
    }
}
static bool metbo_floor_ahead(const Actor* a, v3 direction){
    v3 p = v3_add(a->position, v3_scale14(direction, 200));
    RayHit hit;
    return collision_ray(v3_add(p, v3_scale14(a->up, 100)), v3_sub(p, v3_scale14(a->up, 200)), &hit, 0);
}
static bool metbo_try_search(Actor* a){
    if(!actor_mario_within(a, 800)) return false;
    RayHit hit;
    if(collision_ray(a->position, mario_centre(), &hit, 0)) return false;
    v3 direction = v3_direction14(actor_to_mario(a), a->front);
    if(!metbo_floor_ahead(a, direction)) return false;
    metbo_set(a, METBO_SEARCH, ANIM_BUOYMETBO_SEARCH);
    return true;
}
static void metbo_move(Actor* a, int accel, int gravity, int damp, int turn, v3 target){
    bool ground = (a->flags & ACTOR_ON_GROUND) != 0;
    if(!ground){
        accel = 0;
        gravity = V16(3);
        damp = R14(.99);
        turn = 0;
    }
    if(turn) actor_turn_toward(a, v3_sub(target, a->position), DEG(turn));
    v3 normal = ground ? metbo_ground_normal(a) : a->up;
    a->front = v3_tangent14(a->front, normal, a->front);
    v3 acceleration = v3_scale14(v3_tangent14(a->front, a->up, a->front), accel);
    if(ground) acceleration = v3_reject14(acceleration, normal);

    a->velocity = v3_scale14(v3_add(a->velocity, acceleration), damp);
    if(ground){
        int speed = v3_dot14(a->velocity, normal);
        if(speed < 0) a->velocity = v3_sub(a->velocity, v3_scale14(normal, speed));
    }
    a->velocity = v3_sub(a->velocity, v3_scale14(normal, gravity));
    metbo_bind(a);
}
enum {
    METBO_REPLY_ACCEPTED = 1,
    METBO_REPLY_TRAMPLE_SOUND = 2,
    METBO_REPLY_COUNTER_HIPDROP = 4,
    METBO_REPLY_HITSTOP3 = 8,
    METBO_REPLY_START_SWOON = 16,
    METBO_REPLY_START_BLOW = 32,
    METBO_REPLY_HITSTOP4 = 64,
    METBO_REPLY_KILL = 128
};
typedef struct {
    unsigned bits;
    int state;
} MetboReply;
static inline MetboReply metbo_reply_to(int s, int message){
    MetboReply r = {0, s};
    if(message == METBO_MSG_INHALE){
        r.bits = METBO_REPLY_ACCEPTED | METBO_REPLY_KILL;
        return r;
    }
    if(message == METBO_MSG_LOCKON){
        r.bits = METBO_REPLY_ACCEPTED;
        return r;
    }
    if(message == METBO_MSG_REFLECT){
        r.bits = METBO_REPLY_ACCEPTED | METBO_REPLY_TRAMPLE_SOUND;
        if(!metbo_is_swooning(s) && s != METBO_RECOVER) r.state = METBO_GUARD;
        return r;
    }
    if(metbo_is_down(s)) return r;
    if(message == METBO_MSG_HIPDROP || message == METBO_MSG_TRAMPLE){
        r.bits = METBO_REPLY_TRAMPLE_SOUND;
        if(s == METBO_RECOVER){
            if(message == METBO_MSG_TRAMPLE) r.bits |= METBO_REPLY_ACCEPTED;
            return r;
        }
        if(s == METBO_SWOONSTART) return r;
        r.bits |= METBO_REPLY_ACCEPTED;
        r.state = metbo_is_swooning(s) ? METBO_PRESS : METBO_GUARD;
        if(message == METBO_MSG_HIPDROP && !metbo_is_swooning(s)) r.bits |= METBO_REPLY_COUNTER_HIPDROP;
    } else if(message == METBO_MSG_SPIN){
        if(s == METBO_SWOONSTART) return r;
        r.bits = METBO_REPLY_ACCEPTED | METBO_REPLY_HITSTOP3 | METBO_REPLY_START_SWOON;
        r.state = METBO_SWOONSTART;
    } else if(message == METBO_MSG_INVINCIBLE || message == METBO_MSG_EXPLOSION){
        r.bits = METBO_REPLY_ACCEPTED | METBO_REPLY_START_BLOW |
                 (message == METBO_MSG_INVINCIBLE ? METBO_REPLY_HITSTOP4 : 0);
        r.state = METBO_BLOW;
    }
    return r;
}
static bool metbo_message(Actor* a, int message, v3 sender){
    MetboReply reply = metbo_reply_to(a->state, message);
    if(reply.bits & METBO_REPLY_TRAMPLE_SOUND) metbo_play_cue(METBO_CUE_TRAMPLE);
    if(reply.bits & METBO_REPLY_KILL){
        metbo_kill(a);
        return true;
    }
    if(reply.bits & METBO_REPLY_START_SWOON){
        v3 away =
            v3_set_length(v3_sub(a->position, game_player()->figure.position), R14(.4), v3_neg(a->front));
        a->velocity = v3_scale14(v3_direction14(v3_add(away, a->up), a->up), V16(45));
        a->front = v3_tangent14(actor_to_mario(a), a->up, a->front);
        a->flags &= ~ACTOR_ON_GROUND;
        metbo_play_cue(METBO_CUE_SPIN);
        metbo_set(a, METBO_SWOONSTART, ANIM_BUOYMETBO_SWOONSTART);
    } else if(reply.bits & METBO_REPLY_START_BLOW){
        v3 away = v3_tangent14(v3_sub(a->position, sender), a->up, v3_neg(a->front));
        a->velocity = v3_add(v3_scale14(away, V16(40)), v3_scale14(a->up, V16(20)));
        a->health = 3;
        a->flags &= ~ACTOR_ON_GROUND;
        metbo_set(a, METBO_BLOW, ANIM_BUOYMETBO_BLOW);
    } else if(reply.state != a->state)
        metbo_set(a, reply.state, reply.state == METBO_PRESS ? ANIM_BUOYMETBO_PRESS : ANIM_BUOYMETBO_GUARD);
    if(reply.bits & METBO_REPLY_COUNTER_HIPDROP) actor_hurt_mario(a, 1);

    return (reply.bits & METBO_REPLY_ACCEPTED) != 0;
}
static void metbo_init(Actor* a, const StageObject* o){
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    a->radius = 100;
    a->target = V3(0, 0, 0);
    a->velocity = V3(0, 0, 0);
    a->health = 3;
    a->value = 0;
    a->args[0] = a->up.x;
    a->args[1] = a->up.y;
    a->args[2] = a->up.z;
    metbo_set(a, METBO_WAIT, ANIM_BUOYMETBO_WAIT);
}
__attribute__((noinline)) static bool metbo_unclipped(const Actor* a){
    return a->type == OBJ_BUOYMETBO && a->state == METBO_BLOW;
}
static void metbo_think(Actor* a){
    int step = a->timer - 1;
    WaterSample water;
    if(water_query(a->position, a->up, &water) && water.depth > 0){
        metbo_kill(a);
        return;
    }
    if(a->state != METBO_NONACTIVE){
        a->up = gravity_sample(g_game.stage.header, a->position).up;
        a->front = v3_tangent14(a->front, a->up, a->front);
        if(a->state != METBO_SWOONSTART && !metbo_is_down(a->state) &&
           v3_length_squared64(a->velocity) > (uint64_t)V16(30) * V16(30))
            a->velocity = v3_set_length(a->velocity, V16(30), a->front);
    }
    v3 target = game_player()->figure.position;
    switch(a->state){
    case METBO_NONACTIVE:
        if(!step) a->velocity = V3(0, 0, 0);
        if(actor_mario_within(a, 3000)) metbo_set(a, METBO_WAIT, ANIM_BUOYMETBO_WAIT);
        return;
    case METBO_WAIT:
        metbo_move(a, 0, V16(2), R14(.5), 0, target);
        if(!a->active) return;
        if((!actor_has_switch(a, SWITCH_A) || actor_switch_on(a, SWITCH_A)) && !metbo_try_search(a)){
            if(!actor_mario_within(a, 3000) && (a->flags & ACTOR_ON_GROUND))
                metbo_set(a, METBO_NONACTIVE, -1);
            else if(step == 150) metbo_set(a, METBO_WALK, ANIM_BUOYMETBO_WALK);
        }
        break;
    case METBO_WALK:
        if(v3_length_squared64(v3_sub(a->position, a->home)) > (uint64_t)300 * 300 ||
           !metbo_floor_ahead(a, a->front))
            a->target = v3_sub(a->home, a->position);
        else if(!(step % 10))
            a->target = v3_rotate_axis14(a->target, a->up, (g_game.tick & 1) ? -DEG(20) : DEG(20));
        metbo_move(a, V16(.25), V16(2), R14(.92), 2, v3_add(a->position, a->target));
        if(!metbo_try_search(a) && step == 200)
            metbo_set(a, (g_game.tick & 1) ? METBO_WAIT : METBO_WALK,
                      (g_game.tick & 1) ? ANIM_BUOYMETBO_WAIT : ANIM_BUOYMETBO_WALK);
        break;
    case METBO_SEARCH:
        if(!step) metbo_play_cue(METBO_CUE_FIND);
        metbo_move(a, 0, V16(2), R14(.5), 5, target);
        if(metbo_anim_stopped(a)) metbo_set(a, METBO_CHASESTART, ANIM_BUOYMETBO_RUNSTART);
        break;
    case METBO_CHASESTART:
        metbo_move(a, 0, V16(2), R14(.5), 0, target);
        if(metbo_anim_stopped(a)) metbo_set(a, METBO_CHASE, ANIM_BUOYMETBO_RUN);
        break;
    case METBO_CHASE:
        metbo_move(a, V16(.8), V16(2), R14(.9), 2, target);
        if(!metbo_floor_ahead(a, a->front) ||
           (step >= 120 && (!actor_mario_within(a, 1200) || step == 220 || (g_game.tick % 50) == 0)))
            metbo_set(a, METBO_REST, ANIM_BUOYMETBO_WAIT);
        break;
    case METBO_GUARD:
    case METBO_PRESS:
    case METBO_ATTACK:
    case METBO_REST:
    case METBO_RECOVER:
    case METBO_SWOONWAIT:
        if(!step){
            if(a->state == METBO_PRESS) metbo_play_cue(METBO_CUE_STOMPED);
            if(a->state == METBO_RECOVER) metbo_play_cue(METBO_CUE_RECOVER);
        }
        metbo_move(a, 0, V16(2), R14(.5), 0, target);
        if(a->state == METBO_REST){
            if(step == 60) metbo_set(a, METBO_WAIT, ANIM_BUOYMETBO_WAIT);
        } else if(a->state == METBO_SWOONWAIT){
            if(step == 150) metbo_set(a, METBO_RECOVER, ANIM_BUOYMETBO_RECOVER);
        } else if(metbo_anim_stopped(a)){
            if(a->state == METBO_PRESS){
                collectibles_pop(OBJ_COIN, a->position, a->up, 1);
                metbo_kill(a);
                return;
            }
            metbo_set(a, a->state == METBO_ATTACK ? METBO_REST : METBO_WAIT, ANIM_BUOYMETBO_WAIT);
        }
        break;
    case METBO_SWOONSTART:
        if(!step) metbo_play_cue(METBO_CUE_CRASH);
        else {
            metbo_move(a, 0, V16(3), R14(.99), 0, target);
            if(a->flags & ACTOR_ON_GROUND) metbo_set(a, METBO_SWOONLAND, ANIM_BUOYMETBO_SWOONLAND);
        }
        break;
    case METBO_SWOONLAND:
    case METBO_SWOONSPIN:
        metbo_move(a, 0, V16(2), R14(.98), 0, target);
        if(metbo_anim_stopped(a))
            metbo_set(a, a->state == METBO_SWOONLAND ? METBO_SWOONSPIN : METBO_SWOONWAIT,
                      a->state == METBO_SWOONLAND ? ANIM_BUOYMETBO_SWOONSPIN : ANIM_BUOYMETBO_SWOONWAIT);
        break;
    case METBO_BLOW:
        if(!step) metbo_play_cue(METBO_CUE_BLOW);
        a->velocity = v3_sub(a->velocity, v3_scale14(a->up, V16(.2)));
        metbo_bind(a);
        if(step == 20) metbo_drop_star_bits(a);
        if((a->value && step > 1) || step > 20){
            if(step < 20) metbo_drop_star_bits(a);
            metbo_kill(a);
            return;
        }
        break;
    default: break;
    }
    if(!a->active || metbo_is_down(a->state) || a->state == METBO_NONACTIVE) return;
    int contact = actor_mario_contact(a, 60);
    if(contact == CONTACT_HIPDROP) metbo_message(a, METBO_MSG_HIPDROP, mario_centre());
    else if(contact == CONTACT_TRAMPLE){
        if(metbo_message(a, METBO_MSG_TRAMPLE, mario_centre())) mario_stomp_bounce(g_game.keys.held & KEY_A);
    } else if(contact == CONTACT_SPIN) metbo_message(a, METBO_MSG_SPIN, mario_centre());
    else if(contact == CONTACT_STAR) metbo_message(a, METBO_MSG_INVINCIBLE, mario_centre());
    else if(contact == CONTACT_TOUCH){
        if(metbo_is_roaming(a->state) &&
           mario_sphere_contact(a->position, a->up, 80, false) == CONTACT_TOUCH){
            actor_hurt_mario(a, 1);
            metbo_set(a, METBO_ATTACK, ANIM_BUOYMETBO_ATTACK);
        } else actor_block_mario(a, 100, 0);
    }
}

static void metbo_star_hit(Actor* a){ (void)metbo_message(a, METBO_MSG_REFLECT, mario_centre()); }

static void metbo_animate(Actor* a){
    if(a->state != METBO_NONACTIVE) a->anim_frame += FX_ONE;
}
