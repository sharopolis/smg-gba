typedef struct {
    int16_t ready_chase, appear_dist, sign_step, ready_min, ready_max, give_up_dist;
    int16_t appear_min, appear_max, appear_end, escape_time, escape_dist, escape_min, escape_max,
        escape_angle;
    int16_t escape_hold, laugh, find_min, find_max, find_dmin, find_dmax, knock_stop, knock_step, more_step;
    int32_t hide_max, hide_acc, head_max, head_acc, bust_max, bust_acc, escape_speed, jump_speed, more_max,
        more_acc;
    int16_t hide_damp, head_damp, bust_damp, more_damp;
    int16_t hide_turn, head_turn, bust_turn, escape_turn, more_turn;
    bool angry;
} BurrowsChaseParams;
#define BV(x) ((int32_t)((x) * 256.0f))
#define BD(x) ((int16_t)((x) * 16384.0f))
static const BurrowsChaseParams burrows_chase_params[2] = {
    {60,    600,    30,     45,      55,     800,   120,    150,   5,     250,    1500,
     50,    70,     50,     60,      120,    60,    90,     1200,  1500,  80,     -1,
     360,   BV(16), BV(.1), BV(9),   BV(.1), BV(5), BV(.2), BV(8), BV(5), BV(10), BV(1),
     BD(0), BD(0),  BD(0),  BD(.92), 15,     10,    20,     15,    30,    false},
    {60,    500,    30,      55,      65,     750,   60,     75,    5,     140,    1500,
     40,    60,     70,      60,      120,    60,    90,     1500,  1600,  -1,     45,
     240,   BV(20), BV(.2),  BV(9),   BV(.1), BV(6), BV(.2), BV(9), BV(5), BV(18), BV(.5),
     BD(0), BD(0),  BD(.99), BD(.96), 10,     10,    20,     20,    1,     true}};

static const int32_t burrows_cylinder_base[3] = {-18482, 290, 5001};
#define BURROWS_CYLINDER_RADIUS 650
#define BURROWS_CYLINDER_HEIGHT 2160
static const int32_t burrows_mario_start[3] = {-18472, 1647, 4782};
static const int16_t burrows_mario_up[3] = {676, 10223, 12785};
static const int32_t burrows_cutscene_mario[3] = {-19032, 1540, 4751};
static const int32_t burrows_cutscene_position[3] = {-18482, 1340, 5001};

static int16_t g_burrows_hill_ramp;
static v3 g_burrows_wall_normal;
static bool g_burrows_detour;

static inline const BurrowsChaseParams* burrows_params(const Actor* a){
    return &burrows_chase_params[a->args[0] >= BURROWS_PHASE_ANGRY_CHASE];
}
static inline bool burrows_in_rail_phase(const Actor* a){
    return a->state >= BURROWS_RAIL_HIDE && a->state <= BURROWS_RAIL_SPINOUT;
}

static void burrows_anim(Actor* a, int body, int lead){
    if(body >= 0){
        a->anim = body;
        a->anim_frame = 0;
        a->flags &= ~ACTOR_HIDDEN;
    } else a->flags |= ACTOR_HIDDEN;
    a->args[3] = (int16_t)(lead + 1);
}

static void burrows_set(Actor* a, int state, int body, int lead){
    if(state != BURROWS_CHASE_CHASEHIDE && state != BURROWS_CHASE_READYAPPEAR &&
       state != BURROWS_CHASE_SIGN && state != BURROWS_CHASE_APPEARSTART && state != BURROWS_CHASE_APPEAR &&
       state != BURROWS_CHASE_APPEAREND && state != BURROWS_CHASE_HIDEATTACK && state != BURROWS_CHASE_MORE)
        g_burrows_detour = false;
    actor_set_state(a, state, -1);
    burrows_anim(a, body, lead);
}

static void burrows_snap_to_ground(Actor* a){
    GravitySample g = gravity_sample(g_game.stage.header, a->position);
    if(g.index >= 0){
        v3 old = a->up;
        a->up = g.up;
        a->front = v3_tangent14(v3_rotate_between14(a->front, old, a->up), a->up, a->front);
    }
    RayHit hit;
    if(collision_ray(v3_add(a->position, v3_scale14(a->up, FX(200))),
                     v3_sub(a->position, v3_scale14(a->up, FX(1000))), &hit, 0))
        a->position = hit.point;
}

static bool burrows_inside_cylinder(const Actor* a, v3* out, fx* r){
    v3 d = v3_sub(a->position, v3_from_i32(burrows_cylinder_base));
    if(d.z < 0 || d.z > BURROWS_CYLINDER_HEIGHT) return false;
    v3 flat = V3(d.x, d.y, 0);
    *r = v3_length_exact(flat);
    if(*r >= BURROWS_CYLINDER_RADIUS) return false;
    *out = v3_direction14(flat, a->front);
    return true;
}

static void burrows_push_out_of_cylinder(Actor* a, fx extra){
    v3 out;
    fx r;
    if(!burrows_inside_cylinder(a, &out, &r)) return;
    a->position = v3_add(a->position, v3_scale14(out, BURROWS_CYLINDER_RADIUS - r + extra));
}

static bool burrows_hits_cylinder(const Actor* a){
    v3 d = v3_sub(a->position, v3_from_i32(burrows_cylinder_base));
    if(d.z < 0 || d.z > BURROWS_CYLINDER_HEIGHT) return false;
    v3 flat = V3(d.x, d.y, 0);
    fx r = v3_length_exact(flat);
    if(r > BURROWS_CYLINDER_RADIUS + 2 || r == 0 || v3_length_squared64(a->velocity) < 64) return false;
    v3 n = v3_direction14(flat, a->front);
    if(v3_dot14(n, a->velocity) >= 0) return false;
    g_burrows_wall_normal = n;
    return true;
}
static void burrows_slide_along_wall(Actor* a){
    fx speed = v3_length_exact(a->velocity);
    v3 v = v3_reject14(a->velocity, g_burrows_wall_normal);
    if(v3_length_squared64(v) < 64) v = v3_cross14(a->up, g_burrows_wall_normal);
    a->velocity = v3_scale14(v3_direction14(v, a->front), speed);
}
static void burrows_wall_slide(Actor* a){
    if(burrows_hits_cylinder(a)) burrows_slide_along_wall(a);
}

static bool burrows_snap_to_cylinder(Actor* a){
    v3 d = v3_sub(a->position, v3_from_i32(burrows_cylinder_base));
    v3 flat = V3(d.x, d.y, 0);
    if(v3_length_squared64(flat) == 0) return false;
    g_burrows_wall_normal = v3_direction14(flat, a->front);
    a->position = v3_add(v3_from_i32(burrows_cylinder_base),
                         v3_add(V3(0, 0, d.z), v3_scale14(g_burrows_wall_normal, BURROWS_CYLINDER_RADIUS)));
    return true;
}

static void burrows_move(Actor* a, bool snap){
    burrows_wall_slide(a);
    a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
    burrows_push_out_of_cylinder(a, 0);
    if(snap) burrows_snap_to_ground(a);
}

static void burrows_accelerate(Actor* a, v3 dir, int32_t acc, int32_t damp, int32_t max){
    v3 v = a->velocity;
    v3 along = v3_scale14(dir, v3_dot14(v, dir));
    v = v3_add(v3_add(along, v3_scale14(v3_sub(v, along), damp)), v3_scale14(dir, acc));
    if(v3_length_exact(v) > max) v = v3_scale14(v3_direction14(v, a->front), max);
    a->velocity = v3_reject14(v, a->up);
}

static v3 burrows_to_mario(const Actor* a){
    return v3_reject14(v3_sub(g_game.player.figure.position, a->position), a->up);
}

static void burrows_chase_move(Actor* a, int32_t max, int32_t acc, int32_t damp, int turn10){
    if(g_burrows_detour){
        v3 dir = v3_length_squared64(a->velocity) < 64 ? v3_cross14(a->up, g_burrows_wall_normal)
                                                       : v3_direction14(a->velocity, a->front);
        actor_turn_toward(a, dir, DEG(1.8f));
        burrows_accelerate(a, dir, acc, damp, max);
        a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
        if(!burrows_snap_to_cylinder(a)){
            g_burrows_detour = false;
            burrows_snap_to_ground(a);
            return;
        }
        burrows_snap_to_ground(a);
        burrows_slide_along_wall(a);
        if(v3_dot14(g_burrows_wall_normal, burrows_to_mario(a)) > 0) g_burrows_detour = false;
        return;
    }
    v3 to = burrows_to_mario(a);
    if(v3_length_squared64(to) > 64){
        actor_turn_toward(a, to, DEG(turn10 / 10.0f));
        burrows_accelerate(a, a->front, acc, damp, max);
    }
    if(burrows_hits_cylinder(a)){
        g_burrows_detour = true;
        burrows_slide_along_wall(a);
        return;
    }
    burrows_move(a, true);
}

static void burrows_follow_rail(Actor* a, int32_t speed){
    int rail = a->args[1];
    if(rail < 0) return;
    a->home.y += speed;
    fx len = rail_length(rail);
    if(len > 0) a->home.y %= len << 8;
    fx c = a->home.y >> 8;
    a->position = rail_position_at(rail, c);
    v3 dir = v3_sub(rail_position_at(rail, c + FX(20)), rail_position_at(rail, c - FX(20)));
    burrows_snap_to_ground(a);
    a->front = v3_tangent14(dir, a->up, a->front);
}

static void burrows_set_rail_speed(Actor* a, int32_t max){
    a->home.z = max;
    if(a->home.x > max) a->home.x = max;
}

static bool burrows_mario_hipdropped(const Actor* a){
    const Player* p = &g_game.player;
    return p->action == MA_HIPDROP_LAND && p->action_timer == 0 && v3_dot14(p->up, a->up) >= 0;
}

static fx burrows_mario_distance(const Actor* a){
    return v3_length_exact(v3_sub(g_game.player.figure.position, a->position));
}

static void burrows_trail_hills(Actor* a){
    if(g_burrows_hill_ramp)
        molehill_trail(a, &a->target, &a->args[2], &g_burrows_hill_ramp, FX(150), BURROWS_HILLS_KEPT);
    else a->target = a->position;
}
static void burrows_hills_start(Actor* a){
    if(!g_burrows_hill_ramp){
        g_burrows_hill_ramp = 1;
        a->target = a->position;
    }
}
static void burrows_hills_stop(void){ g_burrows_hill_ramp = 0; }

static void burrows_cutscene_pose(Actor* a, bool reset){
    Player* p = &g_game.player;
    if(reset){
        a->position = v3_from_i32(burrows_cutscene_position);
        burrows_snap_to_ground(a);
        mario_reset(v3_from_i32(burrows_cutscene_mario), v3_from_i16(burrows_mario_up), V3(0, N14_ONE, 0));
    }
    v3 to = v3_reject14(v3_sub(a->position, p->figure.position), p->up);
    p->front = v3_tangent14(to, p->up, p->front);
    if(p->action != MA_DEATH) mario_set_action(MA_TALK);
}

static void burrows_cutscene_end(void){
    if(g_game.player.action == MA_TALK) mario_set_action(MA_WAIT);
}

static void burrows_demo(Actor* a){
    int phase = a->args[0];
    Player* p = &g_game.player;
    if(p->action != MA_TALK && p->action != MA_DEATH) mario_set_action(MA_TALK);
    if(phase == BURROWS_PHASE_OPENING){
        int t = a->timer;
        burrows_follow_rail(a, BV(10));
        burrows_trail_hills(a);
        v3 to = burrows_to_mario(a);
        if(a->value == 0 && t >= 60){
            a->value = 1;
            a->health = (int16_t)t;
            burrows_anim(a, -1, ANIM_DODORYULEADHILL_EFFECTWALKHIDE);
        } else if(a->value == 1 && t - a->health >= 29){
            a->value = 2;
            a->health = (int16_t)t;
            burrows_anim(a, ANIM_DODORYU_OPENINGAPPEAR, ANIM_DODORYULEADHILL_APPEAR);
        } else if(a->value == 2 && t - a->health >= 79){
            a->value = 3;
            a->health = (int16_t)t;
            burrows_anim(a, ANIM_DODORYU_OPENINGWALK, ANIM_DODORYULEADHILL_WALK);
        } else if(a->value == 3 && t - a->health >= 60){
            a->value = 4;
            a->health = (int16_t)t;
            burrows_anim(a, ANIM_DODORYU_HIDE, ANIM_DODORYULEADHILL_HIDE);
        } else if(a->value == 4 && t - a->health >= 49){
            a->value = 5;
            burrows_anim(a, -1, ANIM_DODORYULEADHILL_EFFECTWALK);
        }
        if(a->value >= 2 && a->value <= 4) actor_turn_toward(a, to, DEG(3));
        if(t >= 300){
            burrows_cutscene_pose(a, false);
            burrows_cutscene_end();
            burrows_next_phase(a);
        }
        return;
    }
    int len = phase == BURROWS_PHASE_APPEAR ? 299 : phase == BURROWS_PHASE_ANGRY ? 189 : 369;
    if(phase == BURROWS_PHASE_DOWN && a->timer >= 259) a->flags |= ACTOR_HIDDEN;
    if(a->timer >= len){
        burrows_cutscene_end();
        if(phase == BURROWS_PHASE_APPEAR)
            collectibles_pop(OBJ_STARBIT, v3_add(a->position, v3_scale14(a->up, FX(300))), a->up, 8);
        if(phase == BURROWS_PHASE_ANGRY)
            collectibles_pop(OBJ_STARBIT, v3_add(a->position, v3_scale14(a->up, FX(300))), a->up, 16);
        burrows_next_phase(a);
    }
}

static void burrows_start_phase(Actor* a){
    Game* g = &g_game;
    switch(a->args[0]){
    case BURROWS_PHASE_OPENING:
        a->home.y = 0;
        a->value = 0;
        burrows_hills_start(a);
        burrows_set(a, BURROWS_DEMO, -1, ANIM_DODORYULEADHILL_EFFECTWALK);
        burrows_cutscene_pose(a, false);
        mario_reset(v3_from_i32(burrows_mario_start), v3_from_i16(burrows_mario_up), V3(0, N14_ONE, 0));
        burrows_cutscene_pose(a, false);
        break;
    case BURROWS_PHASE_RAIL:
        switch_set(actor_switch_key(a, SWITCH_B), false);
        a->value = (int16_t)actor_tick_random(a, 150, 200);
        a->home.x = BV(10);
        burrows_set_rail_speed(a, BV(8));
        burrows_hills_start(a);
        burrows_set(a, BURROWS_RAIL_HIDE, -1, ANIM_DODORYULEADHILL_EFFECTWALK);
        break;
    case BURROWS_PHASE_APPEAR:
        audio_stop_music();
        burrows_hills_stop();
        burrows_set(a, BURROWS_DEMO, ANIM_DODORYU_APPEARDEMO, -1);
        burrows_cutscene_pose(a, true);
        break;
    case BURROWS_PHASE_ANGRY:
        audio_stop_music();
        burrows_hills_stop();
        burrows_set(a, BURROWS_DEMO, ANIM_DODORYU_ANGRYDEMO, -1);
        burrows_cutscene_pose(a, true);
        break;
    case BURROWS_PHASE_DOWN:
        audio_stop_music();
        burrows_hills_stop();
        burrows_set(a, BURROWS_DEMO, ANIM_DODORYU_DOWNDEMO, -1);
        burrows_cutscene_pose(a, true);
        break;
    case BURROWS_PHASE_CHASE:
    case BURROWS_PHASE_ANGRY_CHASE:
        audio_play_music(MUSIC_MBGM_BOSS_05_A);
        a->health = 0;
        a->velocity = V3(0, 0, 0);
        burrows_hills_stop();
        burrows_set(a, BURROWS_CHASE_START, ANIM_DODORYU_BURROW, -1);
        break;
    case BURROWS_PHASE_DONE:
        burrows_hills_stop();
        level_star_appear(g->scenario == 4 ? 4 : 2, a->position);
        audio_play_music(MUSIC_GUSTYGARDEN);
        switch_set(actor_switch_key(a, SWITCH_B), true);
        a->flags |= ACTOR_HIDDEN;
        a->args[3] = 0;
        actor_set_state(a, BURROWS_DEMO, -1);
        a->active = 0;
        break;
    }
}

static void burrows_next_phase(Actor* a){
    a->args[0]++;
    burrows_start_phase(a);
}

static void burrows_update_radius(Actor* a){
    bool appear = a->state == BURROWS_RAIL_APPEARSTART || a->state == BURROWS_CHASE_APPEARSTART;
    bool under = (a->flags & ACTOR_HIDDEN) || (appear && a->timer < 10);
    bool rising = appear && a->timer < 30;
    a->radius = under ? FX(120) : rising ? FX(160) : FX(250);
}

static bool burrows_hurt_mario(Actor* a){
    Player* p = &g_game.player;
    if(p->invincible_timer > 0 || p->action == MA_DAMAGE || p->action == MA_DAMAGE_FALL) return false;
    actor_hurt_mario(a, 1);
    camera_shake(1);
    if(burrows_in_rail_phase(a) || a->state == BURROWS_CHASE_DIVE) return true;
    bool hidden = a->state == BURROWS_CHASE_CHASEHIDE || a->state == BURROWS_CHASE_READYAPPEAR ||
                  a->state == BURROWS_CHASE_APPEAREND;
    if(hidden) burrows_set(a, BURROWS_CHASE_HIDEATTACK, -1, ANIM_DODORYULEADHILL_EFFECTWALK);
    else if(a->state != BURROWS_CHASE_LAUGH && a->state != BURROWS_CHASE_HIDEATTACK){
        burrows_set(a, BURROWS_CHASE_LAUGH, ANIM_DODORYU_ATTACKHIT, ANIM_DODORYULEADHILL_ATTACKHIT);
        audio_play_effect(SFX_BURROWSLAUGH, 3);
    }
    return true;
}

static void burrows_knock_back(Actor* a, int state, int anim){
    v3 dir = v3_reject14(v3_sub(a->position, g_game.player.figure.position), a->up);
    a->velocity = v3_direction14(dir, v3_neg(a->front));
    actor_turn_toward(a, v3_neg(a->velocity), DEG(180));
    burrows_set(a, state, anim, -1);
    burrows_hills_stop();
    camera_shake(2);
    audio_play_effect(SFX_BURROWSSPINDMG, 4);
    mario_spin_hop();
}

static void burrows_knocked_move(Actor* a){
    a->position = v3_add(a->position, v3_scale14(a->velocity, 10));
    burrows_push_out_of_cylinder(a, 0);
    burrows_snap_to_ground(a);
}

static void burrows_rail_think(Actor* a){
    burrows_update_radius(a);
    int contact = actor_mario_contact(a, FX(60));
    bool appear = a->state == BURROWS_RAIL_APPEARSTART || a->state == BURROWS_RAIL_APPEAR ||
                  a->state == BURROWS_RAIL_APPEAREND;
    if(burrows_mario_hipdropped(a)){
        burrows_set_rail_speed(a, 0);
        if(a->state == BURROWS_RAIL_HIDE || a->state == BURROWS_RAIL_SIGN){
            burrows_anim(a, ANIM_DODORYU_JUMP, ANIM_DODORYULEADHILL_JUMP);
            audio_play_effect(SFX_BURROWSHIPREACT, 3);
            actor_set_state(a, BURROWS_RAIL_HIDE, -1);
            a->value = (int16_t)actor_tick_random(a, 150, 200);
        } else if(appear){
            burrows_set(a, BURROWS_RAIL_JUMPOUT, ANIM_DODORYU_SWOONSTART, -1);
            burrows_hills_stop();
            audio_play_effect(SFX_BURROWSJUMPOUT, 4);
            camera_shake(1);
            return;
        }
    }

    if(a->home.x < a->home.z) a->home.x = fx_min(a->home.x + BV(.1), a->home.z);
    int32_t speed = fx_min(a->home.x + BV(.05), BV(8));
    switch(a->state){
    case BURROWS_RAIL_HIDE:
        if(a->timer == 1) burrows_set_rail_speed(a, BV(8));
        if(actor_anim_done(a) && !(a->flags & ACTOR_HIDDEN))
            burrows_anim(a, -1, ANIM_DODORYULEADHILL_EFFECTWALK);
        burrows_follow_rail(a, speed);
        if(a->timer >= a->value){
            burrows_set(a, BURROWS_RAIL_SIGN, -1, ANIM_DODORYULEADHILL_EFFECTWALKHIDE);
            burrows_hills_stop();
        }
        break;
    case BURROWS_RAIL_SIGN:
        burrows_follow_rail(a, speed);
        if(a->timer >= 30){
            burrows_set(a, BURROWS_RAIL_APPEARSTART, ANIM_DODORYU_OPENINGAPPEAR, ANIM_DODORYULEADHILL_APPEAR);
            burrows_set_rail_speed(a, BV(4));
            camera_shake(1);
            audio_play_effect(SFX_BURROWSAPPEAR, 3);
            burrows_hills_start(a);
        }
        break;
    case BURROWS_RAIL_APPEARSTART:
        burrows_follow_rail(a, speed);
        if(actor_anim_done(a)){
            burrows_set(a, BURROWS_RAIL_APPEAR, ANIM_DODORYU_OPENINGWALK, ANIM_DODORYULEADHILL_WALK);
            a->value = (int16_t)actor_tick_random(a, 15, 30);
        }
        break;
    case BURROWS_RAIL_APPEAR:
        burrows_follow_rail(a, speed);
        if(a->timer >= a->value)
            burrows_set(a, BURROWS_RAIL_APPEAREND, ANIM_DODORYU_HIDE, ANIM_DODORYULEADHILL_HIDE);
        break;
    case BURROWS_RAIL_APPEAREND:
        burrows_follow_rail(a, speed);
        if(actor_anim_done(a)){
            burrows_set(a, BURROWS_RAIL_HIDE, -1, ANIM_DODORYULEADHILL_EFFECTWALKAPPEAR);
            a->value = (int16_t)actor_tick_random(a, 150, 200);
        }
        break;
    case BURROWS_RAIL_JUMPOUT:
        if(actor_anim_done(a)) burrows_set(a, BURROWS_RAIL_DAMAGE, ANIM_DODORYU_SWOON, -1);
        break;
    case BURROWS_RAIL_DAMAGE:
        if(a->timer >= 180) burrows_set(a, BURROWS_RAIL_DIVE, ANIM_DODORYU_SWOONEND, -1);
        break;
    case BURROWS_RAIL_DIVE:
        if(actor_anim_done(a)){
            burrows_set(a, BURROWS_RAIL_HIDE, -1, ANIM_DODORYULEADHILL_EFFECTWALKAPPEAR);
            a->value = (int16_t)actor_tick_random(a, 150, 200);
            burrows_hills_start(a);
        }
        break;
    case BURROWS_RAIL_SPINOUT:
        burrows_knocked_move(a);
        if(a->timer >= 80) burrows_next_phase(a);
        return;
    }
    burrows_trail_hills(a);
    if((a->state == BURROWS_RAIL_JUMPOUT || a->state == BURROWS_RAIL_DAMAGE) && contact == CONTACT_SPIN){
        burrows_knock_back(a, BURROWS_RAIL_SPINOUT, ANIM_DODORYU_PUNCHDAMAGE);
        return;
    }
    bool attackable = a->state == BURROWS_RAIL_HIDE ||
                      (a->state == BURROWS_RAIL_APPEARSTART && a->timer > 16) ||
                      a->state == BURROWS_RAIL_APPEAR || a->state == BURROWS_RAIL_APPEAREND ||
                      (a->state == BURROWS_RAIL_DIVE && a->timer > 20);
    if(appear && contact == CONTACT_SPIN) return;
    if(attackable && (contact == CONTACT_TOUCH || contact == CONTACT_TRAMPLE)) burrows_hurt_mario(a);
}

static void burrows_start_find(Actor* a){
    const BurrowsChaseParams* c = burrows_params(a);
    burrows_set(a, BURROWS_CHASE_FINDPOS, -1, -1);
    burrows_hills_stop();
    a->health = 0;
    a->value = (int16_t)actor_tick_random(a, c->find_min, c->find_max);
    switch_set(actor_switch_key(a, SWITCH_B), true);

    Player* p = &g_game.player;
    a->position = p->figure.position;
    a->up = p->up;
    v3 dir = v3_rotate_axis14(p->front, p->up, (fx)actor_tick_random(a, 0, FX_TAU - 1));
    fx dist = actor_tick_random(a, c->find_dmin, c->find_dmax);
    a->velocity = v3_scale14(dir, dist * 256 / a->value);
}

static void burrows_chase_think(Actor* a){
    const BurrowsChaseParams* c = burrows_params(a);
    burrows_update_radius(a);
    int contact = (a->state == BURROWS_CHASE_FINDPOS || a->state == BURROWS_CHASE_START ||
                   a->state == BURROWS_CHASE_READYCHASE)
                      ? CONTACT_NONE
                      : actor_mario_contact(a, FX(60));
    if(burrows_mario_hipdropped(a)){
        int s = a->state;
        bool more = s == BURROWS_CHASE_MORESTART || s == BURROWS_CHASE_MORE || s == BURROWS_CHASE_MOREEND;
        if(s == BURROWS_CHASE_READYAPPEAR || s == BURROWS_CHASE_SIGN ||
           (s == BURROWS_CHASE_APPEARSTART && a->timer <= 15)){
            a->velocity = V3(0, 0, 0);
            burrows_anim(a, ANIM_DODORYU_JUMP, ANIM_DODORYULEADHILL_JUMP);
            audio_play_effect(SFX_BURROWSHIPREACT, 3);
            actor_set_state(a, BURROWS_CHASE_CHASEHIDE, -1);
            burrows_hills_stop();
        } else if(s == BURROWS_CHASE_ESCAPE){
            actor_set_state(a, BURROWS_CHASE_ESCAPEJUMP, -1);
            a->velocity = v3_scale14(a->up, BV(30));
        } else if(s == BURROWS_CHASE_APPEARSTART || s == BURROWS_CHASE_APPEAR ||
                  s == BURROWS_CHASE_APPEAREND || more){
            a->velocity = V3(0, 0, 0);
            camera_shake(1);
            audio_play_effect(SFX_BURROWSJUMPOUT, 4);
            if(c->angry && !more){
                burrows_set(a, BURROWS_CHASE_MORESTART, ANIM_DODORYU_ANGRYCHASESTART,
                            ANIM_DODORYULEADHILL_ANGRYCHASESTART);
                v3 away = v3_reject14(v3_sub(a->position, g_game.player.figure.position), a->up);
                a->velocity = v3_scale14(v3_direction14(away, v3_neg(a->front)),
                                         BV(1000) / anim_length_of(a->figure.model, a->anim));
            } else {
                burrows_set(a, BURROWS_CHASE_JUMPOUT, ANIM_DODORYU_SWOONWALKSTART, -1);
                burrows_hills_stop();
            }
            return;
        }
    }
    switch(a->state){
    case BURROWS_CHASE_START:
        if(actor_anim_done(a)) burrows_start_find(a);
        return;
    case BURROWS_CHASE_FINDPOS:
        a->velocity = v3_reject14(a->velocity, a->up);
        a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
        {
            v3 out;
            fx r;
            if(burrows_inside_cylinder(a, &out, &r))
                a->position = v3_add(a->position, v3_scale14(out, BURROWS_CYLINDER_RADIUS - r + 150));
        }
        burrows_snap_to_ground(a);
        if(a->timer > a->value){
            actor_turn_toward(a, burrows_to_mario(a), DEG(180));
            a->velocity = V3(0, 0, 0);
            switch_set(actor_switch_key(a, SWITCH_B), false);
            burrows_set(a, BURROWS_CHASE_READYCHASE, -1, -1);
            camera_shake(1);
            audio_play_effect(SFX_BURROWSSPLASH, 3);
        }
        return;
    case BURROWS_CHASE_READYCHASE:
        if(a->timer >= c->ready_chase){
            burrows_set(a, BURROWS_CHASE_CHASEHIDE, -1, -1);
            a->velocity = V3(0, 0, 0);
        }
        return;
    case BURROWS_CHASE_CHASEHIDE:
        if(actor_anim_done(a)) a->flags |= ACTOR_HIDDEN, a->args[3] = 0;
        burrows_chase_move(a, c->hide_max, c->hide_acc, c->hide_damp, c->hide_turn);
        if(burrows_mario_distance(a) < c->appear_dist){
            burrows_set(a, BURROWS_CHASE_READYAPPEAR, -1, ANIM_DODORYULEADHILL_EFFECTWALKAPPEAR);
            burrows_hills_start(a);
            a->value = (int16_t)actor_tick_random(a, c->ready_min, c->ready_max);
        }
        break;
    case BURROWS_CHASE_READYAPPEAR:
        if(actor_anim_done(a) && a->args[3] == ANIM_DODORYULEADHILL_EFFECTWALKAPPEAR + 1)
            burrows_anim(a, -1, ANIM_DODORYULEADHILL_EFFECTWALK);
        burrows_chase_move(a, c->head_max, c->head_acc, c->head_damp, c->head_turn);
        if(burrows_mario_distance(a) >= c->give_up_dist){
            burrows_set(a, BURROWS_CHASE_CHASEHIDE, -1, ANIM_DODORYULEADHILL_EFFECTWALKHIDE);
            burrows_hills_stop();
        } else if(a->timer >= a->value){
            burrows_set(a, BURROWS_CHASE_SIGN, -1, ANIM_DODORYULEADHILL_EFFECTWALKHIDE);
            burrows_hills_stop();
        }
        break;
    case BURROWS_CHASE_SIGN:
        burrows_chase_move(a, c->head_max, c->head_acc, c->head_damp, c->head_turn);
        if(a->timer >= c->sign_step){
            burrows_set(a, BURROWS_CHASE_APPEARSTART, ANIM_DODORYU_APPEAR, ANIM_DODORYULEADHILL_APPEAR);
            burrows_hills_start(a);
            camera_shake(1);
            audio_play_effect(SFX_BURROWSAPPEAR, 3);
        }
        break;
    case BURROWS_CHASE_APPEARSTART:
        burrows_chase_move(a, c->bust_max, c->bust_acc, c->bust_damp, c->bust_turn);
        if(actor_anim_done(a)){
            burrows_set(a, BURROWS_CHASE_APPEAR, ANIM_DODORYU_WALK, ANIM_DODORYULEADHILL_WALK);
            a->value = (int16_t)actor_tick_random(a, c->appear_min, c->appear_max);
        }
        break;
    case BURROWS_CHASE_APPEAR:
        burrows_chase_move(a, c->bust_max, c->bust_acc, c->bust_damp, c->bust_turn);
        if(a->timer >= a->value){
            burrows_set(a, BURROWS_CHASE_APPEAREND, ANIM_DODORYU_HIDE, ANIM_DODORYULEADHILL_HIDE);
            a->health++;
        }
        break;
    case BURROWS_CHASE_APPEAREND:
        burrows_chase_move(a, c->bust_max, c->bust_acc, c->bust_damp, c->bust_turn);
        if(actor_anim_done(a)){
            burrows_hills_stop();
            if(a->health >= c->appear_end) burrows_start_find(a);
            else burrows_set(a, BURROWS_CHASE_CHASEHIDE, -1, -1);
        }
        break;
    case BURROWS_CHASE_HIDEATTACK:
        burrows_chase_move(a, c->hide_max, c->hide_acc, c->hide_damp, 0);
        if(burrows_mario_distance(a) > FX(1000)){
            burrows_set(a, BURROWS_CHASE_CHASEHIDE, -1, -1);
            burrows_hills_stop();
        }
        break;
    case BURROWS_CHASE_LAUGH:
        if(a->timer >= c->laugh)
            burrows_set(a, BURROWS_CHASE_LAUGHEND, ANIM_DODORYU_HIDE, ANIM_DODORYULEADHILL_HIDE);
        break;
    case BURROWS_CHASE_LAUGHEND:
        if(actor_anim_done(a)) burrows_start_find(a);
        break;
    case BURROWS_CHASE_JUMPOUT: {
        v3 away = v3_neg(burrows_to_mario(a));
        if(v3_length_squared64(away) > 64){
            actor_turn_toward(a, away, DEG(3));
            a->velocity = v3_scale14(v3_direction14(away, a->front), c->jump_speed);
            burrows_move(a, true);
        }
        if(actor_anim_done(a)){
            burrows_set(a, BURROWS_CHASE_ESCAPE, ANIM_DODORYU_SWOONWALK, -1);
            a->health = c->escape_time;
            a->value = 0;
            a->home.x = 0;
        }
        break;
    }
    case BURROWS_CHASE_ESCAPE:
    case BURROWS_CHASE_ESCAPEJUMP:
        if(a->state == BURROWS_CHASE_ESCAPEJUMP){
            a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
            a->velocity = v3_sub(a->velocity, v3_scale14(a->up, BV(1.2)));
            if(a->timer >= 49){
                actor_set_state(a, BURROWS_CHASE_ESCAPE, -1);
                a->value = 0;
                burrows_snap_to_ground(a);
            }
        } else {
            if(a->value <= 0){
                a->value = (int16_t)actor_tick_random(a, c->escape_min, c->escape_max);
                v3 away = v3_neg(burrows_to_mario(a));
                fx turn = (fx)((int32_t)DEG(c->escape_angle) * actor_tick_random(a, -256, 256) / 256);
                a->target = v3_rotate_axis14(v3_direction14(away, a->front), a->up, turn);
            }
            a->value--;
            a->home.x = fx_min(a->home.x + BV(.05), c->escape_speed);
            actor_turn_toward(a, a->target, DEG(c->escape_turn / 10.0f));
            a->velocity = v3_scale14(a->front, a->home.x);
            burrows_move(a, true);
        }
        if(a->health > 0) a->health--;
        if(a->state == BURROWS_CHASE_ESCAPE && a->timer >= c->escape_hold &&
           (burrows_mario_distance(a) >= c->escape_dist || a->health <= 0))
            burrows_set(a, BURROWS_CHASE_RECOVER, ANIM_DODORYU_SWOONWALKEND, -1);
        break;
    case BURROWS_CHASE_RECOVER:
        a->velocity = V3(0, 0, 0);
        if(actor_anim_done(a)) burrows_set(a, BURROWS_CHASE_DIVE, ANIM_DODORYU_DIVE, -1);
        break;
    case BURROWS_CHASE_DIVE:
        if(a->timer == 50) a->flags |= ACTOR_HIDDEN;
        if(actor_anim_done(a)) burrows_start_find(a);
        break;
    case BURROWS_CHASE_KNOCKDOWN:
        if(c->knock_stop < 0 || a->timer < c->knock_stop) burrows_knocked_move(a);
        if(c->knock_step >= 0 ? a->timer >= c->knock_step : actor_anim_done(a)) burrows_next_phase(a);
        return;
    case BURROWS_CHASE_MORESTART:
        actor_turn_toward(a, burrows_to_mario(a), DEG(180));
        burrows_move(a, true);
        if(actor_anim_done(a)){
            a->velocity = V3(0, 0, 0);
            burrows_set(a, BURROWS_CHASE_MORE, ANIM_DODORYU_ANGRYCHASE, ANIM_DODORYULEADHILL_ANGRYCHASE);
            burrows_hills_start(a);
            camera_shake(1);
        }
        break;
    case BURROWS_CHASE_MORE:
        burrows_chase_move(a, c->more_max, c->more_acc, c->more_damp, c->more_turn);
        if(a->timer >= c->more_step)
            burrows_set(a, BURROWS_CHASE_MOREEND, ANIM_DODORYU_ANGRYCHASEEND,
                        ANIM_DODORYULEADHILL_ANGRYCHASEEND);
        break;
    case BURROWS_CHASE_MOREEND:
        if(actor_anim_done(a)) burrows_start_find(a);
        break;
    }
    burrows_trail_hills(a);
    int s = a->state;
    bool down = s == BURROWS_CHASE_JUMPOUT || s == BURROWS_CHASE_ESCAPE || s == BURROWS_CHASE_ESCAPEJUMP ||
                s == BURROWS_CHASE_RECOVER || (s == BURROWS_CHASE_DIVE && a->timer <= 20);
    if(down && contact == CONTACT_SPIN){
        burrows_knock_back(a, BURROWS_CHASE_KNOCKDOWN, ANIM_DODORYU_ANGRYPUNCHDAMAGE);
        return;
    }
    bool reflect = s == BURROWS_CHASE_APPEARSTART || s == BURROWS_CHASE_APPEAR ||
                   s == BURROWS_CHASE_APPEAREND || s == BURROWS_CHASE_MORESTART || s == BURROWS_CHASE_MORE ||
                   s == BURROWS_CHASE_MOREEND;
    if(reflect && contact == CONTACT_SPIN) return;
    bool attackable = s == BURROWS_CHASE_CHASEHIDE || s == BURROWS_CHASE_READYAPPEAR ||
                      s == BURROWS_CHASE_APPEARSTART || s == BURROWS_CHASE_APPEAR ||
                      s == BURROWS_CHASE_APPEAREND || (s == BURROWS_CHASE_DIVE && a->timer > 20) ||
                      s == BURROWS_CHASE_MORESTART || s == BURROWS_CHASE_MORE || s == BURROWS_CHASE_MOREEND;
    if(attackable && (contact == CONTACT_TOUCH || contact == CONTACT_TRAMPLE)) burrows_hurt_mario(a);
}

static void burrows_init(Actor* a, const StageObject* o){
    a->args[0] = BURROWS_PHASE_WAIT;
    a->args[1] = o->rail;
    a->args[2] = 0;
    a->radius = FX(250);
    a->home = V3(BV(8), o->rail >= 0 ? rail_closest_coord(o->rail, a->position) << 8 : 0, BV(8));
    a->target = a->position;
    g_burrows_hill_ramp = 0;
    g_burrows_detour = false;
    if(o->rail >= 0) burrows_follow_rail(a, 0);
    burrows_set(a, BURROWS_WAIT, -1, ANIM_DODORYULEADHILL_EFFECTWALK);
    burrows_hills_start(a);
    switch_set(actor_switch_key(a, SWITCH_B), true);
}

static void burrows_think(Actor* a){
    if(!actor_find_type(OBJ_DODORYULEADHILL)){
        Actor* lead = actor_spawn_reuse(OBJ_DODORYULEADHILL, a->position, a->up, a->front);
        if(lead) lead->value = (int16_t)actor_index(a);
    }
    if(a->state == BURROWS_WAIT){
        burrows_follow_rail(a, BV(8));
        burrows_trail_hills(a);
        if(switch_on(actor_switch_key(a, SWITCH_A)) && g_game.player.action != MA_STAR_LAUNCH) a->value++;
        else a->value = 0;
        if(a->value > 30) burrows_next_phase(a);
        return;
    }
    if(a->state == BURROWS_DEMO){
        burrows_demo(a);
        return;
    }
    if(burrows_in_rail_phase(a)) burrows_rail_think(a);
    else burrows_chase_think(a);
}

static void burrows_lead_think(Actor* a){
    Actor* host = a->value >= 0 && a->value < g_game.actor_count ? &g_game.actors[a->value] : NULL;
    if(!host || !host->active || host->type != OBJ_DODORYU){
        a->active = 0;
        return;
    }
    a->position = host->position;
    a->up = host->up;
    a->front = host->front;
    int want = host->args[3] - 1;
    if(want < 0){
        a->flags |= ACTOR_HIDDEN;
        a->anim = -1;
        return;
    }
    a->flags &= ~ACTOR_HIDDEN;
    if(want != a->anim || (host->anim_frame == 0 && host->timer == 0)){
        a->anim = want;
        a->anim_frame = 0;
    }
}
