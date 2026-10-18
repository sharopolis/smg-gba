enum { PO_FREE = ACTOR_STATE_WAIT, PO_INVALID, PO_RIDE };

static void pole_think(Actor* a){
    Player* p = &g_game.player;
    const StageObject* o = actor_stage_object(a);
    fx height = FX(100) * (o && o->scale[1] ? o->scale[1] : 256) >> 8;
    switch(a->state){
    case PO_INVALID:
        if(p->on_ground || a->timer > 45) a->state = PO_FREE;
        break;
    case PO_FREE: {
        if(!g_mario.jumping || g_mario_link.bind >= 0) break;
        v3 d = v3_sub(p->figure.position, a->position);
        fx h = v3_dot14(d, a->up);
        if(h < 0 || h > height) break;
        v3 r = v3_reject14(d, a->up);
        if(v3_length_squared64(r) > (uint64_t)((int64_t)FX(60) + C_BODY_RADIUS) * (FX(60) + C_BODY_RADIUS))
            break;
        a->velocity.x = fx_clamp(h, FX(120), height) << 8;
        a->velocity.y = 0;
        a->target = v3_direction14(v3_neg(r), p->front);
        mario_bind_start(a);
        mario_restart_anim(ANIM_MARIO_WALLWAIT);
        a->state = PO_RIDE;
        a->timer = 0;
        break;
    }
    case PO_RIDE: {
        if(g_mario_link.bind != actor_index(a)){
            a->state = PO_INVALID;
            a->timer = 0;
            break;
        }
        int32_t* coord = &a->velocity.x;
        bool upk = g_game.keys.held & KEY_UP, downk = g_game.keys.held & KEY_DOWN;
        if(upk){
            *coord = fx_min(*coord + G(6.0f), height << 8);
            a->velocity.y = 0;
        } else if(downk){
            a->velocity.y = fx_max(a->velocity.y - G(0.3f), -G(15.0f));
            *coord += a->velocity.y;
        } else a->velocity.y = 0;
        v3 face = a->target;
        v3 pos = v3_add(v3_add(a->position, v3_scale14(a->up, *coord >> 8)), v3_scale14(face, -FX(40)));
        mario_bind_pose(pos, a->up, face);
        if(*coord <= (FX(120) << 8) && downk){
            mario_bind_release(V3(0, 0, 0), JUMP_FORCED, ANIM_MARIO_FALL);
            a->state = PO_INVALID;
            a->timer = 0;
        } else if(g_game.keys.pressed & KEY_A){
            mario_bind_release(v3_add(v3_scale14(face, -G(10.0f)), v3_scale14(a->up, G(25.0f))), JUMP_FORCED,
                               ANIM_MARIO_WALLJUMP);
            p->front = v3_tangent14(v3_neg(face), p->up, p->front);
            audio_play_effect(SFX_JUMP_WALL, 2);
            a->state = PO_INVALID;
            a->timer = 0;
        }
        break;
    }
    }
}
