static inline const int16_t* rail_point_args(int rail, int i){
    return g_game.stage.rail_points[rail_get(rail)->first_point + i].args;
}

static fx rail_point_coord(int rail, int i){
    const StageRail* r = rail_get(rail);
    if(i <= 0 || !r->part_count) return 0;
    if(i >= r->part_count) return r->length;
    return g_game.stage.rail_parts[r->first_part + i].start;
}

static int32_t mappart_scale14(const Actor* a){
    const StageObject* o = actor_stage_object(a);
    return o ? o->scale[1] << 6 : N14_ONE;
}

static int mappart_set_in(Actor* a, int16_t* slot, const int16_t (*tris)[9], int count){
    int s = *slot;
    if(s < 0 || s >= g_col_dyn_sets_used || g_movers[s].key != a->stage_index || g_movers[s].tris != tris){
        s = tris ? movers_alloc(a->stage_index, tris, count) : -1;
        *slot = (int16_t)s;
        if(s >= 0) movers_enable(s, true);
    }
    return s;
}

static int mappart_set(Actor* a, const int16_t (*tris)[9], int count){
    return mappart_set_in(a, &a->value, tris, count);
}

static void mappart_place_kcl(Actor* a, const int16_t (*tris)[9], int count){
    int set = mappart_set(a, tris, count);
    MoverFrame f = mover_frame_of(a->position, a->up, a->front, mappart_scale14(a));
    movers_place(set, &f);
}

enum { RM_MOVE = 64, RM_STOP };

static const int16_t (*railmove_kcl(int type, int* count))[9] {
    switch(type){
    case OBJ_BROADBEANMOVESTEPA: *count = KCL_BROADBEANMOVESTEPA_COUNT; return kcl_broadbeanmovestepa;
    case OBJ_BROADBEANMOVESTEPB: *count = KCL_BROADBEANMOVESTEPB_COUNT; return kcl_broadbeanmovestepb;
    case OBJ_RGTOWERPRESSGROUND: *count = KCL_RGTOWERPRESSGROUNDA_COUNT; return kcl_rgtowerpressgrounda;
    case OBJ_SIDESPIKEMOVESTEP: *count = KCL_SIDESPIKEMOVESTEPA_COUNT; return kcl_sidespikemovestepa;
    }
    *count = 0;
    return NULL;
}

static void mappart_place(Actor* a){
    int count;
    const int16_t (*tris)[9] = railmove_kcl(a->type, &count);
    mappart_place_kcl(a, tris, count);
}

static void railmove_init(Actor* a, const StageObject* o){
    a->value = -1;
    int rail = actor_rail(a);
    a->target = V3(0, 1, 0);
    if(rail >= 0){
        fx best = 0;
        int64_t best_d = INT64_MAX;
        fx len = rail_length(rail);
        for(fx c = 0; c <= len; c += fx_max(len / 16, 1)){
            uint64_t d = v3_length_squared64(v3_sub(rail_position_at(rail, c), a->home));
            if((int64_t)d < best_d){
                best_d = (int64_t)d;
                best = c;
            }
        }
        a->target.x = best;
        a->velocity = v3_sub(a->home, rail_position_at(rail, best));
    }
    actor_set_state(a, RM_MOVE, -1);
    mappart_place(a);
}

static void railmove_think(Actor* a){
    int rail = actor_rail(a);
    if(rail < 0){
        mappart_place(a);
        return;
    }
    fx len = rail_length(rail);
    int n = rail_point_count(rail);
    fx coord = a->target.x;
    int dir = a->target.y >= 0 ? 1 : -1;
    if(a->state == RM_STOP){
        if(--a->target.z <= 0) actor_set_state(a, RM_MOVE, -1);
    } else {
        int at = 0;
        while(at + 1 < n && rail_point_coord(rail, at + 1) <= coord) at++;
        if(dir < 0 && at > 0 && rail_point_coord(rail, at) == coord) at--;
        int16_t speed_arg = rail_point_args(rail, dir > 0 ? at : (at + 1 < n ? at + 1 : at))[0];
        fx speed = FX(speed_arg > 0 ? speed_arg : 3);
        fx next = coord + dir * speed;

        int target_point = dir > 0 ? at + 1 : (rail_point_coord(rail, at) < coord ? at : at - 1);
        if(target_point < 0) target_point = 0;
        if(target_point >= n) target_point = n - 1;
        fx target_coord = rail_point_coord(rail, target_point);
        bool reached = dir > 0 ? next >= target_coord : next <= target_coord;
        if(reached){
            next = target_coord;
            int stop = rail_point_args(rail, target_point)[5];
            bool end = target_point == 0 || target_point == n - 1;
            if(end && !rail_get(rail)->closed){
                int stop_type = rail_get(rail)->args[1] >= 0 ? rail_get(rail)->args[1] : 1;
                if(stop_type == 1) dir = -dir;
                else if(stop_type == 0) stop = 0x7FFF;
                else if(stop_type == 2) next = 0;
            } else if(end && rail_get(rail)->closed && dir > 0 && target_point == n - 1){
                next = 0;
            }
            if(stop > 0){
                a->target.z = stop;
                actor_set_state(a, RM_STOP, -1);
            }
        }
        coord = fx_clamp(next, 0, len);
    }
    a->target.x = coord;
    a->target.y = dir;
    a->position = v3_add(rail_position_at(rail, coord), a->velocity);
    mappart_place(a);
}
