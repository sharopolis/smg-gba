enum { WATERPRESSURE_WAIT = ACTOR_STATE_WAIT, WATERPRESSURE_PREPARE, WATERPRESSURE_SHOT };
#define WATERPRESSURE_SOUND_DISTANCE FX(7000)

static void waterpressure_sound(v3 at, int effect){
    v3 d = v3_sub(at, mario_centre());
    if(fx_abs(d.x) > WATERPRESSURE_SOUND_DISTANCE || fx_abs(d.y) > WATERPRESSURE_SOUND_DISTANCE ||
       fx_abs(d.z) > WATERPRESSURE_SOUND_DISTANCE)
        return;
    if(v3_length_squared64(d) >
       (uint64_t)((int64_t)WATERPRESSURE_SOUND_DISTANCE * WATERPRESSURE_SOUND_DISTANCE))
        return;
    audio_play_effect(effect, 2);
}

static void waterpressure_fire(Actor* c){
    int16_t pitch = actor_stage_arg(c, 0), speed = actor_stage_arg(c, 2), type = actor_stage_arg(c, 3),
            nospin = actor_stage_arg(c, 7);

    fx ang = (fx)((int32_t)(pitch < 0 ? 0 : pitch) * 1144 / 256);
    v3 dir = v3_add(v3_scale14(c->front, fx_cos(ang) << 6), v3_scale14(c->up, fx_sin(ang) << 6));
    dir = v3_direction14(dir, c->front);
    v3 muzzle = v3_add(v3_add(c->position, v3_scale14(c->up, FX(120))), v3_scale14(dir, FX(200)));
    Actor* b = actor_spawn(OBJ_WATERBULLET, muzzle, c->up, dir);
    if(!b) return;
    b->velocity = v3_scale14(dir, G((float)(speed < 0 ? 30 : speed)));
    b->value = (int16_t)((type < 0 ? 0 : type) | (nospin >= 0 ? 0x100 : 0));
    b->state = ACTOR_STATE_WAIT;
    b->target = c->up;
}

static void waterpressure_think(Actor* a){
    int16_t wait = actor_stage_arg(a, 1);
    if(wait < 0) wait = 300;
    switch(a->state){
    case WATERPRESSURE_WAIT:
        actor_set_anim(a, ANIM_WATERPRESSURE_SHOTSTART);
        if(a->timer >= wait){
            a->state = WATERPRESSURE_PREPARE;
            a->timer = 0;
            actor_restart_anim(a, ANIM_WATERPRESSURE_SHOTSTART);
            waterpressure_sound(a->position, SFX_W_PRESS_PREPARE);
        }
        break;
    case WATERPRESSURE_PREPARE:
        if(actor_anim_frame_done(a, 60)){
            a->state = WATERPRESSURE_SHOT;
            a->timer = 0;
            actor_restart_anim(a, ANIM_WATERPRESSURE_SHOT);
            waterpressure_sound(a->position, SFX_W_PRESS_SHOT);
        }
        break;
    case WATERPRESSURE_SHOT:
        if(a->timer == 16) waterpressure_fire(a);
        if(a->timer > 16 && actor_anim_frame_done(a, 40)){
            a->state = WATERPRESSURE_WAIT;
            a->timer = 0;
        }
        break;
    }
}

static void waterbullet_pop(Actor* b){
    Player* p = &g_game.player;
    if(g_mario_link.bind == actor_index(b)){
        p->up = p->gravity_up;
        mario_bind_release(b->velocity, 13, ANIM_MARIO_FALL);
    }
    b->active = 0;
    waterpressure_sound(b->position, SFX_W_PRESS_BUBBLE_BREAK);
}

static void waterbullet_think(Actor* b){
    Player* p = &g_game.player;
    bool ride = g_mario_link.bind == actor_index(b);
    if((b->value & 0xFF) == 0) b->velocity = v3_sub(b->velocity, v3_scale14(b->target, G(0.4f)));
    v3 next = v3_add(b->position, V3(b->velocity.x >> 8, b->velocity.y >> 8, b->velocity.z >> 8));
    RayHit hit;
    if(b->timer > 2 &&
       collision_ray(b->position, v3_add(next, v3_scale14(v3_direction14(b->velocity, b->front), FX(100))),
                     &hit, COL_NO_CAMERA)){
        waterbullet_pop(b);
        return;
    }
    b->position = next;
    b->front = v3_tangent14(v3_direction14(b->velocity, b->front), b->target, b->front);
    if(b->timer >= ((b->value & 0x100) ? 300 : 180)){
        waterbullet_pop(b);
        return;
    }
    if(ride){
        mario_bind_pose(v3_sub(b->position, v3_scale14(p->up, C_BODY_HEIGHT)), p->up, p->front);
        if(spin_pressed() && !(b->value & 0x100)){
            mario_bind_release(V3(0, 0, 0), JUMP_FORCED, ANIM_MARIO_SPIN);
            b->active = 0;
            audio_play_effect(SFX_W_PRESS_SPIN_OUT, 2);
        }
        return;
    }
    if(g_mario_link.bind < 0 && p->action != MA_DEATH && actor_touches_mario(b, 0)){
        int32_t down = -v3_dot14(v3_direction14(b->velocity, b->front), p->up);
        if(p->on_ground && down > R14(0.5f)){
            waterbullet_pop(b);
            return;
        }
        mario_bind_start(b);
        mario_restart_anim(ANIM_MARIO_SPINDRIVERWAIT);
        audio_play_effect(SFX_W_PRESS_BUBBLE_IN, 2);
    }
}
