static void collapse_init(Actor* a, const StageObject* o){
    a->value = -1;
    a->target.x = 0;
    actor_set_state(a, CP_WAIT, -1);
    mappart_place_kcl(a, kcl_collapseplane, KCL_COLLAPSEPLANE_COUNT);
}

static void collapse_think(Actor* a){
    switch(a->state){
    case CP_WAIT:
        mappart_place_kcl(a, kcl_collapseplane, KCL_COLLAPSEPLANE_COUNT);
        if(a->value >= 0 && mover_carries_mario(a->value)) actor_set_state(a, CP_COLLAPSE, -1);
        break;
    case CP_COLLAPSE: {
        int time = actor_arg_or(a, 0, 140);
        mappart_place_kcl(a, kcl_collapseplane, KCL_COLLAPSEPLANE_COUNT);
        if(a->timer == 1) audio_play_effect(SFX_COLLAPSE_SHRINK, 2);
        if(a->timer >= time){
            if(a->value >= 0) movers_enable(a->value, false);
            audio_play_effect(SFX_COLLAPSE_VANISH, 3);
            a->flags |= ACTOR_HIDDEN;
            actor_set_state(a, CP_END, -1);
        }
        break;
    }
    case CP_END:
        if(a->value >= 0) movers_enable(a->value, false);
        break;
    }
}

static void collapse_figure(const Actor* a, Figure* f){
    if(a->state == CP_COLLAPSE){
        int time = actor_arg_or(a, 0, 140);
        int32_t s = 64 - 64 * 7 * fx_min(a->timer, time) / (10 * time);
        f->scale = V3(s, 64, s);
    }
}
