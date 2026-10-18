static const uint8_t fluff_spin_sfx[3][2] = {
    {SFX_FLUFFSPIN1AIR, SFX_FLUFFSPIN1WIND},
    {SFX_FLUFFSPIN2AIR, SFX_FLUFFSPIN2WIND},
    {SFX_FLUFFSPIN3AIR, SFX_FLUFFSPIN3WIND},
};
static const uint16_t fluff_wind_objects[] = {13, 28, 35};
enum { FL_GROUND = 64, FL_AIR, FL_INVALID, FL_BLOOM, FL_START, FL_FLY, FL_WIND, FL_SPIN, FL_SPIN_WIND };
static EWRAM_BSS int16_t g_fluff_rider;
static EWRAM_BSS uint16_t g_fluff_cooldown;

static void fluff_reset(void){
    g_fluff_rider = -1;
    g_fluff_cooldown = 0;
}
static bool fluff_has_rider(void){ return g_fluff_rider >= 0; }

static void fluff_leaf_state(const Actor* a, int anim){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* leaf = &g_game.actors[i];
        if(leaf->active && leaf->type == OBJ_FLUFFROOT &&
           v3_length_squared64(v3_sub(leaf->home, a->home)) < 16){
            leaf->anim = anim;
            leaf->anim_frame = 0;
        }
    }
}

static uint64_t fluff_part_bound(const StageRailPart* part, v3 p){
    int32_t point[3] = {p.x, p.y, p.z};
    uint64_t d2 = 0;
    for(int axis = 0; axis < 3; axis++){
        int32_t lo = part->p[0][axis], hi = lo;
        int64_t error = 0;
        for(int k = 1; k < 4; k++){
            int32_t v = part->p[k][axis];
            lo = fx_min(lo, v);
            hi = fx_max(hi, v);
            error += (int64_t)fx_abs(v - part->p[0][axis]) * (k < 3 ? 6 : 2);
        }
        int32_t margin = (int32_t)((error + N14_ONE - 1) >> 14) + 3;
        int64_t d = point[axis] < lo - margin   ? (int64_t)lo - margin - point[axis]
                    : point[axis] > hi + margin ? (int64_t)point[axis] - hi - margin
                                                : 0;
        d2 += (uint64_t)(d * d);
    }
    return d2;
}

static uint64_t fluff_part_nearest(const StageRailPart* part, v3 p, int* nearest_t){
    int winner = 0;
    uint64_t local = UINT64_MAX;
    for(int k = 0; k <= 16; k++){
        uint64_t d = v3_length_squared64(v3_sub(rail_part_eval(part, k * 1024), p));
        if(d < local){
            local = d;
            winner = k;
        }
    }
    int lo = fx_max(0, (winner - 1) * 1024), hi = fx_min(N14_ONE, (winner + 1) * 1024);
    for(int k = 0; k < 4; k++){
        int t0 = lo + (hi - lo) / 3, t1 = hi - (hi - lo) / 3;
        uint64_t d0 = v3_length_squared64(v3_sub(rail_part_eval(part, t0), p));
        uint64_t d1 = v3_length_squared64(v3_sub(rail_part_eval(part, t1), p));
        if(d0 < d1) hi = t1;
        else lo = t0;
    }
    *nearest_t = (lo + hi) / 2;
    return v3_length_squared64(v3_sub(rail_part_eval(part, *nearest_t), p));
}

static bool fluff_wind(v3 p, v3* direction, fx* distance){
    uint64_t lower = UINT64_MAX;
    unsigned first = UINT32_MAX;
    const StageRailPart* first_part = NULL;
    const StageObject* first_wind = NULL;
    int bit = 1 << (g_game.scenario - 1);
    for(unsigned n = 0; n < sizeof fluff_wind_objects / sizeof fluff_wind_objects[0]; n++){
        const StageObject* wind = &g_game.stage.objects[fluff_wind_objects[n]];
        if(!(wind->scenarios & bit) || wind->rail < 0 || switch_on(stage_object_switch(wind, SWITCH_B)))
            continue;
        const StageRail* rail = rail_get(wind->rail);
        const StageRailPart* parts = &g_game.stage.rail_parts[rail->first_part];
        for(int j = 0; j < rail->part_count; j++){
            uint64_t bound = fluff_part_bound(&parts[j], p);
            if(bound < lower){
                lower = bound;
                first = (n << 16) | j;
                first_part = &parts[j];
                first_wind = wind;
            }
        }
    }
    if(!first_part){
        *direction = V3(0, 0, 0);
        *distance = -1;
        return false;
    }
    int t;
    uint64_t best = fluff_part_nearest(first_part, p, &t);
    unsigned order = first;
    const StageRailPart* found_part = first_part;
    const StageObject* found_wind = first_wind;
    int found_t = t;
    for(unsigned n = 0; n < sizeof fluff_wind_objects / sizeof fluff_wind_objects[0]; n++){
        const StageObject* wind = &g_game.stage.objects[fluff_wind_objects[n]];
        if(!(wind->scenarios & bit) || wind->rail < 0 || switch_on(stage_object_switch(wind, SWITCH_B)))
            continue;
        const StageRail* rail = rail_get(wind->rail);
        const StageRailPart* parts = &g_game.stage.rail_parts[rail->first_part];
        for(int j = 0; j < rail->part_count; j++){
            unsigned at = (n << 16) | j;
            const StageRailPart* part = &parts[j];
            if(at == first || fluff_part_bound(part, p) > best) continue;
            uint64_t d = fluff_part_nearest(part, p, &t);
            if(d < best || (d == best && at < order)){
                best = d;
                order = at;
                found_part = part;
                found_wind = wind;
                found_t = t;
            }
        }
    }
    *direction = v3_direction14(v3_sub(rail_part_eval(found_part, fx_min(found_t + 64, N14_ONE)),
                                       rail_part_eval(found_part, fx_max(found_t - 64, 0))),
                                V3(found_wind->front[0], found_wind->front[1], found_wind->front[2]));
    *distance = (fx)isqrt64(best);
    return true;
}

static v3 fluff_hang_local(const Actor* a){
    const int16_t (*pose)[3] = fluff_hang_waitair;
    int count = sizeof fluff_hang_waitair / sizeof pose[0], tpk = 4;
    switch(a->anim){
    case ANIM_FLUFF_FLY:
        pose = fluff_hang_fly;
        count = sizeof fluff_hang_fly / sizeof pose[0];
        break;
    case ANIM_FLUFF_FLYWIND:
        pose = fluff_hang_flywind;
        count = sizeof fluff_hang_flywind / sizeof pose[0];
        break;
    case ANIM_FLUFF_SPIN1ST:
        pose = fluff_hang_spin1st;
        count = sizeof fluff_hang_spin1st / sizeof pose[0];
        tpk = 3;
        break;
    case ANIM_FLUFF_SPIN2ND:
        pose = fluff_hang_spin2nd;
        count = sizeof fluff_hang_spin2nd / sizeof pose[0];
        tpk = 3;
        break;
    case ANIM_FLUFF_SPIN3RD:
        pose = fluff_hang_spin3rd;
        count = sizeof fluff_hang_spin3rd / sizeof pose[0];
        tpk = 3;
        break;
    }
    int frame = a->anim_frame >> 8, key = frame / tpk;
    bool once = a->anim >= ANIM_FLUFF_SPIN1ST;
    if(once) key = fx_min(key, count - 1);
    else key %= count;

    return V3(-pose[key][0] / 4, pose[key][2] / 4, pose[key][1] / 4);
}

static void fluff_place_mario(const Actor* a){
    Player* p = &g_game.player;
    v3 local = fluff_hang_local(a), up = a->target;
    v3 front = v3_tangent14(a->front, up, a->front), side = v3_cross14(front, up);
    v3 position = v3_add(a->position, v3_add(v3_scale14(side, local.x),
                                             v3_add(v3_scale14(front, local.y), v3_scale14(up, local.z))));
    p->up = up;
    p->on_ground = false;
    p->on_wall = false;
    mario_hold_move(position, a->front);
}

static void fluff_release(Actor* a, bool jump){
    v3 velocity = a->velocity;
    if(jump) velocity = v3_add(v3_reject14(velocity, a->up), v3_scale14(a->up, V16(18)));
    if(g_fluff_rider == actor_index(a)){
        g_fluff_rider = -1;
        mario_hold_release(velocity);
        g_fluff_cooldown = 30;
    }
    if(actor_has_switch(a, SWITCH_A)) switch_set(actor_switch_key(a, SWITCH_A), false);
    a->position = a->home;
    a->velocity = V3(0, 0, 0);
    a->flags |= ACTOR_HIDDEN;
    audio_play_effect(SFX_FLUFFBREAK, 3);
    actor_set_state(a, FL_INVALID, ANIM_FLUFF_WAITAIR);
}

static void fluff_begin(Actor* a){
    Player* p = &g_game.player;
    v3 velocity = g_mario.jump_vec;
    g_fluff_rider = (int16_t)actor_index(a);
    mario_hold_begin();
    a->velocity = v3_reject14(velocity, a->up);
    fx up = v3_dot14(velocity, a->up);
    if(up > 0) a->velocity = v3_add(a->velocity, v3_scale14(a->up, up));
    a->front = v3_tangent14(p->front, a->up, a->front);
    actor_set_state(a, FL_START, ANIM_FLUFF_FLY);
    mario_restart_anim(ANIM_MARIO_FLUFFSTART);
    audio_play_effect(SFX_FLUFFCATCH, 4);
    if(actor_has_switch(a, SWITCH_A)) switch_set(actor_switch_key(a, SWITCH_A), true);
    fluff_place_mario(a);
}

static bool fluff_init(Actor* a, const StageObject* o){
    if(a->type == OBJ_FLUFF){
        actor_set_state(a, FL_GROUND, ANIM_FLUFF_WAITAIR);
        a->flags |= ACTOR_HIDDEN;
        a->health = 3;
        a->value = -1;
        a->target = a->up;
        return true;
    }
    if(a->type == OBJ_FLUFFROOT){
        actor_set_state(a, 0, ANIM_FLUFFLEAF_WAIT);
        return true;
    }
    if(a->type == OBJ_FLUFFWIND){
        a->flags |= ACTOR_HIDDEN;
        return true;
    }
    return false;
}

static bool fluff_floor_hit(v3 old, v3 next, v3 up){
    RayHit hit;
    v3 centre = v3_sub(next, v3_scale14(up, FX(80)));
    v3 bottom = v3_sub(centre, v3_scale14(up, FX(50)));
    return collision_ray(v3_sub(old, v3_scale14(up, FX(80))), bottom, &hit, 0) &&
           v3_dot14(hit.normal, up) > R14(.5f);
}

static void fluff_think(Actor* a){
    Player* p = &g_game.player;
    if(a->state == FL_INVALID){
        if(a->timer >= 120){
            a->position = a->home;
            a->velocity = V3(0, 0, 0);
            a->health = 3;
            actor_set_state(a, FL_BLOOM, ANIM_FLUFF_WAITAIR);
            fluff_leaf_state(a, ANIM_FLUFFLEAF_BLOOM);
        }
        return;
    }
    if(a->state == FL_GROUND || a->state == FL_BLOOM){
        if((a->state == FL_GROUND || a->timer > 15) && g_mario.attack_timer &&
           actor_touches_mario(a, FX(60))){
            a->flags &= ~ACTOR_HIDDEN;
            a->velocity = v3_scale14(a->up, V16(10));
            actor_set_state(a, FL_AIR, ANIM_FLUFF_WAITAIR);
            fluff_leaf_state(a, ANIM_FLUFFLEAF_SPIN);
        } else if(a->state == FL_BLOOM && a->timer >= 161){
            actor_set_state(a, FL_GROUND, ANIM_FLUFF_WAITAIR);
            fluff_leaf_state(a, ANIM_FLUFFLEAF_WAIT);
        }
        return;
    }
    if(a->state == FL_AIR){
        if(!g_mario_held && !g_fluff_cooldown && !p->on_ground && !bee_active() && p->action != MA_DEATH &&
           actor_touches_mario(a, 0)){
            fluff_begin(a);
            return;
        }
        if(g_mario.attack_timer && a->timer > 60 && actor_touches_mario(a, FX(60))){
            a->velocity = v3_scale14(a->up, V16(5));
            a->timer = 0;
        }
        a->velocity = v3_sub(a->velocity, v3_scale14(a->up, V16(.15f)));
        if(v3_dot14(a->velocity, a->up) < -V16(.8f))
            a->velocity = v3_scale14(v3_direction14(a->velocity, v3_neg(a->up)), V16(.8f));
        v3 old = a->position;
        a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
        if(v3_dot14(a->velocity, a->up) < 0 && fluff_floor_hit(old, a->position, a->up))
            fluff_release(a, false);
        return;
    }
    if(g_fluff_rider != actor_index(a) || !g_mario_held){
        fluff_release(a, false);
        return;
    }
    if(g_game.keys.pressed & KEY_A){
        fluff_release(a, true);
        return;
    }
    GravitySample grav = gravity_sample(g_game.stage.header, a->position);
    if(grav.index >= 0)
        a->up = v3_direction14(v3_add(v3_scale14(a->up, R14(.99f)), v3_scale14(grav.up, R14(.01f))), grav.up);
    a->front = v3_tangent14(a->front, a->up, p->front);
    if(a->state == FL_START && v3_dot14(a->velocity, a->up) < V16(15))
        a->velocity = v3_add(a->velocity, v3_scale14(a->up, V16(.2f)));
    a->velocity = v3_scale14(a->velocity, R14(.98f));
    mario_read_stick(&g_game.keys);
    v3 wind;
    fx distance;
    fluff_wind(a->position, &wind, &distance);
    if(g_mario.stick){
        v3 steer = v3_reject14(g_mario.move_dir, a->up);
        if(distance >= 0) steer = v3_reject14(steer, wind);
        if(v3_length_squared64(steer) > 64){
            v3 facing = v3_direction14(steer, a->front);
            a->front = v3_tangent14(v3_add(v3_scale14(a->front, R14(.9f)), v3_scale14(facing, R14(.1f))),
                                    a->up, a->front);
            a->velocity = v3_add(a->velocity, v3_scale14(steer, V16(.2f)));
        }
    }
    if(distance > 0){
        int factor = fx_clamp((distance - FX(200)) * N14_ONE / FX(1300), 0, N14_ONE);

        int ease = N14_ONE - (fx_cos((FX_PI / 2) * (N14_ONE - factor) / N14_ONE) << 6);
        a->velocity = v3_add(a->velocity, v3_scale14(wind, V16(.08f) + mul14(V16(.22f), ease)));
        fx along = v3_dot14(a->velocity, wind), vertical = v3_dot14(v3_reject14(a->velocity, wind), a->up);
        v3 side = v3_reject14(v3_reject14(a->velocity, wind), a->up);
        fx speed = v3_length_exact(side);
        if(speed > V16(6)) side = v3_scale14(v3_direction14(side, a->front), V16(6));
        a->velocity = v3_add(side, v3_add(v3_scale14(wind, along), v3_scale14(a->up, vertical)));
        if(a->health > 0)
            a->front = v3_tangent14(v3_add(v3_scale14(a->front, R14(.92f)), v3_scale14(wind, R14(.08f))),
                                    a->up, a->front);
        a->target =
            v3_direction14(v3_add(v3_scale14(a->up, N14_ONE - ease / 2), v3_scale14(wind, ease / 2)), a->up);
    } else a->target = a->up;
    if(a->health == 0) a->front = v3_rotate_axis14(a->front, a->up, DEG(10));
    a->velocity = v3_sub(a->velocity, v3_scale14(a->up, V16(.15f)));
    if(distance > 0 && distance < FX(300)) a->value = 30;
    else if(a->value >= 0) a->value--;
    if(a->state == FL_SPIN || a->state == FL_SPIN_WIND){
        if(a->timer < 30)
            a->velocity =
                v3_add(a->velocity, v3_scale14(a->up, a->state == FL_SPIN_WIND ? V16(1.3f) : V16(.8f)));
        if(a->timer >= 120){
            actor_set_state(a, FL_FLY, ANIM_FLUFF_FLY);
            mario_set_anim(a->health ? ANIM_MARIO_FLUFFFLY : ANIM_MARIO_FLUFFFLYLAST);
        }
    }
    if(a->state != FL_START && (g_game.keys.pressed & KEY_B) && a->health > 0 &&
       a->timer > ((a->state == FL_SPIN || a->state == FL_SPIN_WIND) ? 60 : 0)){
        a->health--;
        int effect = fluff_spin_sfx[2 - a->health][a->value > 0];
        audio_play_effect(effect, 5);
        int anim = a->health == 2   ? ANIM_FLUFF_SPIN1ST
                   : a->health == 1 ? ANIM_FLUFF_SPIN2ND
                                    : ANIM_FLUFF_SPIN3RD;
        actor_set_state(a, a->value > 0 ? FL_SPIN_WIND : FL_SPIN, anim);
        mario_restart_anim(ANIM_MARIO_FLUFFSPIN);
        a->velocity = v3_add(a->velocity, v3_scale14(a->up, a->state == FL_SPIN_WIND ? V16(1.3f) : V16(.8f)));
    } else if(a->state == FL_START && mario_anim_done()){
        actor_set_state(a, FL_FLY, ANIM_FLUFF_FLY);
        mario_set_anim(ANIM_MARIO_FLUFFFLY);
    } else if(a->state == FL_FLY && distance > 0 && distance < FX(300)){
        actor_set_state(a, FL_WIND, ANIM_FLUFF_FLYWIND);
        mario_set_anim(ANIM_MARIO_FLUFFFLYWIND);
    } else if(a->state == FL_WIND && distance > FX(400)){
        actor_set_state(a, FL_FLY, ANIM_FLUFF_FLY);
        mario_set_anim(a->health ? ANIM_MARIO_FLUFFFLY : ANIM_MARIO_FLUFFFLYLAST);
    }
    v3 old = a->position, next = v3_add(old, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
    if(a->state != FL_START && v3_dot14(a->velocity, a->up) < 0 && fluff_floor_hit(old, next, a->up)){
        fluff_release(a, false);
        return;
    }
    a->position = next;
    fluff_place_mario(a);
}

static int fluff_rider_slot(void){ return g_fluff_rider; }
static void fluff_figure(const Actor* a, Figure* f){
    if(a->type == OBJ_FLUFF && a->state >= FL_START){
        f->basis_up = a->target;
        f->basis_front = v3_tangent14(a->front, a->target, a->front);
    }
}
