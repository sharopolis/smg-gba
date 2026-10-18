enum {
    FOUNTAIN_WAIT = ACTOR_STATE_WAIT,
    FOUNTAIN_SIGN,
    FOUNTAIN_SIGN_STOP,
    FOUNTAIN_SPOUT,
    FOUNTAIN_SPOUT_END
};

static void fountain_think(Actor* a){
    Player* p = &g_game.player;
    switch(a->state){
    case FOUNTAIN_WAIT:
        actor_show_model(a, false);
        if(a->timer >= 120){
            a->state = FOUNTAIN_SIGN;
            a->timer = 0;
        }
        break;
    case FOUNTAIN_SIGN:
        if(a->timer >= 80){
            a->state = FOUNTAIN_SIGN_STOP;
            a->timer = 0;
        }
        break;
    case FOUNTAIN_SIGN_STOP:
        if(a->timer >= 30){
            a->state = FOUNTAIN_SPOUT;
            a->timer = 0;
            a->value = -1;
            actor_show_model(a, true);
        }
        break;
    case FOUNTAIN_SPOUT: {
        if(a->value >= 0 && ++a->value >= 60 && p->on_ground) a->value = -1;
        if(a->timer >= 180){
            a->state = FOUNTAIN_SPOUT_END;
            a->timer = 0;
            break;
        }
        if(a->value >= 0 || p->action == MA_DEATH) break;

        v3 c = v3_sub(v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT)), a->position);
        fx h = fx_clamp(v3_dot14(c, a->up), FX(60), FX(600));
        v3 off = v3_sub(c, v3_scale14(a->up, h));
        if(v3_length_squared64(off) >
           (uint64_t)((int64_t)(FX(60) + C_BODY_RADIUS) * (FX(60) + C_BODY_RADIUS)))
            break;
        if(bee_active()){
            v3 away = v3_tangent14(off, p->up, v3_neg(p->front));
            mario_take_off(JUMP_FIRST, v3_add(v3_scale14(away, G(10.0f)), v3_scale14(p->up, G(10.0f))), true);
            mario_set_action(MA_DAMAGE_FALL);
            mario_restart_anim(ANIM_MARIO_DAMAGEAIR);
            a->value = 0;
        } else {
            mario_take_off(11, v3_add(v3_scale14(p->front, G(2.0f)), v3_scale14(p->up, G(18.0f))), true);
            mario_set_action(MA_FALL);
            mario_restart_anim(ANIM_MARIO_DAMAGEAIR);
        }
        break;
    }
    case FOUNTAIN_SPOUT_END:
        if(a->timer >= 10){
            a->state = FOUNTAIN_WAIT;
            a->timer = 0;
        }
        break;
    }
}
