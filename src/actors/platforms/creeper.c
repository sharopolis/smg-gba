enum { CR_FREE = ACTOR_STATE_WAIT, CR_FREEINVALID, CR_UP, CR_DOWN };

static void creeper_think(Actor* a){
    Player* p = &g_game.player;
    int rail = actor_rail(a);
    if(rail < 0) return;
    fx len = rail_length(rail);
    int32_t* coord = &a->velocity.x;
    int32_t* speed = &a->velocity.y;
    v3 head = rail_position_at(rail, len);

    a->position = head;
    a->up = rail_tangent(rail, len, a->up);
    a->front = v3_tangent14(a->front, a->up, v3_cross14(a->up, V3(N14_ONE, 0, 0)));
    switch(a->state){
    case CR_FREEINVALID:
        if(a->timer > 20 && actor_mario_distance(a) < FX(200) + len) a->state = CR_FREE;
        break;
    case CR_FREE: {
        if(p->action == MA_DEATH || p->action == MA_TALK || g_mario_link.bind >= 0) break;
        bool ok = g_mario.jumping || p->action == MA_SPIN;
        if(!ok) break;
        v3 body = v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT));
        if(!rail_box_near(rail, body, FX(160))) break;
        fx c = rail_nearest_coord(rail, p->figure.position, FX(50));
        v3 rp = rail_position_at(rail, c);
        if(v3_length_squared64(v3_sub(rp, body)) > (uint64_t)((int64_t)FX(160) * FX(160))) break;
        *coord = fx_max(c, FX(100)) << 8;
        int32_t up_speed = vertical_speed(g_mario.jump_vec, p->up) >> 8;
        mario_bind_start(a);
        if(up_speed > 0){
            *speed = G((float)fx_clamp(up_speed, 5, 15));
            a->state = CR_UP;
        } else {
            *speed = 0;
            a->state = CR_DOWN;
        }
        a->timer = 0;
        audio_play_effect(SFX_LAND, 2);
        mario_restart_anim(ANIM_MARIO_SPINDRIVERWAIT);
        break;
    }
    case CR_UP:
        if((g_game.keys.held & KEY_B) != 0){
            *speed += G(10.0f);
            audio_play_effect(SFX_SPIN, 1);
        }
        *speed = fx_min(*speed - G(0.4f), G(15.0f));
        if(*speed < 0){
            *speed = 0;
            a->state = CR_DOWN;
            break;
        }
        *coord += *speed;
        if(*coord >= len << 8){
            v3 lean = v3_sub(head, rail_position_at(rail, 0));
            v3 gup = g_game.player.gravity_up;
            v3 away = v3_direction14(v3_neg(v3_reject14(lean, gup)), p->front);
            p->front = v3_tangent14(away, gup, p->front);
            mario_bind_pose(head, gup, p->front);
            mario_bind_release(v3_add(v3_scale14(away, G(25.0f)), v3_scale14(gup, G(40.0f))), 13,
                               ANIM_MARIO_JUMPROLL);
            g_mario.spin_used = true;
            a->state = CR_FREEINVALID;
            a->timer = 0;
            audio_play_effect(SFX_JUMP3, 3);
            return;
        }
        break;
    case CR_DOWN:
        if((g_game.keys.held & KEY_B) != 0){
            *speed = G(5.0f);
            a->state = CR_UP;
            break;
        }
        *speed = fx_min(*speed + G(0.4f), G(20.0f));
        *coord -= *speed;
        if(*coord <= FX(80) << 8){
            mario_bind_release(V3(0, 0, 0), JUMP_FORCED, ANIM_MARIO_FALL);
            a->state = CR_FREEINVALID;
            a->timer = 0;
            return;
        }
        break;
    }
    if(a->state == CR_UP || a->state == CR_DOWN){
        if(g_mario_link.bind != actor_index(a)){
            a->state = CR_FREEINVALID;
            a->timer = 0;
            return;
        }
        v3 t = rail_tangent(rail, *coord >> 8, p->up);

        fx turn = (fx)(((int64_t)*speed * 3 / 2) * 1144 / (256 * 256));
        v3 f = v3_rotate_axis14(v3_tangent14(p->front, t, p->front), t, turn);
        mario_bind_pose(rail_position_at(rail, *coord >> 8), t, f);
    }
}
