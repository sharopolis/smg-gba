static void power_star_think(Actor* a){
    Player* p = &g_game.player;
    if(star_appear_think(a)) return;
    bool busy = level_power_star(a);
    if(p->action != MA_DEATH && !busy && actor_touches_mario(a, FX(30))){
        a->active = 0;
        game_collect_star(a->args[0]);
    }
}

static void power_star_fly_home(Actor* a){
    int s = a->value++;

    if(s == 80) audio_queue_effect(SFX_POWER_STAR_APPEAR, 5);
    if(s == 200){
        bool wet = water_query(a->home, a->up, NULL);
        audio_queue_effect(wet ? SFX_POWER_STAR_MOVE_END_WATER : SFX_POWER_STAR_MOVE_END, 5);
    }
    v3 up = a->up;
    v3 risen = v3_add(a->target, v3_scale14(up, FX(300)));
    if(s < 80){
        int32_t t = fx_min(s, 60) * N14_ONE / 60;
        fx e = fx_sin(mul14(t, FXC(1.5708f) << 6) >> 6);
        a->position = v3_add(a->target, v3_scale14(up, (FX(300) * e) >> 8));
    } else if(s < 200){
        int32_t t = (s - 80) * N14_ONE / 120;
        v3 d = v3_sub(a->home, risen);
        fx h = fx_clamp(v3_length_exact(v3_reject14(d, up)) / 2, FX(300), FX(5000));
        fx arc = (h * fx_sin(mul14(t, FX_TAU / 2 << 6) >> 6)) >> 8;
        a->position = v3_add(v3_add(risen, v3_scale14(d, t)), v3_scale14(up, arc));
    } else {
        a->position = a->home;
        if(s >= 290) a->value = -1;
    }
}

static void power_star_spawned(Actor* a, const StageObject* o){
    if(o->flags & 1 && !power_star_revealed(a->stage_index)) a->active = 0;
    if(game_star_collected(g_game.stage_index, o->args[0]))
        a->figure.model = (const FigureHeader*)figure_powerstarempty;
}
