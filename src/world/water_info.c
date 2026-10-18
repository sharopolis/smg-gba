static WaterSample water_sample_at_volume(const WaterVolume* w, v3 position, v3 gravity_up){
    v3 origin = V3(w->origin[0], w->origin[1], w->origin[2]);
    v3 d = v3_sub(position, origin), up = v3_from_i16(w->axes[1]), normal = up, surface = position;
    fx height = gdot(d, w->axes[1]), radius = w->size[0], depth = 0;
    if(w->kind == 0){
        fx signed_depth = w->size[1] - height;
        depth = fx_abs(signed_depth);
        surface = v3_add(position, v3_scale14(up, signed_depth));
    } else if(w->kind == 1){
        v3 tangent = v3_sub(d, v3_scale14(up, height));
        normal = gravity_up;
        if(fx_abs(v3_dot14(up, gravity_up)) > 11583){
            depth = w->size[1] - height;
            surface = v3_add(position, v3_scale14(up, depth));
        } else {
            depth = radius - (fx)isqrt64(v3_length_squared64(tangent));
            surface = v3_add(position, v3_scale14(gravity_up, depth));
        }
    } else if(w->kind == 3){
        depth = -height;
        surface = v3_add(position, v3_scale14(up, depth));
    } else {
        fx distance = (fx)isqrt64(v3_length_squared64(d));
        normal = distance ? v3_direction14(d, gravity_up) : V3(0, 0, 0);
        fx along = v3_dot14(d, gravity_up);
        v3 tangent = v3_sub(d, v3_scale14(gravity_up, along));
        fx span = (fx)isqrt64(v3_length_squared64(tangent));
        fx angle = (fx)((int64_t)span * (FX_PI / 2) / radius);
        depth = (fx)((int64_t)radius * fx_cos(angle) / 256) - along;
        surface = v3_add(origin, v3_scale14(normal, radius));
    }
    if(w->kind <= 2){
        WaterSample next;
        if(water_query_raw(v3_add(surface, v3_scale14(gravity_up, 5)), gravity_up, &next, NULL)){
            depth += next.depth;
            surface = v3_add(surface, v3_scale14(gravity_up, next.depth));
        }
    }
    return (WaterSample){surface, normal, depth, w->cold != 0};
}
