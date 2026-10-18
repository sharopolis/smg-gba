enum { JB_WAIT = 64, JB_BIND, JB_INVALID };
static void jump_branch_init(Actor* a, const StageObject* o){
    actor_set_state(a, JB_WAIT, -1);
}
static void jump_branch_think(Actor* a){
    Player* p = &g_game.player;
    if(a->state == JB_INVALID){
        if(a->timer > 30) actor_set_state(a, JB_WAIT, -1);
        return;
    }
    if(a->state == JB_WAIT){
        if(p->on_ground || g_mario_held || p->action == MA_STAR_LAUNCH || p->action == MA_DEATH ||
           p->action == MA_SWIM)
            return;
        v3 d = v3_sub(mario_centre(), a->position);
        if(fx_abs(v3_dot14(d, a->front)) > 70) return;
        if(v3_length_squared64(d) > (uint64_t)(80 + C_BODY_RADIUS) * (80 + C_BODY_RADIUS)) return;
        mario_hold_begin();
        mario_restart_anim(ANIM_MARIO_SQUAREPOLEWAIT);
        a->value = v3_dot14(p->front, a->front) < 0 ? -1 : 1;
        actor_set_state(a, JB_BIND, -1);
        return;
    }

    if(!g_mario_held || p->action == MA_DEATH){
        actor_set_state(a, JB_INVALID, -1);
        return;
    }
    v3 front = a->value < 0 ? v3_neg(a->front) : a->front;
    mario_hold_move(a->position, front);
    if(g_game.keys.pressed & KEY_A){
        int proj = (g_game.keys.held & KEY_UP) ? 1 : (g_game.keys.held & KEY_DOWN) ? -1 : 0;
        v3 v = v3_add(v3_scale14(front, G(10) * proj), v3_scale14(a->up, G(16)));
        mario_hold_release(v);
        audio_play_effect(SFX_JUMP3, 3);
        actor_set_state(a, JB_INVALID, -1);
    }
}
