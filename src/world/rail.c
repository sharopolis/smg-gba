static inline const StageRail* rail_get(int rail){ return &g_game.stage.rails[rail]; }
static inline fx rail_length(int rail){ return rail_get(rail)->length; }
static inline int rail_point_count(int rail){ return rail_get(rail)->point_count; }

static v3 rail_point(int rail, int i){
    const StageRailPoint* p = &g_game.stage.rail_points[rail_get(rail)->first_point + i];
    return V3(p->position[0], p->position[1], p->position[2]);
}

static v3 rail_part_eval(const StageRailPart* part, int32_t t){
    v3 p0 = V3(part->p[0][0], part->p[0][1], part->p[0][2]);
    int32_t u = N14_ONE - t;
    int32_t b1 = mul14(mul14(u, u), t) * 3, b2 = mul14(mul14(u, t), t) * 3, b3 = mul14(mul14(t, t), t);
    v3 r = p0;
    for(int k = 1; k < 4; k++){
        int32_t b = k == 1 ? b1 : k == 2 ? b2 : b3;
        v3 d = V3(part->p[k][0] - p0.x, part->p[k][1] - p0.y, part->p[k][2] - p0.z);
        r = v3_add(r, V3((fx)(((int64_t)d.x * b) >> 14), (fx)(((int64_t)d.y * b) >> 14),
                         (fx)(((int64_t)d.z * b) >> 14)));
    }
    return r;
}

static int32_t rail_part_t(const StageRailPart* part, fx along){
    if(part->length <= 0) return 0;
    uint32_t share = along <= 0              ? 0
                     : along >= part->length ? 65535u
                                             : (uint32_t)(((int64_t)along << 16) / part->length);
    int k = 0;
    while(k < 15 && part->lut[k] < share) k++;
    uint32_t lo = k ? part->lut[k - 1] : 0, hi = part->lut[k];
    int32_t frac = hi > lo ? (int32_t)(((share - lo) << 10) / (hi - lo)) : 0;
    if(frac > 1024) frac = 1024;
    return (k << 10) + frac;
}

static v3 rail_position_at(int rail, fx coord){
    const StageRail* r = rail_get(rail);
    if(!r->part_count) return rail_point(rail, 0);
    if(r->closed && r->length > 0){
        coord %= r->length;
        if(coord < 0) coord += r->length;
    } else {
        coord = fx_clamp(coord, 0, r->length);
    }
    const StageRailPart* parts = &g_game.stage.rail_parts[r->first_part];
    int i = 0;
    while(i + 1 < r->part_count && coord > parts[i].start + parts[i].length) i++;
    return rail_part_eval(&parts[i], rail_part_t(&parts[i], coord - parts[i].start));
}

__attribute__((noinline)) static bool rail_box_near(int rail, v3 body, fx radius){
    const StageRail* r = rail_get(rail);
    if(!r->part_count){
        v3 p = rail_point(rail, 0), d = v3_sub(body, p);
        return fx_abs(d.x) <= radius && fx_abs(d.y) <= radius && fx_abs(d.z) <= radius;
    }
    const StageRailPart* parts = &g_game.stage.rail_parts[r->first_part];
    for(int part = 0; part < r->part_count; part++){
        bool outside = false;
        for(int axis = 0; axis < 3; axis++){
            fx lo = parts[part].p[0][axis], hi = lo;
            for(int k = 1; k < 4; k++){
                lo = fx_min(lo, parts[part].p[k][axis]);
                hi = fx_max(hi, parts[part].p[k][axis]);
            }
            fx b = (&body.x)[axis];
            if((int64_t)b < (int64_t)lo - 3 - radius || (int64_t)b > (int64_t)hi + radius) outside = true;
        }
        if(!outside) return true;
    }
    return false;
}
