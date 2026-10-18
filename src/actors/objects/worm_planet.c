static fx worm_rail_point_coord(int rail, int k){
    const StageRail* r = rail_get(rail);
    if(k >= r->part_count) return r->length;
    return g_game.stage.rail_parts[r->first_part + k].start;
}
static int worm_rail_point_arg0(int rail, int k){
    return g_game.stage.rail_points[rail_get(rail)->first_point + k].args[0];
}

static fx worm_planet_segment_end(int rail, int segment){
    const StageRail* r = rail_get(rail);
    for(int k = 0, n = 0; k < r->point_count; k++)
        if(worm_rail_point_arg0(rail, k) == 1 && n++ == segment) return worm_rail_point_coord(rail, k);
    return r->length;
}

static void worm_planet_eat_chunks(fx coord, int segment){
    for(int k = 0; k < GUSTY_WORM_CHUNKS; k++){
        const GustyWormChunk* c = &gusty_worm_chunks[k];
        if(c->segment < segment || (c->segment == segment && coord >= c->coord_to)) switch_set(c->sw, true);
    }
}

static int worm_planet_segments_wanted(const Actor* a){
    if(switch_on(actor_switch_key(a, SWITCH_B))) return 3;
    if(switch_on(actor_switch_key(a, SWITCH_A))) return 2;
    return switch_on(2000 + 1000 * 14 + 100) ? 1 : 0;
}

static Actor* actor_find_type(int type){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* b = &g_game.actors[i];
        if(b->active && b->type == type) return b;
    }
    return NULL;
}

static v3 rail_direction_at(int rail, fx coord, v3 fallback){
    v3 a = rail_position_at(rail, coord - FX(20)), b = rail_position_at(rail, coord + FX(20));
    return v3_direction14(v3_sub(b, a), fallback);
}

static void caterpillar_place(Actor* head, int rail, fx coord){
    head->position = rail_position_at(rail, coord);
    GravitySample g = gravity_sample(g_game.stage.header, head->position);
    if(g.index >= 0) head->up = g.up;
    head->front = rail_direction_at(rail, coord, head->front);
    head->up = v3_tangent14(head->up, head->front, head->up);
}

static void worm_planet_init(Actor* a, const StageObject* o){
    a->flags |= ACTOR_HIDDEN;
    a->radius = 0;
    a->value = (int16_t)stream_part_state(a->stage_index);
    actor_set_state(a, WORM_PLANET_WAIT, -1);
}

static void worm_planet_spawn_hill(v3 at, v3 up, v3 front){
    Actor* h = actor_spawn_reuse(OBJ_WORMEATENHILL, at, up, front);
    if(h) h->timer = 0;
}

static void worm_planet_think(Actor* a){
    int rail = actor_rail(a);
    if(rail < 0) return;
    int done = a->value, wanted = worm_planet_segments_wanted(a);
    Player* p = &g_game.player;
    if(a->state == WORM_PLANET_WAIT){
        if(done < 3) worm_planet_eat_chunks(-1, done);
        if(wanted > done){
            Actor* head = actor_find_type(OBJ_CATERPILLAR);
            fx from = done ? worm_planet_segment_end(rail, done - 1) : 0;
            if(!head)
                head = actor_spawn_reuse(OBJ_CATERPILLAR, rail_position_at(rail, from), a->up, a->front);
            if(!head) return;
            head->home.x = from;
            head->args[0] = (int16_t)done;
            head->args[1] = (int16_t)rail;
            head->args[2] = 0;
            caterpillar_place(head, rail, from);
            actor_set_state(head, CATERPILLAR_WRIGGLE, ANIM_GREENCATERPILLARBIGFACE_EAT);
            actor_set_state(a, WORM_PLANET_EATING, -1);
            if(p->action != MA_DEATH) mario_set_action(MA_TALK);
        } else if(done > 0 && !actor_find_type(OBJ_CATERPILLAR)){
            fx at = worm_planet_segment_end(rail, done - 1);
            Actor* head = actor_spawn_reuse(OBJ_CATERPILLAR, rail_position_at(rail, at), a->up, a->front);
            if(head){
                head->home.x = at;
                head->args[0] = (int16_t)(done - 1);
                head->args[1] = (int16_t)rail;
                caterpillar_place(head, rail, at);
                actor_set_state(head, done >= 3 ? CATERPILLAR_END : CATERPILLAR_REST,
                                done >= 3 ? ANIM_GREENCATERPILLARBIGFACE_WAIT
                                          : ANIM_GREENCATERPILLARBIGFACE_EAT);
            }
        }
        return;
    }

    Actor* head = actor_find_type(OBJ_CATERPILLAR);
    if(p->action != MA_TALK && p->action != MA_DEATH && a->timer < 4) mario_set_action(MA_TALK);
    if(!head || head->state != CATERPILLAR_WRIGGLE || a->timer > 60 * 8){
        a->value = (int16_t)(done + 1);
        stream_part_state_set(a->stage_index, (unsigned)a->value);
        worm_planet_eat_chunks(-1, a->value);
        if(p->action == MA_TALK) mario_set_action(MA_WAIT);
        actor_set_state(a, WORM_PLANET_WAIT, -1);
    }
}

static void caterpillar_think(Actor* a){
    int rail = a->args[1];
    if(rail < 0 || !actor_find_type(OBJ_WORMEATENPLANET)){
        a->active = 0;
        return;
    }
    if(a->state != CATERPILLAR_WRIGGLE) return;
    fx end = worm_planet_segment_end(rail, a->args[0]);
    fx before = a->home.x, coord = fx_min(before + FX(50), end);
    a->home.x = coord;
    caterpillar_place(a, rail, coord);
    worm_planet_eat_chunks(coord, a->args[0]);
    const StageRail* r = rail_get(rail);
    for(int k = 0; k < r->point_count; k++){
        fx c = worm_rail_point_coord(rail, k);
        if(c <= before || c > coord) continue;
        int arg = worm_rail_point_arg0(rail, k);
        if(arg == 0 || arg == 2){
            camera_shake(1);
            audio_play_effect(SFX_HIPDROP_LAND, 2);
            worm_planet_spawn_hill(rail_point(rail, k), a->up, a->front);
        }
    }
    if(coord >= end){
        bool last = coord >= r->length;
        actor_set_state(a, last ? CATERPILLAR_END : CATERPILLAR_REST,
                        last ? ANIM_GREENCATERPILLARBIGFACE_WAIT : ANIM_GREENCATERPILLARBIGFACE_EAT);
    }
}

static void worm_hill_think(Actor* a){
    if(!actor_find_type(OBJ_WORMEATENPLANET)) a->active = 0;
}
