static void itembubble_think(Actor* a){
    a->position = v3_add(a->home, v3_scale14(a->up, (fx_sin(FX_TAU * (a->timer % 60) / 60) * 30) >> 8));
    if(actor_mario_contact(a, FX(30)) == CONTACT_NONE) return;
    if(actor_arg_or(a, 0, 0) == 1)
        collectibles_pop_spread(OBJ_STARBIT, a->position, a->up, actor_arg_or(a, 1, 4), 0, 24);
    else
        for(int i = 0; i < actor_arg_or(a, 1, 4); i++)
            actor_spawn_reuse(OBJ_COIN, a->position, a->up, a->front);
    audio_play_effect(SFX_STARBIT, 2);
    a->active = 0;
}
