static void stinkbug_small_think(Actor* a){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    const FigureHeader* fh = (const FigureHeader*)figure_stinkbugsmall;
    fx radius = FX(a->args[1]);
    fx half_angle = FX(a->args[0]) * 314 / 18000;
    v3 to_mario = v3_sub(p->figure.position, a->position);
    fx up_d = v3_dot14(to_mario, a->up);
    v3 side = v3_cross14(a->up, a->front);
    fx side_d = v3_dot14(to_mario, side);
    bool done = a->timer >= anim_length_of(fh, a->anim);
    if(a->state == STINKBUG_SMALL_DOWN){
        if(done){
            a->active = 0;
            for(int k = 0; k < 3; k++){
                Actor* bit = actor_spawn(OBJ_STARBIT, v3_add(actor_centre(a), v3_scale14(a->up, FX(40 * k))),
                                         a->up, a->front);
                if(bit){
                    bit->flags |= 2;
                    bit->figure.model = (const FigureHeader*)starbit_figures[(actor_index(a) + k) % 6];
                }
            }
        }
        return;
    }
    uint64_t d2 = v3_length_squared64(to_mario);
    fx terr = radius + FX(400);
    bool in_territory =
        d2 < (uint64_t)terr * terr && fx_abs(up_d) <= FX(200) && fx_abs(side_d) <= FX(600) &&
        (a->args[0] >= 180 || (a->args[0] == 0 ? v3_dot14(to_mario, a->front) > 0
                                               : angle_between14(v3_tangent14(to_mario, a->up, a->front),
                                                                 a->target) < half_angle));
    switch(a->state){
    case STINKBUG_SMALL_WAIT:
        a->position = a->home;
        if(in_territory) actor_set_state_anim(a, STINKBUG_SMALL_DASHSIGN, ANIM_STINKBUGSMALL_RUNSTART);
        else if(a->args[0] != 0 && a->timer > 1)
            actor_set_state_anim(a, STINKBUG_SMALL_SEARCH, ANIM_STINKBUGSMALL_SEARCH);
        break;
    case STINKBUG_SMALL_SEARCH: {
        a->position = a->home;
        fx step = (a->flags & 4) ? FXC(0.01745f) : -FXC(0.01745f);
        a->front = v3_tangent14(v3_rotate_axis14(a->front, a->up, step), a->up, a->front);
        if(a->args[0] < 180 && angle_between14(a->front, a->target) > half_angle){
            a->flags ^= 4;
            actor_set_state_anim(a, STINKBUG_SMALL_WAIT, ANIM_STINKBUGSMALL_SEARCH);
        } else if(in_territory) actor_set_state_anim(a, STINKBUG_SMALL_DASHSIGN, ANIM_STINKBUGSMALL_RUNSTART);
        break;
    }
    case STINKBUG_SMALL_DASHSIGN: {
        a->position = a->home;
        v3 want = v3_tangent14(to_mario, a->up, a->front);
        fx ang = angle_between14(a->front, want);
        fx step = FXC(0.05236f);
        if(ang <= step) a->front = want;
        else {
            fx sign = v3_dot14(v3_cross14(a->front, want), a->up) >= 0 ? 1 : -1;
            a->front = v3_tangent14(v3_rotate_axis14(a->front, a->up, step * sign), a->up, a->front);
        }
        if(done) actor_set_state_anim(a, STINKBUG_SMALL_DASHSIGNEND, ANIM_STINKBUGSMALL_RUNSTART);
        break;
    }
    case STINKBUG_SMALL_DASHSIGNEND:
        if(a->timer >= 10){
            actor_set_state_anim(a, STINKBUG_SMALL_DASH, ANIM_STINKBUGSMALL_RUSH);
            audio_play_effect(SFX_ENEMY_HIT, 1);
        }
        break;
    case STINKBUG_SMALL_DASH: {
        v3 goal = v3_add(a->home, v3_scale14(a->front, radius));
        v3 to_goal = v3_sub(goal, a->position);
        int32_t left = v3_length_exact(to_goal);
        if(left <= FX(1) || !actor_stick_to_ground(a)){
            actor_set_state_anim(a, STINKBUG_SMALL_DASHEND, ANIM_STINKBUGSMALL_RUSHSTOP);
            break;
        }
        a->position = v3_add(a->position, v3_scale14(a->front, left < FX(20) ? left : FX(20)));
        break;
    }
    case STINKBUG_SMALL_DASHEND:
        if(done) actor_set_state_anim(a, STINKBUG_SMALL_BACK, ANIM_STINKBUGSMALL_BACK);
        break;
    case STINKBUG_SMALL_BACK: {
        v3 to_home = v3_sub(a->home, a->position);
        int32_t left = v3_length_exact(to_home);
        if(left <= FX(10)){
            a->position = a->home;
            actor_set_state_anim(a, STINKBUG_SMALL_WAIT, ANIM_STINKBUGSMALL_SEARCH);
            break;
        }
        a->position =
            v3_add(a->position, v3_scale14(v3_direction14(to_home, a->front), left < FX(5) ? left : FX(5)));
        actor_stick_to_ground(a);
        break;
    }
    case STINKBUG_SMALL_ATTACK:
    case STINKBUG_SMALL_SPIN:
    case STINKBUG_SMALL_SHAKE:
        if(done) actor_set_state_anim(a, STINKBUG_SMALL_BACK, ANIM_STINKBUGSMALL_BACK);
        break;
    case STINKBUG_SMALL_PANIC:
        if(a->timer >= 90){
            actor_set_state_anim(a, STINKBUG_SMALL_SHAKE, ANIM_STINKBUGSMALL_REPEL);
            if(v3_dot14(to_mario, a->up) > FX(60) && actor_touches_mario(a, FX(30))){
                mario_damage(a->position, 0);
            }
        }
        break;
    }
    if(a->state == STINKBUG_SMALL_PANIC || a->state == STINKBUG_SMALL_SHAKE) return;
    bool body = actor_touches_mario(a, 0);
    if(!body && mario_spin_reaches(a) && a->state != STINKBUG_SMALL_DASH && a->state != STINKBUG_SMALL_SPIN){
        actor_set_state_anim(a, STINKBUG_SMALL_SPIN, ANIM_STINKBUGSMALL_SPINACTION);
        audio_play_effect(SFX_ENEMY_HIT, 2);
        return;
    }
    if(!body) return;
    if(p->action == MA_HIPDROP){
        actor_set_state_anim(a, STINKBUG_SMALL_DOWN, ANIM_STINKBUGSMALL_DEATH);
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
        return;
    }
    bool from_above = m->jumping && vertical_speed(m->jump_vec, p->up) < 0 && up_d > FX(60);
    if(from_above){
        mario_stomp_bounce(g_game.keys.held & KEY_A);
        actor_set_state_anim(a, STINKBUG_SMALL_PANIC, ANIM_STINKBUGSMALL_LOSS);
        return;
    }
    if(m->attack_timer > 0 && a->state != STINKBUG_SMALL_DASH && a->state != STINKBUG_SMALL_SPIN){
        actor_set_state_anim(a, STINKBUG_SMALL_SPIN, ANIM_STINKBUGSMALL_SPINACTION);
        audio_play_effect(SFX_ENEMY_HIT, 2);
        return;
    }

    if(v3_dot14(to_mario, a->front) > 0 && a->state != STINKBUG_SMALL_ATTACK){
        mario_damage(a->position, 1);
        if(a->state != STINKBUG_SMALL_DASH)
            actor_set_state_anim(a, STINKBUG_SMALL_ATTACK, ANIM_STINKBUGSMALL_ATTACK);
    }
}

static bool stinkbug_small_view_bounds(const Figure* f, const CameraView* view){
    return view_sphere_visible(view, f->position, FX(320) * f->scale.x / 64);
}

static const FigureOps stinkbug_small_figure_ops = {.view_bounds = stinkbug_small_view_bounds};

static void stinkbug_small_star_hit(Actor* a){
    if(a->state == STINKBUG_SMALL_WAIT || a->state == STINKBUG_SMALL_SEARCH ||
       a->state == STINKBUG_SMALL_DASHSIGN || a->state == STINKBUG_SMALL_DASHSIGNEND ||
       a->state == STINKBUG_SMALL_BACK)
        actor_set_state_anim(a, STINKBUG_SMALL_SPIN, ANIM_STINKBUGSMALL_SPINACTION);
    audio_play_effect(SFX_ENEMY_HIT, 2);
}

static void stinkbug_small_spawned(Actor* a, const StageObject* o){
    a->target = a->front;
    actor_set_state_anim(a, STINKBUG_SMALL_WAIT, ANIM_STINKBUGSMALL_SEARCH);
}
