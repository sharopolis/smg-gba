static const AnimClip* figure_clips(const FigureHeader* f){
    return (const AnimClip*)((const uint8_t*)f + f->clips);
}

static int anim_length(int clip){
    const FigureHeader* f =
        g_game.player.figure.model ? g_game.player.figure.model : (const FigureHeader*)figure_mario;
    const AnimClip* c = &figure_clips(f)[clip];
    return c->loop ? c->key_count * c->ticks_per_key : (c->key_count - 1) * c->ticks_per_key + 1;
}

static int anim_length_of(const FigureHeader* f, int clip){
    const AnimClip* c = &figure_clips(f)[clip];
    return c->loop ? c->key_count * c->ticks_per_key : (c->key_count - 1) * c->ticks_per_key + 1;
}

static int anim_interpolation(int clip){
    switch(clip){
    case ANIM_MARIO_TURN:
    case ANIM_MARIO_JUMP:
    case ANIM_MARIO_JUMP2:
    case ANIM_MARIO_JUMPROLL:
    case ANIM_MARIO_LAND:
    case ANIM_MARIO_JUMP2LAND:
    case ANIM_MARIO_JUMPTURNLAND:
    case ANIM_MARIO_SPIN:
    case ANIM_MARIO_SPINGROUND:
    case ANIM_MARIO_HIPDROP:
    case ANIM_MARIO_WALLKEEP:
    case ANIM_MARIO_WALLJUMP:
    case ANIM_MARIO_STAGESTART:
    case ANIM_MARIO_SLEEPLOOP:
    case ANIM_MARIO_SLEEPLIELOOP:
    case ANIM_MARIO_ICESPIN:
    case ANIM_MARIO_ICESKATESPIN:
    case ANIM_MARIO_ICESPINAIR:
    case ANIM_MARIO_ICEJUMP:
    case ANIM_MARIO_ICEJUMP2:
    case ANIM_MARIO_ICEJUMP3:
    case ANIM_MARIO_ICEJUMPLAND:
    case ANIM_MARIO_ICEJUMPSTOPLAND:
    case ANIM_MARIO_FIRERUNSTART:
    case ANIM_MARIO_FREEZE:
    case ANIM_MARIO_ICEFLICK: return 0;
    case ANIM_MARIO_SPIN2ND: return 1;
    case ANIM_MARIO_HIPDROPSTART:
    case ANIM_MARIO_STAGGER: return 2;
    case ANIM_MARIO_BRAKE:
    case ANIM_MARIO_RUNEND: return 4;
    case ANIM_MARIO_WALLSLIDE: return 10;
    case ANIM_MARIO_FALL: return 15;
    case ANIM_MARIO_DIEFALL: return 20;
    default: return 6;
    }
}

static const void* figure_clip_pose(const FigureHeader* f, int clip, fx frame){
    const AnimClip* c = &figure_clips(f)[clip];
    int key = (frame >> 8) / c->ticks_per_key;
    if(c->loop) key %= c->key_count;
    else if(key >= c->key_count) key = c->key_count - 1;
    return model_keyframe(f, c->first_key + key);
}

static fx mario_clip_advance(int clip, fx frame, fx speed){
    const FigureHeader* f = g_game.player.figure.model;
    const AnimClip* c = &figure_clips(f)[clip];
    int length = c->loop ? c->key_count * c->ticks_per_key : (c->key_count - 1) * c->ticks_per_key;

    if(clip == ANIM_MARIO_SWIMFLUTTERSURFACE && f == (const FigureHeader*)figure_mario && c->loop &&
       c->key_count == 23 && c->ticks_per_key == 4)
        length = 90;
    frame += speed;
    if(c->loop){
        while(length > 0 && frame >= (length << 8)) frame -= length << 8;
    } else if(frame > (length << 8)){
        frame = length << 8;
    }
    return frame;
}

static void mario_anim_tick(void){
    Player* p = &g_game.player;
    if(cutscene_blocks(BLOCK_MARIO_ANIM)) return;
    p->anim_frame = mario_clip_advance(p->anim, p->anim_frame, p->anim_speed);

    if(p->anim2 >= 0){
        if(p->anim_group == ANIM_GROUP_SWIM && p->anim == ANIM_MARIO_SWIMWAIT &&
           p->anim2 == ANIM_MARIO_SWIMFLUTTERSURFACE && p->figure.model == (const FigureHeader*)figure_mario)
            p->anim2_frame = (p->anim_frame * 3) / 4;
        else p->anim2_frame = mario_clip_advance(p->anim2, p->anim2_frame, p->anim_speed);
    }
    if(p->anim_blend_ticks > 0) p->anim_blend_ticks--;
}

static void mario_clip_keys(const FigureHeader* f, int clip, fx frame, const void** a, const void** b,
                            int* t){
    const AnimClip* c = &figure_clips(f)[clip];
    int tpk = c->ticks_per_key;
    int key = (frame >> 8) / tpk;
    int span = tpk;
    if(clip == ANIM_MARIO_SWIMFLUTTERSURFACE && f == (const FigureHeader*)figure_mario && c->loop &&
       c->key_count == 23 && tpk == 4 && key == 22)
        span = 2;
    *t = ((frame - ((key * tpk) << 8)) / span);
    int next = key + 1;
    if(c->loop){
        key %= c->key_count;
        next %= c->key_count;
    } else {
        if(key >= c->key_count - 1){
            key = c->key_count - 1;
            *t = 0;
        }
        if(next >= c->key_count) next = c->key_count - 1;
    }
    *a = model_keyframe(f, c->first_key + key);
    *b = model_keyframe(f, c->first_key + next);
}

static bool mario_blink_hidden(const Player* p){
    return p->invincible_timer > 0 && (p->invincible_timer & 4) && p->action != MA_DAMAGE_FALL;
}

__attribute__((noinline)) static void mario_set_hidden(bool hidden){
    Player* p = &g_game.player;
    p->hidden = hidden;
    p->figure.hidden = hidden || mario_blink_hidden(p);
}

__attribute__((noinline)) static void mario_update_pose(void){
    Player* p = &g_game.player;

    if(g_game.scene == SCENE_PAUSE) return;

    if(g_display_full_res && g_pose_code_ready) model_invalidate_pose_cache();
    Figure* figure = &p->figure;
    const FigureHeader* f = figure->model;
    int point_count = f->point_count;

    const void *key_a, *key_b, *key_a2 = 0, *key_b2 = 0;
    int t, t2 = 0;
    int w2 = p->anim2 >= 0 ? p->anim2_weight : 0;
    if(w2 >= 256) mario_clip_keys(f, p->anim2, p->anim2_frame, &key_a, &key_b, &t);
    else mario_clip_keys(f, p->anim, p->anim_frame, &key_a, &key_b, &t);
    if(w2 > 0 && w2 < 256) mario_clip_keys(f, p->anim2, p->anim2_frame, &key_a2, &key_b2, &t2);
    else w2 = 0;

    bool same = !t || key_a == key_b;
    const int16_t (*a)[3] = model_resolve_pose(key_a, 0);
    const int16_t (*b)[3] = same ? a : model_resolve_pose(key_b, 1);
    const int16_t (*a2)[3] = 0, (*b2)[3] = 0;
    if(w2){
        same = !t2 || key_a2 == key_b2;
        a2 = model_resolve_pose(key_a2, 2);
        b2 = same ? a2 : model_resolve_pose(key_b2, 3);
    }

    int fade = 256;
    if(p->anim_blend_ticks > 0 && p->pose_valid)
        fade = ((p->anim_blend_total - p->anim_blend_ticks) << 8) / p->anim_blend_total;

    int16_t* out = &p->pose[0][0];
    const int16_t* from = &p->pose_from[0][0];
    const int16_t* ka = &a[0][0];
    const int16_t* kb = &b[0][0];
    int n = point_count * 3;
    if(!w2 && fade == 256){
        for(int i = 0; i < n; i++) out[i] = (int16_t)(ka[i] + (((kb[i] - ka[i]) * t) >> 8));
    } else {
        const int16_t* ka2 = w2 ? &a2[0][0] : ka;
        const int16_t* kb2 = w2 ? &b2[0][0] : kb;
        for(int i = 0; i < n; i++){
            int32_t target = ka[i] + (((kb[i] - ka[i]) * t) >> 8);
            if(w2){
                int32_t other = ka2[i] + (((kb2[i] - ka2[i]) * t2) >> 8);
                target += ((other - target) * w2) >> 8;
            }
            if(fade < 256) target = from[i] + (((target - from[i]) * fade) >> 8);
            out[i] = (int16_t)target;
        }
    }
    p->pose_valid = true;
    figure->pose = (const int16_t (*)[3])p->pose;

    figure->basis_front =
        g_mario.turn_yaw ? v3_tangent14(v3_rotate_axis14(p->front, p->up, g_mario.turn_yaw), p->up, p->front)
                         : p->front;
    figure->basis_up = p->up;
    if(p->action == MA_SWIM) mario_swim_apply_pose(figure);
    mario_skate_apply_pose(figure);
    figure->transform_dirty = true;
    figure->has_shadow = p->has_floor_below && p->action != MA_STAR_LAUNCH;
    figure->shadow = p->floor_point;
    figure->hidden = p->hidden || mario_blink_hidden(p);
}

static fx mario_walk_anim_rate(void){
    MarioPhysics* m = &g_mario;
    if(m->walk_index == 0){
        m->start_spin = 0;
        return FXC(0.33f);
    }
    int32_t w = m->ratio;

    int32_t rate = 3 * w;
    int32_t target = m->squat ? 0 : N14_ONE;
    int32_t d = target - w;
    if(w > R14(0.6f)){
        int32_t f = ((w - R14(0.6f)) << 14) / R14(0.4f);
        d = mul14(d, N14_ONE - f) + mul14(mul14(d, d), f);
    } else if(w < R14(0.4f)){
        int32_t f = ((R14(0.4f) - w) << 14) / R14(0.4f);
        int32_t root = d > 0 ? fx_sqrt(d >> 6) << 6 : 0;
        d = mul14(d, f) + mul14(root, N14_ONE - f);
    }
    int32_t f5 = fx_clamp(N14_ONE + 4 * d, N14_ONE, 2 * N14_ONE);
    if(target < R14(0.8f)) f5 = N14_ONE;
    m->start_spin = f5 - N14_ONE;

    int32_t spin = mul14(C_START_SPIN_ANIME, 3 * target);
    if(rate < spin) rate = mul14(rate, 2 * N14_ONE - f5) + mul14(spin, f5 - N14_ONE);
    return rate >> 6;
}

static void mario_choose_ground_anim(void){
    if(cutscene_blocks(BLOCK_GROUND_ANIM) || actors_player_anim()) return;
    if(mario_fireball_animation_active()) return;
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    if(bee_ground_anim()) return;
    if(p->action == MA_SQUAT || p->action == MA_SQUAT_SLIDE){
        if(m->ratio > R14(0.05f) && m->stick){
            mario_set_anim(ANIM_MARIO_SQUATWALK);
        } else {
            mario_set_anim(ANIM_MARIO_SQUATWAIT);
        }
        return;
    }
    if(p->action != MA_WAIT) return;

    if(m->idle_stage){
        int once = m->idle_stage == 1 ? ANIM_MARIO_SLEEP : ANIM_MARIO_SLEEPLIE;
        int loop = m->idle_stage == 1 ? ANIM_MARIO_SLEEPLOOP : ANIM_MARIO_SLEEPLIELOOP;
        if(p->anim == once && p->anim_group == ANIM_GROUP_SINGLE){
            if(mario_anim_done()) mario_set_anim(loop);
        } else if(p->anim != loop || p->anim_group != ANIM_GROUP_SINGLE){
            mario_set_anim(once);
        }
        return;
    }

    if(m->run_end){
        mario_set_anim(ANIM_MARIO_RUNEND);
        if(m->brake_second){
            m->brake_second--;
            if(m->stick) m->brake_second = 0;
            if(m->brake_second == 0){
                mario_stop_walk();
                m->run_end = false;
            }
        } else if(mario_anim_done() || m->walk_index){
            m->run_end = false;
        }
        if(m->run_end) return;
    }
    if(m->brake_pose){
        mario_set_anim(ANIM_MARIO_BRAKE);
        return;
    }

    if(m->step_ticks > 0){
        m->step_ticks--;
        mario_set_group(ANIM_GROUP_STEP, ANIM_MARIO_RUN, ANIM_MARIO_WAIT, FXC(0.25f), 2);
        p->anim_speed = FX_ONE;
        return;
    }

    int32_t want = m->walk_index ? N14_ONE : 0;
    m->run_weight += mul14(want - m->run_weight, C_WEIGHT_BLEND_RATIO);
    int wait = p->health == 1 ? ANIM_MARIO_DAMAGEWAIT : ANIM_MARIO_WAIT;
    mario_set_group(ANIM_GROUP_BASIC, ANIM_MARIO_RUN, wait, (N14_ONE - m->run_weight) >> 6, 6);
    p->anim_speed = mario_walk_anim_rate();

    if(m->walk_index > 5 && m->ratio > R14(0.9f) && !(p->ground_attr & COL_SLIPPERY))
        m->brake_first = C_BRAKE_FIRST_TIMER;
    if(m->brake_first) m->brake_first--;
    if(m->walk_index == 0){
        if(m->brake_first && !(p->ground_attr & COL_SLIPPERY)){
            m->run_end = true;
            m->brake_second = C_BRAKE_SECOND_TIMER;
            mario_set_anim(ANIM_MARIO_RUNEND);
        }
        m->brake_first = 0;
    }
}
