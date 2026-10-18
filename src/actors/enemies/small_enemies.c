static void small_enemy_think(Actor* a){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    if(a->state == ACTOR_STATE_DONE){
        if(a->timer > 20) a->active = 0;
        return;
    }
    v3 to = v3_sub(p->figure.position, a->position);
    uint64_t d2 = v3_length_squared64(to);
    switch(a->type){
    case OBJ_TAKOHEI:
        if(d2 < (uint64_t)FX(1600) * FX(1600)){
            a->front = v3_turn_toward(a->front, to, a->up, FXC(0.1047f));
            fx dir = d2 < (uint64_t)FX(700) * FX(700) ? -FX(3) : FX(3);
            v3 np = v3_add(a->position, v3_scale14(a->front, dir));
            if(v3_length_squared64(v3_sub(np, a->home)) < (uint64_t)FX(800) * FX(800)) a->position = np;
            actor_stick_to_ground(a);
        }
        break;
    case OBJ_KARIPON:
        if(d2 < (uint64_t)FX(1000) * FX(1000) && (a->timer % 30) < 12){
            a->front = v3_turn_toward(a->front, to, a->up, FXC(0.25f));
            a->position = v3_add(a->position, v3_scale14(a->front, FX(10)));
            actor_stick_to_ground(a);
        }
        break;
    case OBJ_KOTEBUG:
        if(bee_active() && d2 < (uint64_t)FX(2000) * FX(2000)){
            a->front = v3_turn_toward(a->front, to, a->up, FXC(0.1f));
            a->position = v3_add(a->position, v3_scale14(a->front, FX(4)));
            v3 hp = a->position;
            if(actor_stick_to_ground(a)) a->position = v3_add(a->position, v3_scale14(a->up, FX(100)));
            else a->position = hp;
        } else {
            a->position = v3_add(a->home, V3(0, 0, 0));
        }
        break;
    }
    if(p->action == MA_DEATH) return;
    bool touch = actor_touches_mario(a, 0);
    if(!touch && !mario_spin_reaches(a)) return;
    bool from_above =
        touch && m->jumping && vertical_speed(m->jump_vec, p->up) < 0 && v3_dot14(to, a->up) > FX(60);
    if(touch && (p->action == MA_HIPDROP || from_above)){
        if(p->action != MA_HIPDROP) mario_stomp_bounce(g_game.keys.held & KEY_A);
        if(a->type != OBJ_KARIPON) collectibles_pop_column(a, OBJ_COIN, 1);
        a->state = ACTOR_STATE_DONE;
        a->timer = 0;
        actor_show_model(a, false);
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
    } else if(mario_spin_reaches(a)){
        collectibles_pop_column(a, OBJ_STARBIT, a->type == OBJ_KARIPON ? 1 : 3);
        mario_spin_hop();
        a->state = ACTOR_STATE_DONE;
        a->timer = 0;
        actor_show_model(a, false);
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
    } else if(touch && a->type != OBJ_KARIPON && (a->type != OBJ_KOTEBUG || bee_active())){
        mario_damage(a->position, 1);
    }
}
