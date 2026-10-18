static EWRAM_BSS GalaxyCamera g_gcam;

static const StageCamera gcam_water_default = {.type = CAM_WATER_FOLLOW,
                                               .camint = 120,
                                               .fovy = 45,
                                               .axis = {1000, 900, 0},
                                               .zone_up = {0, 0, N14_ONE},
                                               .woffset = {0, 0, 170},
                                               .loffset = 100,
                                               .up = {0, 41, 0}};
static const StageCamera gcam_surface_default = {.type = CAM_FOLLOW,
                                                 .camint = 120,
                                                 .fovy = 45,
                                                 .axis = {900, 600, 0},
                                                 .zone_up = {0, 0, N14_ONE},
                                                 .woffset = {0, 0, 170},
                                                 .loffset = 100,
                                                 .angle_a = 715,
                                                 .angle_b = 1430};
static inline const StageCamera* gcam_param(int chunk){
    int n = g_game.stage.header->camera_count;
    return chunk < n    ? &g_game.stage.cameras[chunk]
           : chunk == n ? &gcam_water_default
                        : &gcam_surface_default;
}

static int gcam_category(void){
    const Player* p = &g_game.player;
    v3 selected, held;
    if(p->action == MA_TALK && pointer_pull_camera_targets(&selected, &held)) return 3;
    return p->action != MA_SWIM ? 0 : p->water_state == WATER_SURFACE ? 2 : 1;
}
static inline v3 gcam_v32(const int32_t* v){ return V3(v[0], v[1], v[2]); }
static inline v3 gcam_v16(const int16_t* v){ return V3(v[0], v[1], v[2]); }
static inline fx gcam_q12_to_fx(int16_t q){ return q >> 4; }

static void camera_galaxy_reset(void){
    g_gcam = (GalaxyCamera){0};
    g_gcam.chunk = g_gcam.area_chunk = g_gcam.start_chunk = -1;
}

static void camera_event_start(int first, int count, v3 pos, v3 up, v3 front){
    GalaxyCamera* c = &g_gcam;
    if(first < 0 || !g_game.stage.header->camera_count) return;
    c->event_active = true;
    c->event_first = first;
    c->event_count = count > 0 ? count : 1;
    c->event_index = 0;
    c->event_frame = 0;
    c->event_on_mario = false;
    c->event_target = (CamTarget){pos, up, front, V3(0, 0, 0)};
}

static void camera_event_target(v3 pos, v3 dir, v3 front){
    GalaxyCamera* c = &g_gcam;
    if(!c->event_active || c->event_on_mario) return;
    CamTarget* t = &c->event_target;
    t->last_move = v3_add(t->last_move, v3_sub(pos, t->pos));
    t->front = v3_tangent14(v3_rotate_between14(t->front, t->up, dir), dir, front);
    t->up = dir;
    t->pos = pos;
}

static void camera_event_end_at_landing(void){ g_gcam.event_on_mario = true; }
static void camera_event_cancel(void){ g_gcam.event_active = false; }
static bool camera_event_busy(void){ return g_gcam.event_active; }
static void camera_event_landed(void){
    if(g_gcam.event_active && g_gcam.event_on_mario) g_gcam.event_active = false;
}

static void camera_shake(int level){
    g_gcam.shake_level = level;
    g_gcam.shake_timer = level > 1 ? 24 : 12;
}

static void camera_galaxy_snap(void){
    g_gcam.start_chunk = -1;
    g_gcam.out_valid = false;
    g_gcam.snap = true;
    g_gcam.chunk = -2;
    g_gcam.target_prev = g_game.player.figure.position;
}

static inline bool gcam_fixed_camera(void){ return level_has(LEVEL_FIXED_CAMERA); }

static void camera_galaxy_stage_start(int start_chunk){
    camera_galaxy_reset();
    if(gcam_fixed_camera()){
        g_gcam.snap = true;
        g_gcam.chunk = -2;
        return;
    }
    if(start_chunk >= 0 && start_chunk < (int)g_game.stage.header->camera_count)
        g_gcam.start_chunk = start_chunk;
}

static bool gcam_area_contains(const StageArea* a, v3 p){
    v3 q = v3_sub(p, gcam_v32(a->centre));
    if(fx_abs(q.x) > a->bound || fx_abs(q.y) > a->bound || fx_abs(q.z) > a->bound) return false;
    switch(a->shape){
    case SHAPE_BOX:
        for(int k = 0; k < 3; k++)
            if(fx_abs(gdot(q, a->axis[k])) > a->extent[k]) return false;
        return true;
    case SHAPE_SPHERE: return v3_length_squared64(q) < (uint64_t)((int64_t)a->extent[0] * a->extent[0]);
    case SHAPE_BOWL:
        return v3_length_squared64(q) <= (uint64_t)((int64_t)a->extent[0] * a->extent[0]) &&
               gdot(q, a->axis[1]) < 0;
    default: {
        fx h = gdot(q, a->axis[1]);
        if(h < 0 || h > a->extent[1]) return false;
        v3 r = v3_sub(q, gscale(a->axis[1], h));
        return v3_length_squared64(r) < (uint64_t)((int64_t)a->extent[0] * a->extent[0]);
    }
    }
}

static const StageArea* stage_area_category_at(int cls, v3 p, int category){
    const Stage* st = &g_game.stage;
    uint8_t scen = gravity_scenario_bit();
    const StageArea* best = NULL;
    uint32_t n = st->header->area_count;
    const uint16_t* list =
        st->header->area_grid ? bound_grid_list(st->header, st->header->area_grid, p, &n) : NULL;
    for(uint32_t j = 0; j < n; j++){
        const StageArea* a = &st->areas[list ? list[j] : j];
        if(a->cls != cls || !(a->scenarios & scen)) continue;
        if(cls == AREA_CAMERA){
            int cat = a->args[3] < 0 ? 0 : a->args[3];
            uint32_t mask = cat < 16 ? 1u << cat : (uint32_t)cat >> 4;
            if(!(mask & (1u << category))) continue;
        }
        if(best && a->priority < best->priority) continue;
        if(gcam_area_contains(a, p)) best = a;
    }
    return best;
}

static bool camera_pull_preview_allowed(void){
    const Stage* st = &g_game.stage;
    if(!st->header || !st->header->area_count) return false;
    v3 at = g_game.player.figure.position;
    if(stage_area_category_at(AREA_CAMERA, at, 3)) return true;
    const StageArea* normal = stage_area_category_at(AREA_CAMERA, at, 0);
    if(!normal || normal->camera < 0) return false;
    uint8_t scenario = gravity_scenario_bit();
    for(uint32_t i = 0; i < st->header->area_count; i++){
        const StageArea* a = &st->areas[i];
        if(a->cls != AREA_CAMERA || !(a->scenarios & scenario) || a->zone != normal->zone ||
           a->camera != normal->camera)
            continue;
        int category = a->args[3] < 0 ? 0 : a->args[3];
        uint32_t mask = category < 16 ? 1u << category : (uint32_t)category >> 4;
        if(mask & (1u << 3)) return true;
    }
    return false;
}

static const StageArea* stage_area_at(int cls, v3 p){
    return stage_area_category_at(cls, p, gcam_category());
}

static CamTarget gcam_mario_target(void){
    Player* p = &g_game.player;
    v3 move = v3_sub(p->figure.position, g_gcam.target_prev);
    if(v3_length_squared64(move) > (uint64_t)FX(2000) * FX(2000)) move = V3(0, 0, 0);
    CamTarget t = {p->figure.position, p->up, p->front, move};
    return t;
}

static v3 gcam_watch(const StageCamera* cp, const CamTarget* t, bool immediate){
    GalaxyCamera* c = &g_gcam;

    v3 watch_front = g_game.player.action == MA_SWIM && !c->event_active
                         ? mario_swim_camera_front(t->front, t->up)
                         : t->front;
    v3 want = v3_add(v3_scale14(watch_front, cp->loffset), v3_scale14(t->up, cp->loffsetv));
    int n = g_cam_ticks;
    int32_t k = 0;
    if(immediate || c->reset) k = N14_ONE;
    else if(n){
        int32_t speed = (int32_t)((int64_t)v3_length_exact(t->last_move) / n * R14(0.1f) / FX(15));
        k = cam_ease(fx_min(speed, N14_ONE), n);
    }
    c->local_ofs = v3_add(c->local_ofs, v3_scale14(v3_sub(want, c->local_ofs), k));
    return v3_add(v3_add(t->pos, gcam_v32(cp->woffset)), c->local_ofs);
}

__attribute__((noinline)) static void gcam_spiral(const StageCamera* cp, const CamTarget* t){
    GalaxyCamera* c = &g_gcam;
    int elapsed = fx_clamp(c->event_frame - cp->num2, 0, fx_max(cp->num1 - cp->num2, 1));
    int32_t q = elapsed * N14_ONE / fx_max(cp->num1 - cp->num2, 1), rate = q;
    if(cp->extra[0]){
        int32_t q2 = mul14(q, q), q3 = mul14(q2, q), q4 = mul14(q3, q), q5 = mul14(q4, q);
        rate = fx_clamp(6 * q5 - 15 * q4 + 10 * q3, 0, N14_ONE);
    }
    fx y = cp->wpoint[1] + mul14(cp->axis[1] - cp->wpoint[1], rate);
    fx dist = cp->wpoint[2] + mul14(cp->axis[2] - cp->wpoint[2], rate);
    fx start_angle = gcam_q12_to_fx(cp->angle_a);
    fx angle = start_angle + mul14(gcam_q12_to_fx(cp->angle_b) - start_angle, rate);
    v3 side = v3_direction14(v3_cross14(t->up, t->front), V3(-N14_ONE, 0, 0));
    v3 spiral = v3_add(v3_scale14(side, fx_sin(angle) * 64), v3_scale14(t->front, fx_cos(angle) * 64));
    v3 offset = v3_add(v3_scale14(spiral, dist), v3_scale14(t->up, y));
    c->watch = gcam_watch(cp, t, false);
    c->eye = v3_add(c->watch, offset);
    c->up = v3_rotate_between14(t->up, v3_direction14(spiral, t->front), v3_direction14(offset, t->front));
}

static v3 gcam_vpan(const StageCamera* cp, v3 watch){
    GalaxyCamera* c = &g_gcam;
    if(!cp->vpan) return watch;
    v3 ax = gcam_v16(cp->zone_up);
    fx h = v3_dot14(watch, ax);
    if(c->reset || !c->vpan_valid){
        c->vpan_h = h;
        c->vpan_valid = true;
    } else {
        bool air = g_mario.jumping && !cutscene_blocks(BLOCK_CAMERA_PLAYER_FOCUS);
        fx d = h - c->vpan_h;

        fx half = mul14(v3_length_exact(v3_sub(g_game.camera.target, g_game.camera.position)), R14(0.414f));
        fx up_band = (fx)(((int64_t)half * cp->upper) >> 12),
           low_band = (fx)(((int64_t)half * cp->lower) >> 12);
        if(!air) c->vpan_h += mul14(d, cam_ease(R14(0.05f), g_cam_ticks));
        else if(d > up_band) c->vpan_h += d - up_band;
        else if(d < -low_band) c->vpan_h += d + low_band;
    }
    return v3_add(watch, v3_scale14(ax, c->vpan_h - h));
}

static v3 gcam_up_from(v3 up, v3 eye, v3 watch){
    v3 f = v3_direction14(v3_sub(watch, eye), V3(0, N14_ONE, 0));

    if(fx_abs(v3_dot14(f, up)) > R14(0.98f)) up = g_game.camera.up;
    return v3_tangent14(up, f, g_game.camera.up);
}

static bool gcam_registered_vec(const StageCamera* cp, v3* out){
    if(cp->pad >= OBJ_TYPE_COUNT || !(actor_kinds[cp->pad].flags & KIND_CAMERA_VECTOR)) return false;
    for(int i = 0; i < g_game.actor_count; i++){
        const Actor* a = &g_game.actors[i];

        if(a->type != cp->pad || a->stage_index < 0 ||
           a->stage_index >= (int)g_game.stage.header->object_count)
            continue;
        const StageObject* placed = &g_game.stage.objects[a->stage_index];
        if(placed->args[7] != cp->num2) continue;
        *out = a->position;
        return true;
    }
    return false;
}

__attribute__((noinline)) static void gcam_charmed_tower(const StageCamera* cp, const CamTarget* t){
    v3 registered;
    if(!gcam_registered_vec(cp, &registered)) return;
    v3 axis = v3_direction14(gcam_v32(cp->axis), gcam_v16(cp->zone_up));
    v3 radial = v3_sub(t->pos, gcam_v32(cp->wpoint));
    radial = v3_sub(radial, v3_scale14(axis, v3_dot14(radial, axis)));
    if(v3_length_squared64(radial) < 1) return;
    v3 front = v3_direction14(radial, t->front);
    v3 reference = v3_add(t->pos, v3_scale14(front, cp->dist));
    v3 reg_dir = v3_sub(registered, reference);
    reg_dir = v3_sub(reg_dir, v3_scale14(axis, v3_dot14(reg_dir, axis)));
    reg_dir = v3_direction14(reg_dir, V3(0, 0, 0));
    fx yaw = angle_between14(reg_dir, v3_neg(front));
    yaw = fx_min(yaw, gcam_q12_to_fx(cp->angle_b));
    if(v3_dot14(v3_cross14(axis, v3_neg(front)), reg_dir) < 0) yaw = -yaw;
    v3 horizontal = v3_rotate_axis14(front, axis, yaw);
    v3 direction = v3_rotate_axis14(horizontal, v3_cross14(horizontal, axis), gcam_q12_to_fx(cp->angle_a));
    g_gcam.eye = v3_add(t->pos, v3_scale14(direction, cp->dist));
    g_gcam.watch = gcam_watch(cp, t, false);
    g_gcam.up = axis;
}

static void gcam_pose(const StageCamera* cp, const CamTarget* t, const KeyState* keys){
    GalaxyCamera* c = &g_gcam;
    v3 zup = gcam_v16(cp->zone_up);
    v3 prev_eye = c->eye, prev_watch = c->watch, prev_up = c->up;
    if(c->reset){
        prev_eye = c->out_eye;
        prev_watch = c->out_watch;
        prev_up = c->out_up;
    }
    switch(cp->type){
    case CAM_SPIRAL: gcam_spiral(cp, t); break;
    case CAM_CHARMED_VECREG_TOWER: gcam_charmed_tower(cp, t); break;
    case CAM_XZ_PARA:
    default: {
        v3 watch = gcam_vpan(cp, gcam_watch(cp, t, false));
        v3 off = gcam_v32(cp->offset);
        if(cp->num1 == 1){
            if(keys->held & KEY_L){
                if(keys->pressed & KEY_LEFT) c->round_target -= FXC(0.7853982f);
                if(keys->pressed & KEY_RIGHT) c->round_target += FXC(0.7853982f);
            }
            fx d = c->round_target - c->round_angle;
            c->round_angle += fx_clamp(d, -FXC(0.08f), FXC(0.08f));
            if(c->round_angle) off = v3_rotate_axis14(off, zup, c->round_angle);
        }
        c->watch = watch;
        c->eye = v3_add(watch, off);
        c->up = zup;
        break;
    }
    case CAM_WATER_FOLLOW: {
        v3 watch = gcam_watch(cp, t, false), d = v3_sub(watch, prev_eye);
        fx len = fx_clamp(v3_length_exact(d), cp->axis[1] > 0 ? cp->axis[1] : FX(300),
                          cp->axis[0] > 0 ? cp->axis[0] : FX(1200));
        v3 front = v3_direction14(d, t->front);
        v3 old_front = v3_direction14(v3_sub(prev_watch, prev_eye), front);
        v3 up = v3_rotate_between14(prev_up, old_front, front);
        v3 side = v3_tangent14(v3_cross14(up, front), t->up, v3_cross14(t->up, t->front));
        up = v3_direction14(v3_cross14(front, side), t->up);
        if(v3_dot14(up, t->up) < 0){
            v3 f = v3_dot14(front, t->up) < 0 ? v3_neg(t->up) : t->up;
            up = v3_rotate_between14(up, front, f);
            side = v3_rotate_between14(side, front, f);
            front = f;
        }
        v3 target_side = v3_direction14(v3_cross14(t->up, t->front), side);
        int32_t rate = cam_ease(mul14(cp->up[1] << 2, fx_abs(v3_dot14(t->up, front))), g_cam_ticks);
        CamRot rot = cam_rotation_towards(side, target_side, rate, FX_PI, t->up);
        front = cam_rotate(front, rot);
        up = cam_rotate(up, rot);
        c->watch = watch;
        c->eye = v3_sub(watch, v3_scale14(front, len));
        c->up = v3_tangent14(up, front, t->up);
        break;
    }
    case CAM_FOLLOW: {
        v3 watch = gcam_vpan(cp, gcam_watch(cp, t, false)), up = zup;
        v3 d = v3_sub(prev_eye, watch);
        fx len = v3_length_exact(d), lo = cp->axis[1], hi = cp->axis[0];
        bool follow = level_has(LEVEL_FOLLOW_CAMERA);
        if(len < lo) len = follow && g_game.player.action != MA_SWIM ? lo : (len + lo) / 2;
        else if(hi > 0 && len > hi) len = (len + hi) / 2;
        fx vert = v3_dot14(d, up);
        v3 horiz = v3_sub(d, v3_scale14(up, vert));
        fx pitch = fx_atan2_fast(vert, v3_length_exact(horiz));
        fx min_pitch = gcam_q12_to_fx(cp->angle_a), max_pitch = min_pitch + gcam_q12_to_fx(cp->angle_b);
        pitch += mul14(fx_clamp(pitch, min_pitch, max_pitch) - pitch, cam_ease(R14(0.5f), g_cam_ticks));
        v3 bearing = v3_direction14(horiz, v3_neg(t->front));
        v3 behind = v3_tangent14(v3_neg(t->front), up, bearing);
        fx speed = g_cam_ticks && !(follow && g_game.player.action != MA_SWIM && g_mario.jumping)
                       ? v3_length_exact(v3_reject14(t->last_move, follow ? t->up : up)) / g_cam_ticks
                       : 0;
        fx yaw_step =
            mul14(FXC(0.15f * 3.14159265f / 180.0f) * g_cam_ticks, fx_min(N14_ONE, speed * N14_ONE / FX(15)));
        if(keys->pressed & KEY_L) yaw_step = FXC(0.2f) * g_cam_ticks;
        bearing = cam_rotate(bearing, cam_rotation_towards(bearing, behind, N14_ONE, yaw_step, up));
        v2 sc = fx_sin_cos(pitch);
        c->watch = watch;
        c->eye =
            v3_add(watch, v3_add(v3_scale14(bearing, fx_mul(len, sc.y)), v3_scale14(up, fx_mul(len, sc.x))));
        c->up = up;
        break;
    }
    case CAM_WONDER_PLANET:
    case CAM_MEDIAN_PLANET: {
        v3 watch = gcam_watch(cp, t, false);
        v3 up = t->up, front = t->front, side = v3_cross14(up, front);
        v3 d = v3_sub(prev_eye, watch);
        fx len = v3_length_exact(d);
        if(len == 0){
            d = v3_scale14(v3_neg(front), cp->axis[1]);
            len = cp->axis[1];
        }
        fx lx = v3_dot14(d, side);
        fx ly = v3_dot14(d, up), lz = v3_dot14(d, front);
        fx horiz = (fx)isqrt64((uint64_t)((int64_t)lx * lx + (int64_t)lz * lz));
        fx el = fx_atan2_fast(ly, horiz);
        fx min_el = gcam_q12_to_fx(cp->angle_a);
        if(el < min_el){
            el += mul14(min_el - el, cam_ease(R14(0.05f), g_cam_ticks));
            v2 sc = fx_sin_cos(el);
            v3 hdir = horiz ? v3_direction14(V3(lx, 0, lz), V3(0, 0, -N14_ONE)) : V3(0, 0, -N14_ONE);
            fx h = fx_mul(len, sc.y);
            lx = mul14(hdir.x, h);
            lz = mul14(hdir.z, h);
            ly = fx_mul(len, sc.x);
        }
        d = v3_add(v3_add(v3_scale14(side, lx), v3_scale14(up, ly)), v3_scale14(front, lz));
        len = v3_length_exact(d);
        fx dmin = cp->axis[0], dmax = cp->axis[1];
        if(len < dmin && len) d = v3_set_length(d, dmin, d);
        if(len > dmax && dmax > 0) d = v3_set_length(d, dmax, d);
        v3 eye = v3_add(watch, d);

        v3 f0 = v3_direction14(v3_sub(prev_watch, prev_eye), front),
           f1 = v3_direction14(v3_sub(watch, eye), front);
        v3 u = c->reset ? prev_up : v3_rotate_between14(prev_up, f0, f1);
        u = v3_tangent14(u, f1, prev_up);
        if(keys->pressed & KEY_L){
            fx hl = (fx)isqrt64((uint64_t)((int64_t)lx * lx + (int64_t)lz * lz));
            eye = v3_add(watch, v3_add(v3_scale14(v3_neg(front), hl), v3_scale14(up, ly)));
            u = up;
            c->blend_t = 0;
            c->blend_total = 30;
        }
        c->watch = watch;
        c->eye = eye;
        c->up = u;
        break;
    }
    case CAM_EYEPOS_FIX: {
        v3 watch = gcam_watch(cp, t, false), eye = gcam_v32(cp->wpoint);
        v3 u = cp->num1 == 2 ? t->up : zup;
        if(cp->num1 == 1){
            v3 f0 = v3_direction14(v3_sub(prev_watch, prev_eye), zup),
               f1 = v3_direction14(v3_sub(watch, eye), zup);
            u = v3_rotate_between14(prev_up, f0, f1);
        }
        c->eye = eye;
        c->watch = watch;
        c->up = u;
        break;
    }
    case CAM_EYEPOS_FIX_THERE: {
        v3 watch = gcam_watch(cp, t, false);
        v3 eye = prev_eye;
        v3 f0 = v3_direction14(v3_sub(prev_watch, prev_eye), prev_up),
           f1 = v3_direction14(v3_sub(watch, eye), f0);
        v3 u = v3_rotate_between14(prev_up, f0, f1);
        if(c->reset){
            c->there_up = prev_up;
            c->there_roll = 0;
            if(cp->num1 == 0){
                GravitySample g = gravity_sample(g_game.stage.header, t->pos);
                v3 gu = g.index >= 0 ? g.up : t->up;
                if(fx_abs(v3_dot14(f0, gu)) <= R14(0.866f)){
                    v3 a = v3_tangent14(gu, f0, prev_up), b = v3_tangent14(prev_up, f0, gu);
                    c->there_up = gu;
                    c->there_roll = fx_atan2(v3_dot14(v3_cross14(a, b), f0) >> 6, v3_dot14(a, b) >> 6);
                }
            }
        }
        if(cp->num1 == 0){
            u = v3_tangent14(c->there_up, f1, u);
            if(c->there_roll) u = v3_rotate_axis14(u, f1, -c->there_roll);
        }
        c->eye = eye;
        c->watch = watch;
        c->up = u;
        break;
    }
    case CAM_POINT_FIX: {
        v3 w = gcam_v32(cp->wpoint);
        c->eye = v3_add(w, gcam_v32(cp->offset));
        c->watch = v3_add(w, gcam_v32(cp->woffset));
        c->up = gcam_v32(cp->up);
        break;
    }
    case CAM_CHARMED_FIX: {
        v3 base = gcam_v32(cp->wpoint);
        v3 f = v3_direction14(v3_sub(t->pos, base), zup);
        v3 u = gcam_v32(cp->up);
        v3 side = v3_direction14(v3_cross14(u, f), V3(N14_ONE, 0, 0));
        u = v3_cross14(f, side);
        v3 lo = gcam_v32(cp->offset);
        c->eye =
            v3_add(t->pos, v3_add(v3_add(v3_scale14(side, lo.x), v3_scale14(u, lo.y)), v3_scale14(f, lo.z)));
        c->watch = v3_sub(c->eye, v3_scale14(f, lo.z));
        c->up = u;
        gcam_watch(cp, t, false);
        break;
    }
    case CAM_TOWER:
    case CAM_TOWER_POS: {
        v3 ax = v3_direction14(gcam_v32(cp->axis), zup);
        v3 base = gcam_v32(cp->wpoint);
        v3 watch = gcam_watch(cp, t, false);
        if(cp->type == CAM_TOWER) watch = gcam_vpan(cp, watch);
        fx dist = cp->type == CAM_TOWER_POS ? cp->up[0] : cp->dist;
        v3 rel = v3_sub(watch, base);
        v3 hf = v3_direction14(v3_sub(rel, v3_scale14(ax, v3_dot14(rel, ax))),
                               v3_tangent14(v3_neg(t->front), ax, zup));

        v3 pe = v3_sub(prev_eye, base);
        v3 ph = v3_sub(pe, v3_scale14(ax, v3_dot14(pe, ax)));
        if(!c->reset && (ph.x | ph.y | ph.z)){
            v3 phd = v3_direction14(ph, hf);
            fx dev = angle_between14(phd, hf);
            fx limit = (fx)(((int64_t)FX_PI * (N14_ONE - (cp->angle_a << 2))) >> 15);
            if(dev > limit){
                fx sign = v3_dot14(v3_cross14(hf, phd), ax) >= 0 ? 1 : -1;
                hf = v3_rotate_axis14(hf, ax, sign * limit);
            } else {
                hf = phd;
            }
        }
        v3 side = v3_cross14(hf, ax);

        v3 f = v3_rotate_axis14(hf, side, gcam_q12_to_fx(cp->angle_b));
        c->watch = watch;
        c->eye = v3_add(watch, v3_scale14(f, dist));
        c->up = ax;
        break;
    }
    case CAM_OBJ_PARALLEL: {
        v3 watch = gcam_watch(cp, t, false);
        v3 side = v3_cross14(t->up, t->front);
        v3 lo = gcam_v32(cp->offset), lu = gcam_v16(cp->extra);
        c->watch = watch;
        c->eye = v3_add(watch, v3_add(v3_add(v3_scale14(side, lo.x), v3_scale14(t->up, lo.y)),
                                      v3_scale14(t->front, lo.z)));
        c->up = v3_add(v3_add(v3_scale14(side, lu.x), v3_scale14(t->up, lu.y)), v3_scale14(t->front, lu.z));
        break;
    }
    case CAM_CUBE_PLANET: {
        v3 u = c->reset ? t->up
                        : cam_slerp14(prev_up, t->up, cam_ease(cp->angle_b << 2, g_cam_ticks),
                                      v3_cross14(prev_up, t->front));
        v3 watch = gcam_watch(cp, t, true);
        v3 f = v3_tangent14(v3_sub(watch, prev_eye), u, t->front);
        v3 axis = v3_direction14(v3_cross14(u, f), V3(N14_ONE, 0, 0));
        f = v3_rotate_axis14(f, axis, gcam_q12_to_fx(cp->angle_a));
        u = v3_rotate_axis14(u, axis, gcam_q12_to_fx(cp->angle_a));
        c->watch = watch;
        c->eye = v3_sub(watch, v3_scale14(f, cp->dist));
        c->up = u;
        break;
    }
    case CAM_FRONT_AND_BACK: {
        v3 ax = v3_direction14(gcam_v32(cp->axis), V3(N14_ONE, 0, 0));
        v3 zx = gcam_v16(cp->extra);
        v3 f = v3_rotate_between14(v3_cross14(zx, zup), zx, ax);
        v3 u = v3_rotate_between14(zup, zx, ax);
        fx rot = gcam_q12_to_fx(cp->angle_a);
        f = v3_rotate_axis14(f, ax, rot);
        u = v3_rotate_axis14(u, ax, rot);
        v3 rel = v3_sub(t->pos, gcam_v32(cp->wpoint));
        v3 d = v3_direction14(v3_sub(rel, v3_scale14(ax, v3_dot14(rel, ax))), f);
        fx ang = angle_between14(f, d), max_dip = gcam_q12_to_fx(cp->angle_b);
        if(ang > 0){
            int32_t r = ang <= max_dip ? N14_ONE : (int32_t)(((int64_t)max_dip << 14) / ang);
            v3 nf = v3_blend14(f, d, r);
            u = v3_rotate_between14(u, f, nf);
            f = nf;
        }
        v3 eye = v3_add(t->pos, v3_scale14(f, cp->dist));
        if(!c->reset && v3_length_squared64(v3_sub(eye, prev_eye)) > (uint64_t)FX(500) * FX(500)){
            c->blend_t = 0;
            c->blend_total = 120;
        }
        c->watch = gcam_watch(cp, t, false);
        c->eye = eye;
        c->up = u;
        break;
    }
    case CAM_TRUNDLE: {
        v3 side = v3_direction14(gcam_v32(cp->axis), V3(N14_ONE, 0, 0));
        v3 rel = v3_sub(t->pos, gcam_v32(cp->wpoint));
        v3 u = v3_sub(rel, v3_scale14(side, v3_dot14(rel, side)));
        v3 z = v3_direction14(v3_cross14(v3_direction14(u, zup), side), t->front);
        z = v3_rotate_axis14(z, side, -gcam_q12_to_fx(cp->angle_a));
        u = v3_cross14(side, z);
        if(cp->up[0]){
            int32_t r =
                fx_clamp((int32_t)(((int64_t)v3_dot14(rel, side) << 14) / cp->up[0]), -N14_ONE, N14_ONE);
            z = v3_rotate_axis14(z, u, (fx)(((int64_t)gcam_q12_to_fx(cp->angle_b) * r) >> 14));
        }
        v3 watch = gcam_watch(cp, t, false);
        c->watch = watch;
        c->eye = v3_sub(watch, v3_scale14(z, cp->dist));
        c->up = u;
        break;
    }
    }

    if(cp->roll)
        c->up =
            v3_rotate_axis14(c->up, v3_direction14(v3_sub(c->eye, c->watch), zup), gcam_q12_to_fx(cp->roll));

    if(v3_length_squared64(v3_sub(c->watch, c->eye)) < (uint64_t)FX(300) * FX(300)){
        v3 f = v3_direction14(v3_sub(c->watch, c->eye), zup);
        c->watch = v3_add(c->eye, v3_scale14(f, FX(300)));
    }
}

#define GCAM_DIST_SCALE R14(0.7f)
#define GCAM_DIST_CAP   FX(1000)
#define GCAM_EVENT_CAP  FX(1400)
#define GCAM_MIN_BLEND  20

static v3 gcam_frame(v3 eye, v3* watch, const CamTarget* t, fx cap, int anchor_height){
    v3 d = v3_sub(eye, *watch);
    fx len = v3_length_exact(d);
    if(v3_length_squared64(v3_sub(*watch, t->pos)) > (uint64_t)FX(800) * FX(800)){
        *watch = v3_add(t->pos, v3_scale14(t->up, FX(80)));
        d = v3_sub(eye, *watch);
        len = v3_length_exact(d);
    }
    if(len <= 0) return eye;
    fx want = fx_min(mul14(len, GCAM_DIST_SCALE), cap);
    want = fx_mul(want, camera_zooms[g_game.camera_zoom]);
    if(want < FX(300)) want = fx_min(FX(300), len);
    v3 centre = v3_add(t->pos, v3_scale14(t->up, FX(80)));
    if(anchor_height >= 0){
        int32_t ratio = (int32_t)(((int64_t)want << 14) / len);
        v3 anchor = anchor_height ? centre : t->pos;
        *watch = v3_add(anchor, v3_scale14(v3_sub(*watch, anchor), ratio));
    }
    eye = v3_add(*watch, v3_set_length(d, want, V3(0, 0, N14_ONE)));

    v3 f = v3_direction14(v3_sub(*watch, eye), V3(0, 0, N14_ONE));
    v3 rel = v3_sub(centre, *watch);
    v3 perp = v3_sub(rel, v3_scale14(f, v3_dot14(rel, f)));
    fx depth = v3_dot14(v3_sub(centre, eye), f);
    fx allowed = mul14(fx_max(depth, FX(200)), R14(0.55f * 0.414f));
    fx off = v3_length_exact(perp);
    if(off > allowed) *watch = v3_add(*watch, v3_set_length(perp, off - allowed, f));
    return eye;
}

static void gcam_collide(v3* eye, v3* watch, bool reset, fx* col_len, fx* col_hit, bool cast){
    v3 centre = v3_add(g_game.player.figure.position, v3_scale14(g_game.player.up, FX(80)));
    if(cutscene_blocks(BLOCK_CAMERA_PLAYER_FOCUS) ||
       (g_gcam.event_active && !g_gcam.event_on_mario && cutscene_blocks(BLOCK_EVENT_PLAYER_FOCUS)))
        centre = *watch;
    v3 d = v3_sub(*eye, *watch);
    fx len = v3_length_exact(d);
    fx allowed = len;
    RayHit hit;
    if(cast || reset){
        *col_hit = 0x7FFFFFFF;
        if(len > FX(300) && collision_ray(centre, *eye, &hit, COL_NO_CAMERA)){
            fx from_mario = v3_length_exact(v3_sub(*eye, centre));
            *col_hit = len - (fx)(((int64_t)from_mario * (N14_ONE - hit.t)) >> 14) - FX(60);
            if(*col_hit < FX(300)) *col_hit = FX(300);
        }
    }
    if(*col_hit < allowed) allowed = *col_hit;
    if(reset) *col_len = allowed;
    else if(allowed < *col_len) *col_len += mul14(allowed - *col_len, cam_ease(R14(0.25f), g_cam_ticks));
    else *col_len += mul14(allowed - *col_len, cam_ease(R14(0.08f), g_cam_ticks));
    if(*col_len >= len || len <= 0) return;
    int32_t k = (int32_t)(((int64_t)*col_len << 14) / len);
    *watch = v3_add(centre, v3_scale14(v3_sub(*watch, centre), k));
    *eye = v3_add(*watch, v3_scale14(d, k));
}

static bool gcam_keep_visible(v3* eye, v3* watch, const CamTarget* t){
    v3 centre = v3_add(t->pos, v3_scale14(t->up, FX(80)));

    bool disk_ground_raised = false;

    if(g_gcam.active && g_game.player.on_ground && level_camera_hold_height() && !g_game.camera_orbit_held){
        fx height = v3_dot14(v3_sub(*eye, centre), t->up);
        if(height < FX(100)){
            *eye = v3_add(*eye, v3_scale14(t->up, FX(100) - height));
            disk_ground_raised = true;
        }
    }

    if(!g_gcam.active && !g_game.camera_orbit_held && g_game.player.on_ground &&
       g_game.player.action == MA_WAIT){
        v3 floor_up = g_game.player.ground_normal;
        int32_t floor_dot = v3_dot14(floor_up, t->up);
        if(floor_dot >= R14(0.17f) && floor_dot < R14(0.75f)){
            fx floor_height = v3_dot14(v3_sub(*eye, t->pos), floor_up);
            if(floor_height < FX(180)){
                *eye = v3_add(*eye, v3_scale14(floor_up, FX(180) - floor_height));
                disk_ground_raised = true;
            }
        }
    }
    v3 d = v3_sub(*eye, centre);
    fx len = v3_length_exact(d);
    RayHit hit;
    bool blocked = collision_ray(centre, *eye, &hit, COL_NO_CAMERA);

    fx depth = v3_dot14(v3_neg(d), g_game.camera.front);

    bool disk_blend_below =
        level_camera_blend_below() && g_game.player.on_ground && g_game.player.action == MA_WAIT &&
        v3_dot14(g_game.camera.front, t->up) > -R14(0.2f) && v3_dot14(v3_sub(g_gcam.eye, centre), t->up) > 0;
    if(!blocked && depth >= FX(300) && !disk_blend_below) return disk_ground_raised;
    fx clear = blocked ? mul14(len, hit.t) - FX(60) : 0;
    if(depth < FX(300) && g_gcam.active) d = v3_sub(g_gcam.eye, centre);
    if(clear < FX(300)){
        v3 horizontal = v3_tangent14(d, t->up, v3_neg(t->front));
        fx rope = fx_clamp(len, FX(300), GCAM_DIST_CAP);

        v3 raised = v3_add(centre, v3_scale14(v3_direction14(v3_add(horizontal, t->up), t->up), rope));
        RayHit raised_hit;
        if(collision_ray(centre, raised, &raised_hit, COL_NO_CAMERA)){
            raised = v3_add(
                centre,
                v3_scale14(v3_direction14(v3_add(v3_scale14(horizontal, R14(0.2f)), t->up), t->up), rope));
            if(collision_ray(centre, raised, &raised_hit, COL_NO_CAMERA)){
                raised = v3_add(centre, v3_scale14(t->up, rope));
                if(collision_ray(centre, raised, &raised_hit, COL_NO_CAMERA))
                    raised = v3_add(raised_hit.point, v3_scale14(raised_hit.normal, FX(60)));
            }
        }
        *eye = raised;
        *watch = centre;
    } else {
        *eye = v3_add(centre, v3_set_length(d, clear, v3_neg(t->front)));
        *watch = centre;
    }
    return true;
}

static fx gcam_pull_fit(v3 eye_dir, v3 up, v3 watch, const v3* points, int count){
    v3 front = v3_neg(eye_dir);
    v3 right = v3_direction14(v3_cross14(front, up), g_game.camera.heading);
    v3 vertical = v3_direction14(v3_cross14(right, front), up);
    fx rope = FX(700);
    for(int i = 0; i < count; i++){
        v3 d = v3_sub(points[i], watch);
        fx z = v3_dot14(d, front);

        rope = fx_max(rope, (fx_abs(v3_dot14(d, right)) + FX(100)) * 193 / 96 - z);
        rope = fx_max(rope, (fx_abs(v3_dot14(d, vertical)) + FX(100)) * 193 / 52 - z);
    }
    return rope;
}

static bool gcam_pull_frame(Camera* cam){
    v3 selected, held;
    if(!pointer_pull_camera_targets(&selected, &held)) return false;
    const Player* p = &g_game.player;
    v3 points[4] = {p->figure.position, v3_add(p->figure.position, v3_scale14(p->up, FX(160))), selected,
                    held};

    bool shown = true;
    v3 right = v3_cross14(cam->front, cam->up);
    for(int i = 0; i < 4 && shown; i++){
        v3 d = v3_sub(points[i], cam->position);
        fx depth = v3_dot14(d, cam->front);
        if(depth <= FX(50) || (fx_abs(v3_dot14(d, right)) + FX(100)) * 193 > depth * 96 ||
           (fx_abs(v3_dot14(d, cam->up)) + FX(100)) * 193 > depth * 52)
            shown = false;
        else if(i){
            RayHit hit;
            if(collision_ray(points[i], cam->position, &hit, COL_NO_CAMERA)) shown = false;
        }
    }
    if(shown) return true;

    v3 watch = v3_add(v3_add(points[1], selected), held);
    watch = V3(watch.x / 3, watch.y / 3, watch.z / 3);
    v3 old = v3_direction14(v3_sub(cam->position, cam->target), v3_neg(cam->front));
    v3 horizontal = v3_tangent14(old, p->up, v3_neg(p->front));
    v3 dirs[4] = {old, v3_direction14(v3_add(horizontal, p->up), p->up), p->up,
                  v3_direction14(v3_add(v3_neg(horizontal), p->up), p->up)};
    v3 eye = cam->position, up = cam->up;
    int best = 5;
    for(int k = 0; k < 4; k++){
        v3 candidate_up = k ? p->up : cam->up;
        candidate_up = v3_tangent14(candidate_up, dirs[k], cam->up);
        fx rope = gcam_pull_fit(dirs[k], candidate_up, watch, points, 4);
        v3 candidate = v3_add(watch, v3_scale14(dirs[k], rope));
        int hidden = 0;
        for(int i = 1; i < 4; i++){
            RayHit hit;
            if(collision_ray(points[i], candidate, &hit, COL_NO_CAMERA)) hidden++;
        }
        if(hidden < best){
            best = hidden;
            eye = candidate;
            up = candidate_up;
        }
        if(!hidden) break;
    }
    cam->position = eye;
    cam->target = watch;
    cam->front = v3_direction14(v3_sub(watch, eye), cam->front);
    cam->up = gcam_up_from(up, eye, watch);
    cam->smooth_up = cam->up;
    return true;
}

__attribute__((noinline)) static bool gcam_arena_occluded(v3 point, v3 eye){
    CollisionCandidates cand;
    collision_gather_around(point, eye, FXC(0.05f), &cand);
    level_camera_arena_clear(&cand);
    RayHit hit;
    return collision_raycast(&cand, point, eye, &hit, 0);
}

static EWRAM_BSS uint32_t g_arena_cam_tick;
static EWRAM_BSS int8_t g_arena_cam_dir = -1;
__attribute__((noinline, optimize("Os"))) static bool gcam_arena_frame(Camera* cam){
    v3 points[8], gravity_up, arena_front;
    int count = 0;
    if(!level_camera_arena(points, &count, &gravity_up, &arena_front)) return false;
    v3 watch = V3(0, 0, 0);
    for(int i = 0; i < count; i++) watch = v3_add(watch, points[i]);
    watch = V3(watch.x / count, watch.y / count, watch.z / count);
    v3 old =
        v3_direction14(g_gcam.active ? v3_sub(g_gcam.eye, g_gcam.watch) : v3_sub(cam->position, cam->target),
                       v3_neg(cam->front));
    v3 horizontal = v3_tangent14(old, gravity_up, v3_neg(g_game.player.front));
    v3 lift = v3_scale14(gravity_up, R14(.15f));
    v3 arena = v3_tangent14(arena_front, gravity_up, horizontal);

    v3 dirs[5] = {old, v3_direction14(v3_add(v3_neg(arena), lift), horizontal),
                  v3_direction14(v3_add(arena, lift), horizontal),
                  v3_direction14(v3_add(horizontal, lift), horizontal),
                  v3_direction14(v3_add(v3_neg(horizontal), lift), horizontal)};
    v3 eye = cam->position, up = cam->up;
    int best = 100;
    bool search =
        g_arena_cam_dir < 0 || g_game.tick - g_arena_cam_tick >= 8 || g_game.tick < g_arena_cam_tick;
    for(int k = search ? 0 : g_arena_cam_dir; k < 5; k++){
        v3 candidate_up = v3_tangent14(k ? gravity_up : cam->up, dirs[k], cam->up);
        fx rope = gcam_pull_fit(dirs[k], candidate_up, watch, points, count);
        v3 candidate = v3_add(watch, v3_scale14(dirs[k], rope));
        int hidden = 0;
        if(!search){
            if(!gcam_arena_occluded(points[1], candidate)){
                eye = candidate;
                up = candidate_up;
                break;
            }
            search = true;
            k = -1;
            continue;
        }
        for(int i = 0; i < count && hidden < best; i++){
            if(gcam_arena_occluded(points[i], candidate)) hidden += i < 2 ? 4 : i < 4 ? 2 : 1;
        }
        if(hidden < best){
            best = hidden;
            eye = candidate;
            up = candidate_up;
            g_arena_cam_dir = (int8_t)k;
        }
        if(!hidden) break;
    }
    if(search) g_arena_cam_tick = g_game.tick;
    cam->position = eye;
    cam->target = watch;
    cam->front = v3_direction14(v3_sub(watch, eye), cam->front);
    cam->up = gcam_up_from(up, eye, watch);
    cam->smooth_up = cam->up;
    return true;
}

static void camera_visibility_update(void){
    if(cutscene_blocks(BLOCK_GALAXY_CAMERA) || g_gcam.event_active) return;
    if(gcam_fixed_camera()) return;
    if(g_gcam.active && g_gcam.chunk >= 0 && gcam_param(g_gcam.chunk)->type == CAM_TRUNDLE) return;
    Camera* cam = &g_game.camera;
    const Player* p = &g_game.player;
    CamTarget target = {p->figure.position, p->up, p->front, V3(0, 0, 0)};
    if(!gcam_arena_frame(cam) && !gcam_pull_frame(cam) &&
       !gcam_keep_visible(&cam->position, &cam->target, &target))
        return;
    cam->front = v3_direction14(v3_sub(cam->target, cam->position), cam->front);
    cam->up = gcam_up_from(cam->up, cam->position, cam->target);
    if(g_gcam.active){
        g_gcam.out_eye = cam->position;
        g_gcam.out_watch = cam->target;
        g_gcam.out_up = cam->up;
    }
    camera_update_angles(cam);
}

static void camera_manual_resume_source(void){
    GalaxyCamera* c = &g_gcam;
    if(!c->active) return;
    const Camera* cam = &g_game.camera;
    c->out_eye = c->blend_eye = cam->position;
    c->out_watch = c->blend_watch = cam->target;
    c->out_up = c->blend_up = cam->up;
    c->out_valid = true;
    c->blend_t = 0;
    c->blend_total = fx_max(c->chunk >= 0 ? gcam_param(c->chunk)->camint : 0, GCAM_MIN_BLEND);
    c->force_change = false;
}

__attribute__((noinline)) static bool gcam_blend_blocked(v3 eye, v3 watch, int total){
    const GalaxyCamera* c = &g_gcam;
    v3 be = c->blend_eye, bw = c->blend_watch, bu = c->blend_up, oe = c->out_eye, prev = oe;
    v3 d1 = v3_sub(eye, watch);
    fx l1 = v3_length_exact(d1);
    v3 dir1 = v3_direction14(d1, V3(0, 0, N14_ONE));
    int32_t k = cam_ease(R14(0.3f), 1);
    int step = total / 15 > 0 ? total / 15 : 1;
    for(int t = 0; t < total; t++){
        int32_t r = (int32_t)(((int64_t)t << 14) / total);
        r = mul14(r, r);
        v3 d0 = v3_sub(be, bw);
        fx l0 = v3_length_exact(d0);
        v3 dir = cam_slerp14(v3_direction14(d0, dir1), dir1, r, bu);
        bw = v3_add(bw, v3_scale14(v3_sub(watch, bw), r));
        be = v3_add(bw, v3_set_length(dir, l0 + (fx)(((int64_t)(l1 - l0) * r) >> 14), dir1));
        oe = v3_add(oe, v3_scale14(v3_sub(be, oe), k));
        if((t + 1) % step == 0 || t + 1 == total){
            if(level_camera_path_blocked(prev, oe)) return true;
            prev = oe;
        }
    }
    return false;
}

static bool camera_galaxy_update(fx dt, const KeyState* keys){
    if(cutscene_blocks(BLOCK_GALAXY_CAMERA)){
        camera_update_angles(&g_game.camera);
        return true;
    }
    GalaxyCamera* c = &g_gcam;
    Game* game = &g_game;
    Player* p = &game->player;
    Camera* cam = &game->camera;
    int n = g_cam_ticks;
    if(!game->stage.header->camera_count && !gcam_category()) return false;

    int chunk = -1, source = CAMSRC_DEFAULT;
    CamTarget target = gcam_mario_target();
    c->target_prev = p->figure.position;
    if(c->event_active){
        const StageCamera* ec = gcam_param(c->event_first + c->event_index);
        c->event_frame += n;
        if(ec->evfrm && c->event_frame > ec->evfrm && c->event_index + 1 < c->event_count){
            c->event_index++;
            c->event_frame = 0;
        }
        chunk = c->event_first + c->event_index;
        source = CAMSRC_EVENT;
        if(!c->event_on_mario){
            target = c->event_target;
            cutscene_camera_target(&target.pos, &target.up, &target.front, &target.last_move);
        }
    } else {
        if(c->start_chunk >= 0){
            if(v3_length_squared64(target.last_move) > 1) c->start_moved += n;
            if(c->start_moved > 30) c->start_chunk = -1;
        }
        if(c->start_chunk >= 0){
            chunk = c->start_chunk;
            source = CAMSRC_START;
        } else {
            v3 at = p->figure.position;
            if(g_mario.jumping && v3_dot14(g_mario.jump_vec, p->up) > 0)
                at = v3_add(at, v3_scale14(p->up, FX(100)));
            int category = gcam_category();
            int water_source = category == 1   ? CAMSRC_SWIM
                               : category == 2 ? CAMSRC_SURFACE
                               : category == 3 ? CAMSRC_CAPTURE
                                               : CAMSRC_AREA;
            if(c->source != water_source && (c->source == CAMSRC_SWIM || c->source == CAMSRC_SURFACE ||
                                             c->source == CAMSRC_CAPTURE || category))
                c->area_chunk = -1;
            const StageArea* area = stage_area_at(AREA_CAMERA, at);

            if(gcam_fixed_camera() && area && area->shape != SHAPE_CYLINDER){
                const Stage* st = &g_game.stage;
                for(uint32_t i = 0; i < st->header->area_count; i++){
                    const StageArea* a = &st->areas[i];
                    if(a->cls == AREA_CAMERA && a->shape == SHAPE_CYLINDER &&
                       (a->scenarios & gravity_scenario_bit()) && gcam_area_contains(a, at)){
                        area = a;
                        break;
                    }
                }
            }

            if(!area && category == 3) area = stage_area_category_at(AREA_CAMERA, at, 0);
            if(area){
                c->area_chunk = area->camera;
                c->area_exit = at;
            } else if(c->area_chunk >= 0 &&
                      v3_length_squared64(v3_sub(at, c->area_exit)) > (uint64_t)FX(1500) * FX(1500)){
                c->area_chunk = -1;
            }
            chunk = c->area_chunk;
            source = chunk >= 0 ? water_source : CAMSRC_DEFAULT;
            if(chunk < 0 && (category == 1 || category == 2)){
                chunk = game->stage.header->camera_count + (category == 2);
                source = water_source;
            }
        }
    }
    c->event_target.last_move = V3(0, 0, 0);
    if(chunk >= 0 && gcam_param(chunk)->type == CAM_FOLLOW && source != CAMSRC_SURFACE &&
       source != CAMSRC_SWIM && !level_has(LEVEL_FOLLOW_CAMERA))
        chunk = -1;

    if(!c->out_valid || !c->active){
        c->out_eye = c->blend_eye = cam->position;
        c->out_watch = c->blend_watch = cam->target;
        c->out_up = c->blend_up = cam->up;
        c->out_valid = true;
    }
    int old_chunk = c->chunk;
    if(chunk != c->chunk || source != c->source){
        c->reset = true;
        c->blend_t = 0;
        c->blend_total = chunk >= 0 ? gcam_param(chunk)->camint : 0;

        if(source == CAMSRC_EVENT && chunk >= 0 && !(gcam_param(chunk)->flags & CAMF_ERP_FRAME))
            c->blend_total = 60;
        if(c->source == CAMSRC_EVENT && source != CAMSRC_EVENT){
            const StageCamera* old = c->chunk >= 0 ? gcam_param(c->chunk) : NULL;
            c->blend_total = !old                              ? 60
                             : old->flags & CAMF_END_ERP_FRAME ? old->camendint
                             : old->flags & CAMF_ERP_FRAME     ? old->camint
                                                               : 60;
        }
        if(!c->blend_total && source == CAMSRC_AREA && c->chunk != -2) c->blend_total = GCAM_MIN_BLEND;
        c->force_change = !c->blend_total;
        c->vpan_valid = false;
        c->round_angle = c->round_target = 0;
        if(chunk < 0){
            cam->target = c->out_watch;
            cam->smooth_up =
                v3_tangent14(c->out_up, v3_direction14(v3_sub(c->out_watch, c->out_eye), cam->front), p->up);
            cam->heading = v3_tangent14(v3_sub(c->out_watch, c->out_eye), cam->smooth_up, cam->heading);
        }
        c->chunk = chunk;
        c->source = source;
    }
    c->active = chunk >= 0;
    if(chunk < 0){
        c->snap = false;
        return false;
    }

    const StageCamera* cp = gcam_param(chunk);
    gcam_pose(cp, &target, keys);
    bool reset = c->reset;
    c->reset = false;

    v3 watch = c->watch, up = c->up;

    bool framed = cutscene_blocks(BLOCK_CAMERA_FRAMING);
    fx cap = framed                   ? FX(8000)
             : cp->type == CAM_SPIRAL ? FX(4500)
             : source == CAMSRC_EVENT ? GCAM_EVENT_CAP
                                      : GCAM_DIST_CAP;

    int anchor_height = cp->type == CAM_CHARMED_VECREG_TOWER || cp->type == CAM_TRUNDLE ? 0
                        : level_camera_low_anchor(cp, source, chunk)                    ? 80
                                                                                        : -1;
    bool fixed = gcam_fixed_camera() && cp->type == CAM_POINT_FIX;
    v3 eye = fixed ? c->eye : gcam_frame(c->eye, &watch, &target, cap, anchor_height);

    if(reset && c->blend_total > 0 && level_camera_blend_checked(old_chunk, chunk) &&
       gcam_blend_blocked(eye, watch, c->blend_total))
        c->snap = true;
    bool snapped = c->snap;
    c->parity = !c->parity;

    bool side_view = cp->type == CAM_TRUNDLE || fixed || framed;
    if(!side_view) gcam_collide(&eye, &watch, reset && snapped, &c->col_len, &c->col_hit, c->parity);
    if(c->snap){
        c->snap = false;
        c->blend_total = 0;
        c->force_change = true;
    }

    v3 move = n && !fixed ? V3(target.last_move.x / n, target.last_move.y / n, target.last_move.z / n)
                          : V3(0, 0, 0);
    if(c->blend_total <= 0 || c->blend_t >= c->blend_total){
        c->blend_eye = eye;
        c->blend_watch = watch;
        c->blend_up = up;
    } else {
        v3 d1 = v3_sub(eye, watch);
        fx l1 = v3_length_exact(d1);
        v3 dir1 = v3_direction14(d1, V3(0, 0, N14_ONE));
        for(int i = 0; i < n && c->blend_t < c->blend_total; i++){
            c->blend_eye = v3_add(c->blend_eye, move);
            c->blend_watch = v3_add(c->blend_watch, move);
            int32_t r = (int32_t)(((int64_t)c->blend_t << 14) / c->blend_total);
            r = mul14(r, r);
            v3 d0 = v3_sub(c->blend_eye, c->blend_watch);
            fx l0 = v3_length_exact(d0);
            v3 dir0 = v3_direction14(d0, dir1);
            v3 dir = cam_slerp14(dir0, dir1, r, c->blend_up);
            fx l = l0 + (fx)(((int64_t)(l1 - l0) * r) >> 14);
            c->blend_watch = v3_add(c->blend_watch, v3_scale14(v3_sub(watch, c->blend_watch), r));
            c->blend_eye = v3_add(c->blend_watch, v3_set_length(dir, l, dir1));
            c->blend_up = cam_slerp14(c->blend_up, up, r, dir);
            c->blend_t++;
        }
    }
    if(c->force_change || (cp->flags & CAMF_ANTIBLUR_OFF)){
        c->force_change = false;
        c->out_eye = c->blend_eye;
        c->out_watch = c->blend_watch;
        c->out_up = c->blend_up;
    } else if(n){
        int32_t k = cam_ease(R14(0.3f), n);
        v3 carried = V3(move.x * n, move.y * n, move.z * n);
        c->out_eye = v3_add(c->out_eye, carried);
        c->out_watch = v3_add(c->out_watch, carried);
        c->out_eye = v3_add(c->out_eye, v3_scale14(v3_sub(c->blend_eye, c->out_eye), k));
        c->out_watch = v3_add(c->out_watch, v3_scale14(v3_sub(c->blend_watch, c->out_watch), k));
        c->out_up = cam_slerp14(c->out_up, c->blend_up, k, v3_sub(c->out_watch, c->out_eye));
    }
    eye = c->out_eye;
    watch = c->out_watch;
    up = c->out_up;

    if(c->blend_t < c->blend_total && !side_view)
        gcam_collide(&eye, &watch, reset && snapped, &c->col_shown, &c->col_hit_shown, !c->parity);
    else c->col_shown = v3_length_exact(v3_sub(eye, watch));
    if(c->shake_timer > 0){
        c->shake_timer = fx_max(c->shake_timer - n, 0);
        fx a = (c->shake_timer & 2 ? 1 : -1) * c->shake_level * FX(12) * c->shake_timer / 24;
        eye = v3_add(eye, v3_scale14(up, a));
        watch = v3_add(watch, v3_scale14(up, a));
    }

    cam->position = eye;
    cam->target = watch;
    cam->front = v3_direction14(v3_sub(watch, eye), cam->front);
    cam->up = gcam_up_from(up, eye, watch);
    cam->smooth_up = cam->up;
    cam->heading = v3_tangent14(cam->front, cam->smooth_up, cam->heading);
    camera_update_angles(cam);
    return true;
}
