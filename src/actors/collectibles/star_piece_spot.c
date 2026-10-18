static void starpiecespot_think(Actor* a){
    bool appeared = actor_has_switch(a, SWITCH_APPEAR);
    if(appeared || (g_mario.attack_timer > 0 && actor_mario_within(a, FX(150)))){
        collectibles_pop_spread(OBJ_STARBIT, a->position, a->up, actor_arg_or(a, 0, 5), 15, 40);
        a->active = 0;
        audio_play_effect(SFX_STARBIT, 2);
    }
}

static void starpiecegroup_think(Actor* a){
    if(!actor_mario_within(a, FX(3000))) return;
    int n = actor_arg_or(a, 0, 6);
    fx r = FX(actor_arg_or(a, 1, 400));
    v3 side = v3_cross14(a->up, a->front);
    int rail = actor_rail(a);
    for(int i = 0; i < n; i++){
        fx ang = (fx)(FX_TAU * i / n);
        v3 dir = v3_add(v3_scale14(side, fx_cos(ang) << 6), v3_scale14(a->front, fx_sin(ang) << 6));
        v3 at = v3_add(a->position, v3_scale14(dir, n > 1 ? r : 0));
        if(rail >= 0) at = rail_position_at(rail, n > 1 ? rail_length(rail) * i / (n - 1) : 0);
        Actor* b = actor_spawn_reuse(OBJ_STARBIT, at, a->up, a->front);
        if(!b) break;
        b->figure.model = (const FigureHeader*)starbit_figures[i % 6];
    }
    a->active = 0;
}

static void starpiecespot_init(Actor* a, const StageObject* o){
    a->args[3] = (a->flags & ACTOR_WAITING) ? 1 : 0;
}
