static inline bool gsmall(fx a){ return (uint32_t)(a + 65535) < 131071u; }
static inline bool gsmall3(v3 a){ return gsmall(a.x) && gsmall(a.y) && gsmall(a.z); }
static inline fx gdot(v3 a, const int16_t* n){
    if(gsmall3(a)) return (a.x * n[0] + a.y * n[1] + a.z * n[2]) >> 14;
    return (fx)(((int64_t)a.x * n[0] + (int64_t)a.y * n[1] + (int64_t)a.z * n[2]) >> 14);
}
static inline v3 gscale(const int16_t* n, fx s){
    if(gsmall(s)) return V3((n[0] * s) >> 14, (n[1] * s) >> 14, (n[2] * s) >> 14);
    return V3((fx)(((int64_t)n[0] * s) >> 14), (fx)(((int64_t)n[1] * s) >> 14),
              (fx)(((int64_t)n[2] * s) >> 14));
}
static inline v3 gaxis(const int16_t* n){ return V3(n[0], n[1], n[2]); }

static inline uint64_t gravity_length_squared(v3 a){
    uint32_t x = (uint32_t)fx_abs(a.x), y = (uint32_t)fx_abs(a.y), z = (uint32_t)fx_abs(a.z);
    if((x | y | z) < 26754) return (uint64_t)(x * x + y * y + z * z);
    return v3_length_squared64(a);
}

static inline bool gravity_beyond(uint64_t l2, fx limit){
    return limit >= 0 && l2 >= (uint64_t)((int64_t)limit * limit) && (limit <= 65536 || l2 <= 0xFFFFFFFFu);
}

static bool gravity_towards(v3 p, v3 target, v3* up, fx* distance, v3 fallback, fx limit){
    v3 w = v3_sub(p, target);
    uint64_t l2 = gravity_length_squared(w);
    if(gravity_beyond(l2, limit)) return false;
    *distance = (fx)isqrt64(l2);
    if(*distance == 0) return false;
    *up = v3_direction14(w, fallback);
    return true;
}

static bool gravity_volume_eval(const StageHeader* header, const Gravity* g, v3 p, v3* up, fx* distance){
    v3 c = V3(g->centre[0], g->centre[1], g->centre[2]);
    v3 q = v3_sub(p, c);
    if(g->bound >= 0 && (fx_abs(q.x) > g->bound || fx_abs(q.y) > g->bound || fx_abs(q.z) > g->bound))
        return false;
    const int16_t* ay = g->axis[1];
    bool check_range = true;
    fx limit = g->range >= 0 ? g->range + g->distant : -1;
    switch(g->kind){
    case GRAV_POINT: {
        uint64_t l2 = gravity_length_squared(q);
        if(gravity_beyond(l2, limit)) return false;
        fx d = (fx)isqrt64(l2);
        if(d == 0) return false;
        *up = v3_direction14(q, V3(0, 0, N14_ONE));
        *distance = d;
        break;
    }
    case GRAV_CUBE: {
        int outside = 0;
        v3 target = c;
        for(int k = 0; k < 3; k++){
            fx t = gdot(q, g->axis[k]), e = g->extent[k];
            fx clamp = t;
            if(t < -e){
                if(!(g->faces & (2 << (2 * k)))) return false;
                clamp = -e;
                outside++;
            } else if(t > e){
                if(!(g->faces & (1 << (2 * k)))) return false;
                clamp = e;
                outside++;
            }
            target = v3_add(target, gscale(g->axis[k], clamp));
        }
        if(!outside) return false;
        if(!gravity_towards(p, target, up, distance, gaxis(ay), limit)) return false;
        break;
    }
    case GRAV_PLANE:
        if(g->range >= 0 && gravity_length_squared(q) >= (uint64_t)((int64_t)g->range * g->range))
            return false;
        *up = gaxis(ay);
        *distance = g->base;
        check_range = false;
        break;
    case GRAV_PLANE_BOX: {
        fx t[3];
        bool small = gsmall3(q);
        for(int k = 0; k < 3; k++){
            const int16_t* n = g->axis[k];
            t[k] = small ? (q.x * n[0] + q.y * n[1] + q.z * n[2]) >> 14 : gdot(q, n);
            if(fx_abs(t[k]) > g->extent[k]) return false;
        }
        *up = gaxis(ay);
        *distance = g->base + (g->faces < 3 ? fx_abs(t[g->faces]) : 0);
        check_range = false;
        break;
    }
    case GRAV_PLANE_CYLINDER: {
        fx h = gdot(q, ay);
        if(h < 0 || h > g->extent[1]) return false;
        v3 r = v3_sub(q, gscale(ay, h));
        uint64_t r2 = gravity_length_squared(r);
        if(r2 > (uint64_t)((int64_t)g->extent[0] * g->extent[0])) return false;
        *up = gaxis(ay);
        *distance = g->base + (fx)isqrt64(r2);
        check_range = false;
        break;
    }
    case GRAV_DISK:
    case GRAV_DISK_TORUS: {
        fx y = gdot(q, ay);
        if(!(g->flags & GRAVF_BOTH_SIDES) && y < 0) return false;

        if(limit >= 0 && fx_abs(y) >= limit && limit < 32768) return false;
        v3 r = v3_sub(q, gscale(ay, y));
        fx rr = (fx)isqrt64(gravity_length_squared(r));

        v3 rn = V3(0, 0, 0);
        bool have_rn = false;
        if(g->kind == GRAV_DISK && g->valid_cos > -N14_ONE){
            if(rr) rn = v3_direction14(r, gaxis(g->axis[0]));
            have_rn = true;
            if(v3_dot14(rn, V3(g->ref_side[0], g->ref_side[1], g->ref_side[2])) < g->valid_cos) return false;
        }
        fx rim = -1;
        if(g->kind == GRAV_DISK){
            if(rr > g->extent[0]){
                if(!(g->flags & GRAVF_EDGE)) return false;
                rim = g->extent[0];
            }
        } else {
            fx inner = g->extent[0] - g->extent[1];
            if(rr < inner){
                if(g->faces == 0 || g->faces == 2) return false;
                rim = inner;
            } else if(rr > g->extent[0]){
                if(g->faces == 0 || g->faces == 1) return false;
                rim = g->extent[0];
            }
            if(!rr){
                rn = gaxis(g->axis[0]);
                have_rn = true;
            }
        }
        if(rim < 0){
            *up = y >= 0 ? gaxis(ay) : v3_neg(gaxis(ay));
            *distance = fx_abs(y);
        } else {
            if(!have_rn && rr) rn = v3_direction14(r, gaxis(g->axis[0]));
            if(!gravity_towards(p, v3_add(c, v3_scale14(rn, rim)), up, distance, gaxis(ay), limit))
                return false;
        }
        break;
    }
    case GRAV_SEGMENT: {
        fx t = gdot(q, ay);
        if(g->valid_cos > -N14_ONE){
            v3 r = v3_sub(q, gscale(ay, t));
            v3 rn = (r.x | r.y | r.z) ? v3_direction14(r, gaxis(g->axis[0])) : V3(0, 0, 0);
            if(v3_dot14(rn, V3(g->ref_side[0], g->ref_side[1], g->ref_side[2])) < g->valid_cos) return false;
        }
        if(t < 0){
            if(!(g->flags & GRAVF_EDGE0)) return false;
            t = 0;
        } else if(t > g->extent[1]){
            if(!(g->flags & GRAVF_EDGE1)) return false;
            t = g->extent[1];
        }
        if(!gravity_towards(p, v3_add(c, gscale(ay, t)), up, distance, gaxis(g->axis[0]), limit))
            return false;
        break;
    }
    case GRAV_WIRE: {
        const int32_t (*points)[3] = (const int32_t (*)[3])((const uint8_t*)header + g->extent[0]);
        uint64_t best = UINT64_MAX;
        v3 target = c;
        for(int i = 0; i < g->extent[1] - 1; i++){
            v3 start = V3(points[i][0], points[i][1], points[i][2]);

            if(best != UINT64_MAX){
                v3 out;
                fx e;
                for(int k = 0; k < 3; k++){
                    fx a = (&start.x)[k], b = points[i + 1][k], lo = a < b ? a : b, hi = a < b ? b : a,
                       x = (&p.x)[k];
                    e = x < lo ? lo - x : x > hi ? x - hi : 0;
                    (&out.x)[k] = e;
                }
                if(gravity_length_squared(out) >= best) continue;
            }
            v3 d = V3(points[i + 1][0] - start.x, points[i + 1][1] - start.y, points[i + 1][2] - start.z),
               w = v3_sub(p, start);
            int64_t n = v3_length_squared64(d),
                    dot = (int64_t)w.x * d.x + (int64_t)w.y * d.y + (int64_t)w.z * d.z;
            int t = n ? (int)fx_clamp((dot * N14_ONE) / n, 0, N14_ONE) : 0;
            v3 q = v3_add(start, v3_scale14(d, t));
            uint64_t dist = gravity_length_squared(v3_sub(p, q));
            if(dist < best){
                best = dist;
                target = q;
            }
        }
        if(!gravity_towards(p, target, up, distance, gaxis(ay), limit)) return false;
        break;
    }
    default: return false;
    }
    if(check_range && g->range >= 0 && *distance >= g->range + g->distant) return false;
    if(g->flags & GRAVF_INVERSE) *up = v3_neg(*up);
    return true;
}

static inline uint8_t gravity_scenario_bit(void){
    return g_game.scenario > 0 ? (uint8_t)(1 << (g_game.scenario - 1)) : 1;
}

static const uint16_t* bound_grid_list(const StageHeader* header, uint32_t grid, v3 p, uint32_t* n){
    const uint8_t* blob = (const uint8_t*)header;
    const GravityGrid* gg = (const GravityGrid*)(blob + grid);
    const uint16_t* lists = (const uint16_t*)(blob + gg->lists);
    uint32_t c[3];
    const int32_t* pc = &p.x;
    for(int k = 0; k < 3; k++){
        c[k] = (uint32_t)(pc[k] - gg->origin[k]) >> gg->shift;
        if(pc[k] < gg->origin[k] || c[k] >= gg->dims[k]){
            const uint16_t* l = lists + gg->outside;
            *n = l[0];
            return l + 1;
        }
    }
    const uint16_t* top = (const uint16_t*)(blob + gg->cells);
    uint32_t bs = gg->block_shift, m = (1u << bs) - 1;
    uint32_t bdx = ((uint32_t)gg->dims[0] + m) >> bs, bdy = ((uint32_t)gg->dims[1] + m) >> bs;
    uint32_t block = top[((c[2] >> bs) * bdy + (c[1] >> bs)) * bdx + (c[0] >> bs)];
    const uint16_t* l =
        lists +
        top[gg->top_count + (block << (3 * bs)) + ((((c[2] & m) << bs | (c[1] & m)) << bs) | (c[0] & m))];
    *n = l[0];
    return l + 1;
}

IWRAM_FN static uint32_t gravity_inverse46(uint32_t d){
    int j = 32 - __builtin_clz(d - 1);
    uint32_t rem = 1u << j, q = 0;
    for(int bit = 46 - j; bit >= 0; bit--){
        q <<= 1;
        if(rem >= d){
            rem -= d;
            q |= 1;
        }
        rem <<= 1;
    }
    return q;
}
static void gravity_weigh(int64_t acc[3], v3 up, int64_t r){
    int64_t d = r * r;
    int64_t w = d >= 32768 && d < ((int64_t)1 << 31) ? (int64_t)gravity_inverse46((uint32_t)d)
                                                     : ((int64_t)1 << 46) / d;
    acc[0] += up.x * w;
    acc[1] += up.y * w;
    acc[2] += up.z * w;
}

typedef struct {
    v3 p;
    const StageHeader* header;
    uint32_t generation;
    GravitySample sample;
} GravityCacheEntry;
#define GRAVITY_CACHE_SIZE 8
static EWRAM_BSS GravityCacheEntry g_gravity_cache[GRAVITY_CACHE_SIZE];

static GravitySample gravity_sample(const StageHeader* header, v3 p){
    uint32_t generation = switch_generation() << 8 | gravity_scenario_bit();
    uint32_t h =
        ((uint32_t)p.x * 0x9E3779B1u) ^ ((uint32_t)p.y * 0x85EBCA77u) ^ ((uint32_t)p.z * 0xC2B2AE3Du);
    GravityCacheEntry* e = &g_gravity_cache[h >> 29];
    if(e->header == header && e->generation == generation && e->p.x == p.x && e->p.y == p.y && e->p.z == p.z)
        return e->sample;
    GravitySample s = gravity_sample_uncached(header, p);
    e->p = p;
    e->header = header;
    e->generation = generation;
    e->sample = s;
    return s;
}
_Static_assert(GRAVITY_CACHE_SIZE == 8, "gravity cache index uses the top three hash bits");

static GravitySample gravity_sample_uncached(const StageHeader* header, v3 p){
    const Gravity* volumes = (const Gravity*)((const uint8_t*)header + header->gravities);
    GravitySample best = {V3(0, 0, N14_ONE), 0x7FFFFFFF, -1};
    uint8_t scen = gravity_scenario_bit();
    int best_priority = 0, count = 0;
    bool legacy = false;
    int64_t acc[3] = {0, 0, 0};
    v3 first_up = V3(0, 0, 0);
    int64_t first_r = 1;
    uint32_t n = header->gravity_count;
    const uint16_t* list = header->gravity_grid ? bound_grid_list(header, header->gravity_grid, p, &n) : NULL;
    for(uint32_t j = 0; j < n; j++){
        uint32_t i = list ? list[j] : j;
        const Gravity* g = &volumes[i];
        if(count && g->priority < best_priority) break;
        if(!(g->scenarios & scen)) continue;
        if((g->switch_a >= 0 && !switch_on(g->switch_a)) || (g->switch_b >= 0 && switch_on(g->switch_b)) ||
           (g->switch_appear >= 0 && !switch_on(g->switch_appear)) ||
           (g->switch_sleep >= 0 && !switch_on(g->switch_sleep)))
            continue;
        v3 up;
        fx d;
        if(!gravity_volume_eval(header, g, p, &up, &d)) continue;
        if(!count){
            best_priority = g->priority;
            legacy = g->flags & GRAVF_LEGACY;
            best.up = up;
            best.distance = d - g->distant;
            best.index = (int)i;
        } else if(legacy){
            if(d - g->distant < best.distance){
                best.up = up;
                best.distance = d - g->distant;
                best.index = (int)i;
            }
        }
        if(!legacy){
            if(count == 1) gravity_weigh(acc, first_up, first_r);
            int64_t r = fx_max(d - g->distant, 1);
            if(count) gravity_weigh(acc, up, r);
            else {
                first_up = up;
                first_r = r;
            }
        }
        count++;
    }
    if(count > 1 && !legacy){
        int64_t m = 0;
        for(int k = 0; k < 3; k++){
            int64_t a = acc[k] < 0 ? -acc[k] : acc[k];
            if(a > m) m = a;
        }

        int s = 0;
        uint32_t hi = (uint32_t)(m >> 32);
        if(hi)
            for(s = 32; hi >> (s - 32); s++){
            }
        s = s > 21 ? s - 21 : 0;
        while((m >> s) > (1 << 20)) s++;
        v3 sum = V3((fx)(acc[0] >> s), (fx)(acc[1] >> s), (fx)(acc[2] >> s));
        if(sum.x | sum.y | sum.z) best.up = v3_direction14(sum, best.up);
    }
    return best;
}
