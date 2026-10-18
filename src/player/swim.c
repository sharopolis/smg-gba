static EWRAM_BSS SwimPhysics g_swim;
static const int16_t swim_stroke_acc[40] = {
    G(-.2), G(-.2), G(-.2), G(-.1), G(-.1), G(-.1), G(-.1), G(-.1), 0,     0,     0,     0,     0,     0,
    0,      0,      0,      G(.2),  G(.2),  G(.2),  G(.3),  G(.3),  G(.4), G(.5), G(.5), G(.5), G(.5), G(.5),
    G(.4),  G(.4),  G(.3),  G(.3),  G(.2),  G(.2),  G(.1),  G(.1),  G(.1), G(.1), G(.1), 0};
static const int16_t swim_spin_acc[30] = {
    0, 0, 0, 0, 0, G(8),  G(6),  G(4),  G(2),  G(1),  0,     0,     0,     0,     0,
    0, 0, 0, 0, 0, G(-1), G(-1), G(-1), G(-1), G(-1), G(-1), G(-1), G(-1), G(-1), G(-1)};
static void mario_swim_reset(void){ g_swim = (SwimPhysics){0}; }
static int mario_swim_life(void){ return g_swim.active && g_swim.surface_full < 60 ? g_swim.oxygen : -1; }
static void mario_swim_recover(void){
    if(!g_swim.active) return;
    g_swim.oxygen = fx_min(3600, g_swim.oxygen + 450);
    g_swim.oxygen_delay = 120;
    g_swim.drown = 0;
}

static int32_t mario_swim_body_sin14(int32_t a){
    if(a > R14(1.5707963268f)) a = R14(3.1415926536f) - a;
    int32_t x2 = (a * a) >> 14;
    int32_t q = 2959;
    q = -213044 + (int32_t)(((int64_t)x2 * q) >> 14);
    q = 8947849 + (int32_t)(((int64_t)x2 * q) >> 14);
    q = -178956971 + (int32_t)(((int64_t)x2 * q) >> 14);
    q = (int32_t)(((int64_t)x2 * q) >> 14);
    return a + (int32_t)(((int64_t)a * q) >> 30);
}
static int32_t mario_swim_body_angle14(v3 from, v3 to){
    int32_t y = v3_length_exact(v3_cross14(from, to));
    int32_t x = v3_dot14(from, to), ax = fx_abs(x);
    if(!y) return x < 0 ? R14(3.1415926536f) : 0;
    bool swap = y > ax;
    int32_t num = swap ? ax : y, den = swap ? y : ax;
    int32_t z = (int32_t)(((uint32_t)num << 14) / (uint32_t)den);
    int32_t a = mul14(z, R14(.78539816f)) + mul14(mul14(z, R14(.2732f)), N14_ONE - z);
    if(swap) a = R14(1.5707963f) - a;
    if(x < 0) a = R14(3.14159265f) - a;
    return a;
}
__attribute__((noinline)) static v3 mario_swim_body_blend14(v3 from, v3 to){
    int32_t a = mario_swim_body_angle14(from, to);
    if(a < R14(.1f)) return v3_blend14(from, to, R14(.1f));

    if(a == R14(3.1415926536f)) return from;
    int32_t w_to = mario_swim_body_sin14(a / 10);
    int32_t w_from = mario_swim_body_sin14(a - a / 10);

    return v3_direction14(v3_add(v3_scale14(from, w_from), v3_scale14(to, w_to)), from);
}
__attribute__((noinline)) static bool mario_swim_body_heading_projected(v3* heading_out){
    Player* p = &g_game.player;

    const int64_t d =
        (int64_t)p->front.x * p->up.x + (int64_t)p->front.y * p->up.y + (int64_t)p->front.z * p->up.z;
    v3 heading = V3((int32_t)(((int64_t)p->front.x * 268435456 - (int64_t)p->up.x * d) / 16384),
                    (int32_t)(((int64_t)p->front.y * 268435456 - (int64_t)p->up.y * d) / 16384),
                    (int32_t)(((int64_t)p->front.z * 268435456 - (int64_t)p->up.z * d) / 16384));

    if(fx_abs(heading.x) <= 268435 && fx_abs(heading.y) <= 268435 && fx_abs(heading.z) <= 268435)
        return false;
    *heading_out = v3_direction14(heading, p->front);
    return true;
}
__attribute__((noinline)) static bool mario_swim_body_target(v3* side, v3* target){
    Player* p = &g_game.player;
    SwimPhysics* s = &g_swim;
    v3 heading;
    if(!mario_swim_body_heading_projected(&heading)) return false;
    *side = v3_direction14(v3_cross14(p->up, heading), V3(-N14_ONE, 0, 0));
    v2 sc = fx_sin_cos(s->pitch);
    *target = v3_direction14(v3_add(v3_scale14(heading, sc.y * 64), v3_scale14(p->up, -sc.x * 64)), heading);
    return true;
}
__attribute__((noinline)) static void mario_swim_body_finish(v3 side, v3 front){
    Player* p = &g_game.player;
    SwimPhysics* s = &g_swim;
    v3 head = v3_cross14(front, side);
    if(v3_length_squared64(head) < 64u * 64u) return;
    head = v3_direction14(head, p->gravity_up);
    s->body_up[0] = (int16_t)head.x;
    s->body_up[1] = (int16_t)head.y;
    s->body_up[2] = (int16_t)head.z;
    v3 next = v3_direction14(v3_cross14(side, head), v3_from_i16(s->body_front));
    s->body_front[0] = (int16_t)next.x;
    s->body_front[1] = (int16_t)next.y;
    s->body_front[2] = (int16_t)next.z;
}

__attribute__((noinline)) static v3 mario_swim_body_transport_heading(v3 from, v3 side){
    v3 old_side = v3_cross14(v3_from_i16(g_swim.body_up), from);
    if(v3_dot14(old_side, side) >= 0) return from;
    v3 up = g_game.player.up;
    v3 old_heading = v3_direction14(v3_cross14(old_side, up), g_game.player.front);
    v3 heading = v3_direction14(v3_cross14(side, up), g_game.player.front);
    return v3_direction14(
        v3_add(v3_scale14(heading, v3_dot14(from, old_heading)), v3_scale14(up, v3_dot14(from, up))), from);
}
__attribute__((noinline)) static void mario_swim_body_tick(void){
    if(!g_swim.active || g_game.player.action != MA_SWIM) return;
    bool immediate = g_swim.body_immediate;
    g_swim.body_immediate = 0;
    v3 side, target;
    if(!mario_swim_body_target(&side, &target)) return;
    v3 from = v3_from_i16(g_swim.body_front);
    if(!immediate) from = mario_swim_body_transport_heading(from, side);
    v3 front = immediate ? target : mario_swim_body_blend14(from, target);
    mario_swim_body_finish(side, front);
}
static void mario_swim_apply_pose(Figure* figure){
    figure->basis_front = v3_from_i16(g_swim.body_front);
    figure->basis_up = v3_from_i16(g_swim.body_up);
    figure->local_pitch = 0;
    figure->rotation_pivot_height = 0;
}
static v3 mario_swim_camera_front(v3 front, v3 up){
    v2 sc = fx_sin_cos(g_swim.pitch);
    return v3_add(v3_scale14(front, sc.y * 64), v3_scale14(up, -sc.x * 64));
}

static bool mario_swim_ground_is_slip(uint8_t code, v3 normal){
    int dot = v3_dot14(normal, g_game.player.gravity_up);
    if(code == 2) return dot <= 16362;
    if(code == 3 || code == 13 || code == 20 || code == 21 || code == 22 || code == 30) return false;
    return dot <= 11701;
}
static void mario_swim_exit_ground(bool power_jump){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    g_swim.active = 0;
    p->water_state = WATER_DRY;
    p->figure.local_pitch = 0;

    mario_start_anim(ANIM_GROUP_SINGLE, ANIM_MARIO_SWIMLANDEXIT, -1, 16);
    audio_play_effect(SFX_SWIM_WALK, 0);
    if(power_jump){
        v3 displacement = v3_add(v3_scale14(p->front, G(5)), v3_scale14(p->gravity_up, G(15)));
        mario_take_off(13, displacement, true);
        m->ratio = m->stick;
        m->chainable = false;
        m->no_control_timer = 0;
        m->spin_timer = 0;
        m->spin_used = false;
        m->jump_buffer = 0;
        p->was_on_ground = false;
        p->ground_normal = p->gravity_up;
        mario_set_action(MA_FALL);
        m->jump_vec = v3_sub(displacement, v3_scale14(p->gravity_up, G(.9)));
        m->air_frames = 1;
        mario_move_and_collide(displacement, false);
        audio_play_effect(SFX_SWIM_JUMP, 2);
        return;
    }
    p->figure.position = p->floor_point;
    m->position16 = V3(p->floor_point.x << 8, p->floor_point.y << 8, p->floor_point.z << 8);
    m->jump_vec = V3(0, 0, 0);
    m->jumping = false;
    m->hold = false;
    m->chainable = false;
    mario_stop_walk();
    p->on_ground = true;
    p->was_on_ground = true;
    mario_set_action(MA_LAND);
}
static bool mario_swim_try_ground(void){
    Player* p = &g_game.player;

    if(!p->has_floor_below || p->height_above_ground < 0) return false;
    if(water_query(v3_add(p->floor_point, v3_scale14(p->up, 80)), p->gravity_up, 0)) return false;
    if(!p->on_ground){
        RayHit floor;
        v3 from = v3_add(p->figure.position, v3_scale14(p->up, 10));
        v3 to = v3_sub(p->figure.position, v3_scale14(p->up, 900));
        if(!collision_ray(from, to, &floor, 0)) return false;
        p->floor_point = floor.point;
        p->ground_normal = floor.normal;
        p->ground_attr = floor.attr;
        p->ground_code = col_tri(floor.tri)->floor_code;
        p->height_above_ground = v3_dot14(v3_sub(p->figure.position, floor.point), p->up);
    }
    bool power = v3_length_squared64(v3_sub(p->floor_point, p->figure.position)) >= 10000 ||
                 mario_swim_ground_is_slip(p->ground_code, p->ground_normal);
    mario_swim_exit_ground(power);
    return true;
}
static void mario_swim_jump_out(void){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    m->ratio = fx_clamp((int32_t)((int64_t)g_swim.speed * N14_ONE / G(10)), 0, N14_ONE);
    g_swim.active = 0;
    p->water_state = WATER_DRY;
    m->squat = false;
    m->skidding = false;
    m->turn_ready = 0;
    m->chainable = false;
    mario_jump();
    p->figure.local_pitch = 0;
    audio_play_effect(SFX_SWIM_JUMP, 2);
}

static void mario_swim_surface_animation(bool surface, fx depth){
    SwimPhysics* s = &g_swim;
    Player* p = &g_game.player;
    if(depth > 160){
        s->rise_armed = true;
        return;
    }
    if(!surface || depth > 33 || !s->rise_armed) return;
    s->rise_armed = false;
    if(s->dive || s->stroke || s->spin || s->spin_phase || s->surface_hold) return;
    if(p->anim_group == ANIM_GROUP_SINGLE && !mario_anim_done()) return;

    mario_start_anim(ANIM_GROUP_SINGLE, ANIM_MARIO_SWIMRISE, -1, 16);
}
static void mario_swim_basic_anim(bool surface, bool held_a){
    Player* p = &g_game.player;
    bool one_shot = p->anim_group == ANIM_GROUP_SINGLE &&
                    (p->anim == ANIM_MARIO_SWIMBREAST || p->anim == ANIM_MARIO_SWIMBREASTSURFACE ||
                     p->anim == ANIM_MARIO_SWIMDIVE || p->anim == ANIM_MARIO_SWIMSPIN ||
                     p->anim == ANIM_MARIO_SWIMSPINSURFACE || p->anim == ANIM_MARIO_SWIMSPINATTACK ||
                     p->anim == ANIM_MARIO_SWIMSPINATTACKSURFACE || p->anim == ANIM_MARIO_SWIMRISE ||
                     p->anim == ANIM_MARIO_LANDWATER || p->anim == ANIM_MARIO_LANDWATERDIVE);

    bool moving_rise = surface && p->anim == ANIM_MARIO_SWIMRISE && g_mario.stick > R14(.1f);
    if(one_shot && !mario_anim_done() && !moving_rise) return;

    int base = surface ? ANIM_MARIO_SWIMWAIT : ANIM_MARIO_SWIMDRIFT;
    int moving = surface ? ANIM_MARIO_SWIMFLUTTERSURFACE : ANIM_MARIO_SWIMFLUTTER;
    int target = (surface ? g_mario.stick : held_a) ? 256 : 0;
    int old = p->anim2_weight;
    if(p->anim_group != ANIM_GROUP_SWIM || p->anim != base){
        fx phase = p->anim_frame;
        bool continue_phase = p->anim_group == ANIM_GROUP_SWIM &&
                              p->figure.model == (const FigureHeader*)figure_mario &&
                              (p->anim == ANIM_MARIO_SWIMWAIT || p->anim == ANIM_MARIO_SWIMDRIFT) &&
                              phase >= 0 && phase < 120 * FX_ONE;
        mario_start_anim(ANIM_GROUP_SWIM, base, moving, 6);
        if(continue_phase){
            p->anim_frame = phase;
            p->anim2_frame = surface ? (phase * 3) / 4 : phase;
        }
    }

    int rate = surface && target ? 51 : 10;
    p->anim2_weight = old + ((target - old) * rate >> 8);
    if(fx_abs(target - p->anim2_weight) < 3) p->anim2_weight = target;
    p->anim_speed = target ? (surface ? FXC(1.8) : FXC(1.6)) : FX_ONE;
}
static bool mario_swim_tick(const KeyState* keys){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    SwimPhysics* s = &g_swim;
    WaterSample water, head;
    bool body_in = water_query(p->figure.position, p->gravity_up, &water);
    bool head_in = water_query(v3_add(p->figure.position, v3_scale14(p->gravity_up, s->active ? 80 : 110)),
                               p->gravity_up, &head);

    if(!s->active && m->jumping && vertical_speed(m->jump_vec, p->up) > 0) body_in = false;
    p->water_state = body_in ? (head_in ? WATER_SUBMERGED : WATER_SURFACE) : WATER_DRY;
    if(body_in && bee_active()) bee_end();
    if(!s->active){
        if(m->jumping && p->anim == ANIM_MARIO_SWIMLANDEXIT && (m->stick || mario_anim_done()))
            mario_set_anim(ANIM_MARIO_FALL);
        if(!head_in || !body_in || (m->jumping && vertical_speed(m->jump_vec, p->up) > 0)) return false;
        int32_t entering = vertical_speed(m->jump_vec, p->up);
        *s = (SwimPhysics){
            .active = 1,
            .oxygen = 3600,
            .oxygen_delay = 120,
            .buoyancy_delay = 120,
            .pitch = FX_PI / 2,
            .speed = fx_min((fx)isqrt64(v3_length_squared64(horizontal(m->jump_vec, p->up))), G(11.5))};
        s->body_front[0] = (int16_t)p->front.x;
        s->body_front[1] = (int16_t)p->front.y;
        s->body_front[2] = (int16_t)p->front.z;
        s->body_up[0] = (int16_t)p->up.x;
        s->body_up[1] = (int16_t)p->up.y;
        s->body_up[2] = (int16_t)p->up.z;
        flight_water_landed();
        mario_set_action(MA_SWIM);
        m->jump_buffer = 0;
        m->attack_timer = 0;
        mario_restart_anim(entering < -G(10) ? ANIM_MARIO_LANDWATERDIVE : ANIM_MARIO_LANDWATER);
    }
    if(!body_in){
        if(mario_swim_try_ground()) return true;
        s->active = 0;
        p->water_state = WATER_DRY;
        m->jumping = true;
        mario_set_action(MA_FALL);
        mario_set_anim(ANIM_MARIO_FALL);
        return false;
    }
    if(!head_in && mario_swim_try_ground()) return true;
    bool at_surface = water.depth < 80;

    bool climb_out =
        at_surface &&
        (((keys->pressed & KEY_A) && p->on_wall && v3_dot14(p->wall_normal, p->front) <= -11584) ||
         (keys->pressed & KEY_B));
    bool pad_dive = !climb_out && (keys->held & (KEY_UP | KEY_A)) == (KEY_UP | KEY_A);
    bool surface = at_surface && !(keys->held & KEY_R) && !pad_dive;

    if(at_surface && pad_dive){
        s->pitch = fx_max(s->pitch, FX_PI / 2 + FXC(.04));
        s->buoyancy = 0;
        s->buoyancy_delay = 50;
    }
    if(s->age < 30000) s->age++;
    if(surface){
        s->oxygen_delay = 120;
        s->oxygen = fx_min(3600, s->oxygen + 32);
        s->drown = 0;
    } else if(s->oxygen_delay > 0) s->oxygen_delay--;
    else if(s->oxygen > 0) s->oxygen--;
    else if(++s->drown >= 180){
        s->drown = 0;
        if(p->health > 0){
            int previous_health = p->health;
            p->health--;
            mario_lifeup_health_changed(previous_health);
        }
        p->invincible_timer = 120;
    }
    if(water.cold){
        if(++s->cold >= 120){
            s->cold = 0;
            if(p->health > 0){
                int previous_health = p->health;
                p->health--;
                mario_lifeup_health_changed(previous_health);
            }
            p->invincible_timer = 120;
        }
    } else s->cold = 0;
    if(p->health <= 0){
        s->active = 0;
        mario_set_action(MA_DEATH);
        mario_restart_anim(ANIM_MARIO_DIEOVER);
        audio_play_effect(SFX_DIE, 5);
        return true;
    }
    s->surface_full = surface && s->oxygen == 3600 ? fx_min(s->surface_full + 1, 60) : 0;
    if(surface){
        if(keys->held & KEY_A){
            if(s->surface_hold < 30) s->surface_hold++;

            if((keys->pressed & KEY_A) && p->on_wall && v3_dot14(p->wall_normal, p->front) <= -11584)
                s->surface_hold = 30;
        } else {
            if(s->surface_hold && s->surface_hold < 30 && !s->stroke){
                s->stroke = 40;
                mario_restart_anim(ANIM_MARIO_SWIMBREASTSURFACE);
            }
            s->surface_hold = 0;
        }
        if(s->age > 6 && (s->surface_hold >= 30 || (keys->pressed & KEY_B))){
            mario_swim_jump_out();
            return true;
        }
    } else {
        s->surface_hold = 0;
        if(keys->pressed & KEY_A && (!s->stroke || (p->anim_frame >> 8) >= 35)){
            s->stroke = 40;
            s->buoyancy = 0;
            if(!(keys->held & (KEY_UP | KEY_DOWN))) s->pitch = fx_max(s->pitch, FX_PI / 2);
            mario_restart_anim(ANIM_MARIO_SWIMBREAST);
        }
        if(keys->pressed & KEY_B && !s->spin && !s->spin_phase){
            s->spin_phase = 1;
            s->spin = 24;
            m->attack_timer = 40;
            mario_restart_anim(ANIM_MARIO_SWIMSPIN);
            audio_play_effect(SFX_SPIN, 1);
        }
    }
    if(s->spin_phase && p->anim == ANIM_MARIO_SWIMSPIN &&
       (p->anim_frame >= FX_ONE || keys->pressed & KEY_A)){
        s->spin_phase = 0;
        mario_restart_anim(ANIM_MARIO_SWIMSPINATTACK);
    }
    if(keys->pressed & KEY_R){
        s->dive = 35;
        mario_restart_anim(ANIM_MARIO_SWIMDIVE);
    }
    if(s->dive && --s->dive == 0){
        s->body_immediate = 1;
        s->pitch = FXC(3.1);
        s->speed = G(3);
        s->buoyancy_delay = 50;
    }
    int sy = ((keys->held & KEY_UP) != 0) - ((keys->held & KEY_DOWN) != 0);
    int sx = ((keys->held & KEY_RIGHT) != 0) - ((keys->held & KEY_LEFT) != 0);
    s->pitch_input = (int16_t)((s->pitch_input + sy * N14_ONE) / 2);
    s->turn_input = (int16_t)((s->turn_input * 3 + sx * N14_ONE * 2) / 5);
    if(surface){
        if(m->stick) p->front = v3_tangent14(m->move_dir, p->up, p->front);
    } else {
        int res = fx_clamp(N14_ONE - (int32_t)((int64_t)s->speed * N14_ONE / G(11.5)), 0, N14_ONE);
        fx yaw = mul14(mul14(FXC(.012), 2 * N14_ONE + res), s->turn_input);
        if(yaw) p->front = v3_tangent14(v3_rotate_axis14(p->front, p->up, -yaw), p->up, p->front);
    }
    if((surface && m->stick) || (!surface && keys->held & KEY_A))
        s->speed = (int32_t)(((int64_t)s->speed * R14(.985) + (int64_t)G(7.5) * (N14_ONE - R14(.985))) >> 14);
    else s->speed = (int32_t)((int64_t)s->speed * (surface ? R14(.98) : R14(.992)) >> 14);
    if(s->stroke > 0){
        int32_t ratio = R14(.1) + (int32_t)((int64_t)s->speed * R14(.9) / G(11.5));
        s->speed += mul14(swim_stroke_acc[40 - s->stroke--], mul14(ratio, R14(1.5)));
    }
    if(s->spin > 0){
        int32_t acc = swim_spin_acc[30 - s->spin--];
        s->speed += mul14(acc, acc < 0 ? R14(.1) : R14(.7));
    }
    s->speed = fx_max(0, fx_min(s->spin > 0 ? G(70) : G(11.5), s->speed));
    if(s->buoyancy_delay > 0) s->buoyancy_delay--;
    else s->buoyancy = fx_min(G(1), s->buoyancy + G(.1));

    int target = surface ? FXC(.5235988) : (keys->held & KEY_R) ? FXC(3.1) : FXC(1.0471976);
    if(!surface && !(keys->held & KEY_R)){
        if(sy > 0){
            int weight =
                fx_clamp(N14_ONE - (int32_t)((int64_t)(s->speed - G(2)) * N14_ONE / G(8)), 0, N14_ONE);
            target = mul14(FXC(2.0943952), weight) + mul14(FXC(3.14128), N14_ONE - weight);
        } else if(sy < 0) target = FXC(.0314159);
        int res = fx_clamp(N14_ONE - (int32_t)((int64_t)s->speed * N14_ONE / G(11.5)), 0, N14_ONE);
        s->pitch += mul14(mul14(FXC(.04), s->pitch_input), res);
        if(!sy && (keys->held & KEY_A)) target = s->pitch;
    }
    int rate = s->dive ? 38 : surface ? 26 : 13;
    s->pitch = fx_clamp(s->pitch + ((target - s->pitch) * rate >> 8), FXC(.0314159), FXC(3.14128));
    v2 sc = fx_sin_cos(s->pitch);
    int32_t horizontal_speed = (int32_t)((int64_t)s->speed * sc.x >> 8);
    if(keys->held & KEY_A) s->buoyancy = 0;
    int32_t vertical = (int32_t)((int64_t)s->speed * sc.y >> 8) + s->buoyancy;
    if(surface){
        horizontal_speed = s->speed;
        vertical = fx_clamp((water.depth - 30) * 256 / 4, -G(7.5), G(7.5));
    }
    m->jump_vec = v3_add(v3_scale14(p->front, horizontal_speed), v3_scale14(p->up, vertical));
    m->jumping = true;
    m->squat = false;
    m->ratio = 0;
    m->void_ticks = 0;
    mario_move_and_collide(m->jump_vec, surface);
    if(mario_swim_try_ground()) return true;

    if(p->on_ground){
        s->buoyancy = fx_max(s->buoyancy, 0);
        s->pitch = fx_min(s->pitch, FX_PI / 2);
    }
    p->on_ground = false;
    m->jumping = true;
    mario_set_action(MA_SWIM);
    mario_swim_surface_animation(surface, water.depth);
    mario_swim_basic_anim(surface, keys->held & KEY_A);
    return true;
}
