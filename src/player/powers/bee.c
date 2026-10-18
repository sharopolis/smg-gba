#define BEE_FLY_CONSTANT     G(5.0f)
#define BEE_ACCEL_RATIO      R14(0.02f)
#define BEE_UP_SPEED_MAX     G(5.0f)
#define BEE_UP_DOWN_KILLER   R14(0.3f)
#define BEE_FREE_DROP_ACC    G(0.1f)
#define BEE_FREE_DROP_MAX    G(5.0f)
#define BEE_RISE_ERASER      R14(0.9f)
#define BEE_AIR_WALK_ACC     G(0.1f)
#define BEE_AIR_WALK_LIMIT   G(8.0f)
#define BEE_TURN             R14(0.05f)
#define BEE_SPEED_ROTATE     R14(0.02f)
#define BEE_REVIVE_TIME      120
#define BEE_POWER_TIME       60
#define BEE_POWER_TIME_V     120
#define BEE_INHIBIT_TIME     30
#define BEE_INHIBIT_TIME_V   25
#define BEE_GRAVITY_AIR_WALK G(0.1f)

static EWRAM_BSS BeeState g_bee;

static inline bool bee_active(void){
    return g_bee.on && g_game.player.figure.model == (const FigureHeader*)figure_mariobee;
}

static int bee_fly_time(void){ return bee_active() ? g_bee.fly_time : -1; }

static void bee_start(void){
    Player* p = &g_game.player;
    g_bee = (BeeState){0};
    g_bee.on = true;
    g_bee.fly_time = BEE_AIR_WALK_TIME;
    p->figure.model = (const FigureHeader*)figure_mariobee;
    p->pose_valid = false;
    mario_restart_anim(p->anim_group == ANIM_GROUP_SINGLE ? p->anim : ANIM_MARIO_WAIT);
}

static void bee_end(void){
    Player* p = &g_game.player;
    audio_stop_level_effect(SFX_BEE_FLY);
    if(!bee_active()){
        g_bee.on = false;
        return;
    }
    g_bee.on = false;
    g_bee.wall = 0;
    p->figure.model = (const FigureHeader*)figure_mario;
    p->pose_valid = false;
    p->anim_group = -1;
    mario_restart_anim(g_mario.jumping ? ANIM_MARIO_FALL : ANIM_MARIO_WAIT);
}

static int bee_anim(int anim){
    if(!bee_active()) return anim;
    if(anim >= 1000) return anim - 1000;
    switch(anim){
    case ANIM_MARIO_WAIT:
    case ANIM_MARIO_DAMAGEWAIT:
    case ANIM_MARIO_STAGESTART:
    case ANIM_MARIO_RISE:
    case ANIM_MARIO_SLEEP:
    case ANIM_MARIO_SLEEPLOOP:
    case ANIM_MARIO_SLEEPLIE:
    case ANIM_MARIO_SLEEPLIELOOP:
    case ANIM_MARIO_SQUATWAIT:
    case ANIM_MARIO_SQUATSTART:
    case ANIM_MARIO_PUSH:
    case ANIM_MARIO_WALLWAIT: return ANIM_MARIOBEE_WAIT;
    case ANIM_MARIO_WALLWALKL:
    case ANIM_MARIO_WALLWALKR:
    case ANIM_MARIO_SQUATWALK:
    case ANIM_MARIO_WALK: return ANIM_MARIOBEE_WALK;
    case ANIM_MARIO_RUN: return ANIM_MARIOBEE_RUN;
    case ANIM_MARIO_TURN: return ANIM_MARIOBEE_TURN;
    case ANIM_MARIO_JUMP:
    case ANIM_MARIO_JUMP2:
    case ANIM_MARIO_JUMPROLL:
    case ANIM_MARIO_JUMPTURN:
    case ANIM_MARIO_JUMPBROAD:
    case ANIM_MARIO_JUMPBACK:
    case ANIM_MARIO_JUMPPRESS2ND:
    case ANIM_MARIO_JUMPPRESS3RD: return ANIM_MARIOBEE_BEEJUMP;
    case ANIM_MARIO_FALL:
    case ANIM_MARIO_WALLKEEP:
    case ANIM_MARIO_WALLSLIDE: return ANIM_MARIOBEE_BEEFLYWAIT;
    case ANIM_MARIO_WALLJUMP: return ANIM_MARIOBEE_BEEWALLJUMP;
    case ANIM_MARIO_LAND:
    case ANIM_MARIO_JUMP2LAND:
    case ANIM_MARIO_JUMPROLLLAND:
    case ANIM_MARIO_JUMPTURNLAND:
    case ANIM_MARIO_JUMPBROADLAND:
    case ANIM_MARIO_JUMPBACKLAND: return ANIM_MARIOBEE_LAND;
    case ANIM_MARIO_SPIN:
    case ANIM_MARIO_SPIN2ND: return ANIM_MARIOBEE_BEESPIN;
    case ANIM_MARIO_SPINGROUND: return ANIM_MARIOBEE_BEESPINGROUND;
    case ANIM_MARIO_HIPDROPSTART: return ANIM_MARIOBEE_BEEHIPDROPSTART;
    case ANIM_MARIO_HIPDROP: return ANIM_MARIOBEE_BEEHIPDROP;
    case ANIM_MARIO_HIPDROPLAND: return ANIM_MARIOBEE_BEEHIPDROPLAND;
    case ANIM_MARIO_DAMAGEFRONT: return ANIM_MARIOBEE_DAMAGEFRONT;
    case ANIM_MARIO_DAMAGEBACK: return ANIM_MARIOBEE_DAMAGEBACK;
    case ANIM_MARIO_DAMAGEAIR: return ANIM_MARIOBEE_DAMAGEAIR;
    case ANIM_MARIO_DAMAGELAND: return ANIM_MARIOBEE_DAMAGELAND;
    case ANIM_MARIO_DIEOVER: return ANIM_MARIOBEE_DIEOVER;
    case ANIM_MARIO_DIEFALL: return ANIM_MARIOBEE_DIEFALL;
    case ANIM_MARIO_STAGGER: return ANIM_MARIOBEE_STAGGER;
    case ANIM_MARIO_TRAPEZEBACKSPIN: return ANIM_MARIOBEE_TRAPEZEBACKSPIN;
    case ANIM_MARIO_TRAPEZEFRONTSPIN: return ANIM_MARIOBEE_TRAPEZEFRONTSPIN;
    case ANIM_MARIO_TRAPEZESLIDEDOWN: return ANIM_MARIOBEE_TRAPEZESLIDEDOWN;
    case ANIM_MARIO_TRAPEZESLIDEDOWNSTART: return ANIM_MARIOBEE_TRAPEZESLIDEDOWNSTART;
    case ANIM_MARIO_TRAPEZESLOWSWINGBACKEND: return ANIM_MARIOBEE_TRAPEZESLOWSWINGBACKEND;
    case ANIM_MARIO_TRAPEZESLOWSWINGBACKSTART: return ANIM_MARIOBEE_TRAPEZESLOWSWINGBACKSTART;
    case ANIM_MARIO_TRAPEZESLOWSWINGFRONTEND: return ANIM_MARIOBEE_TRAPEZESLOWSWINGFRONTEND;
    case ANIM_MARIO_TRAPEZESLOWSWINGFRONTSTART: return ANIM_MARIOBEE_TRAPEZESLOWSWINGFRONTSTART;
    case ANIM_MARIO_TRAPEZESWINGBACKEND: return ANIM_MARIOBEE_TRAPEZESWINGBACKEND;
    case ANIM_MARIO_TRAPEZESWINGBACKSTART: return ANIM_MARIOBEE_TRAPEZESWINGBACKSTART;
    case ANIM_MARIO_TRAPEZESWINGFRONTEND: return ANIM_MARIOBEE_TRAPEZESWINGFRONTEND;
    case ANIM_MARIO_TRAPEZESWINGFRONTSTART: return ANIM_MARIOBEE_TRAPEZESWINGFRONTSTART;
    case ANIM_MARIO_TRAPEZEWAIT: return ANIM_MARIOBEE_TRAPEZEWAIT;
    case ANIM_MARIO_SPINDRIVERWAIT: return ANIM_MARIOBEE_SPINDRIVERWAIT;
    case ANIM_MARIO_LAUNCHSTART: return ANIM_MARIOBEE_LAUNCHSTART;
    case ANIM_MARIO_SPACEFLY: return ANIM_MARIOBEE_SPACEFLY;
    case ANIM_MARIO_SPACEFLYEND: return ANIM_MARIOBEE_SPACEFLYEND;
    case ANIM_MARIO_SPACEFLYSTART: return ANIM_MARIOBEE_SPACEFLYSTART;
    case ANIM_MARIO_SPACEFLYSHORTSTART: return ANIM_MARIOBEE_SPACEFLYSHORTSTART;
    case ANIM_MARIO_SPACEFLYSHORT: return ANIM_MARIOBEE_SPACEFLYSHORT;
    case ANIM_MARIO_LANDSTIFFEN: return ANIM_MARIOBEE_LANDSTIFFEN;
    case ANIM_MARIO_STARGET: return ANIM_MARIOBEE_STARGET;
    case ANIM_MARIO_BRAKE: return ANIM_MARIOBEE_BRAKE;
    case ANIM_MARIO_RUNEND: return ANIM_MARIOBEE_RUNEND;
    default: return ANIM_MARIOBEE_WAIT;
    }
}

#define BEE_CLIP(c) ((c) + 1000)

static void bee_turn_front(v3 dir, int32_t rate){
    Player* p = &g_game.player;
    if(!(dir.x | dir.y | dir.z)) return;
    p->front = v3_tangent14(v3_blend14(p->front, dir, rate), p->up, p->front);
}

static bool bee_update_gravity(void){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    BeeState* b = &g_bee;
    if(!bee_active()){
        b->wall = 0;
        return false;
    }
    if(b->wall_cooldown) b->wall_cooldown--;
    if(p->water_state != WATER_DRY || (p->on_ground && (p->ground_attr & COL_WATER))){
        bee_end();
        return false;
    }

    if(p->on_ground && !m->jumping){
        audio_stop_level_effect(SFX_BEE_FLY);
        b->fly_time = fx_min(b->fly_time + 4, BEE_AIR_WALK_TIME);
        b->drift = 0;
        b->flying = false;
        b->revive = 0;
        b->power = 0;
    }
    if(b->wall == 0 && b->wall_cooldown == 0 && p->action != MA_HIPDROP && p->action != MA_HIPDROP_START &&
       p->action != MA_STAR_LAUNCH){
        v3 centre = v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT));
        RayHit hit;
        bool stick = false;
        if(p->on_wall &&
           collision_ray(centre, v3_sub(centre, v3_scale14(p->wall_normal, FX(300))), &hit, 0) &&
           (hit.attr & COL_HONEY))
            stick = true;
        if(!stick && m->jumping && vertical_speed(m->jump_vec, p->up) > 0 &&
           collision_ray(centre, v3_add(centre, v3_scale14(p->up, FX(200))), &hit, 0) &&
           (hit.attr & COL_HONEY))
            stick = true;

        if(!stick && p->on_ground && (p->ground_attr & COL_HONEY) &&
           v3_dot14(p->ground_normal, p->up) < R14(0.94f)){
            hit.point = p->figure.position;
            hit.normal = p->ground_normal;
            stick = true;
        }
        if(stick){
            v3 head = p->up;
            b->wall = 5;
            b->wall_up = hit.normal;
            m->position16 = V3(hit.point.x << 8, hit.point.y << 8, hit.point.z << 8);
            p->figure.position = hit.point;
            m->jumping = false;
            m->jump_vec = V3(0, 0, 0);
            m->ratio = 0;
            p->up = hit.normal;
            p->gravity_up = hit.normal;
            p->front = v3_tangent14(head, p->up, p->front);
            p->on_ground = true;
            p->ground_normal = hit.normal;
            p->ground_attr = COL_HONEY;
            audio_stop_level_effect(SFX_BEE_FLY);
            b->flying = false;
            mario_set_action(MA_WAIT);
            mario_restart_anim(BEE_CLIP(ANIM_MARIOBEE_BEECREEPLAND));
            return true;
        }
    }
    if(b->wall == 0) return false;

    if(p->on_ground && !m->jumping && (p->ground_attr & COL_HONEY)){
        b->wall = 5;
        b->wall_up = p->ground_normal;
    } else {
        b->wall--;
    }
    if(b->wall == 0){
        b->wall_cooldown = 30;
        if(!m->jumping){
            v3 off = v3_scale14(b->wall_up, G(8.0f));
            mario_take_off(JUMP_FIRST, v3_add(off, v3_scale14(p->up, G(10.0f))), true);
            mario_set_action(MA_FALL);
            mario_set_anim(ANIM_MARIO_FALL);
            b->power = BEE_POWER_TIME;
            m->air_frames = BEE_INHIBIT_TIME - 5;
        }
        return false;
    }

    v3 old = p->up;
    p->up = v3_blend14(p->up, b->wall_up, R14(0.3f));
    p->gravity_up = b->wall_up;
    p->front = v3_tangent14(v3_rotate_between14(p->front, old, p->up), p->up, p->front);
    return true;
}

static bool bee_ground_anim(void){
    Player* p = &g_game.player;
    if(!bee_active() || g_bee.wall == 0 || p->action != MA_WAIT) return false;
    if(p->anim == ANIM_MARIOBEE_BEECREEPLAND && p->anim_group == ANIM_GROUP_SINGLE && !mario_anim_done())
        return true;
    if(g_mario.walk_index){
        mario_set_anim(BEE_CLIP(ANIM_MARIOBEE_BEECREEPWALK));
        p->anim_speed = fx_max(mario_walk_anim_rate(), FXC(0.5f));
    } else {
        mario_set_anim(BEE_CLIP(ANIM_MARIOBEE_BEECREEPWAIT));
        p->anim_speed = FX_ONE;
    }
    return true;
}

static v3 bee_air_physics(const KeyState* keys){
    MarioPhysics* m = &g_mario;
    Player* p = &g_game.player;
    BeeState* b = &g_bee;
    v3 up = p->up;
    if(!(keys->held & KEY_A)) m->hold = false;
    v3 extra = V3(0, 0, 0);
    bool flew = false;
    bool flight_sound = false;
    int32_t vs = vertical_speed(m->jump_vec, up);
    b->flapping = false;

    if(vs < G(5.0f) || b->revive){
        v3 h = horizontal(m->jump_vec, up);
        int32_t hs = v3_length_exact(h);
        if(hs > 16){
            v3 d = v3_blend14(v3_direction14(h, p->front), p->front, BEE_SPEED_ROTATE);
            m->jump_vec = v3_add(v3_scale14(v3_direction14(d, p->front), hs), v3_scale14(up, vs));
        }
        if(!b->flying && b->fly_time){
            if(b->revive < 30) b->revive = 30;
            b->drift = 0;
            b->flying = true;
        }
        int inhibit = hs < G(5.0f) ? BEE_INHIBIT_TIME_V : BEE_INHIBIT_TIME;
        if((keys->held & KEY_A) && b->fly_time && m->air_frames > inhibit && p->action != MA_DAMAGE_FALL){
            audio_level_effect(SFX_BEE_FLY);
            flight_sound = true;
            if(m->stick) bee_turn_front(m->move_dir, BEE_TURN);
            if(b->fly_time > BEE_AIR_WALK_TIME) b->fly_time = BEE_AIR_WALK_TIME;
            b->fly_time--;
            if(b->fly_time == 0){
                b->flying = false;
            } else {
                b->flapping = true;
                if(p->action != MA_SPIN && p->action != MA_WALL_JUMP){
                    mario_set_anim(BEE_CLIP(ANIM_MARIOBEE_BEEFLY));
                    p->anim_speed = b->fly_time < BEE_AIR_WALK_TIME / 2 ? FXC(1.5f) : FX_ONE;
                }

                b->drift -= mul14(BEE_FLY_CONSTANT, BEE_ACCEL_RATIO);
                if(b->drift > 0) b->drift = mul14(b->drift, BEE_UP_DOWN_KILLER);
                if(b->drift < -BEE_UP_SPEED_MAX) b->drift = -BEE_UP_SPEED_MAX;
                extra = v3_scale14(up, -b->drift);

                if(vs < 0)
                    m->jump_vec =
                        v3_add(horizontal(m->jump_vec, up), v3_scale14(up, mul14(vs, BEE_RISE_ERASER)));
                if(++b->power > 120) b->power = 120;
                flew = true;
            }
        }
        if(!flew){
            if(p->action == MA_JUMP || p->action == MA_FALL || p->action == MA_JUMP2 ||
               p->action == MA_JUMP3){
                if(p->anim != ANIM_MARIOBEE_BEEJUMP || mario_anim_done())
                    mario_set_anim(BEE_CLIP(ANIM_MARIOBEE_BEEFLYWAIT));
            }
            if(b->power) b->power--;
            if(b->drift < BEE_FREE_DROP_MAX) b->drift += BEE_FREE_DROP_ACC;
            extra = v3_scale14(up, -b->drift);
            if(m->stick) bee_turn_front(m->move_dir, BEE_TURN);
        }
    }
    if(flew) b->revive = BEE_REVIVE_TIME;

    if(!flight_sound) audio_stop_level_effect(SFX_BEE_FLY);

    vs = vertical_speed(m->jump_vec, up);
    bool rising = vs > 0;
    int32_t accel = jump_gravity[m->jump_type];
    if(b->revive){
        int32_t t = (b->revive << 14) / BEE_REVIVE_TIME;
        accel = mul14(G(1.0f), N14_ONE - t) + mul14(BEE_GRAVITY_AIR_WALK, t);
        b->revive--;
    }
    bool half = m->hold || m->digital || !rising;
    int32_t scale = half ? C_GRAVITY_RATIO_A : N14_ONE;
    if(m->jump_type == JUMP_DAMAGE || m->jump_type == 13) scale = N14_ONE;

    v3 h = horizontal(m->jump_vec, up);
    if(m->stick && m->no_control_timer == 0) h = v3_add(h, v3_scale14(m->move_dir, BEE_AIR_WALK_ACC));
    if(v3_length_squared64(h) > (uint64_t)((int64_t)BEE_AIR_WALK_LIMIT * BEE_AIR_WALK_LIMIT))
        h = v3_set_length(h, BEE_AIR_WALK_LIMIT, p->front);
    m->jump_vec = v3_add(h, v3_scale14(up, vs));

    v3 displacement = v3_add(m->jump_vec, extra);
    if(!rising && m->stick && m->no_control_timer == 0)
        displacement = v3_add(displacement, v3_scale14(m->move_dir, G(13.0f * 0.15f)));
    m->jump_vec = v3_sub(m->jump_vec, v3_scale14(up, mul14(accel, scale)));
    vs = vertical_speed(m->jump_vec, up);
    if(vs < -C_MAX_DROP_SPEED)
        m->jump_vec = v3_add(horizontal(m->jump_vec, up), v3_scale14(up, -C_MAX_DROP_SPEED));
    m->air_frames++;
    return displacement;
}

static inline bool bee_owns_air(void){
    const Player* p = &g_game.player;

    return bee_active() && g_mario.jump_type != JUMP_DAMAGE && !flight_input_locked() &&
           p->action != MA_STAR_LAUNCH;
}

static void bee_reset(void){
    g_bee.on = false;
    g_bee.wall = 0;
}
