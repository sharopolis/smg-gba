enum {
    PLANT_SEED_FAR = ACTOR_STATE_WAIT,
    PLANT_SEED_SEED,
    PLANT_SEED_GROW,
    PLANT_SEED_GROWN,
    PLANT_SEED_INVALID,
    PLANT_SEED_UP,
    PLANT_SEED_DOWN
};

static void plant_seed_think(Actor* a){
    Player* p = &g_game.player;
    int rail = actor_rail(a);
    if(rail < 0) return;
    fx len = rail_length(rail);
    int32_t* coord = &a->velocity.x;
    int32_t* speed = &a->velocity.y;
    fx* grown = &a->velocity.z;

    if(a->timer == 1 && a->state == PLANT_SEED_FAR && (stream_part_state(a->stage_index) & 1u)){
        a->state = PLANT_SEED_GROWN;
        *grown = len;
    }
    if(a->timer == 1 && a->state == PLANT_SEED_FAR && actor_switch_key(a, SWITCH_APPEAR) >= 0)
        a->state = PLANT_SEED_GROW;
    switch(a->state){
    case PLANT_SEED_FAR:
        a->position = rail_position_at(rail, 0);
        if(actor_mario_distance(a) < FX(700)){
            a->state = PLANT_SEED_SEED;
            audio_play_effect(SFX_PLANT_BUD, 2);
        }
        break;
    case PLANT_SEED_SEED:
        if(p->action == MA_SPIN && g_mario.attack_timer > 0 && actor_mario_distance(a) < FX(300)){
            a->state = PLANT_SEED_GROW;
            a->timer = 0;
        }
        break;
    case PLANT_SEED_GROW:
        if(a->timer == 1) audio_play_effect(SFX_PLANT_APPEAR, 2);
        audio_level_effect(SFX_PLANT_GROW);
        *grown = fx_min(*grown + FX(40), len);
        if(*grown >= len){
            stream_part_state_set(a->stage_index, stream_part_state(a->stage_index) | 1u);
            a->state = PLANT_SEED_GROWN;
            a->timer = 0;
            audio_stop_level_effect(SFX_PLANT_GROW);
        }
        break;
    case PLANT_SEED_INVALID:
        if(a->timer > 60 || p->on_ground) a->state = PLANT_SEED_GROWN;
        break;
    case PLANT_SEED_GROWN: {
        if(!g_mario.jumping || g_mario_link.bind >= 0 || p->action == MA_DEATH) break;
        fx c = fx_min(rail_nearest_coord(rail, p->figure.position, FX(30)), *grown);
        v3 body = v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT));
        if(v3_length_squared64(v3_sub(rail_position_at(rail, c), body)) >
           (uint64_t)((int64_t)FX(160) * FX(160)))
            break;
        *coord = c << 8;
        int32_t up_speed = vertical_speed(g_mario.jump_vec, p->up) >> 8;
        mario_bind_start(a);
        audio_play_effect(SFX_PLANT_CATCH_SOURCE, 2);
        mario_restart_anim(ANIM_MARIO_SPINDRIVERWAIT);
        if(c >= len - FX(150) || up_speed < -2){
            *speed = 0;
            a->state = PLANT_SEED_DOWN;
        } else {
            *speed = G((float)fx_clamp(up_speed, 15, 35));
            a->state = PLANT_SEED_UP;
        }
        a->timer = 0;
        break;
    }
    case PLANT_SEED_UP:
        if((g_game.keys.held & KEY_B) != 0) *speed = fx_max(*speed + G(1.0f), G(10.0f));
        *speed = fx_clamp(*speed - G(0.3f), 0, G(35.0f));
        if(*speed <= 0){
            a->state = PLANT_SEED_DOWN;
            break;
        }
        *coord += *speed;
        if(*coord >= len << 8){
            v3 tip_up = rail_tangent(rail, len, p->up);
            int16_t js = actor_stage_arg(a, 0), jy = actor_stage_arg(a, 1);
            v3 v = v3_add(v3_scale14(tip_up, G((float)(js < 0 ? 30 : js))),
                          v3_scale14(g_game.player.gravity_up, G((float)(jy < 0 ? 0 : jy))));
            mario_bind_pose(rail_position_at(rail, len), tip_up, p->front);
            mario_bind_release(v, 13, ANIM_MARIO_JUMPROLL);
            audio_play_effect(SFX_JUMP3, 3);
            a->state = PLANT_SEED_INVALID;
            a->timer = 0;
            return;
        }
        break;
    case PLANT_SEED_DOWN:
        if((g_game.keys.held & KEY_B) != 0 && a->timer > 1){
            *speed = G(10.0f);
            a->state = PLANT_SEED_UP;
            break;
        }
        *speed = fx_min(*speed + G(0.3f), G(20.0f));
        *coord -= *speed;
        if(*coord <= FX(10) << 8){
            mario_bind_release(V3(0, 0, 0), JUMP_FORCED, ANIM_MARIO_FALL);
            a->state = PLANT_SEED_INVALID;
            a->timer = 0;
            return;
        }
        break;
    }
    if(a->state == PLANT_SEED_UP || a->state == PLANT_SEED_DOWN){
        if(g_mario_link.bind != actor_index(a)){
            a->state = PLANT_SEED_INVALID;
            a->timer = 0;
            return;
        }
        v3 t = rail_tangent(rail, *coord >> 8, p->up);
        fx turn = (fx)((int64_t)fx_min(*speed * 13 / 10, G(30.0f)) * 1144 / (256 * 256));
        mario_bind_pose(rail_position_at(rail, *coord >> 8), t,
                        v3_rotate_axis14(v3_tangent14(p->front, t, p->front), t, turn));
    }

    if(a->state != PLANT_SEED_FAR && a->state != PLANT_SEED_SEED){
        a->position = rail_position_at(rail, *grown);
        a->up = rail_tangent(rail, *grown, a->up);
        a->front = v3_tangent14(a->front, a->up, v3_cross14(a->up, V3(N14_ONE, 0, 0)));
    }
}
