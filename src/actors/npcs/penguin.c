
enum { SWIMMING_PENGUIN_WAIT = 160, SWIMMING_PENGUIN_DIVE };

__attribute__((noinline)) static void swimming_penguin_anim(Actor* a, int clip){
    if(a->anim != clip){
        a->anim = clip;
        a->anim_frame = 0;
    }
}

__attribute__((noinline)) static void swimming_penguin_wait(Actor* a){
    a->state = SWIMMING_PENGUIN_WAIT;
    a->timer = 0;
    a->value = plant_rand(120, 300);
    swimming_penguin_anim(a, ANIM_PENGUINSKATER_SWIMWAITSURFACE);
}

__attribute__((noinline)) static void swimming_penguin_init(Actor* a, const StageObject* o){
    GravitySample gravity = gravity_sample(g_game.stage.header, a->position);
    a->up = gravity.up;
    a->front = v3_tangent14(a->front, a->up, V3(0, N14_ONE, 0));
    a->velocity = V3(buoy_penguin_coord(o->id) << 8, 0, 1);
    if(o->rail >= 0) a->position = rail_position_at(o->rail, a->velocity.x >> 8);

    WaterSample water;
    if(water_query(v3_sub(a->position, v3_scale14(a->up, 1)), a->up, &water)) a->position = water.surface;
    a->target = V3(a->position.x << 8, a->position.y << 8, a->position.z << 8);
    swimming_penguin_wait(a);
}

__attribute__((noinline)) static void swimming_penguin_move(Actor* a, const StageObject* o){
    a->velocity.y = fx_min(V16(5), a->velocity.y + V16(.1f));
    a->velocity.x += a->velocity.y * a->velocity.z;
    fx length = rail_length(o->rail);
    const StageRail* rail = rail_get(o->rail);
    if(rail->closed){
        if(length > 0){
            a->velocity.x %= length << 8;
            if(a->velocity.x < 0) a->velocity.x += length << 8;
        }
    } else {
        a->velocity.x = fx_clamp(a->velocity.x, 0, length << 8);
        if(a->velocity.x == 0 || a->velocity.x == (length << 8)) a->velocity.z = -a->velocity.z;
    }
    fx coord = a->velocity.x >> 8;
    v3 want = rail_position_at(o->rail, coord);
    a->target = v3_add(a->target,
                       v3_scale14(v3_sub(V3(want.x << 8, want.y << 8, want.z << 8), a->target), R14(.08f)));
    a->position = V3(a->target.x >> 8, a->target.y >> 8, a->target.z >> 8);
    v3 ahead = rail_position_at(o->rail, coord + FX(15) * a->velocity.z);
    v3 behind = rail_position_at(o->rail, coord - FX(15) * a->velocity.z);

    v3 front = v3_tangent14(v3_direction14(v3_sub(ahead, behind), a->front), a->up, a->front);
    a->front =
        v3_tangent14(v3_add(v3_scale14(a->front, R14(.92f)), v3_scale14(front, R14(.08f))), a->up, a->front);
    swimming_penguin_anim(a, ANIM_PENGUINSKATER_SWIMSURFACE);
}

__attribute__((noinline)) static void swimming_penguin_turn(Actor* a, v3 want){
    want = v3_tangent14(v3_direction14(want, a->front), a->up, a->front);
    if(v3_dot14(a->front, want) >= R14(.997564f)) a->front = want;
    else {
        fx sign = v3_dot14(v3_cross14(a->front, want), a->up) >= 0 ? 1 : -1;
        a->front = v3_tangent14(v3_rotate_axis14(a->front, a->up, FXC(.06981317f) * sign), a->up, a->front);
    }
}

__attribute__((noinline)) static void swimming_penguin_think(Actor* a){
    if(a->state == SWIMMING_PENGUIN_DIVE){
        if((a->anim_frame >> 8) >= 269) swimming_penguin_wait(a);
        return;
    }
    talk_actor_think(a);
    const StageObject* o = actor_stage_object(a);
    if(!o) return;
    if(a->args[0] == 3){
        v3 delta = v3_sub(g_game.player.figure.position, a->position);
        swimming_penguin_turn(a, v3_length_squared64(delta) < (uint64_t)2000 * 2000 ? delta
                                                                                    : v3_from_i16(o->front));
        swimming_penguin_anim(a, ANIM_PENGUINSKATER_SWIMWAITSURFACE);
        if(a->timer >= a->value){
            a->state = SWIMMING_PENGUIN_DIVE;
            a->timer = 0;
            swimming_penguin_anim(a, ANIM_PENGUINSKATER_SWIMDIVE);
        }
    } else if(talk_actor_normal_talking(a))
        swimming_penguin_turn(a, v3_sub(g_game.player.figure.position, a->position));
    else if(o->rail >= 0) swimming_penguin_move(a, o);

    int period = a->anim == ANIM_PENGUINSKATER_SWIMSURFACE       ? 386
                 : a->anim == ANIM_PENGUINSKATER_SWIMSURFACETALK ? 241
                                                                 : 121;
    if((a->anim_frame >> 8) >= period - 1) a->anim_frame = -FX_ONE;
}
