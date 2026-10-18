enum {
    BOSS_STINKBUG_PHASE_WAITSTART,
    BOSS_STINKBUG_PHASE_OPENING,
    BOSS_STINKBUG_PHASE_GROUND,
    BOSS_STINKBUG_PHASE_FLYDEMO,
    BOSS_STINKBUG_PHASE_FLYLOW,
    BOSS_STINKBUG_PHASE_ANGRYDEMO,
    BOSS_STINKBUG_PHASE_FLYHIGH,
    BOSS_STINKBUG_PHASE_FINISH,
    BOSS_STINKBUG_PHASE_END
};
enum {
    BOSS_STINKBUG_START = ACTOR_STATE_WAIT,
    BOSS_STINKBUG_WALK,
    BOSS_STINKBUG_FIND,
    BOSS_STINKBUG_RUN,
    BOSS_STINKBUG_SHAKEOFF,
    BOSS_STINKBUG_SHAKEOFFRUN,
    BOSS_STINKBUG_COOLDOWN,
    BOSS_STINKBUG_TURN,
    BOSS_STINKBUG_ATTACKOK,
    BOSS_STINKBUG_DAMAGE,
    BOSS_STINKBUG_DEMO,
    BOSS_STINKBUG_DASH,
    BOSS_STINKBUG_FLY,
    BOSS_STINKBUG_SHAKESIGN,
    BOSS_STINKBUG_ROLL,
    BOSS_STINKBUG_ROLLWAIT,
    BOSS_STINKBUG_ROLLEND,
    BOSS_STINKBUG_FTURNSIGN,
    BOSS_STINKBUG_FTURN,
    BOSS_STINKBUG_FALL,
    BOSS_STINKBUG_FALLDAMAGE,
    BOSS_STINKBUG_ROLLSIGN
};

static bool boss_stinkbug_can_hip_drop(int phase, int nerve){
    if(phase == BOSS_STINKBUG_PHASE_GROUND){
        return nerve == BOSS_STINKBUG_WALK || nerve == BOSS_STINKBUG_FIND ||
               nerve == BOSS_STINKBUG_SHAKEOFF || nerve == BOSS_STINKBUG_SHAKEOFFRUN ||
               nerve == BOSS_STINKBUG_RUN || nerve == BOSS_STINKBUG_TURN || nerve == BOSS_STINKBUG_COOLDOWN;
    }
    if(phase == BOSS_STINKBUG_PHASE_FLYLOW || phase == BOSS_STINKBUG_PHASE_FLYHIGH){
        return nerve == BOSS_STINKBUG_FLY ||
               (phase == BOSS_STINKBUG_PHASE_FLYHIGH && nerve == BOSS_STINKBUG_ROLLSIGN) ||
               nerve == BOSS_STINKBUG_SHAKESIGN || nerve == BOSS_STINKBUG_ROLL ||
               nerve == BOSS_STINKBUG_ROLLEND || nerve == BOSS_STINKBUG_FTURNSIGN ||
               nerve == BOSS_STINKBUG_FTURN;
    }
    return false;
}

static void boss_stinkbug_pose_world(const Actor* a, const HoneyBossPose* pose, bool placement, v3* pos,
                                     v3* up, v3* front, int16_t scale[3]){
    const StageObject* object = actor_stage_object(a);
    v3 u = placement && object ? v3_from_i16(object->up) : a->up;
    v3 f = placement && object ? v3_from_i16(object->front) : a->front;
    v3 side = v3_cross14(u, f);
    int16_t base[3];
    actor_stage_scale(a, base);
    *pos = dyncol_point(pose->pos, placement ? a->home : a->position, side, u, f, base);
    *up = v3_direction14(
        v3_add(v3_add(v3_scale14(side, pose->up[0]), v3_scale14(u, pose->up[1])), v3_scale14(f, pose->up[2])),
        u);
    *front = v3_tangent14(v3_add(v3_add(v3_scale14(side, pose->front[0]), v3_scale14(u, pose->front[1])),
                                 v3_scale14(f, pose->front[2])),
                          *up, f);
    for(int k = 0; k < 3; k++) scale[k] = (int16_t)((base[k] * pose->scale[k]) >> 8);
}

__attribute__((noinline)) static void boss_stinkbug_body_frame(const Actor* a, v3* pos, v3* up, v3* front,
                                                               int16_t scale[3]){
    const HoneyBossBodyClip* clip = &hb_body_clips[0];
    for(unsigned k = 0; k < sizeof(hb_body_clips) / sizeof(hb_body_clips[0]); k++)
        if(hb_body_clips[k].anim == a->anim){
            clip = &hb_body_clips[k];
            break;
        }
    int frame = fx_max(a->anim_frame >> 8, 0);
    if(clip->loop) frame %= clip->period;
    if(frame >= clip->count) frame = clip->count - 1;
    boss_stinkbug_pose_world(a, &clip->pose[frame], false, pos, up, front, scale);
}

__attribute__((noinline)) static void boss_stinkbug_collision(Actor* a, v3 mario){
    int phase = a->value;
    if(phase == BOSS_STINKBUG_PHASE_WAITSTART || phase == BOSS_STINKBUG_PHASE_OPENING ||
       phase == BOSS_STINKBUG_PHASE_END || a->state == BOSS_STINKBUG_ATTACKOK)
        return;
    v3 delta = v3_sub(a->position, mario);
    if(fx_abs(delta.x) > FX(3000) || fx_abs(delta.y) > FX(3000) || fx_abs(delta.z) > FX(3000)) return;
    v3 pos, up, front;
    int16_t scale[3];
    boss_stinkbug_body_frame(a, &pos, &up, &front, scale);

    dyncol_add(&honey_boss_colmeshes[HBCOL_BOSSHIP], pos, up, front, scale, actor_index(a) | (1 << 8), 0,
               mario, FX(600));
    int mesh = (phase == BOSS_STINKBUG_PHASE_GROUND || phase == BOSS_STINKBUG_PHASE_OPENING ||
                phase == BOSS_STINKBUG_PHASE_FLYDEMO)
                   ? HBCOL_BOSSGROUND
                   : HBCOL_BOSSBASE;
    dyncol_add(&honey_boss_colmeshes[mesh], pos, up, front, scale, actor_index(a), 0, mario, FX(600));
}

static const HoneyBossPose (*boss_stinkbug_demo_frames(int phase, int* length))[2] {
    switch(phase){
    case BOSS_STINKBUG_PHASE_OPENING: *length = HB_DEMO_OPENINGDEMO_LENGTH; return hb_demo_openingdemo;
    case BOSS_STINKBUG_PHASE_FLYDEMO: *length = HB_DEMO_FLYDEMO_LENGTH; return hb_demo_flydemo;
    case BOSS_STINKBUG_PHASE_ANGRYDEMO: *length = HB_DEMO_ANGRYDEMO_LENGTH; return hb_demo_angrydemo;
    default: *length = HB_DEMO_FINISHDEMO_LENGTH; return hb_demo_finishdemo;
    }
}

__attribute__((noinline)) static void boss_stinkbug_demo_pose(Actor* a, int phase, int frame){
    int length;
    const HoneyBossPose(*poses)[2] = boss_stinkbug_demo_frames(phase, &length);
    if(frame > length) frame = length;
    if(frame < 0) frame = 0;
    Player* p = &g_game.player;
    v3 pos, up, front;
    int16_t scale[3];
    boss_stinkbug_pose_world(a, &poses[frame][0], true, &pos, &up, &front, scale);
    mario_talk_pause(2);
    p->figure.position = pos;
    p->up = up;
    p->gravity_up = gravity_up_at(pos, up);
    p->front = front;
    p->on_ground = false;
    g_mario.position16 = V3(pos.x << 8, pos.y << 8, pos.z << 8);
    g_mario.jump_vec = V3(0, 0, 0);
    g_mario.jumping = false;
    g_mario_link.ride = -1;
    g_mario_link.hipdrop = -1;
    boss_stinkbug_pose_world(a, &poses[frame][1], true, &pos, &up, &front, scale);
    a->position = pos;
    a->up = up;
    a->front = front;
}

static void boss_stinkbug_demo_release(void){
    g_mario_link.talk_timer = 0;
    if(g_game.player.action == MA_TALK){
        g_game.player.on_ground = false;
        g_mario.jumping = true;
        g_mario.jump_vec = V3(0, 0, 0);
        g_mario.hold = false;
        g_mario.air_frames = 0;
        mario_set_action(MA_FALL);
    }
}

static void boss_stinkbug_set(Actor* a, int nerve, int anim){
    a->state = (uint8_t)nerve;
    a->timer = 0;
    actor_restart_anim(a, anim);
}

static void boss_stinkbug_phase(Actor* a, int phase){
    a->value = (int16_t)phase;
    if(phase == BOSS_STINKBUG_PHASE_GROUND || phase == BOSS_STINKBUG_PHASE_FLYLOW ||
       phase == BOSS_STINKBUG_PHASE_FLYHIGH){
        int rail = actor_rail(a);
        a->velocity.x = rail_nearest_coord(rail, a->position, FX(50)) << 8;
        v3 at = rail_position_at(rail, a->velocity.x >> 8);
        v3 up = gravity_up_at(at, a->up);
        a->velocity.z = phase == BOSS_STINKBUG_PHASE_GROUND ? 0 : v3_dot14(v3_sub(a->position, at), up);
    }
    switch(phase){
    case BOSS_STINKBUG_PHASE_OPENING:
        boss_stinkbug_set(a, BOSS_STINKBUG_DEMO, ANIM_BOSSSTINKBUG_OPENINGDEMO);
        break;
    case BOSS_STINKBUG_PHASE_GROUND:
        boss_stinkbug_set(a, BOSS_STINKBUG_START, ANIM_BOSSSTINKBUG_GROUNDWALK);
        a->velocity.z = 0;
        break;
    case BOSS_STINKBUG_PHASE_FLYDEMO:
        boss_stinkbug_set(a, BOSS_STINKBUG_DEMO, ANIM_BOSSSTINKBUG_FLYDEMO);
        break;
    case BOSS_STINKBUG_PHASE_FLYLOW:
        collectibles_pop_column(a, OBJ_STARBIT, 8);
        a->target.x = -1;
        boss_stinkbug_set(a, BOSS_STINKBUG_DASH, ANIM_BOSSSTINKBUG_FLYWAIT);
        break;
    case BOSS_STINKBUG_PHASE_ANGRYDEMO:
        boss_stinkbug_set(a, BOSS_STINKBUG_DEMO, ANIM_BOSSSTINKBUG_ANGRYDEMO);
        break;
    case BOSS_STINKBUG_PHASE_FLYHIGH:
        collectibles_pop_column(a, OBJ_STARBIT, 16);
        a->target.x = -1;
        a->target.z = 0;
        boss_stinkbug_set(a, BOSS_STINKBUG_DASH, ANIM_BOSSSTINKBUG_FLYWAIT);
        break;
    case BOSS_STINKBUG_PHASE_FINISH:
        boss_stinkbug_set(a, BOSS_STINKBUG_DEMO, ANIM_BOSSSTINKBUG_FINISHDEMO);
        break;
    }
}

static void boss_stinkbug_throw_bomb(Actor* a){
    Actor* b = actor_spawn(OBJ_BOSSBOMB, v3_sub(a->position, v3_scale14(a->up, FX(150))), a->up, a->front);
    if(!b) return;
    uint32_t r = (uint32_t)(g_game.tick * 2654435761u + (uint32_t)actor_index(a));
    v3 rnd = V3((int32_t)(r & 0xFFF) * 20 / 4096 - 10, (int32_t)(r >> 12 & 0xFFF) * 20 / 4096 - 10,
                (int32_t)(r >> 24 & 0xFF) * 20 / 256 - 10);
    b->velocity = v3_add(v3_scale14(a->up, -G(20.0f)), V3(rnd.x << 8, rnd.y << 8, rnd.z << 8));
    b->target = a->up;
    b->state = ACTOR_STATE_WAIT;
}

static void boss_stinkbug_dispose_bombs(void){
    for(int i = 0; i < g_game.actor_count; i++)
        if(g_game.actors[i].type == OBJ_BOSSBOMB) g_game.actors[i].active = 0;
}

static bool boss_stinkbug_mario_behind(const Actor* a, int rail, fx dist){
    fx mc = rail_nearest_coord(rail, g_game.player.figure.position, FX(100));
    fx bc = a->velocity.x >> 8;
    fx diff = mc - bc;
    const StageRail* source_rail = rail_get(rail);
    if(source_rail->closed && source_rail->length > 0){
        fx length = source_rail->length;
        if(diff > length / 2) diff -= length;
        else if(diff < -length / 2) diff += length;
    }
    return (a->target.x > 0 ? diff : -diff) < -dist;
}

static int boss_stinkbug_roll_time(int rail){
    int roll = (int)(sizeof(hb_body_flyrollright) / sizeof(hb_body_flyrollright[0])) - 1;
    int end = (int)(sizeof(hb_body_flyrollendright) / sizeof(hb_body_flyrollendright[0])) - 1;
    return fx_max((rail_length(rail) * 2 - (roll + end + 60) * 60) / 120, 0);
}

static void boss_stinkbug_think(Actor* a){
    Player* p = &g_game.player;
    int rail = actor_rail(a);
    if(rail < 0) return;
    fx len = rail_length(rail);
    int phase = a->value;
    bool ground = phase == BOSS_STINKBUG_PHASE_GROUND || phase == BOSS_STINKBUG_PHASE_OPENING ||
                  phase == BOSS_STINKBUG_PHASE_WAITSTART;
    if(phase == BOSS_STINKBUG_PHASE_END){
        for(int i = 0; i < g_game.actor_count; i++){
            Actor* star = &g_game.actors[i];
            if(star->active && star->type == OBJ_POWERSTAR && star->args[0] == 3){
                if(!level_power_star(star)){
                    switch_set(actor_switch_key(a, SWITCH_DEAD), true);
                    a->active = 0;
                }
                break;
            }
        }
        return;
    }
    if(phase == BOSS_STINKBUG_PHASE_WAITSTART){
        int sw = actor_switch_key(a, SWITCH_A);
        actor_show_model(a, sw < 0);
        if(sw < 0 || switch_on(sw)){
            actor_show_model(a, true);
            a->velocity.x = rail_nearest_coord(rail, a->position, FX(50)) << 8;
            a->target.x = 1;
            boss_stinkbug_phase(a, BOSS_STINKBUG_PHASE_OPENING);
            mario_talk_pause(180);
        }
        return;
    }
    bool hip = g_mario_link.hipdrop == actor_index(a) && boss_stinkbug_can_hip_drop(phase, a->state);
    bool on_back = g_mario_link.ride == actor_index(a);
    int32_t speed = 0;
    switch(a->state){
    case BOSS_STINKBUG_DEMO: {
        int length;
        boss_stinkbug_demo_frames(phase, &length);

        boss_stinkbug_demo_pose(a, phase, a->timer);

        if(phase == BOSS_STINKBUG_PHASE_FLYDEMO && a->timer == 231)
            audio_queue_effect(SFX_BOSS_BUG_CLOSE_FEET, 5);
        if(phase == BOSS_STINKBUG_PHASE_FINISH){
            if(a->timer == length + 1){
                actor_show_model(a, false);
                boss_stinkbug_dispose_bombs();
                audio_queue_effect(SFX_BOSS_BUG_EXPLODE, 6);
            }
            if(a->timer >= length + 60){
                boss_stinkbug_demo_release();
                level_star_appear(3, a->position);
                a->value = BOSS_STINKBUG_PHASE_END;
            }
        } else if(a->timer >= length){
            boss_stinkbug_demo_release();
            boss_stinkbug_phase(a, phase == BOSS_STINKBUG_PHASE_OPENING   ? BOSS_STINKBUG_PHASE_GROUND
                                   : phase == BOSS_STINKBUG_PHASE_FLYDEMO ? BOSS_STINKBUG_PHASE_FLYLOW
                                                                          : BOSS_STINKBUG_PHASE_FLYHIGH);
        }
        return;
    }

    case BOSS_STINKBUG_START:
        speed = G(5.0f);
        if(a->timer > 90) boss_stinkbug_set(a, BOSS_STINKBUG_WALK, ANIM_BOSSSTINKBUG_GROUNDWALK);
        break;
    case BOSS_STINKBUG_WALK: {
        speed = G(5.0f);
        v3 to = v3_sub(p->figure.position, a->position);
        uint64_t d2 = v3_length_squared64(to);
        if(d2 > (uint64_t)FX(500) * FX(500) && d2 < (uint64_t)FX(1900) * FX(1900) &&
           angle_between14(v3_tangent14(to, a->up, a->front), a->front) < FXC(0.7854f))
            boss_stinkbug_set(a, BOSS_STINKBUG_FIND, ANIM_BOSSSTINKBUG_FIND);
        else if(boss_stinkbug_mario_behind(a, rail, FX(2500)))
            boss_stinkbug_set(a, BOSS_STINKBUG_TURN, ANIM_BOSSSTINKBUG_GROUNDTURNLEFT);
        break;
    }
    case BOSS_STINKBUG_FIND:
        if(actor_anim_frame_done(a, 40)) boss_stinkbug_set(a, BOSS_STINKBUG_RUN, ANIM_BOSSSTINKBUG_GROUNDRUN);
        break;
    case BOSS_STINKBUG_RUN:
    case BOSS_STINKBUG_SHAKEOFFRUN:
        speed = G(20.0f) * fx_min(a->timer, 60) / 60;
        if(a->timer > 180) boss_stinkbug_set(a, BOSS_STINKBUG_COOLDOWN, ANIM_BOSSSTINKBUG_COOLDOWN);
        break;
    case BOSS_STINKBUG_SHAKEOFF:
        speed = a->velocity.y * 95 / 100;
        if(a->timer > 90) boss_stinkbug_set(a, BOSS_STINKBUG_SHAKEOFFRUN, ANIM_BOSSSTINKBUG_GROUNDRUN);
        break;
    case BOSS_STINKBUG_COOLDOWN:
    case BOSS_STINKBUG_ATTACKOK:
        speed = a->velocity.y * (a->state == BOSS_STINKBUG_COOLDOWN ? 95 : 75) / 100;
        if((a->state == BOSS_STINKBUG_COOLDOWN && actor_anim_frame_done(a, 60)) || a->timer > 180){
            v3 to = v3_tangent14(v3_sub(p->figure.position, a->position), a->up, a->front);
            boss_stinkbug_set(a, v3_dot14(to, a->front) >= 0 ? BOSS_STINKBUG_WALK : BOSS_STINKBUG_TURN,
                              v3_dot14(to, a->front) >= 0 ? ANIM_BOSSSTINKBUG_GROUNDWALK
                                                          : ANIM_BOSSSTINKBUG_GROUNDTURNLEFT);
        }
        break;
    case BOSS_STINKBUG_TURN:
        if(a->timer == 1) a->target.x = -a->target.x;
        if(a->timer > 160) boss_stinkbug_set(a, BOSS_STINKBUG_WALK, ANIM_BOSSSTINKBUG_GROUNDWALK);
        break;
    case BOSS_STINKBUG_DAMAGE:
        if(actor_anim_frame_done(a, 90)) boss_stinkbug_phase(a, BOSS_STINKBUG_PHASE_FLYDEMO);
        break;

    case BOSS_STINKBUG_DASH: {
        int t = a->timer;
        int32_t s = t < 30    ? G(15.0f) + (G(25.0f) * t / 30)
                    : t < 100 ? G(40.0f) - G(26.0f) * (t - 30) / 70
                              : G(14.0f);
        speed = phase == BOSS_STINKBUG_PHASE_FLYHIGH ? s * 20 / 14 : s;
        if(t > 100) boss_stinkbug_set(a, BOSS_STINKBUG_FLY, ANIM_BOSSSTINKBUG_FLYWAIT);
        break;
    }
    case BOSS_STINKBUG_FLY:
        speed = phase == BOSS_STINKBUG_PHASE_FLYHIGH ? G(20.0f) : G(14.0f);
        if(phase == BOSS_STINKBUG_PHASE_FLYHIGH && a->target.z < 2){
            if(a->timer > boss_stinkbug_roll_time(rail)){
                a->target.z++;
                boss_stinkbug_set(a, BOSS_STINKBUG_ROLLSIGN, ANIM_BOSSSTINKBUG_FLYROLLSIGNRIGHT);
            }
        } else if(boss_stinkbug_mario_behind(a, rail,
                                             phase == BOSS_STINKBUG_PHASE_FLYHIGH ? FX(1500) : FX(2000))){
            a->target.z = 0;
            boss_stinkbug_set(a, BOSS_STINKBUG_FTURNSIGN, ANIM_BOSSSTINKBUG_FLYTURN);
        }
        break;
    case BOSS_STINKBUG_SHAKESIGN:
        speed = a->timer < 10 ? G(14.0f) - G(11.0f) * a->timer / 10 : G(3.0f);
        if(a->timer >= 120) boss_stinkbug_set(a, BOSS_STINKBUG_ROLL, ANIM_BOSSSTINKBUG_FLYROLLRIGHT);
        break;
    case BOSS_STINKBUG_ROLLSIGN:
        speed = G(20.0f);
        if(a->timer > 60) boss_stinkbug_set(a, BOSS_STINKBUG_ROLL, ANIM_BOSSSTINKBUG_FLYROLLRIGHT);
        break;
    case BOSS_STINKBUG_ROLL:
        speed = phase == BOSS_STINKBUG_PHASE_FLYHIGH ? G(20.0f) : G(14.0f);
        if(on_back){
            mario_take_off(
                JUMP_FIRST,
                v3_add(v3_scale14(v3_cross14(a->up, a->front), G(12.0f)), v3_scale14(a->up, G(10.0f))), true);
            mario_set_action(MA_FALL);
            mario_set_anim(ANIM_MARIO_FALL);
        }
        if(actor_anim_frame_done(a, 60))
            boss_stinkbug_set(a, BOSS_STINKBUG_ROLLWAIT,
                              phase == BOSS_STINKBUG_PHASE_FLYHIGH ? ANIM_BOSSSTINKBUG_FLYROLLWAITRIGHT
                                                                   : ANIM_BOSSSTINKBUG_FLYWAIT);
        break;
    case BOSS_STINKBUG_ROLLWAIT:
        speed = phase == BOSS_STINKBUG_PHASE_FLYHIGH ? G(20.0f) : G(35.0f);
        if(a->timer > (phase == BOSS_STINKBUG_PHASE_FLYHIGH ? boss_stinkbug_roll_time(rail) : 30))
            boss_stinkbug_set(a, BOSS_STINKBUG_ROLLEND,
                              phase == BOSS_STINKBUG_PHASE_FLYHIGH ? ANIM_BOSSSTINKBUG_FLYROLLENDRIGHT
                                                                   : ANIM_BOSSSTINKBUG_FLYWAIT);
        break;
    case BOSS_STINKBUG_ROLLEND:
        speed = phase == BOSS_STINKBUG_PHASE_FLYHIGH ? G(20.0f) : G(14.0f);
        if(phase == BOSS_STINKBUG_PHASE_FLYHIGH ? actor_anim_frame_done(a, 120) : a->timer > 40){
            a->target.y = 0;
            boss_stinkbug_set(a, BOSS_STINKBUG_FLY, ANIM_BOSSSTINKBUG_FLYWAIT);
        }
        break;
    case BOSS_STINKBUG_FTURNSIGN:
        speed = G(14.0f);
        if(actor_anim_frame_done(a, 40)) boss_stinkbug_set(a, BOSS_STINKBUG_FTURN, ANIM_BOSSSTINKBUG_FLYTURN);
        break;
    case BOSS_STINKBUG_FTURN:
        speed = a->timer < 60 ? G(14.0f) * (60 - a->timer) / 60 : G(14.0f) * (a->timer - 60) / 60;
        if(a->timer == 60) a->target.x = -a->target.x;
        if(a->timer > 120) boss_stinkbug_set(a, BOSS_STINKBUG_FLY, ANIM_BOSSSTINKBUG_FLYWAIT);
        break;
    case BOSS_STINKBUG_FALL:
        a->velocity.z = fx_max(a->velocity.z - fx_min(a->timer * 2, 30), 0);
        if(a->velocity.z == 0) boss_stinkbug_set(a, BOSS_STINKBUG_FALLDAMAGE, ANIM_BOSSSTINKBUG_FALLDAMAGE);
        break;
    case BOSS_STINKBUG_FALLDAMAGE:
        if(actor_anim_frame_done(a, 90))
            boss_stinkbug_phase(a, phase == BOSS_STINKBUG_PHASE_FLYLOW ? BOSS_STINKBUG_PHASE_ANGRYDEMO
                                                                       : BOSS_STINKBUG_PHASE_FINISH);
        break;
    }
    if(a->state != BOSS_STINKBUG_DEMO && a->state != BOSS_STINKBUG_DAMAGE && a->state != BOSS_STINKBUG_FALL &&
       a->state != BOSS_STINKBUG_FALLDAMAGE)
        a->velocity.y = speed;
    else a->velocity.y = 0;

    if(!ground && a->state != BOSS_STINKBUG_FALL && a->state != BOSS_STINKBUG_FALLDAMAGE &&
       a->state != BOSS_STINKBUG_DEMO){
        fx want = phase == BOSS_STINKBUG_PHASE_FLYHIGH
                      ? FX(1000)
                      : FX(900) + (fx_sin((fx)(g_game.tick * 13 % FX_TAU)) * 100 >> 8);
        a->velocity.z += fx_clamp(want - a->velocity.z, -FX(15), FX(15));
    }

    a->velocity.x += a->target.x > 0 ? a->velocity.y : -a->velocity.y;
    if(rail_get(rail)->closed && len > 0){
        int32_t period = len << 8;
        a->velocity.x %= period;
        if(a->velocity.x < 0) a->velocity.x += period;
    } else {
        if(a->velocity.x < 0){
            a->velocity.x = 0;
            a->target.x = 1;
        }
        if(a->velocity.x > len << 8){
            a->velocity.x = len << 8;
            a->target.x = -1;
        }
    }
    v3 rp = rail_position_at(rail, a->velocity.x >> 8);
    a->up = gravity_up_at(rp, a->up);
    a->position = v3_add(rp, v3_scale14(a->up, a->velocity.z));
    v3 dir = rail_tangent(rail, a->velocity.x >> 8, a->front);
    if(a->target.x < 0) dir = v3_neg(dir);
    a->front = v3_turn_toward(a->front, dir, a->up, FXC(0.1745f));

    if(!ground && (a->state == BOSS_STINKBUG_FLY || a->state == BOSS_STINKBUG_SHAKESIGN ||
                   a->state == BOSS_STINKBUG_ROLLSIGN || a->state == BOSS_STINKBUG_ROLL ||
                   a->state == BOSS_STINKBUG_ROLLWAIT)){
        if(--a->target.y <= 0){
            boss_stinkbug_throw_bomb(a);
            a->target.y = phase == BOSS_STINKBUG_PHASE_FLYHIGH ? 41 : 61;
        }
    }

    if(a->state == BOSS_STINKBUG_DEMO || a->state == BOSS_STINKBUG_DAMAGE || a->state == BOSS_STINKBUG_FALL ||
       a->state == BOSS_STINKBUG_FALLDAMAGE)
        return;
    if(hip){
        audio_play_effect(SFX_ENEMY_DEFEAT, 5);
        camera_shake(2);
        boss_stinkbug_dispose_bombs();
        if(ground) boss_stinkbug_set(a, BOSS_STINKBUG_DAMAGE, ANIM_BOSSSTINKBUG_GROUNDDAMAGE);
        else boss_stinkbug_set(a, BOSS_STINKBUG_FALL, ANIM_BOSSSTINKBUG_FALL);
        mario_talk_pause(90);
        return;
    }
    if(on_back){
        if(ground && (a->state == BOSS_STINKBUG_WALK || a->state == BOSS_STINKBUG_RUN))
            boss_stinkbug_set(a, BOSS_STINKBUG_SHAKEOFF, ANIM_BOSSSTINKBUG_GROUNDRIDE);
        if(!ground && (a->state == BOSS_STINKBUG_FLY || a->state == BOSS_STINKBUG_ROLLEND))
            boss_stinkbug_set(a, BOSS_STINKBUG_SHAKESIGN, ANIM_BOSSSTINKBUG_FLYRIDE);
        return;
    }

    v3 to = v3_sub(p->figure.position, a->position);
    if(p->action != MA_DEATH && v3_length_squared64(to) < (uint64_t)FX(420) * FX(420) &&
       v3_dot14(to, a->up) < FX(100) &&
       angle_between14(v3_tangent14(to, a->up, a->front), a->front) < FXC(0.7854f)){
        if(ground && (a->state == BOSS_STINKBUG_RUN || a->state == BOSS_STINKBUG_SHAKEOFFRUN)){
            mario_damage(a->position, 1);
            boss_stinkbug_set(a, BOSS_STINKBUG_ATTACKOK, ANIM_BOSSSTINKBUG_ATTACKSUCCESS);
        } else if(!ground){
            mario_damage(a->position, 1);
        }
    }
}

static inline bool boss_stinkbug_solid(const Actor* a){
    return a->value != BOSS_STINKBUG_PHASE_WAITSTART && a->value != BOSS_STINKBUG_PHASE_END &&
           a->state != BOSS_STINKBUG_ROLL;
}

static void boss_bomb_think(Actor* b){
    Player* p = &g_game.player;
    if(b->state == ACTOR_STATE_DONE){
        fx r = FX(280) * fx_min(b->timer, 10) / 10;
        if(p->action != MA_DEATH &&
           v3_length_squared64(v3_sub(mario_centre(), b->position)) < (uint64_t)r * r)
            mario_damage(b->position, 1);
        if(b->timer > 20) b->active = 0;
        return;
    }
    b->velocity = v3_sub(b->velocity, v3_scale14(b->target, G(1.2f)));
    v3 next = v3_add(b->position, V3(b->velocity.x >> 8, b->velocity.y >> 8, b->velocity.z >> 8));
    RayHit hit;
    if(collision_ray(v3_add(b->position, v3_scale14(b->target, FX(60))), next, &hit, COL_NO_CAMERA)){
        int32_t vn = v3_dot14(b->velocity, hit.normal);
        if(vn < -G(5.0f)) b->velocity = v3_sub(b->velocity, v3_scale14(hit.normal, vn * 18 / 10));
        else b->velocity = v3_sub(b->velocity, v3_scale14(hit.normal, vn));
        b->velocity = v3_scale14(b->velocity, R14(0.9f));
        next = hit.point;
    } else {
        b->velocity = v3_scale14(b->velocity, R14(0.98f));
    }
    b->position = next;
    bool touch = p->action != MA_DEATH && actor_touches_mario(b, 0);
    if(b->timer > 150 || touch){
        if(touch){
            mario_damage(b->position, 1);
            b->active = 0;
        }
        b->state = ACTOR_STATE_DONE;
        b->timer = 0;
        actor_show_model(b, false);
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
    }
}
