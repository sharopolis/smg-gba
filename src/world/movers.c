static EWRAM_BSS MoverSet g_movers[COL_DYN_SETS];

static int movers_alloc(int key, const int16_t (*tris)[9], int count){
    for(int s = 0; s < g_col_dyn_sets_used; s++)
        if(g_movers[s].key == key && g_movers[s].tris == tris){
            g_movers[s].used_tick = g_game.tick;
            return s;
        }
    if(g_collision.node_count && g_game.stage.header->collision_triangle_count >= COL_DYN_BASE) return -1;
    int s = -1;
    if(g_col_dyn_sets_used < COL_DYN_SETS && g_col_dyn_used + count <= COL_DYN_MAX){
        s = g_col_dyn_sets_used++;
        g_col_dyn_sets[s].first = (uint16_t)g_col_dyn_used;
        g_movers[s].capacity = count;
        g_col_dyn_used += count;
    } else {
        uint32_t oldest = 0;
        for(int k = 0; k < g_col_dyn_sets_used; k++){
            uint32_t age = g_game.tick - g_movers[k].used_tick;
            if(g_movers[k].capacity >= count && age > 120 && age > oldest){
                oldest = age;
                s = k;
            }
        }
        if(s < 0) return -1;
    }
    ColDynSet* d = &g_col_dyn_sets[s];
    d->count = (uint16_t)count;
    d->on = 0;
    int cap = g_movers[s].capacity;
    g_movers[s] =
        (MoverSet){.tris = tris, .count = count, .capacity = cap, .key = key, .used_tick = g_game.tick};
    return s;
}

static inline v3 mover_to_world(const MoverFrame* f, v3 local){
    local = v3_scale14(local, f->scale);
    return v3_add(f->position, v3_add(v3_add(v3_scale14(f->x, local.x), v3_scale14(f->y, local.y)),
                                      v3_scale14(f->z, local.z)));
}

static inline v3 mover_offset_local(const MoverFrame* f, v3 world){
    v3 d = v3_sub(world, f->position);
    return V3(v3_dot14(d, f->x), v3_dot14(d, f->y), v3_dot14(d, f->z));
}

static inline v3 mover_offset_world(const MoverFrame* f, v3 local){
    return v3_add(f->position, v3_add(v3_add(v3_scale14(f->x, local.x), v3_scale14(f->y, local.y)),
                                      v3_scale14(f->z, local.z)));
}

static bool mover_carries_mario(int s){
    Player* p = &g_game.player;
    if(!p->on_ground || g_mario_held || p->action == MA_STAR_LAUNCH) return false;
    const ColDynSet* d = &g_col_dyn_sets[s];
    if(!d->on) return false;
    CollisionCandidates cand;
    cand.count = 0;
    for(int t = 0; t < d->count; t++) cand.index[cand.count++] = (uint16_t)(COL_DYN_BASE + d->first + t);
    RayHit hit;
    v3 feet = p->figure.position;
    return collision_raycast(&cand, v3_add(feet, v3_scale14(p->up, FX(30))),
                             v3_sub(feet, v3_scale14(p->up, FX(40))), &hit, 0) &&
           v3_dot14(hit.normal, p->up) > R14(0.5f);
}

static void movers_place(int s, const MoverFrame* f){
    if(s < 0) return;
    MoverSet* m = &g_movers[s];
    ColDynSet* d = &g_col_dyn_sets[s];
    m->used_tick = g_game.tick;
    if(m->placed && !memcmp(&m->frame, f, sizeof *f)) return;

    bool keep_normals =
        m->placed && m->frame.scale == f->scale && !memcmp(&m->frame.x, &f->x, 3 * sizeof(v3));
    if(m->placed && mover_carries_mario(s)){
        Player* p = &g_game.player;
        v3 local = mover_offset_local(&m->frame, p->figure.position);
        v3 moved = v3_sub(mover_offset_world(f, local), p->figure.position);
        mario_nudge(moved);

        v3 fl = V3(v3_dot14(p->front, m->frame.x), v3_dot14(p->front, m->frame.y),
                   v3_dot14(p->front, m->frame.z));
        v3 front = v3_add(v3_add(v3_scale14(f->x, fl.x), v3_scale14(f->y, fl.y)), v3_scale14(f->z, fl.z));
        p->front = v3_tangent14(front, p->up, p->front);
    }
    m->frame = *f;
    m->placed = true;
    int shift = g_collision.shift;
    int32_t lo[3] = {INT32_MAX, INT32_MAX, INT32_MAX}, hi[3] = {INT32_MIN, INT32_MIN, INT32_MIN};
    for(int t = 0; t < m->count; t++){
        CollisionTriangle* c = &g_col_dyn[d->first + t];
        v3 w[3];
        for(int k = 0; k < 3; k++){
            w[k] = mover_to_world(f, V3(m->tris[t][k * 3], m->tris[t][k * 3 + 1], m->tris[t][k * 3 + 2]));
            int32_t q[3] = {w[k].x >> shift, w[k].y >> shift, w[k].z >> shift};
            for(int a = 0; a < 3; a++){
                c->v[k][a] = (int16_t)q[a];
                if(q[a] < lo[a]) lo[a] = q[a];
                if(q[a] > hi[a]) hi[a] = q[a];
            }
        }
        v3 n;
        if(keep_normals){
            n = V3(c->normal[0], c->normal[1], c->normal[2]);
        } else {
            v3 e1 = v3_sub(w[1], w[0]), e2 = v3_sub(w[2], w[0]);
            int64_t nx = (int64_t)e1.y * e2.z - (int64_t)e1.z * e2.y,
                    ny = (int64_t)e1.z * e2.x - (int64_t)e1.x * e2.z,
                    nz = (int64_t)e1.x * e2.y - (int64_t)e1.y * e2.x;
            while(nx > (1 << 24) || nx < -(1 << 24) || ny > (1 << 24) || ny < -(1 << 24) || nz > (1 << 24) ||
                  nz < -(1 << 24)){
                nx >>= 1;
                ny >>= 1;
                nz >>= 1;
            }
            n = v3_direction14(V3((int32_t)nx, (int32_t)ny, (int32_t)nz), f->y);
        }
        c->normal[0] = (int16_t)n.x;
        c->normal[1] = (int16_t)n.y;
        c->normal[2] = (int16_t)n.z;
        c->attr = 0;

        c->plane = (int32_t)(((int64_t)n.x * w[0].x + (int64_t)n.y * w[0].y + (int64_t)n.z * w[0].z) >> 14);
    }
    for(int a = 0; a < 3; a++){
        d->lo[a] = (int16_t)(lo[a] - 1);
        d->hi[a] = (int16_t)(hi[a] + 1);
    }
}

static void movers_enable(int s, bool on){
    if(s < 0) return;
    g_col_dyn_sets[s].on = on;
    g_movers[s].used_tick = g_game.tick;
}

static MoverFrame mover_frame_of(v3 position, v3 up, v3 front, int32_t scale14){
    v3 z = v3_tangent14(front, up, V3(0, N14_ONE, 0));
    return (MoverFrame){position, v3_cross14(up, z), up, z, scale14};
}
