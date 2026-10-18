static void icestep_stage_reset(void){ icestep_reset(level_has(LEVEL_ICE_STEPS)); }
static void icestep_kill(Actor* a){
    icestep_cache_invalidate();
    int slot = a->value;
    if(slot >= 0 && slot < ICESTEP_SLOTS && g_icestep_actor[slot] == actor_index(a))
        g_icestep_actor[slot] = -1;
    if(a->active && g_icestep_alive) g_icestep_alive--;
    a->active = 0;
    a->stage_index = -1;
}
static void icestep_destroy(Actor* a){
    a->state = ICESTEP_END;
    a->args[0] = 0;
    a->timer = 0;
    a->anim = ANIM_ICESTEP_END;
    a->anim_frame = 0;
}
static void icestep_think(Actor* a){
    if(a->state == ICESTEP_WAIT){
        if(a->args[0] > 0) a->args[0]--;
        if(a->args[0] == 1) icestep_destroy(a);
    } else if(a->state == ICESTEP_END){
        if(a->timer >= anim_length_of((const FigureHeader*)figure_icestep, ANIM_ICESTEP_END)) icestep_kill(a);
    } else if(a->state == ICESTEP_HIT){
        if(a->timer >= 5) a->flags |= ACTOR_HIDDEN;
        if(a->timer >= 15) icestep_kill(a);
    }
}
static bool icestep_create(v3 position, v3 up, v3 front){
    if(!icestep_stage()) return false;
    int slot = g_icestep_next;
    Actor* a = icestep_actor(slot);
    if(!a){
        a = actor_spawn(OBJ_ICESTEP, position, up, front);
        if(!a && stream_evict(g_game.player.figure.position))
            a = actor_spawn(OBJ_ICESTEP, position, up, front);
        if(!a) return false;
        g_icestep_alive++;
        g_icestep_actor[slot] = (int8_t)actor_index(a);
    }
    icestep_cache_invalidate();
    a->home = position;
    a->up = up;
    a->front = v3_tangent14(front, up, V3(0, N14_ONE, 0));
    int offset = slot / 2;
    if(slot & 1) offset -= 10;

    a->position = v3_add(position, v3_scale14(up, offset / 5));
    a->value = slot;
    a->state = ICESTEP_WAIT;
    a->timer = 0;
    a->args[0] = 1200;
    a->anim = ANIM_ICESTEP_START;
    a->anim_frame = 0;
    a->flags = 0;
    a->figure.model = (const FigureHeader*)figure_icestep;
    Actor* next = icestep_actor((slot + 1) % ICESTEP_SLOTS);
    if(next) icestep_destroy(next);
    g_icestep_next = (slot + 1) % ICESTEP_SLOTS;
    return true;
}
