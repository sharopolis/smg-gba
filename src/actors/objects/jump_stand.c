enum { JS_WAIT = ACTOR_STATE_WAIT, JS_TRAMPLE, JS_HIPDROP };

static void jumpstand_launch(Actor* a, int32_t speed, bool high){
    Player* p = &g_game.player;
    v3 dir = high ? a->up : p->up;
    mario_take_off(high ? JUMP_FORCED : JUMP_FIRST, v3_scale14(dir, G((float)speed)), true);
    mario_set_action(MA_JUMP);
    mario_restart_anim(high ? ANIM_MARIO_JUMPROLL : ANIM_MARIO_JUMP);
    audio_play_effect(SFX_JUMP3, 3);
}

static void jumpstand_think(Actor* a){
    Player* p = &g_game.player;

    if(g_mario_link.ride == actor_index(a) && p->action != MA_TALK){
        bool hip = g_mario_link.hipdrop == actor_index(a);
        a->state = hip ? JS_HIPDROP : JS_TRAMPLE;
        a->timer = 0;
        a->value = 0;
        actor_restart_anim(a, hip ? ANIM_JUMPSTAND_BOUNDHIPDROP : ANIM_JUMPSTAND_BOUND);
        mario_stop_walk();
        mario_set_action(MA_TALK);
        audio_play_effect(SFX_LAND, 2);
        return;
    }
    switch(a->state){
    case JS_WAIT: actor_set_anim(a, ANIM_JUMPSTAND_WAIT); break;
    case JS_TRAMPLE:
        if(a->timer <= 5 && (g_game.keys.held & KEY_A)) a->value = 1;
        if(a->timer == 5){
            jumpstand_launch(a, a->value ? 32 : 26, false);
            g_mario_link.ride = -1;
        }
        if(a->timer > 5 && actor_anim_frame_done(a, 20)) a->state = JS_WAIT;
        break;
    case JS_HIPDROP:
        if(a->timer < 15){
            v3 down = v3_scale14(a->up, -FX(5));
            p->figure.position = v3_add(p->figure.position, down);
            g_mario.position16 =
                V3(p->figure.position.x << 8, p->figure.position.y << 8, p->figure.position.z << 8);
        } else if(a->timer == 15){
            jumpstand_launch(a, 43, true);
            g_mario_link.ride = -1;
        } else if(actor_anim_frame_done(a, 30)) a->state = JS_WAIT;
        break;
    }
}

static inline bool jumpstand_solid(const Actor* a){ return a->state == JS_WAIT || a->timer >= 10; }
