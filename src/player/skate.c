typedef struct {
    v3 velocity;
    int32_t yaw;
    fx roll;
    uint16_t ticks;
    uint8_t back, foot, ready, initial, idle, jump;
} MarioSkateState;
static EWRAM_BSS MarioSkateState g_skate;

static void mario_skate_reset(void){ g_skate = (MarioSkateState){0}; }
static bool mario_ice_floor(void){ return (g_game.player.ground_attr & COL_ICE) != 0; }
static int32_t mario_ice_inertia(int32_t speed, int32_t start_spin){
    speed = fx_min(speed, N14_ONE);
    int32_t inertia = R14(.985f) + mul14(speed, R14(.995f) - R14(.985f));
    inertia += mul14(start_spin, R14(.98f) - inertia);
    return speed == 0 ? R14(.992f) : inertia;
}

static int32_t mario_ice_walk_inertia(int32_t speed, int32_t start_spin){
    int32_t inertia = R14(.98f) + mul14(speed, R14(.99f) - R14(.98f));
    inertia += mul14(start_spin, R14(.985f) - inertia);
    return speed == 0 ? R14(.98f) : inertia;
}

static void mario_skate_start(bool landed){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    g_skate = (MarioSkateState){
        .velocity = horizontal(m->jump_vec, p->up), .initial = 1, .idle = m->walk_index < 3};
    mario_set_action(MA_SKATE);
    mario_restart_anim(landed ? ANIM_MARIO_ICEJUMPLAND : ANIM_MARIO_ICESPIN);
    m->squat = false;
    m->attack_timer = landed ? 0 : 25;
    if(!landed) audio_play_effect(SFX_SPIN, 1);
}

static bool mario_skate_land(void){
    bool jump = g_skate.jump;
    g_skate.jump = 0;
    if(!jump || !mario_ice_floor() || bee_active()) return false;
    mario_skate_start(true);
    return true;
}

static void mario_skate_close(void){
    Figure* f = &g_game.player.figure;
    f->local_yaw = f->local_roll = 0;
    f->transform_dirty = true;
    mario_set_action(g_mario.jumping ? MA_FALL : MA_WAIT);
    mario_set_anim(g_mario.jumping ? ANIM_MARIO_FALL : ANIM_MARIO_WAIT);
}

static bool mario_skate_tick(const KeyState* keys){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    MarioSkateState* s = &g_skate;
    if(p->action != MA_SKATE){
        if(!p->on_ground || m->jumping || !mario_ice_floor() || bee_active() ||
           (p->action != MA_WAIT && p->action != MA_SKID && p->action != MA_LAND && p->action != MA_SPIN) ||
           !(keys->pressed & KEY_B) || m->squat || m->spin_cooldown)
            return false;
        mario_skate_start(false);
    }
    if(!p->on_ground || m->jumping || !mario_ice_floor()){
        mario_skate_close();
        return false;
    }
    if(keys->pressed & KEY_A){
        bool idle = s->idle;
        mario_skate_close();
        mario_jump();
        s->jump = 1;
        if(!idle){
            m->jump_vec = v3_add(v3_scale14(horizontal(m->jump_vec, p->up), R14(1.5f)),
                                 v3_scale14(p->up, vertical_speed(m->jump_vec, p->up)));
            mario_restart_anim(m->jump_type == JUMP_SECOND   ? ANIM_MARIO_ICEJUMP2
                               : m->jump_type == JUMP_TRIPLE ? ANIM_MARIO_ICEJUMP3
                                                             : ANIM_MARIO_ICEJUMP);
        }
        return false;
    }
    if(s->idle){
        if(mario_anim_done()){
            mario_skate_close();
            return false;
        }
        return true;
    }

    mario_ground_move(keys);
    m->turn_armed = m->skidding = false;
    m->turn_ready = 0;
    if(keys->pressed & KEY_R){
        s->ticks = 20;
        s->back = !s->back;
        s->ready = 1;
        s->foot = 1 - s->foot;
    }
    if(p->anim == ANIM_MARIO_ICEJUMPLAND){
        if(mario_anim_done()) s->ready = 1;
    } else if(s->initial == 1 && (p->anim_frame >> 8) > 20 + 50 * (1 - (m->stick != 0)) && m->stick)
        s->ready = 1;
    if((keys->pressed & KEY_B) && s->initial && (p->anim != ANIM_MARIO_ICESKATESPIN || mario_anim_done())){
        mario_restart_anim(ANIM_MARIO_ICESKATESPIN);
        if(m->ratio < R14(1.25f)) m->ratio = mul14(m->ratio, R14(1.5f));
        audio_play_effect(SFX_SPIN, 1);
        m->attack_timer = 25;
    }
    if((keys->pressed & KEY_B) && (p->anim_frame >> 8) > 20) s->ready = 1;
    if(m->ratio < (m->stick ? R14(1.2f) : 0)) m->ratio = R14(1.2f);
    if(s->back) s->yaw = fx_clamp(s->yaw + R14(.04f), -N14_ONE, N14_ONE);
    else {
        if(s->yaw == N14_ONE) s->yaw = -N14_ONE;
        bool negative = s->yaw <= 0;
        s->yaw += R14(.04f);
        s->yaw = negative ? -fx_clamp(-s->yaw, 0, N14_ONE) : fx_clamp(s->yaw, -N14_ONE, N14_ONE);
    }

    fx angle = m->stick ? -signed_angle14(p->front, m->move_dir, p->up) : 0;
    s->ticks++;
    if(s->ticks >= (s->back ? 60 : 30)){
        if(s->ready){
            if(!(angle >= FXC(.1f) && s->foot == 1) && !(angle <= -FXC(.1f) && s->foot == 0)){
                s->ready = 0;
                s->ticks = 0;
                s->foot = 1 - s->foot;
                mario_restart_anim(s->back ? (s->foot ? ANIM_MARIO_SKATEBACKL : ANIM_MARIO_SKATEBACKR)
                                           : (s->foot ? ANIM_MARIO_SKATER : ANIM_MARIO_SKATEL));
            }
        } else if(s->ticks > 90){
            mario_skate_close();
            return false;
        }
    }
    p->anim_speed = p->anim == ANIM_MARIO_ICEJUMPLAND ? FX_ONE : (m->stick ? FX_ONE : FX_ONE / 2);
    fx tilt = fx_clamp(angle, -(s->back ? FXC(.3926991f) : FXC(.5235988f)),
                       s->back ? FXC(.3926991f) : FXC(.5235988f));
    if(!m->stick) tilt = fx_mul(tilt, FXC(.2f));
    int32_t blend = fx_abs(tilt) > fx_abs(s->roll) ? R14(.1f) : R14(.05f);
    s->roll += mul14(tilt - s->roll, blend);

    int32_t speed = mul14(G(13), m->ratio);
    v3 velocity = v3_scale14(p->front, speed), normal = p->ground_normal;
    if((normal.x | normal.y | normal.z) && v3_dot14(normal, p->up) > R14(.5f))
        velocity = v3_set_length(v3_reject14(velocity, normal), speed, p->front);
    s->velocity = velocity;
    m->jump_vec = s->velocity;
    mario_move_and_collide(s->velocity, true);
    if(p->action != MA_SKATE){
        p->figure.local_yaw = p->figure.local_roll = 0;
        p->figure.transform_dirty = true;
    }
    return true;
}

static void mario_skate_apply_pose(Figure* f){
    if(g_game.player.action != MA_SKATE) return;
    f->local_yaw = mul14(g_skate.yaw, FXC(3.1415927f));
    f->local_roll = g_skate.roll;
}
