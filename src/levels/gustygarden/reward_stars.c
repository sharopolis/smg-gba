static bool gusty_star_busy(const Actor* a){
    return g_game.stage_index == STAGE_GUSTYGARDEN && a->type == OBJ_POWERSTAR && a->value >= 0;
}

static bool gusty_star_unclipped(const Actor* a){
    return a->type == OBJ_POWERSTAR && a->stage_index >= 0 && power_star_revealed(a->stage_index);
}

static v3 gusty_star_appear_home(const StageObject* o, v3 from){
    v3 home = V3(o->position[0], o->position[1], o->position[2]);
    uint8_t mask = (uint8_t)(1u << (g_game.scenario - 1));
    uint64_t best = UINT64_MAX;

    for(unsigned i = 0; i < sizeof gusty_star_appear_points / sizeof gusty_star_appear_points[0]; i++){
        const GustyStarAppearPoint* p = &gusty_star_appear_points[i];
        if(p->star_id != o->id || p->zone != o->zone || !(p->scenarios & mask)) continue;
        v3 position = V3(p->position[0], p->position[1], p->position[2]);
        uint64_t d = v3_length_squared64(v3_sub(position, from));
        if(d < best){
            best = d;
            home = position;
        }
    }
    return home;
}

static void gusty_star_appear(int id, v3 from){
    unsigned mask = 1u << (g_game.scenario - 1);
    for(unsigned i = 0; i < g_game.stage.header->object_count; i++){
        const StageObject* o = &g_game.stage.objects[i];
        if(o->type != OBJ_POWERSTAR || o->args[0] != id || !(o->scenarios & mask)) continue;
        g_power_star_revealed[i >> 3] |= (uint8_t)(1u << (i & 7));
        Actor* reward = NULL;
        for(int j = 0; j < g_game.actor_count; j++){
            Actor* star = &g_game.actors[j];
            if(star->stage_index != (int)i || star->type != OBJ_POWERSTAR) continue;
            reward = star;
            break;
        }
        if(!reward) reward = actors_spawn_object(&g_game.stage, i);
        if(!reward) continue;
        reward->active = 1;
        reward->timer = 0;
        reward->value = 0;
        reward->target = from;
        reward->position = from;
        reward->home = gusty_star_appear_home(o, from);

        reward->up = gravity_sample(g_game.stage.header, reward->home).up;
        reward->front =
            v3_tangent14(V3(o->front[0], o->front[1], o->front[2]), reward->up, V3(0, 0, N14_ONE));
        g_object_state[i] |= OBJECT_STATE_OUT | OBJECT_STATE_WAS_ACTIVE;
    }
}
