enum { LANDING_NONE, LANDING_LAUNCH_STAR, LANDING_SLING_STAR };

#define FLIGHT_BCK_END      50
#define FLIGHT_BCK_START    15
#define JUMP_POWER          13
#define FLIGHT_RELEASE_SIDE FX(24)

#define STAR_SPIN(x) ((fx)((x) * 256.0f * 64.0f))
enum { STARF_POSED = 1, STARF_NO_PULL = 2, STARF_REWAIT = 4 };

__attribute__((pure, noinline)) static bool flight_default_branch(const StageObject* o){
    bool (*branch)(const StageObject* o) = level_current()->launch_star_branch;
    return branch && branch(o);
}
static bool flight_branch_actor(const Actor* a){
    return a->stage_index >= 0 && flight_default_branch(&g_game.stage.objects[a->stage_index]);
}
static bool flight_prefetch_near(v3 pos){
    v3 d = v3_sub(pos, g_game.camera.position);
    if(fx_abs(d.x) > FX(20000) || fx_abs(d.y) > FX(20000) || fx_abs(d.z) > FX(20000)) return false;
    return v3_length_squared64(d) <= (uint64_t)FX(20000) * FX(20000);
}
static bool flight_resident_wait_clock(const Actor* a){
    return a->type == OBJ_LAUNCHSTAR && a->state == STAR_WAIT && a->timer <= FLIGHT_BIND_TIME &&
           flight_branch_actor(a) && flight_prefetch_near(a->position);
}

static bool flight_star_keep(const Actor* a){
    if(a->type != OBJ_LAUNCHSTAR && a->type != OBJ_SLINGSTAR) return false;
    return a->state == STAR_BOUND || a->state == STAR_COOLDOWN ||
           (a->state == STAR_WAIT && a->timer <= FLIGHT_BIND_TIME && (a->velocity.z & STARF_REWAIT)) ||
           (a->type == OBJ_LAUNCHSTAR && flight_branch_actor(a) && flight_prefetch_near(a->position));
}

static EWRAM_BSS Flight g_flight;

static void flight_parabola_params(float h, float d, float* a, float* b){
    float disc = h * (h - d);
    if(disc <= 0){
        *a = -h;
        *b = d + h;
    } else if(d < 0.0001f && d > -0.0001f){
        *a = -4 * h;
        *b = 4 * h;
    } else {
        float r = disc > 1 ? disc / 2 : 1;
        for(int i = 0; i < 30; i++) r = 0.5f * (r + disc / r);
        float t1 = (h + r) / d, t2 = (h - r) / d;
        float ta = (t1 >= 0 && t1 <= 1) ? t1 : (t2 >= 0 && t2 <= 1) ? t2 : 1;
        *a = -h / (ta * ta);
        *b = -2 * ta * *a;
    }
}

static void flight_init_parabola(v3 start, v3 end, v3 up_point){
    Flight* f = &g_flight;
    v3 ay = v3_direction14(v3_sub(up_point, end), g_game.player.up);
    fx h = v3_dot14(v3_sub(up_point, start), ay);
    fx d = v3_dot14(v3_sub(end, start), ay);
    v3 horiz = v3_sub(v3_sub(end, start), v3_scale14(ay, d));
    f->path = 1;
    f->par_y = ay;
    f->par_z = v3_tangent14(v3_direction14(horiz, g_game.player.front), ay, g_game.player.front);
    f->par_h = v3_length_exact(horiz);
    float a, b;
    flight_parabola_params((float)h, (float)d, &a, &b);
    f->par_a = (fx)a;
    f->par_b = (fx)b;
}

static v3 flight_path_at(int32_t t){
    Flight* f = &g_flight;
    if(f->path == 1){
        fx y = (fx)(((int64_t)f->par_a * mul14(t, t) + (int64_t)f->par_b * t) >> 14);
        fx z = (fx)(((int64_t)f->par_h * t) >> 14);
        return v3_add(f->start, v3_add(v3_scale14(f->par_y, y), v3_scale14(f->par_z, z)));
    }
    if(f->path == 2){
        int32_t tc = fx_clamp(t, 0, N14_ONE);
        v3 p = rail_position_at(f->rail, (fx)(((int64_t)rail_length(f->rail) * tc) >> 14));

        int32_t e = fx_min(tc * 2, N14_ONE);
        int32_t w = N14_ONE - (fx_sin(mul14(e, FXC(1.5707963f) << 6) >> 6) << 6);
        return v3_add(p, v3_scale14(f->rail_offset, fx_max(w, 0)));
    }
    return f->start;
}

static v3 flight_path_dir(int32_t t, v3 fallback){
    int32_t step = R14(0.01f);
    int32_t a = t, b = t + step;
    if(b > N14_ONE){
        a = N14_ONE - step;
        b = N14_ONE;
    }
    return v3_direction14(v3_sub(flight_path_at(b), flight_path_at(a)), fallback);
}

static bool flight_setup_path(const Actor* a){
    Flight* f = &g_flight;
    const StageObject* o = a->stage_index >= 0 ? &g_game.stage.objects[a->stage_index] : NULL;
    f->start = a->position;
    f->path = 0;
    f->rail = o ? o->rail : -1;
    if(f->rail >= 0 && rail_point_count(f->rail) >= 3){
        f->path = 2;
        f->rail_offset = v3_sub(a->position, rail_position_at(f->rail, 0));
    } else if(f->rail >= 0 && rail_point_count(f->rail) >= 1){
        flight_init_parabola(a->position, rail_point(f->rail, rail_point_count(f->rail) - 1),
                             rail_point(f->rail, 0));
    } else if(!(a->target.x == a->position.x && a->target.y == a->position.y &&
                a->target.z == a->position.z)){
        v3 mid = V3((a->position.x + a->target.x) / 2, (a->position.y + a->target.y) / 2,
                    (a->position.z + a->target.z) / 2);
        fx dist = v3_length_exact(v3_sub(a->target, a->position));
        v3 lift = v3_direction14(v3_add(gravity_sample(g_game.stage.header, a->position).up,
                                        gravity_sample(g_game.stage.header, a->target).up),
                                 a->up);
        flight_init_parabola(a->position, a->target, v3_add(mid, v3_scale14(lift, dist / 3)));
    }
    if(!f->path) return false;

    f->init_up = flight_path_dir(0, a->up);
    v3 end = flight_path_at(N14_ONE);
    f->init_front = v3_tangent14(v3_direction14(v3_sub(end, f->start), a->front), f->init_up, a->front);
    int deg = a->stage_index >= 0 && o->args[5] > 0 ? o->args[5] : 0;
    if(deg) f->init_front = v3_rotate_axis14(f->init_front, f->init_up, (fx)((int32_t)deg * 1144 / 256));
    f->init_side = v3_cross14(f->init_front, f->init_up);
    return true;
}

static inline int flight_arg(const Actor* a, int k, int def){
    if(a->stage_index < 0) return k < 4 && a->args[k] ? a->args[k] : def;
    int v = g_game.stage.objects[a->stage_index].args[k];
    return v < 0 ? def : v;
}

static bool flight_takeover_accepts(const Actor* a){
    return g_game.player.action == MA_STAR_LAUNCH && g_flight.phase && !g_flight.starter &&
           a->type == OBJ_LAUNCHSTAR && a->stage_index != g_flight.star_object &&
           g_flight.phase != FLIGHT_SHOOT_START &&
           (g_flight.sling || g_flight.phase != FLIGHT_SHOOT || g_flight.step >= 40);
}

static void flight_star_spawned(Actor* a, const StageObject* o){
    if(flight_default_branch(o) && stream_launch_ready(a->stage_index)) a->timer = FLIGHT_BIND_TIME + 1;
}

static void flight_star_think(Actor* a){
    Game* game = &g_game;
    Player* p = &game->player;
    bool sling = a->type == OBJ_SLINGSTAR;
    if(!(a->velocity.z & STARF_POSED)){
        a->velocity.z |= STARF_POSED;
        Flight saved = g_flight;
        if(!sling && flight_setup_path(a)){
            a->up = g_flight.init_up;
            a->front = g_flight.init_front;
        }
        g_flight = saved;
    }

    a->velocity.x = (a->velocity.x + (a->velocity.y >> 6)) % FX_TAU;
    a->velocity.y = sling ? a->velocity.y * 243 / 256 : a->velocity.y * 252 / 256;

    v3 d = v3_sub(p->figure.position, a->position);
    bool far = v3_length_squared64(d) > (uint64_t)FX(350) * FX(350);
    if(far) a->velocity.z &= ~STARF_NO_PULL;
    if(a->state == STAR_BOUND && g_flight.phase != FLIGHT_OFF && g_flight.star >= 0 &&
       &g_game.actors[g_flight.star] == a && p->action != MA_STAR_LAUNCH){
        g_flight.phase = FLIGHT_OFF;
        camera_event_cancel();
    }
    if(a->state == STAR_BOUND && (g_flight.phase == FLIGHT_OFF || g_flight.star_object != a->stage_index ||
                                  g_flight.star < 0 || &g_game.actors[g_flight.star] != a)){
        a->state = STAR_COOLDOWN;
        a->timer = 0;
        a->velocity.z |= STARF_REWAIT;
        stream_launch_ready_set(a->stage_index, false);
    }
    if(a->state == STAR_COOLDOWN){
        if(a->timer > 60){
            a->state = STAR_WAIT;
            a->timer = 0;
        }
        return;
    }
    if(a->state == STAR_WAIT && a->timer > FLIGHT_BIND_TIME){
        a->velocity.z &= ~STARF_REWAIT;
        if(flight_branch_actor(a)) stream_launch_ready_set(a->stage_index, true);
    }
    if(a->state != STAR_WAIT || p->action == MA_DEATH) return;

    bool takeover = flight_takeover_accepts(a);
    if((p->action == MA_STAR_LAUNCH || g_flight.phase) && !takeover) return;

    v3 body_delta = v3_sub(v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT)), a->position);
    fx bind_radius = FX(300) + C_BODY_RADIUS;
    if(v3_length_squared64(body_delta) > (uint64_t)((int64_t)bind_radius * bind_radius)) return;
    game->hint = HINT_LAUNCH;
    if(a->timer <= FLIGHT_BIND_TIME) return;
    if(game->keys.pressed & KEY_B){
        flight_bind(a, false);
        return;
    }
    bool pull = flight_arg(a, 2, 1) != 0 && !(a->velocity.z & STARF_NO_PULL);
    if(pull && v3_length_squared64(d) < (uint64_t)FX(240) * FX(240)) flight_bind(a, true);
}

static void flight_turn_head(v3 dir, int32_t rate){
    Flight* f = &g_flight;
    v3 h = v3_blend14(f->head, dir, rate);
    f->front = v3_tangent14(v3_rotate_between14(f->front, f->head, h), h, f->front);
    f->head = h;
}

static void flight_pose_towards_star(int32_t rate){
    Flight* f = &g_flight;
    v3 h = v3_blend14(f->head0, f->init_up, rate);
    v3 fr = v3_blend14(f->front0, f->init_front, rate);
    f->head = h;
    f->front = v3_tangent14(fr, h, f->front);
}

static void flight_apply_pose(Figure* figure){
    Flight* f = &g_flight;
    if(!f->phase) return;
    figure->basis_up = f->head;
    figure->basis_front = f->front;

    figure->local_yaw = -f->roll;
    figure->local_pitch = -f->flip;
    figure->transform_dirty = true;
}

static void flight_set_mario(void){
    Flight* f = &g_flight;
    Player* p = &g_game.player;
    p->figure.position = f->pos;
    g_mario.position16 = V3(f->pos.x << 8, f->pos.y << 8, f->pos.z << 8);
}

static void flight_bind(Actor* a, bool capture){
    Flight* f = &g_flight;
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    bool sling = a->type == OBJ_SLINGSTAR;
    v3 last_move = V3(m->jump_vec.x >> 8, m->jump_vec.y >> 8, m->jump_vec.z >> 8);
    v3 head0 = p->up, front0 = p->front;
    if(f->phase){
        last_move = f->vel;
        front0 = v3_rotate_axis14(f->front, f->head, f->roll);
        v3 right = v3_cross14(front0, f->head);
        head0 = v3_rotate_axis14(f->head, right, -f->flip);
        front0 = v3_rotate_axis14(front0, right, -f->flip);
        if(f->star >= 0 && f->star < g_game.actor_count){
            Actor* previous = &g_game.actors[f->star];
            if(previous->active && previous->stage_index == f->star_object){
                previous->state = STAR_COOLDOWN;
                previous->timer = 0;
                previous->velocity.z |= STARF_REWAIT;
                stream_launch_ready_set(previous->stage_index, false);
            }
        }
        camera_event_cancel();
        audio_stop_level_effect(SFX_SPACEFLY);
    }
    *f = (Flight){0};
    if(!flight_setup_path(a)){
        if(!sling) return;
        f->init_up = a->up;
        f->init_front = a->front;
        f->init_side = v3_cross14(a->front, a->up);
        f->speed = FX(flight_arg(a, 0, 40));
    }

    gcapture_cancel_for_launch();
    const StageObject* o = a->stage_index >= 0 ? &g_game.stage.objects[a->stage_index] : NULL;
    f->sling = sling;
    f->star = (int16_t)(a - g_game.actors);
    f->star_object = a->stage_index;
    f->flight_time = f->path ? flight_arg(a, 0, 300) : 0;
    if(f->path && a->stage_index < 0 && !a->args[0]) f->flight_time = 300;
    f->camera = o && o->camera >= 0 ? o->camera : -1;
    f->cameras = flight_arg(a, 3, 1);
    f->pos = p->figure.position;

    f->vel = last_move;
    if(v3_length_squared64(f->vel) > (uint64_t)FX(40) * FX(40))
        f->vel = v3_set_length(f->vel, FX(40), p->front);
    f->head = f->head0 = head0;
    f->front = f->front0 = front0;
    f->phase = capture ? FLIGHT_CAPTURE : FLIGHT_SHOOT_START;
    f->step = 0;
    a->state = STAR_BOUND;
    a->timer = 0;
    a->velocity.z |= STARF_REWAIT;
    stream_launch_ready_set(a->stage_index, false);
    m->jumping = true;
    m->jump_vec = V3(0, 0, 0);
    m->spin_timer = 0;
    m->no_control_timer = 0;
    mario_set_action(MA_STAR_LAUNCH);

    if(!sling) camera_event_start(f->camera, f->cameras, f->start, f->init_up, f->init_front);
}

static Actor* flight_star_actor(void){
    Flight* f = &g_flight;
    if(f->star < 0 || f->star >= g_game.actor_count) return NULL;
    Actor* a = &g_game.actors[f->star];
    return a->active && a->stage_index == f->star_object ? a : NULL;
}

static void flight_cancel(void){
    Flight* f = &g_flight;
    audio_stop_level_effect(SFX_SPACEFLY);
    Actor* a = flight_star_actor();
    if(a){
        a->state = STAR_WAIT;
        a->timer = 0;
        a->velocity.z |= STARF_NO_PULL;
        stream_launch_ready_set(a->stage_index, false);
    }
    f->phase = FLIGHT_OFF;
    camera_event_cancel();
    mario_take_off(JUMP_FIRST, V3(f->vel.x << 8, f->vel.y << 8, f->vel.z << 8), true);
    g_mario.hold = false;
    mario_set_action(MA_FALL);
    mario_set_anim(ANIM_MARIO_FALL);
}

static void flight_motion_times(void){
    Flight* f = &g_flight;
    int t = f->flight_time;
    f->roll_total = 0;
    if(f->sling){
        if(t < FLIGHT_BCK_END + 20){
            f->t_loop = -1;
            f->t_end = 0;
            f->t_fall = t;
        } else {
            f->t_end = t - (FLIGHT_BCK_END + 20);
            f->t_fall = t - 20;
            f->t_loop = f->t_end / 5;
        }
        return;
    }
    if(t < 20){
        f->t_loop = -1;
        f->t_end = -1;
        f->t_fall = 0;
    } else if(t < 20 + FLIGHT_BCK_END){
        f->t_loop = -1;
        f->t_end = 0;
        f->t_fall = f->starter ? -1 : t - 20;
    } else {
        f->t_end = t - (20 + FLIGHT_BCK_END);
        f->t_fall = t - 20;
        int tmp = t / 5;
        if(tmp > 90) tmp = 90;
        f->t_loop = tmp / FLIGHT_BCK_START * FLIGHT_BCK_START;
        if(f->t_end <= f->t_loop) f->t_loop = 0;
        int turns = (f->t_end - f->t_loop) * 50 / 6283;
        f->roll_total = turns * FX_TAU;
    }
}

static int32_t flight_norm(int step, int a, int b){
    if(b <= a) return step >= b ? N14_ONE : 0;
    return fx_clamp((int32_t)(((step - a) << 14) / (b - a)), 0, N14_ONE);
}

static void flight_release(void){
    Flight* f = &g_flight;
    audio_stop_level_effect(SFX_SPACEFLY);
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    Actor* a = flight_star_actor();
    if(a){
        a->state = STAR_COOLDOWN;
        a->timer = 0;
        stream_launch_ready_set(a->stage_index, false);
    }
    f->phase = FLIGHT_OFF;
    if(f->starter) scenario_starter_released();
    f->landing = f->sling ? LANDING_SLING_STAR : LANDING_LAUNCH_STAR;
    flight_set_mario();
    p->front = v3_tangent14(f->front, p->up, p->front);

    v3 vel = f->vel;
    if(f->path){
        int32_t vert = v3_dot14(vel, p->up);
        v3 side = v3_sub(vel, v3_scale14(p->up, vert));
        if(v3_length_squared64(side) > (uint64_t)FLIGHT_RELEASE_SIDE * FLIGHT_RELEASE_SIDE)
            side = v3_set_length(side, FLIGHT_RELEASE_SIDE, p->front);
        vel = v3_add(side, v3_scale14(p->up, vert));
    }
    mario_take_off(JUMP_POWER, V3(vel.x << 8, vel.y << 8, vel.z << 8), true);
    m->spin_used = false;
    m->chainable = false;
    m->no_control_timer = 0x7FFF;
    mario_set_action(MA_FALL);
    camera_event_end_at_landing();
}

static void flight_tick(void){
    Flight* f = &g_flight;
    Actor* star = flight_star_actor();
    const KeyState* keys = &g_game.keys;
    if(f->phase == FLIGHT_OFF){
        mario_set_action(MA_FALL);
        return;
    }

    mario_update_gravity();
    int step = f->step;
    switch(f->phase){
    case FLIGHT_CAPTURE: {
        if(step == 0){
            mario_restart_anim(ANIM_MARIO_SPINDRIVERWAIT);
            f->pos16 = V3(f->pos.x << 8, f->pos.y << 8, f->pos.z << 8);
            f->vel16 = V3(f->vel.x << 8, f->vel.y << 8, f->vel.z << 8);
        }
        f->pos16 = v3_add(f->pos16, f->vel16);
        v3 to = v3_sub(V3(f->start.x << 8, f->start.y << 8, f->start.z << 8), f->pos16);
        f->vel16 = v3_add(f->vel16, V3(to.x / 80, to.y / 80, to.z / 80));
        f->vel16 = V3(f->vel16.x * 4 / 5, f->vel16.y * 4 / 5, f->vel16.z * 4 / 5);
        f->pos = V3(f->pos16.x >> 8, f->pos16.y >> 8, f->pos16.z >> 8);
        f->vel = V3(f->vel16.x >> 8, f->vel16.y >> 8, f->vel16.z >> 8);
        int span = f->sling ? 40 : 60;
        flight_pose_towards_star(fx_min((step << 14) / span, N14_ONE));
        if(star) star->velocity.y += f->sling ? STAR_SPIN(0.008f) : STAR_SPIN(0.004f);
        if(keys->pressed & KEY_B){
            f->phase = FLIGHT_SHOOT_START;
            f->step = 0;
            flight_set_mario();
            return;
        }
        if(step > span && v3_length_squared64(v3_sub(f->pos, f->start)) < (uint64_t)FX(15) * FX(15)){
            flight_set_mario();
            flight_cancel();
            return;
        }
        break;
    }
    case FLIGHT_SHOOT_START:
        if(!f->sling){
            if(step == 0){
                f->from = f->pos;
                f->head = f->init_up;
                f->front = f->init_front;
                mario_restart_anim(ANIM_MARIO_LAUNCHSTART);
                audio_play_effect(SFX_SPIN, 3);
            }
            int32_t r = fx_min((step << 14) / 15, N14_ONE);
            f->pos = v3_add(f->from, v3_scale14(v3_sub(f->start, f->from), r));
            if(star) star->velocity.y = star->velocity.y * 4 / 5;
            if(step > 45){
                f->phase = FLIGHT_SHOOT;
                f->step = 0;
                flight_set_mario();
                return;
            }
        } else {
            if(step == 0){
                f->from = f->pos;
                f->head0 = f->head;
                f->front0 = f->front;
                mario_restart_anim(ANIM_MARIO_SPIN);
                audio_play_effect(SFX_SPIN, 3);
                camera_event_start(f->camera, f->cameras, f->start, f->init_up, f->init_front);
            }
            int32_t pr = fx_min((step << 14) / 20, N14_ONE);
            int32_t h = 2 * (pr - N14_ONE / 2);
            int32_t v = mul14(mul14(h, h), mul14(h, h));
            v3 target =
                v3_sub(f->start, v3_scale14(f->init_up, (fx)(((int64_t)FX(200) * (N14_ONE - v)) >> 14)));
            int32_t w = fx_min(pr * 2, N14_ONE);
            f->pos = v3_add(f->from, v3_scale14(v3_sub(target, f->from), w));
            flight_pose_towards_star(w);
            if(star) star->velocity.y += STAR_SPIN(0.04f);
            if(step > 20){
                f->phase = FLIGHT_SHOOT;
                f->step = 0;
                flight_set_mario();
                return;
            }
        }
        break;
    case FLIGHT_SHOOT: {
        if(step == 0){
            flight_motion_times();
            mario_restart_anim(f->sling ? ANIM_MARIO_SPACEFLYSHORTSTART : ANIM_MARIO_SPACEFLYSTART);
            if(!f->starter){
                audio_play_effect(SFX_LAUNCH, 5);
                camera_shake(f->sling ? 1 : 2);
            }
            if(!f->path){
                f->vel = v3_scale14(f->init_up, f->speed);
                flight_release();
                return;
            }
        }
        if(step < f->t_fall) audio_level_effect(SFX_SPACEFLY);
        else audio_stop_level_effect(SFX_SPACEFLY);
        if(f->starter) scenario_starter_tick(step, f->flight_time);
        v3 prev = f->pos;
        int32_t t = (int32_t)(((int64_t)step << 14) / f->flight_time);
        if(!f->sling) t = fx_min(t, N14_ONE);
        f->pos = flight_path_at(t);
        v3 dir = flight_path_dir(fx_min(t, N14_ONE), f->head);
        f->vel = v3_sub(f->pos, prev);
        if(!f->sling){
            flight_turn_head(dir, R14(0.4f));
            if(step >= f->t_loop && step <= f->t_end && f->t_end > f->t_loop){
                int32_t n = flight_norm(step, f->t_loop, f->t_end);
                f->roll =
                    (fx)(((int64_t)f->roll_total * (fx_sin(mul14(n, FXC(1.5707963f) << 6) >> 6) << 6)) >> 14);
            }

            f->flip = (fx)(((int64_t)FX_PI * flight_norm(step, f->t_end, f->t_fall)) >> 14);
            if(step == f->t_loop) mario_set_anim(ANIM_MARIO_SPACEFLY);
            if(step == f->t_end) mario_set_anim(ANIM_MARIO_SPACEFLYEND);
            if(step == f->t_fall) mario_set_anim(ANIM_MARIO_FALL);
        } else {
            v3 d = v3_direction14(f->vel, f->head);
            int32_t r = flight_norm(step, f->t_end, f->t_fall);
            if(r > 0) d = v3_rotate_axis14(d, f->init_side, (fx)(((int64_t)FX_PI * r) >> 14));
            flight_turn_head(d, R14(0.4f));
            if(step == f->t_loop) mario_set_anim(ANIM_MARIO_SPACEFLYSHORT);
            if(step == f->t_end) mario_set_anim(ANIM_MARIO_SPACEFLYEND);
            if(step == f->t_fall) mario_set_anim(ANIM_MARIO_FALL);
        }
        camera_event_target(f->pos, dir, f->front);
        flight_set_mario();

        if(f->sling ? step > f->flight_time : step >= f->flight_time){
            flight_release();
            return;
        }
        break;
    }
    }
    flight_set_mario();
    f->step++;
}

static bool flight_start_scenario(int object){
    const StageObject* o = &g_game.stage.objects[object];
    if(o->rail < 0 || rail_point_count(o->rail) < 2) return false;
    Flight* f = &g_flight;
    *f = (Flight){.star = -1,
                  .star_object = object,
                  .starter = true,
                  .phase = FLIGHT_SHOOT,
                  .path = 2,
                  .rail = o->rail,
                  .flight_time = o->args[0] > 0 ? o->args[0] : 300};
    f->start = f->pos = rail_position_at(f->rail, 0);
    f->init_up = flight_path_dir(0, g_game.player.up);
    f->init_front =
        v3_tangent14(v3_direction14(v3_sub(flight_path_at(N14_ONE), f->start), g_game.player.front),
                     f->init_up, g_game.player.front);
    if(o->args[5] > 0)
        f->init_front = v3_rotate_axis14(f->init_front, f->init_up, (fx)(o->args[5] * 1144 / 256));
    f->init_side = v3_cross14(f->init_front, f->init_up);
    f->head = f->head0 = f->init_up;
    f->front = f->front0 = f->init_front;
    f->camera = o->camera;
    f->cameras = o->args[3] > 0 ? o->args[3] : 1;
    g_mario.jumping = true;
    g_mario.jump_vec = V3(0, 0, 0);
    g_mario.spin_timer = 0;
    mario_set_action(MA_STAR_LAUNCH);
    flight_set_mario();
    camera_event_start(f->camera, f->cameras, f->start, f->init_up, f->init_front);
    return true;
}

static bool flight_land(void){
    Flight* f = &g_flight;
    MarioPhysics* m = &g_mario;
    if(f->landing == LANDING_NONE) return false;
    int landing = f->landing;
    f->landing = LANDING_NONE;
    f->gravity_turn = 0;
    m->no_control_timer = 0;
    camera_event_landed();
    if(landing != LANDING_LAUNCH_STAR) return false;

    mario_stop_walk();
    mario_set_action(MA_LAND);
    mario_restart_anim(ANIM_MARIO_LANDSTIFFEN);
    audio_play_effect(SFX_LAND, 2);
    camera_shake(1);
    return true;
}

static void flight_water_landed(void){
    if(g_flight.landing == LANDING_NONE) return;
    g_flight.landing = LANDING_NONE;
    g_flight.gravity_turn = 0;
    g_mario.no_control_timer = 0;
    camera_event_landed();
}

static inline bool flight_input_locked(void){ return g_flight.landing != LANDING_NONE; }

static inline bool flight_free_sling_power_jump(void){
    return g_flight.phase == FLIGHT_OFF && g_flight.landing == LANDING_SLING_STAR && !g_flight.path &&
           g_mario.jumping && g_mario.jump_type == JUMP_POWER;
}

static __attribute__((noinline)) v3 flight_sling_gravity(v3 previous_up, v3 sampled_up, v3* jump_vec){
    Flight* f = &g_flight;

    GravitySample feet = gravity_sample(g_game.stage.header, g_game.player.figure.position);
    if(feet.index >= 0 && v3_dot14(sampled_up, feet.up) < R14(0.9f) &&
       v3_dot14(previous_up, feet.up) > v3_dot14(previous_up, sampled_up))
        sampled_up = feet.up;
    if(f->gravity_turn){
        sampled_up = previous_up;
    } else if(v3_dot14(previous_up, sampled_up) <= R14(0.309016994f)){
        *jump_vec = v3_scale14(horizontal(*jump_vec, previous_up), R14(0.5f));
        f->gravity_turn = 15;
    }

    if(f->gravity_turn) f->gravity_turn--;
    return sampled_up;
}

static __attribute__((noinline)) int32_t flight_sling_gravity_scale(v3 up, v3* jump_vec){
    if(!g_flight.gravity_turn) return N14_ONE;
    Player* p = &g_game.player;

    fx height =
        p->has_floor_below ? fx_max(0, v3_dot14(v3_sub(p->figure.position, p->floor_point), up)) : FX(900);
    if(height >= FX(300)) return R14(0.3f);
    if(height >= FX(200)) return 0;
    *jump_vec = horizontal(*jump_vec, up);
    return -R14(0.1f);
}

static void flight_reset(void){
    audio_stop_level_effect(SFX_SPACEFLY);
    g_flight = (Flight){0};
    g_flight.star = -1;
}
