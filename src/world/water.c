static const WaterVolume* water_volume_at(unsigned i){
    unsigned count = sizeof water_volumes / sizeof water_volumes[0];
    if(i < count) return &water_volumes[i];
    return NULL;
}

static fx water_reach(const WaterVolume* w){
    fx reach = w->kind == 0   ? w->size[0] + w->size[1] + w->size[2]
               : w->kind == 1 ? w->size[0] + w->size[1]
                              : w->size[0];
    return reach + (reach >> 5) + 16;
}

#define WATER_REGION  1024
#define WATER_REGIONS 4
typedef struct {
    v3 lo, hi;
    int8_t stage;
    uint8_t scenario, valid;
    uint32_t mask[2];
} WaterRegion;
static WaterRegion g_water_regions[WATER_REGIONS];
static uint8_t g_water_region_next;
static bool water_query_raw(v3 position, v3 gravity_up, WaterSample* sample, const WaterVolume** volume){
    uint8_t scenario = g_game.scenario > 0 ? 1u << (g_game.scenario - 1) : 1;
    unsigned stage = (unsigned)g_game.stage_index;
    WaterStageSpan span = {0, 0};
    if(stage < sizeof water_stage_spans / sizeof water_stage_spans[0]) span = water_stage_spans[stage];
    unsigned fixture_count = 0;

    unsigned total = span.count + fixture_count;
    const WaterRegion* region = NULL;
    if(total <= 64){
        for(int r = 0; r < WATER_REGIONS && !region; r++){
            const WaterRegion* c = &g_water_regions[r];
            if(c->valid && c->stage == (int8_t)stage && c->scenario == scenario && position.x >= c->lo.x &&
               position.x <= c->hi.x && position.y >= c->lo.y && position.y <= c->hi.y &&
               position.z >= c->lo.z && position.z <= c->hi.z)
                region = c;
        }
    }
    if(total <= 64 && !region){
        WaterRegion* c = &g_water_regions[g_water_region_next];
        g_water_region_next = (uint8_t)((g_water_region_next + 1) % WATER_REGIONS);
        v3 lo = v3_sub(position, V3(WATER_REGION, WATER_REGION, WATER_REGION)),
           hi = v3_add(position, V3(WATER_REGION, WATER_REGION, WATER_REGION));
        c->mask[0] = c->mask[1] = 0;
        for(unsigned at = 0; at < total; at++){
            unsigned i = at < span.count ? span.first + at
                                         : sizeof water_volumes / sizeof water_volumes[0] + at - span.count;
            const WaterVolume* w = water_volume_at(i);
            if(w->stage != g_game.stage_index || !(w->scenarios & scenario)) continue;
            fx reach = water_reach(w);
            if(w->origin[0] - reach > hi.x || w->origin[0] + reach < lo.x || w->origin[1] - reach > hi.y ||
               w->origin[1] + reach < lo.y || w->origin[2] - reach > hi.z || w->origin[2] + reach < lo.z)
                continue;
            c->mask[at >> 5] |= 1u << (at & 31);
        }
        c->lo = lo;
        c->hi = hi;
        c->stage = (int8_t)stage;
        c->scenario = scenario;
        c->valid = 1;
        region = c;
    }
    uint32_t bits[2] = {region ? region->mask[0] : 0, region ? region->mask[1] : 0};
    for(unsigned at = 0; at < total; at++){
        if(region && !(bits[at >> 5] >> (at & 31) & 1u)){
            if(!(bits[at >> 5] >> (at & 31))) at |= 31;
            continue;
        }
        unsigned i = at < span.count ? span.first + at
                                     : sizeof water_volumes / sizeof water_volumes[0] + at - span.count;
        const WaterVolume* w = water_volume_at(i);
        if(w->stage != g_game.stage_index || !(w->scenarios & scenario)) continue;
        v3 origin = V3(w->origin[0], w->origin[1], w->origin[2]);
        v3 d = v3_sub(position, origin), up = v3_from_i16(w->axes[1]);

        fx reach = water_reach(w);
        if(fx_abs(d.x) > reach || fx_abs(d.y) > reach || fx_abs(d.z) > reach) continue;
        fx height = gdot(d, w->axes[1]), radius = w->size[0];
        v3 normal = up;
        fx depth = 0;
        v3 surface = position;
        if(w->kind == 0){
            if(height < 0 || height >= w->size[1]) continue;
            fx x = gdot(d, w->axes[0]);
            if(x < -w->size[0] || x >= w->size[0]) continue;
            fx z = gdot(d, w->axes[2]);
            if(z < -w->size[2] || z >= w->size[2]) continue;
            depth = w->size[1] - height;
            surface = v3_add(position, v3_scale14(normal, depth));
        } else if(w->kind == 1){
            v3 tangent = v3_sub(d, v3_scale14(up, height));
            if(height < 0 || height > w->size[1] || v3_length_squared64(tangent) >= (int64_t)radius * radius)
                continue;

            normal = gravity_up;
            if(fx_abs(v3_dot14(up, gravity_up)) > 11583){
                depth = w->size[1] - height;
                surface = v3_add(position, v3_scale14(up, depth));
            } else {
                depth = radius - (fx)isqrt64(v3_length_squared64(tangent));
                surface = v3_add(position, v3_scale14(gravity_up, depth));
            }
        } else {
            fx distance = (fx)isqrt64(v3_length_squared64(d));
            if(distance > radius || (w->kind == 3 && height > 0)) continue;
            if(w->kind == 3) depth = -height;
            else {
                normal = distance ? v3_direction14(d, gravity_up) : V3(0, 0, 0);
                if(w->kind == 2 || w->kind == 4){
                    fx along = gdot(d, (int16_t[3]){gravity_up.x, gravity_up.y, gravity_up.z});
                    v3 tangent = v3_sub(d, v3_scale14(gravity_up, along));
                    fx span = (fx)isqrt64(v3_length_squared64(tangent));
                    fx angle = (fx)((int64_t)span * (FX_PI / 2) / radius);
                    depth = (fx)((int64_t)radius * fx_cos(angle) / 256) - along;
                } else depth = radius - distance;
            }
            surface = (w->kind == 2 || w->kind == 4) ? v3_add(origin, v3_scale14(normal, radius))
                                                     : v3_add(position, v3_scale14(normal, depth));
        }
        if(sample){
            *sample = (WaterSample){surface, normal, depth, w->cold != 0};
        }
        if(volume) *volume = w;
        return true;
    }

    if(span.count) return false;
    fx level;
    if(stage_water_level_at(position, &level) && position.z < level){
        if(sample)
            *sample = (WaterSample){V3(position.x, position.y, level), V3(0, 0, N14_ONE), level - position.z,
                                    false};
        return true;
    }
    return false;
}
static bool water_query(v3 position, v3 gravity_up, WaterSample* sample){
    const WaterVolume* volume = NULL;
    WaterSample first;
    if(!water_query_raw(position, gravity_up, sample ? &first : NULL, &volume)) return false;
    if(!sample) return true;

    if(volume && volume->kind <= 2){
        WaterSample next;
        v3 test = v3_add(first.surface, v3_scale14(gravity_up, 5));
        if(water_query_raw(test, gravity_up, &next, NULL)){
            first.depth += next.depth;
            first.surface = v3_add(first.surface, v3_scale14(gravity_up, next.depth));
        }
    }
    *sample = first;
    return true;
}
