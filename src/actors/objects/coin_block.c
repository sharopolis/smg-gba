static void coin_block_think(Actor* a){
    Player* p = &g_game.player;
    if(a->state == ACTOR_STATE_WAIT && a->value == 0) a->value = 20;
    if(a->state == ACTOR_STATE_DONE) return;
    if(a->state == ACTOR_STATE_TALK && a->timer < 240){
    }
    int32_t vs = vertical_speed(g_mario.jump_vec, p->up);
    v3 head = v3_add(p->figure.position, v3_scale14(p->up, FX(160)));
    v3 d = v3_sub(head, a->position);
    if(!g_mario.jumping || vs <= 0 || v3_length_squared64(d) > (uint64_t)FX(130) * FX(130)) return;
    if(-v3_dot14(v3_direction14(d, a->up), a->up) < R14(0.25f)) return;
    if(a->flags & 0x80) return;
    a->flags |= 0x80;
    g_mario.jump_vec = horizontal(g_mario.jump_vec, p->up);
    if(a->state == ACTOR_STATE_WAIT){
        a->state = ACTOR_STATE_TALK;
        a->timer = 0;
    }
    bool last = a->value <= 11 || a->timer >= 240;
    collectibles_pop_column(a, OBJ_STARBIT, last ? a->value : 1);
    a->value -= last ? a->value : 1;
    audio_play_effect(SFX_STARBIT, 2);
    if(a->value <= 0){
        a->state = ACTOR_STATE_DONE;
        a->active = 0;
    }
}

static void coin_block_rearm(Actor* a){
    if(!g_mario.jumping) a->flags &= ~0x80;
}
