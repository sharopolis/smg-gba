enum {
    BUOY_WAIT = 64,
    BUOY_MOVE,
    BUOY_RETURN,
    BUOY_ADJUST,
    BUOY_SCREW,
    BUOY_END,
    BUOY_BREAK,
    BUOY_APPEAR,
    BUOY_CHASE,
    BUOY_TARGET,
    BUOY_CHASESTART,
    BUOY_BUBBLE_KILLWAIT
};

static EWRAM_BSS BuoyBaseState g_buoy;
static bool buoy_stage(void){ return g_game.stage_index == STAGE_BUOYBASE; }

enum { BUOY_STAR_RISE = 64, BUOY_STAR_MOVE, BUOY_STAR_WAIT };
enum { BUOY_STAR_STREAM_PENDING = 1, BUOY_STAR_STREAM_DONE = 2 };

static bool buoy_cage_star_owner(const Actor* a){
    if(!buoy_stage() || a->type != OBJ_POWERSTAR || a->stage_index < 0) return false;
    const StageObject* o = actor_stage_object(a);
    return o && o->id == 111 && o->args[0] == 2;
}
static bool buoy_cage_star_busy(const Actor* a){
    return buoy_cage_star_owner(a) && (a->state == BUOY_STAR_RISE || a->state == BUOY_STAR_MOVE);
}
static void buoy_cage_star_begin(Actor* a, v3 from){
    a->state = BUOY_STAR_RISE;
    a->value = 0;
    a->timer = 0;
    a->target = from;
    a->position = from;

    GravitySample gravity = gravity_sample(g_game.stage.header, from);
    if(gravity.index >= 0) a->up = gravity.up;
    a->front = v3_tangent14(a->front, a->up, a->front);
    v3 risen = v3_add(from, v3_scale14(a->up, FX(300)));
    fx arc = fx_clamp(v3_length_exact(v3_reject14(v3_sub(a->home, risen), a->up)) / 2, FX(300), FX(5000));
    a->velocity = V3(0, arc, 0);
    power_star_reveal(a);
    a->flags &= ~ACTOR_HIDDEN;
    g_object_state[a->stage_index] |= OBJECT_STATE_WAS_ACTIVE;
}
static v3 buoy_cage_star_origin(void){
    for(uint32_t i = 0; i < g_game.stage.header->object_count; i++){
        const StageObject* cage = &g_game.stage.objects[i];
        if(cage->id == 120 && cage->type == OBJ_BUOYCAGE && cage->args[7] == 7)
            return v3_add(V3(cage->position[0], cage->position[1], cage->position[2]),
                          v3_scale14(v3_from_i16(cage->up), FX(150)));
    }
    return V3(0, 0, 0);
}
static void buoy_cage_star_init(Actor* a, const StageObject* o){
    if(!buoy_stage() || o->type != OBJ_POWERSTAR || o->id != 111 || o->args[0] != 2) return;
    a->state = BUOY_STAR_WAIT;
    a->value = -1;
    a->velocity = V3(0, 0, 0);
    if(stream_part_state(a->stage_index) == BUOY_STAR_STREAM_PENDING)
        buoy_cage_star_begin(a, buoy_cage_star_origin());
}
static void buoy_cage_star_request(const Actor* cage){
    if(!buoy_stage()) return;
    const StageObject* co = actor_stage_object(cage);
    if(!co || co->id != 120 || co->args[7] != 7) return;
    for(uint32_t i = 0; i < g_game.stage.header->object_count; i++){
        const StageObject* o = &g_game.stage.objects[i];
        if(o->type != OBJ_POWERSTAR || o->id != 111 || o->args[0] != 2 ||
           (o->scenarios != 0xff && !(o->scenarios & (1 << (g_game.scenario - 1)))))
            continue;
        if(power_star_revealed((int)i)) return;

        stream_part_state_set((int)i, BUOY_STAR_STREAM_PENDING);
        g_power_star_revealed[i >> 3] |= (uint8_t)(1 << (i & 7));
        Actor* star = NULL;
        for(int j = 0; j < g_game.actor_count; j++){
            Actor* a = &g_game.actors[j];
            if(a->type == OBJ_POWERSTAR && a->stage_index == (int)i){
                star = a;
                break;
            }
        }
        if(!star) star = actors_spawn_object(&g_game.stage, i);
        if(star && !buoy_cage_star_busy(star))
            buoy_cage_star_begin(star, v3_add(cage->position, v3_scale14(cage->up, FX(150))));
        return;
    }
}
static void buoy_cage_star_tick(Actor* a){
    if(!a->active || !buoy_cage_star_owner(a)) return;
    fx rotation = FX_TAU * 3 / 360;
    if(buoy_cage_star_busy(a)){
        int s = a->value++;
        v3 risen = v3_add(a->target, v3_scale14(a->up, FX(300)));
        if(s <= 80){
            int t = fx_min(s, 60) * N14_ONE / 60;
            fx eased = fx_sin(mul14(t, FX_TAU / 4 << 6) >> 6);
            a->position = v3_add(a->target, v3_scale14(a->up, (FX(300) * eased) >> 8));
            rotation = FX_TAU * 10 / 360;
        } else {
            int step = s - 81;
            a->state = BUOY_STAR_MOVE;
            if(step == 0) audio_queue_effect(SFX_POWER_STAR_APPEAR, 5);
            if(step < 120){
                int t = step * N14_ONE / 120;
                fx arc = (a->velocity.y * fx_sin(mul14(t, FX_TAU / 2 << 6) >> 6)) >> 8;
                a->position =
                    v3_add(v3_add(risen, v3_scale14(v3_sub(a->home, risen), t)), v3_scale14(a->up, arc));
                rotation = FX_TAU * 20 / 360;
            } else {
                a->position = a->home;
                int t = fx_min(step - 120, 90) * N14_ONE / 90;
                fx ease = fx_sin(mul14(t, FX_TAU / 4 << 6) >> 6);
                rotation = FX_TAU * 20 / 360 - (((FX_TAU * 20 / 360 - FX_TAU * 3 / 360) * ease) >> 8);
                if(step == 120)
                    audio_queue_effect(water_query(a->home, a->up, NULL) ? SFX_POWER_STAR_MOVE_END_WATER
                                                                         : SFX_POWER_STAR_MOVE_END,
                                       5);
                if(step == 210){
                    a->state = BUOY_STAR_WAIT;
                    a->value = -1;
                    stream_part_state_set(a->stage_index, BUOY_STAR_STREAM_DONE);
                }
            }
        }
    }
    a->velocity.x = (a->velocity.x + rotation) % FX_TAU;
}

__attribute__((noinline)) static bool buoy_floater_unclipped(const Actor* a){
    return buoy_stage() && a->type == OBJ_BUOYFLOATERU && g_buoy.rise_step >= 90;
}

__attribute__((noinline)) static fx buoy_figure_radius(const Figure* figure){
    if(!buoy_stage() || (figure->model != (const FigureHeader*)figure_hatchwaterplanet &&
                         figure->model != (const FigureHeader*)figure_oceanfloatertypeu))
        return 0;
    fx scale = fx_max(fx_abs(figure->scale.x), fx_max(fx_abs(figure->scale.y), fx_abs(figure->scale.z)));
    return fx_mul((fx)figure->model->extent, scale) * 2;
}

static bool buoy_figure_both_sides(const Figure* figure){
    return buoy_stage() && figure->model == (const FigureHeader*)figure_oceanfloatertypeu;
}
__attribute__((noinline)) static bool buoy_floater_draw(uint8_t* framebuffer, Figure* figure,
                                                        const CameraView* cv){
    if(!g_oceanbowl_split || !buoy_figure_both_sides(figure)) return false;
    g_engine_fb = framebuffer;
    const FigureHeader* model = figure->model;
    const m3* m = &figure->transform.m;
    v3 d = V3(fx_mul(m->r[0].x, cv->direction.x) + fx_mul(m->r[1].x, cv->direction.y) +
                  fx_mul(m->r[2].x, cv->direction.z),
              fx_mul(m->r[0].y, cv->direction.x) + fx_mul(m->r[1].y, cv->direction.y) +
                  fx_mul(m->r[2].y, cv->direction.z),
              fx_mul(m->r[0].z, cv->direction.x) + fx_mul(m->r[1].z, cv->direction.y) +
                  fx_mul(m->r[2].z, cv->direction.z));
    int axis = view_nearest_axis(d);
    const uint16_t* order = model_draw_order(model, axis);
    xform t = xform_compose(&figure->transform, &cv->to_screen);
    const int16_t (*pose)[3] = model_resolve_pose(figure->pose, 4);
    TransformJob xj = geometry_transform_job(pose, g_figure_points, g_figure_screen_positions, NULL,
                                             model->point_count, &t, 0);
    engine_transform_points(&xj);
    const v3 r = figure->transform.m.r[2];
    bool below = g_oceanbowl.side;
    for(uint32_t i = 0; i < model->point_count; i++){
        fx z = figure->transform.o.z + ((r.x * pose[i][0] + r.y * pose[i][1] + r.z * pose[i][2]) >> 8);
        if((z < 0) != below) g_figure_points[i].clip |= CLIP_SIDES;
    }
    figure_render_sources(figure, false);
    int count = model->triangle_count, step = 2;
    if(axis & 1){
        const uint16_t* minus = figure_minus_orders(model);
        if(minus) order = minus + (axis >> 1) * count;
        else {
            order += count - 1;
            step = -2;
        }
    }
    FigureRasterJob job = {
        g_figure_points,         g_figure_screen_positions, order,  count,        step,
        g_figure_draw_triangles, g_figure_draw_textures,    RM_TEX, g_flat_small, figure_clipped};
    engine_draw_figure_triangles(&job);
    return true;
}

__attribute__((noinline)) static bool buoy_figure_wide_visible(const Figure* figure, const CameraView* view,
                                                               v3 position, fx depth){
    fx radius = buoy_figure_radius(figure);
    if(!radius) return false;
    const m3* m = &view->to_screen.m;
    fx margin[3];
    for(int a = 0; a < 3; a++)
        margin[a] =
            (fx)(((int64_t)radius * (fx_abs(m->r[a].x) + fx_abs(m->r[a].y) + fx_abs(m->r[a].z))) >> 8);
    if(position.x - position.z > margin[0] + margin[2] || -position.x - position.z > margin[0] + margin[2] ||
       position.y - position.z > margin[1] + margin[2] || -position.y - position.z > margin[1] + margin[2] ||
       position.z + margin[2] < CLIP_NEAR_Z || position.z - margin[2] > depth)
        return false;
    return true;
}

static const FigureOps buoy_hatch_figure_ops = {.wide_visible = buoy_figure_wide_visible};

static const FigureOps buoy_floater_figure_ops = {
    .draw = buoy_floater_draw,
    .wide_visible = buoy_figure_wide_visible,
    .both_sides = buoy_figure_both_sides,
};

static void buoy_reset(void){
    oceanbowl_reset();
    baby_attack_permit_reset();
    g_buoy.rise_step = -1;
    g_buoy.hatch_step = -1;
    g_buoy.bind = -1;
    g_buoy.hatch_open = 0;
}
static const BuoyPartParam* buoy_param(const Actor* a){
    const StageObject* o = actor_stage_object(a);
    if(!o) return NULL;
    for(unsigned i = 0; i < sizeof buoy_part_params / sizeof *buoy_part_params; i++)
        if(buoy_part_params[i].id == o->id) return &buoy_part_params[i];
    return NULL;
}
static bool buoy_no_yaw_kind(int type){
    switch(type){
    case OBJ_BUOYBABYLAUNCHER:
    case OBJ_BUOYMETBO:
    case OBJ_BUOYFLOATERU:
    case OBJ_BUOYHEXAGON:
    case OBJ_BUOYROTATEA:
    case OBJ_BUOYROTATEB:
    case OBJ_BUOYROTATED:
    case OBJ_BUOYFIXATION:
    case OBJ_BUOYCAGE:
    case OBJ_BUOYSCREW:
    case OBJ_BUOYSCREWREVERSE:
    case OBJ_BUOYVALVE:
    case OBJ_BUOYTORPEDOLAUNCHER:
    case OBJ_BUOYTORPEDO:
    case OBJ_BUOYAIRBUBBLE:
    case OBJ_BUOYOXYGEN:
    case OBJ_BUOYCHAIN:
    case OBJ_BUOYCHAINLINK:
    case OBJ_BUOYCHAINFIX:
    case OBJ_BUOYHATCH:
    case OBJ_BUOYJUMPGUARDER:
    case OBJ_BUOYJUMPHEAD:
    case OBJ_BUOYBEGOBABY:
    case OBJ_BUOYPENGUIN:
    case OBJ_BUOYDOSSUN:
    case OBJ_BUOYBIGBIRIKYU: return true;
    default: return false;
    }
}
static bool buoy_actor_init(Actor* a, const StageObject* o){
    if(!buoy_stage()) return false;
    if(a->type == OBJ_BUOYMETBO){
        metbo_init(a, o);
        return true;
    }
    if(a->type == OBJ_BUOYBABYLAUNCHER){
        babylauncher_init(a, o);
        return true;
    }
    if(a->type == OBJ_BUOYPENGUIN){
        swimming_penguin_init(a, o);
        return true;
    }
    if(a->type == OBJ_BUOYJUMPGUARDER){
        jumpguarder_init(a, o);
        return true;
    }
    if(a->type == OBJ_CARETAKER){
        a->figure.model = (const FigureHeader*)(o->args[3] == 1   ? figure_caretaker1
                                                : o->args[3] == 2 ? figure_caretaker2
                                                                  : figure_caretaker);
        a->state = AS_IDLE;
        a->anim = ANIM_CARETAKER_WAIT;
        a->anim_frame = 0;
        a->value = 0;
        return true;
    }
    if(!buoy_no_yaw_kind(a->type)) return false;
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    a->radius = a->radius * o->scale[0] / 256;
    actor_set_state(a, BUOY_WAIT, -1);
    a->value = 0;
    if(a->type == OBJ_BUOYHEXAGON){
        a->target = V3((o->id * 24833) % 411775, 0, 0);
        a->velocity = V3(0, 0, 0);
    }
    if(a->type == OBJ_BUOYCHAIN) a->health = 0;
    if(a->type == OBJ_BUOYROTATEA || a->type == OBJ_BUOYROTATEB || a->type == OBJ_BUOYROTATED)
        a->target = V3(0, 0, 0);
    if(a->type == OBJ_BUOYTORPEDOLAUNCHER){
        a->value = -1;
        a->health = 0;
    }
    if(a->type == OBJ_BUOYFLOATERU && switch_on(stage_object_switch(o, SWITCH_A)))
        a->position = rail_position_at(o->rail, rail_length(o->rail));
    if(a->type == OBJ_BUOYCAGE && stream_part_state(a->stage_index)){
        a->active = 0;
    }
    if(a->type == OBJ_BUOYDOSSUN) dossun_init(a);
    if(a->type == OBJ_BUOYBIGBIRIKYU) bigbirikyu_init(a, o);
    return true;
}
static const BuoyJointPose* buoy_joint_pose(const Actor* a, bool receiver){
    int frame = fx_clamp(a->state == BUOY_SCREW || a->state == BUOY_END ? a->anim_frame >> 8 : 0, 0, 255);
    if(a->type == OBJ_BUOYSCREW){
        frame = fx_min(frame, BUOY_SCREWSWITCHON_SCREW_FRAMES);
        return receiver ? &buoy_joint_screwswitchon_screwreceive[frame]
                        : &buoy_joint_screwswitchon_screw[frame];
    }
    if(a->type == OBJ_BUOYSCREWREVERSE){
        frame = fx_min(frame, BUOY_SCREWSWITCHREVERSEON_SCREW_FRAMES);
        return &buoy_joint_screwswitchreverseon_screw[frame];
    }
    if(a->type == OBJ_BUOYVALVE){
        bool reverse = a->args[0] > 0;
        frame =
            fx_min(frame, reverse ? BUOY_VALVESWITCHREVERSEON_VALVE_FRAMES : BUOY_VALVESWITCHON_VALVE_FRAMES);
        return reverse ? &buoy_joint_valveswitchreverseon_valve[frame]
                       : &buoy_joint_valveswitchon_valve[frame];
    }
    return NULL;
}
static v3 buoy_local_vec(v3 local, v3 side, v3 up, v3 front){
    return v3_add(v3_add(v3_scale14(side, local.x), v3_scale14(up, local.y)), v3_scale14(front, local.z));
}
static void buoy_joint_frame(const Actor* a, bool receiver, v3* pos, v3* up, v3* front){
    const BuoyJointPose* j = buoy_joint_pose(a, receiver);
    *pos = a->position;
    *up = a->up;
    *front = a->front;
    if(!j) return;
    int16_t scale[3];
    actor_stage_scale(a, scale);
    v3 side = v3_cross14(a->up, a->front);
    *pos = dyncol_point(j->pos, a->position, side, a->up, a->front, scale);
    *up = v3_direction14(buoy_local_vec(v3_from_i16(j->up), side, a->up, a->front), a->up);
    *front = v3_tangent14(buoy_local_vec(v3_from_i16(j->front), side, a->up, a->front), *up, a->front);
}
static void buoy_add_mesh(Actor* a, const HoneyColMesh* mesh, const uint16_t* codes, int32_t radius,
                          bool joint, bool receiver){
    v3 pos = a->position, up = a->up, front = a->front;
    int16_t scale[3];
    actor_stage_scale(a, scale);
    if(joint) buoy_joint_frame(a, receiver, &pos, &up, &front);

    int64_t sc = fx_max(fx_abs(scale[0]), fx_max(fx_abs(scale[1]), fx_abs(scale[2])));
    int64_t reach = (((int64_t)radius * sc >> 8) + FX(300) + 64) * 1774 >> 10;
    if((int64_t)v3_length_squared64(v3_sub(pos, g_game.player.figure.position)) > reach * reach) return;
    dyncol_add_codes(mesh, pos, up, front, scale, receiver ? -1 : actor_index(a), 0,
                     g_game.player.figure.position, FX(300), codes);
}
static void buoy_collision_build(void){
    dyncol_clear();
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(!a->active || a->state == BUOY_BREAK) continue;
        switch(a->type){
        case OBJ_BUOYFLOATERU:
            buoy_add_mesh(a, &buoy_mesh_oceanfloatertypeu, buoy_codes_oceanfloatertypeu,
                          buoy_r_oceanfloatertypeu, false, false);
            break;
        case OBJ_BUOYHEXAGON:
            buoy_add_mesh(a, &buoy_mesh_oceanhexagonfloater, buoy_codes_oceanhexagonfloater,
                          buoy_r_oceanhexagonfloater, false, false);
            break;
        case OBJ_BUOYROTATEA:
            buoy_add_mesh(a, &buoy_mesh_oceanfloatertowerrotatestepa, buoy_codes_oceanfloatertowerrotatestepa,
                          buoy_r_oceanfloatertowerrotatestepa, false, false);
            break;
        case OBJ_BUOYROTATEB:
            buoy_add_mesh(a, &buoy_mesh_oceanfloatertowerrotatestepb, buoy_codes_oceanfloatertowerrotatestepb,
                          buoy_r_oceanfloatertowerrotatestepb, false, false);
            break;
        case OBJ_BUOYROTATED:
            buoy_add_mesh(a, &buoy_mesh_oceanfloatertowerrotatestepd, buoy_codes_oceanfloatertowerrotatestepd,
                          buoy_r_oceanfloatertowerrotatestepd, false, false);
            break;
        case OBJ_BUOYFIXATION:
            buoy_add_mesh(a, &buoy_mesh_breakablefixation, buoy_codes_breakablefixation,
                          buoy_r_breakablefixation, false, false);
            break;
        case OBJ_BUOYCAGE:
            buoy_add_mesh(a, &buoy_mesh_breakablecage, buoy_codes_breakablecage, buoy_r_breakablecage, false,
                          false);
            break;
        case OBJ_BUOYSCREW:
            buoy_add_mesh(a, &buoy_mesh_screwswitch_screwcol, buoy_codes_screwswitch_screwcol,
                          buoy_r_screwswitch_screwcol, true, false);
            buoy_add_mesh(a, &buoy_mesh_screwswitch_screwreceivecol, buoy_codes_screwswitch_screwreceivecol,
                          buoy_r_screwswitch_screwreceivecol, true, true);
            break;
        case OBJ_BUOYSCREWREVERSE:
            if(a->state != BUOY_END && !(a->state == BUOY_SCREW && a->timer >= 25))
                buoy_add_mesh(a, &buoy_mesh_screwswitchreverse_screwreversecol,
                              buoy_codes_screwswitchreverse_screwreversecol,
                              buoy_r_screwswitchreverse_screwreversecol, true, false);
            break;
        case OBJ_BUOYVALVE:
            buoy_add_mesh(a, &buoy_mesh_valveswitch_valvecol, buoy_codes_valveswitch_valvecol,
                          buoy_r_valveswitch_valvecol, true, false);
            break;
        case OBJ_BUOYAIRBUBBLE:
            buoy_add_mesh(a, &buoy_mesh_airbubblegenerator, buoy_codes_airbubblegenerator,
                          buoy_r_airbubblegenerator, false, false);
            break;
        case OBJ_BUOYCHAINLINK:
            buoy_add_mesh(a, &buoy_mesh_largechain, buoy_codes_largechain, buoy_r_largechain, false, false);
            break;
        case OBJ_BUOYCHAINFIX:
            buoy_add_mesh(a, &buoy_mesh_largechainfixpoint, buoy_codes_largechainfixpoint,
                          buoy_r_largechainfixpoint, false, false);
            break;
        case OBJ_BUOYDOSSUN:
            buoy_add_mesh(a, &buoy_mesh_dossun, buoy_codes_dossun, buoy_r_dossun, false, false);
            break;
        case OBJ_EARTHENPIPE: break;
        default: break;
        }
    }
}
static void buoy_parts_hidden(void){
    if(!buoy_stage()) return;

    g_parts_hidden = g_buoy.hatch_open ? 1u : 2u;
    g_col_parts_hidden = g_parts_hidden;
}

static bool buoy_rise_camera_switch(int old_chunk, int new_chunk){
    return buoy_stage() && ((old_chunk >= BUOY_RISE_CAMERA && old_chunk < BUOY_RISE_CAMERA + 3) ||
                            (new_chunk >= BUOY_RISE_CAMERA && new_chunk < BUOY_RISE_CAMERA + 3));
}

static bool buoy_camera_path_blocked(v3 from, v3 to){
    RayHit hit;
    if(collision_ray(from, to, &hit, COL_NO_CAMERA)) return true;

    for(int i = 0; i < g_game.actor_count; i++){
        const Actor* a = &g_game.actors[i];
        if(!a->active || a->type != OBJ_BUOYFLOATERU) continue;
        int64_t o[3] = {-(from.x - a->position.x), from.z - a->position.z, from.y - a->position.y};
        int64_t d[3] = {-(to.x - from.x), to.z - from.z, to.y - from.y};
        for(unsigned t = 0; t < sizeof buoy_t_oceanfloatertypeu / sizeof *buoy_t_oceanfloatertypeu; t++){
            const int16_t* v0 = buoy_v_oceanfloatertypeu[buoy_t_oceanfloatertypeu[t][0]];
            const int16_t* v1 = buoy_v_oceanfloatertypeu[buoy_t_oceanfloatertypeu[t][1]];
            const int16_t* v2 = buoy_v_oceanfloatertypeu[buoy_t_oceanfloatertypeu[t][2]];
            int64_t e1[3], e2[3], tv[3], pv[3], qv[3];
            for(int k = 0; k < 3; k++){
                e1[k] = v1[k] - v0[k];
                e2[k] = v2[k] - v0[k];
                tv[k] = o[k] - v0[k];
            }
            pv[0] = d[1] * e2[2] - d[2] * e2[1];
            pv[1] = d[2] * e2[0] - d[0] * e2[2];
            pv[2] = d[0] * e2[1] - d[1] * e2[0];
            int64_t det = e1[0] * pv[0] + e1[1] * pv[1] + e1[2] * pv[2];
            if(!det) continue;
            int sg = det < 0 ? -1 : 1;
            det *= sg;
            int64_t u = (tv[0] * pv[0] + tv[1] * pv[1] + tv[2] * pv[2]) * sg;
            if(u < 0 || u > det) continue;
            qv[0] = tv[1] * e1[2] - tv[2] * e1[1];
            qv[1] = tv[2] * e1[0] - tv[0] * e1[2];
            qv[2] = tv[0] * e1[1] - tv[1] * e1[0];
            int64_t w = (d[0] * qv[0] + d[1] * qv[1] + d[2] * qv[2]) * sg;
            if(w < 0 || u + w > det) continue;
            int64_t tt = (e2[0] * qv[0] + e2[1] * qv[1] + e2[2] * qv[2]) * sg;
            if(tt >= 0 && tt <= det) return true;
        }
        break;
    }
    return false;
}
static void buoy_events_tick(void){
    floater_rise_events();
    if(g_buoy.hatch_step < 0 && switch_on(1007)) g_buoy.hatch_step = 0;
    else if(g_buoy.hatch_step >= 0 && g_buoy.hatch_step < 255) ++g_buoy.hatch_step;

    if(g_buoy.hatch_step >= 31 + BUOY_HATCH_BCK_FRAMES) g_buoy.hatch_open = 1;
    buoy_parts_hidden();
    if(g_buoy.hatch_step == 30) audio_play_effect(SFX_BUOY_OJ_HATCH_WATER_PNT_ST, 2);
    if(g_buoy.hatch_step == 209) audio_play_effect(SFX_BUOY_OJ_HATCH_WATER_PNT_ED, 2);
    if(switch_on(1001)) return;
    int count = 0;
    for(uint32_t i = 0; i < g_game.stage.header->object_count; i++){
        const StageObject* o = &g_game.stage.objects[i];
        if(o->type == OBJ_BLUECHIP && o->args[0] == 0 && (o->scenarios & (1 << (g_game.scenario - 1))) &&
           stream_object_collected(i))
            count++;
    }
    if(count == 5) switch_set(1001, true);
}
static void buoy_chain_clip(void){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(!a->active || (a->type != OBJ_BUOYCHAINLINK && a->type != OBJ_BUOYCHAINFIX)) continue;
        int parent = a->args[3];
        if(parent < 0 || parent >= (int)g_game.stage.header->object_count){
            a->active = 0;
            continue;
        }
        const StageObject* o = &g_game.stage.objects[parent];
        int sleep = stage_object_switch(o, SWITCH_SLEEP);
        if((sleep >= 0 && !switch_on(sleep)) ||
           !stream_near(V3(o->position[0], o->position[1], o->position[2]), g_game.player.figure.position,
                        STREAM_KEEP)){
            a->active = 0;
            for(int j = 0; j < g_game.actor_count; j++)
                if(g_game.actors[j].active && g_game.actors[j].stage_index == parent)
                    g_game.actors[j].health = 0;
        }
    }
}

static void buoy_actors_pre(void){
    if(!buoy_stage()) return;
    jumpguarder_pre_update();
    oceanbowl_tick();
    switch_objects_tick();
    buoy_events_tick();
    buoy_chain_clip();
    buoy_collision_build();
    mario_ride_find();
}

static bool buoy_ride_carry_step(void){
    if(g_mario_link.ride < 0) return false;
    const Actor* a = &g_game.actors[g_mario_link.ride];
    if(!a->active || (a->type != OBJ_BUOYROTATEA && a->type != OBJ_BUOYROTATEB && a->type != OBJ_BUOYROTATED))
        return false;
    v3 f0 = g_mario_link.ride_front, f1 = a->front;
    int64_t c = (int64_t)f0.x * f1.x + (int64_t)f0.y * f1.y, s = (int64_t)f0.x * f1.y - (int64_t)f0.y * f1.x;
    if(!s) return true;
    Player* p = &g_game.player;
    int64_t dx = p->figure.position.x - a->position.x, dy = p->figure.position.y - a->position.y;
    v3 w = V3(a->position.x + (fx)((dx * c - dy * s + (1 << 27)) >> 28),
              a->position.y + (fx)((dx * s + dy * c + (1 << 27)) >> 28), p->figure.position.z);
    int64_t fx_ = p->front.x, fy = p->front.y;
    p->front = v3_tangent14(V3((fx)((fx_ * c - fy * s) >> 28), (fx)((fx_ * s + fy * c) >> 28), p->front.z),
                            p->up, p->front);
    p->figure.position = w;
    g_mario.position16 = V3(w.x << 8, w.y << 8, w.z << 8);
    return true;
}
static void buoy_actors_post(void){
    if(buoy_stage() && !buoy_ride_carry_step()) mario_ride_carry();
}
static void buoy_break_cage(Actor* a){
    if(a->state != BUOY_WAIT) return;
    const StageObject* o = actor_stage_object(a);
    actor_set_state(a, BUOY_BREAK, -1);
    a->flags |= ACTOR_HIDDEN;
    if(a->type == OBJ_BUOYCAGE){
        a->flags &= ~ACTOR_HIDDEN;
        a->figure.model = (const FigureHeader*)figure_breakablecagebreak;
        actor_set_state(a, BUOY_BREAK, ANIM_BREAKABLECAGEBREAK_BREAK);
    }
    audio_play_effect(
        a->type == OBJ_BUOYFIXATION ? SFX_BUOY_OJ_BREAK_FIXATION_BREAK : SFX_BUOY_OJ_IRON_CAGE_BREAK, 3);
    stream_part_state_set(a->stage_index, 1);
    if(a->type == OBJ_BUOYFIXATION || (o && o->args[7] == 7) || (o && o->args[2] > 0))
        switch_set(stage_object_switch(o, SWITCH_DEAD), true);
    if(o && o->args[7] == 7) buoy_cage_star_request(a);
}
static bool chasing_killer_hit_target(Actor* p){
    if(!buoy_stage()) return false;

    for(int i = 0; i < g_game.actor_count; i++){
        Actor* target = &g_game.actors[i];
        if(!target->active || target->type != OBJ_BUOYMETBO) continue;
        fx r = 100 + 70;
        v3 d = v3_sub(target->position, p->position);
        if(fx_abs(d.x) > r || fx_abs(d.y) > r || fx_abs(d.z) > r) continue;
        if(v3_length_squared64(d) <= (uint64_t)r * r &&
           metbo_message(target, METBO_MSG_EXPLOSION, p->position))
            return true;
    }
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(!a->active || a->state != BUOY_WAIT || (a->type != OBJ_BUOYFIXATION && a->type != OBJ_BUOYCAGE))
            continue;
        fx r = a->radius + FX(70);
        {
            v3 d = v3_sub(a->position, p->position);
            if(fx_abs(d.x) > r || fx_abs(d.y) > r || fx_abs(d.z) > r) continue;
        }
        if(v3_length_squared64(v3_sub(a->position, p->position)) <= (uint64_t)r * r){
            buoy_break_cage(a);
            return true;
        }
    }
    return false;
}

static bool chasing_killer_ray_hit_cage(int tri){
    if(!buoy_stage()) return false;
    int owner = dyncol_owner(tri);
    if(owner < 0 || owner >= g_game.actor_count) return false;
    Actor* target = &g_game.actors[owner];
    if(!target->active || target->state != BUOY_WAIT ||
       (target->type != OBJ_BUOYCAGE && target->type != OBJ_BUOYFIXATION))
        return false;
    buoy_break_cage(target);
    return true;
}

static bool chasing_killer_cage_target(Actor* a, v3* aim){
    if(!buoy_stage()) return false;
    if(a->state == 67 && a->value >= 0 && a->value < g_game.actor_count){
        Actor* target = &g_game.actors[a->value];
        fx r = FX(750) + target->radius;
        if(target->active && target->state == BUOY_WAIT &&
           (target->type == OBJ_BUOYCAGE || target->type == OBJ_BUOYFIXATION) &&
           v3_length_squared64(v3_sub(target->position, a->position)) <= (uint64_t)r * r){
            *aim = v3_sub(target->position, a->position);
            return true;
        }
        return false;
    }
    if(a->state == KILLER_TARGET) return false;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* target = &g_game.actors[i];
        if(!target->active || target->state != BUOY_WAIT ||
           (target->type != OBJ_BUOYCAGE && target->type != OBJ_BUOYFIXATION))
            continue;
        fx r = FX(750) + target->radius;
        if(v3_length_squared64(v3_sub(target->position, a->position)) <= (uint64_t)r * r){
            a->value = i;
            *aim = v3_sub(target->position, a->position);
            return true;
        }
    }
    return false;
}

static bool chasing_killer_can_turn(Actor* a, v3 aim){
    if(a->state != KILLER_TARGET) return true;
    if(a->value < 0 || a->value >= g_game.actor_count) return false;
    v3 target = v3_direction14(v3_sub(g_game.actors[a->value].position, a->position), V3(0, 0, 0));
    v3 proposed = v3_direction14(aim, a->front);
    return v3_dot14(target, a->front) < v3_dot14(target, proposed);
}

static void chasing_killer_ground_aim(Actor* a, v3 gravity_up, v3* aim){
    if(!buoy_stage()) return;
    RayHit hit;
    v3 from = v3_add(a->position, v3_scale14(gravity_up, FX(50)));

    v3 to = v3_sub(a->position, v3_scale14(gravity_up, FX(301)));
    if(!collision_ray(from, to, &hit, 0)) return;
    uint64_t distance2 = v3_length_squared64(v3_sub(hit.point, a->position));
    int owner = dyncol_owner(hit.tri);
    Actor* sensor = owner >= 0 && owner < g_game.actor_count ? &g_game.actors[owner] : NULL;
    if(sensor && sensor->active && sensor->type == OBJ_BUOYCAGE){
        if(distance2 < (uint64_t)FX(300) * FX(300)) *aim = v3_sub(sensor->position, a->position);
    } else if(distance2 < (uint64_t)FX(150) * FX(150)){
        v3 target = v3_direction14(*aim, a->front), cross = v3_cross14(target, a->up);

        if(fx_abs(cross.x) > R14(.01f) || fx_abs(cross.y) > R14(.01f) || fx_abs(cross.z) > R14(.01f))
            *aim = v3_tangent14(target, a->up, a->front);
    }
}
static void buoy_float_think(Actor* a){
    bool riding = g_mario_link.ride == actor_index(a);
    const BuoyPartParam* q = buoy_param(a);
    if(!q) return;

    int32_t phase = a->target.x, step = 411775 / 330;
    if(riding){
        phase += (phase > 102944 && phase < 308831) ? step : -step;
        if(fx_abs(phase - 308831) <= 2 * step) phase = 308831;
    } else phase += step;
    phase = (phase + 411775) % 411775;
    a->target.x = phase;
    bool locked = stage_object_switch(actor_stage_object(a), SWITCH_A) >= 0 &&
                  switch_on(stage_object_switch(actor_stage_object(a), SWITCH_A));
    if(a->state == BUOY_WAIT && riding && !locked){
        a->velocity.x = -q->axis * V16(.1f);
        a->velocity.y = 0;
        actor_set_state(a, BUOY_MOVE, -1);
    }
    if(a->state == BUOY_MOVE && (!riding || locked)){
        a->target.y = v3_length_exact(v3_sub(a->position, a->home)) << 8;
        a->velocity.x = a->velocity.y = 0;
        actor_set_state(a, BUOY_RETURN, -1);
    }
    if(a->state == BUOY_RETURN && riding && !locked){
        a->velocity.x = -q->axis * V16(.1f);
        a->velocity.y = 0;
        actor_set_state(a, BUOY_MOVE, -1);
    }
    if(a->state == BUOY_MOVE || a->state == BUOY_RETURN){
        int32_t rest = a->state == BUOY_MOVE ? -q->condition * 256 : a->target.y;
        int32_t speed = a->velocity.x * (locked ? 3 : 1);
        a->velocity.z += speed;
        a->velocity.y += a->velocity.x;
        a->velocity.x += (int32_t)((int64_t)(rest - a->velocity.y) * q->speed / 1000);
        a->velocity.x = (int32_t)((int64_t)a->velocity.x * q->angle / 1000);
        if(a->state == BUOY_RETURN && fx_abs(a->velocity.z) < V16(.1f) && fx_abs(a->velocity.x) < V16(.1f)){
            a->velocity = V3(0, 0, 0);
            actor_set_state(a, BUOY_WAIT, -1);
        }
    }
    fx wave = mul14(FX(50), fx_sin(phase >> 8) << 6);
    a->position = v3_add(a->home, v3_scale14(a->up, (a->velocity.z >> 8) + wave));
}
static void buoy_switch_think(Actor* a){
    const StageObject* o = actor_stage_object(a);
    if(!o) return;
    bool rev = a->type == OBJ_BUOYSCREWREVERSE;
    v3 pos, up, front;
    buoy_joint_frame(a, false, &pos, &up, &front);
    pos = v3_add(pos, v3_scale14(a->up, FX(rev ? 130 : a->type == OBJ_BUOYVALVE ? 75 : 150)));
    Player* p = game_player();
    if(a->state == BUOY_WAIT){
        fx r = FX(rev ? 120 : 150) + C_BODY_RADIUS;
        v3 body = v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT));
        if(!g_mario_held && g_mario_link.ride == actor_index(a) && g_mario.attack_timer &&
           v3_length_squared64(v3_sub(body, pos)) <= (uint64_t)r * r){
            g_buoy.bind = (int16_t)actor_index(a);
            a->target = p->figure.position;
            mario_hold_begin();
            actor_set_state(a, BUOY_ADJUST, -1);
        }
    } else if(a->state == BUOY_ADJUST){
        int t = fx_min(a->timer * N14_ONE / 3, N14_ONE);
        mario_hold_move(v3_add(a->target, v3_scale14(v3_sub(pos, a->target), t)), p->front);
        if(a->timer >= 3){
            actor_set_state(a, BUOY_SCREW,
                            a->type == OBJ_BUOYSCREW ? ANIM_SCREWSWITCH_ON
                            : rev                    ? ANIM_SCREWSWITCHREVERSE_ON
                            : a->args[0] > 0         ? ANIM_VALVESWITCH_REVERSEON
                                                     : ANIM_VALVESWITCH_ON);
            mario_restart_anim(rev ? ANIM_MARIO_SCREWSWITCHREVERSEON : ANIM_MARIO_SCREWSWITCHON);
        }
    } else if(a->state == BUOY_SCREW){
        if(g_buoy.bind == actor_index(a)) mario_hold_move(pos, p->front);
        if(rev && a->timer == 25 && g_buoy.bind == actor_index(a)){
            mario_hold_release(a->args[0] > 0 ? v3_scale14(p->up, V16(28)) : V3(0, 0, 0));
            g_buoy.bind = -1;
        }
        int stop = a->type == OBJ_BUOYSCREW ? BUOY_SCREWSWITCHON_SCREW_FRAMES
                   : rev                    ? BUOY_SCREWSWITCHREVERSEON_SCREW_FRAMES
                   : a->args[0] > 0         ? BUOY_VALVESWITCHREVERSEON_VALVE_FRAMES
                                            : BUOY_VALVESWITCHON_VALVE_FRAMES;

        if(a->type == OBJ_BUOYSCREW && a->timer == 31) audio_play_effect(SFX_BUOY_OJ_SCREW_SWITCH_ON, 2);
        if(a->timer >= stop){
            switch_set(stage_object_switch(o, SWITCH_A), true);
            if(a->type != OBJ_BUOYSCREW)
                audio_play_effect(rev ? SFX_BUOY_OJ_SCR_SWITCH_R_ON : SFX_BUOY_OJ_VALVE_SWITCH_ON, 2);
            if(g_buoy.bind == actor_index(a)){
                mario_hold_release(V3(0, 0, 0));
                g_buoy.bind = -1;
                mario_set_action(MA_WAIT);
            }
            a->state = BUOY_END;
            a->timer = 0;
            if(rev) a->active = 0;
        }
    }
}

static void buoy_torpedo_break(Actor* a){
    a->state = AS_DEFEATED;
    a->timer = 0;
    audio_play_effect(SFX_BUOY_EM_TORPEDO_EXPLOSION, 3);
}

static bool buoy_torpedo_appear(Actor* a){
    if(a->state != BUOY_APPEAR) return false;
    int step = a->timer > 0 ? a->timer - 1 : 0;
    a->position = v3_add(a->home, v3_scale14(a->target, FX(fx_min(step, 20) * 10)));
    if(step > 20 && step < 42){
        int rumble = step - 20;
        fx sine = fx_sin(rumble * 100 * FX_TAU / 360) * 64;
        fx offset = (22 - rumble) * mul14(FX(18), sine) / 22;
        a->position = v3_add(a->position, v3_scale14(a->target, offset));
    }
    a->front = a->target;
    a->anim_frame = -FX_ONE;
    if(step == 20) audio_play_effect(SFX_BUOYROCKET_KILLER_STANDBY_END, 2);
    if(step >= 72){
        actor_set_state(a, BUOY_MOVE, ANIM_TORPEDO_MOVE);
        audio_play_effect(SFX_BUOY_EM_KILLER_FIRING, 2);
    }
    return true;
}
static void buoy_torpedo_think(Actor* a){
    Player* p = game_player();
    if(buoy_torpedo_appear(a)) return;

    if(a->state == BUOY_CHASESTART && a->timer == 1) audio_play_effect(SFX_BUOYROCKET_KILLER_JET, 2);
    v3 aim = v3_sub(v3_add(p->figure.position, v3_scale14(p->up, FX(75))), a->position);
    if(a->state == BUOY_MOVE && a->timer >= 35 && v3_dot14(aim, a->front) > 0 &&
       v3_length_squared64(aim) <=
           (uint64_t)FX(a->args[0] > 0 ? a->args[0] : 2000) * FX(a->args[0] > 0 ? a->args[0] : 2000))
        actor_set_state(a, BUOY_CHASESTART, ANIM_TORPEDO_CHASESTART);
    if(a->state == BUOY_CHASESTART && a->timer >= 59) actor_set_state(a, BUOY_CHASE, ANIM_TORPEDO_CHASE);
    if(a->state == BUOY_CHASE || a->state == BUOY_TARGET || a->state == BUOY_CHASESTART){
        if(!actor_mario_within(a, FX(3000))){
            buoy_torpedo_break(a);
            return;
        } else if(a->state == BUOY_CHASE)
            for(int i = 0; i < g_game.actor_count; i++){
                Actor* target = &g_game.actors[i];
                if(!target->active || target->state != BUOY_WAIT ||
                   (target->type != OBJ_BUOYCAGE && target->type != OBJ_BUOYFIXATION))
                    continue;
                if(v3_length_squared64(v3_sub(target->position, a->position)) <=
                   (uint64_t)(FX(750) + target->radius) * (FX(750) + target->radius)){
                    a->value = (int16_t)i;
                    actor_set_state(a, BUOY_TARGET, ANIM_TORPEDO_CHASE);
                    break;
                }
            }
    }
    if(a->state == BUOY_TARGET && a->value >= 0 && a->value < g_game.actor_count){
        Actor* t = &g_game.actors[a->value];
        fx r = FX(750) + t->radius;
        if(t->active && t->state == BUOY_WAIT && (t->type == OBJ_BUOYCAGE || t->type == OBJ_BUOYFIXATION) &&
           v3_length_squared64(v3_sub(t->position, a->position)) <= (uint64_t)r * r)
            aim = v3_sub(t->position, a->position);
        else actor_set_state(a, BUOY_CHASE, ANIM_TORPEDO_CHASE);
    }
    v3 g = gravity_sample(g_game.stage.header, a->position).up;
    if(a->state == BUOY_CHASE || a->state == BUOY_TARGET || a->state == BUOY_CHASESTART)
        a->front = chasing_killer_steer(a->front, aim, g, actor_arg_or(a, 1, 3));
    else
        a->front = v3_direction14(
            v3_add(a->front, v3_scale14(v3_sub(v3_tangent14(a->front, g, a->front), a->front), R14(.03f))),
            a->front);
    a->up = v3_tangent14(g, a->front, a->up);
    v3 next = v3_add(a->position, v3_scale14(a->front, FX(5)));
    RayHit h;

    bool ignores_gravity = actor_arg_or(a, 2, 0) > 0 || actor_arg_or(a, 3, 0) > 0;
    bool water_break = (a->state != BUOY_MOVE || ignores_gravity) && !water_query(a->position, a->up, NULL);
    bool blast = (a->state == BUOY_MOVE && ++a->health > 600) || water_break || chasing_killer_hit_target(a);
    if(collision_ray(a->position, next, &h, 0)){
        chasing_killer_ray_hit_cage(h.tri);
        blast = true;
    }
    a->position = next;
    if(actor_mario_contact(a, 0) != CONTACT_NONE){
        actor_hurt_mario(a, 1);
        blast = true;
    }
    if(blast){
        buoy_torpedo_break(a);
    }
}

static bool buoy_bubble_unclipped(const Actor* a){
    return buoy_stage() && a->type == OBJ_BUOYOXYGEN &&
           (a->state == BUOY_MOVE || a->state == BUOY_BREAK || a->state == BUOY_BUBBLE_KILLWAIT);
}

static void buoy_bubble_spawn(Actor* generator){
    unsigned live = 0;
    for(int i = 0; i < g_game.actor_count; i++)
        if(g_game.actors[i].active && g_game.actors[i].type == OBJ_BUOYOXYGEN) ++live;
    if(live >= 32) return;
    Actor* b =
        actor_spawn_reuse(OBJ_BUOYOXYGEN, v3_add(generator->position, v3_scale14(generator->up, FX(120))),
                          generator->up, generator->front);
    if(!b) return;
    b->flags &= ~ACTOR_HIDDEN;
    b->velocity = v3_scale14(generator->up, V16(7));
    b->health = generator->args[1] > 0 ? generator->args[1] : 360;
    b->target = generator->front;
    actor_set_state(b, BUOY_MOVE, ANIM_AIRBUBBLE_MOVE);
}

static bool buoy_bubble_spin_near(const Actor* a){
    v3 d = v3_sub(mario_centre(), a->position);
    return v3_length_squared64(d) < (uint64_t)FX(250) * FX(250);
}

static void buoy_bubble_think(Actor* a){
    unsigned elapsed = (uint16_t)a->timer;
    unsigned step = elapsed ? elapsed - 1 : 0;
    if(a->type == OBJ_BUOYAIRBUBBLE){
        unsigned wait = a->args[0] > 0 ? (unsigned)a->args[0] : 180;
        if(a->state == BUOY_WAIT && step == wait - 1){
            actor_set_state(a, BUOY_APPEAR, -1);
        } else if(a->state == BUOY_APPEAR){
            if(step == 0){
                audio_play_effect(SFX_BUOY_OJ_AIR_BUBBLE_APPEAR, 1);
                a->anim = ANIM_AIRBUBBLEGENERATOR_GENERATE;
                a->anim_frame = 0;
            }
            if(step > 6){
                buoy_bubble_spawn(a);
                actor_set_state(a, BUOY_WAIT, -1);
            }
        }
        return;
    }

    bool touch = g_game.player.action != MA_DEATH && actor_touches_mario(a, 0);
    bool spin = g_game.player.action != MA_DEATH && (a->state == BUOY_WAIT || a->state == BUOY_MOVE) &&
                g_mario.attack_timer && buoy_bubble_spin_near(a);
    if(touch || spin){
        bool quiet = a->state == BUOY_BUBBLE_KILLWAIT;
        actor_set_state(a, BUOY_BREAK, -1);
        if(!quiet) audio_play_effect(SFX_BUOY_OJ_AIR_BUBBLE_BREAK, 1);

        for(unsigned i = 0; i < 8; i++) mario_swim_recover();
        a->active = 0;
        return;
    }
    if(a->state == BUOY_BUBBLE_KILLWAIT){
        if(step > 90){
            a->active = 0;
            return;
        }

        a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
        return;
    }
    if(a->state != BUOY_MOVE) return;
    a->up = gravity_sample(g_game.stage.header, a->position).up;
    a->target = v3_tangent14(v3_rotate_axis14(a->target, a->up, DEG(1.5f)), a->up, a->front);
    a->velocity = v3_scale14(
        v3_add(a->velocity, v3_add(v3_scale14(a->target, V16(.1f)), v3_scale14(a->up, V16(.3f)))), R14(.85f));
    if(step > (unsigned)(uint16_t)a->health){
        a->flags |= ACTOR_HIDDEN;
        audio_play_effect(SFX_BUOY_OJ_AIR_BUBBLE_BREAK, 1);
        actor_set_state(a, BUOY_BUBBLE_KILLWAIT, -1);
    }

    a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
}
static bool buoy_actor_think(Actor* a){
    if(!buoy_stage()) return false;
    if(a->type == OBJ_BUOYMETBO){
        metbo_think(a);
        return true;
    }
    if(a->type == OBJ_BUOYBABYLAUNCHER){
        babylauncher_think(a);
        return true;
    }
    if(a->type == OBJ_BUOYPENGUIN){
        swimming_penguin_think(a);
        return true;
    }
    if(jumpguarder_dispatch(a)) return true;
    if(a->type == OBJ_TAKOHEI || a->type == OBJ_WATERPRESSURE || a->type == OBJ_WATERBULLET){
        honey_kind_think(a);
        return true;
    }
    switch(a->type){
    case OBJ_CARETAKER: {
        if(a->value > 0) --a->value;
        if(a->anim == ANIM_CARETAKER_REACTION || a->anim == ANIM_CARETAKER_TRAMPLED){
            if((a->anim_frame >> 8) < anim_length_of((const FigureHeader*)figure_caretaker, a->anim))
                return true;
            a->anim = ANIM_CARETAKER_WAIT;
            a->anim_frame = 0;
        }
        int contact = actor_mario_contact(a, 0);
        if(contact == CONTACT_SPIN){
            a->anim = ANIM_CARETAKER_REACTION;
            a->anim_frame = 0;
            audio_play_effect(SFX_BUOY_CARETAKER_SPIN_SOURCE, 2);
            return true;
        }
        if((contact == CONTACT_TRAMPLE && !a->value) || contact == CONTACT_HIPDROP){
            if(contact == CONTACT_TRAMPLE) a->value = 30;
            a->anim = ANIM_CARETAKER_TRAMPLED;
            a->anim_frame = 0;
            return true;
        }
        talk_actor_think(a);
        return true;
    }
    case OBJ_BUOYCHAIN: {
        const StageObject* o = actor_stage_object(a);
        if(!o) return true;
        if(g_buoy.rise_step >= 75){
            a->active = 0;
            return true;
        }
        int n = fx_max(1, o->args[0] / 200);
        if(!a->health){
            bool complete = true;
            for(int k = -1; k <= n; k++){
                bool found = false;
                for(int j = 0; j < g_game.actor_count; j++){
                    Actor* b = &g_game.actors[j];
                    if(b->active && (b->type == OBJ_BUOYCHAINLINK || b->type == OBJ_BUOYCHAINFIX) &&
                       b->args[3] == a->stage_index && b->value == k){
                        found = true;
                        break;
                    }
                }
                if(found) continue;
                Actor* b =
                    actor_spawn_reuse(k < 0 || k == n ? OBJ_BUOYCHAINFIX : OBJ_BUOYCHAINLINK,
                                      v3_add(a->position, V3(0, 0, FX(fx_max(0, k) * 200))), a->up, a->front);
                if(!b){
                    complete = false;
                    break;
                }
                b->value = k;
                b->args[0] = n;
                b->args[3] = a->stage_index;
                b->figure.scale = a->figure.scale;
                if(k == n){
                    b->up = v3_neg(a->up);
                    b->front = v3_rotate_axis14(a->front, a->up, DEG(90));
                }
            }
            a->health = complete;
        }
        return true;
    }
    case OBJ_BUOYCHAINLINK:
    case OBJ_BUOYCHAINFIX:
        if(g_buoy.rise_step >= 30){
            int step = (g_buoy.rise_step - 30) / 5;
            if(a->value < 0 || fx_min(a->value, a->args[0] - 1) <= step){
                a->active = 0;
                audio_play_effect(SFX_BUOY_OJ_LARGE_CHAIN_BREAK, 1);
            }
        }
        return true;
    case OBJ_BUOYFLOATERU:
        if(g_buoy.rise_step >= 90){
            const StageObject* o = actor_stage_object(a);
            if(o && o->rail >= 0)
                a->position =
                    rail_position_at(o->rail, fx_min(FX((g_buoy.rise_step - 90) * 10), rail_length(o->rail)));
        }
        return true;
    case OBJ_BUOYHATCH:
        if(g_buoy.hatch_step < 30){
            a->anim_frame = 0;
        } else if(a->state == BUOY_WAIT){
            actor_set_state(a, BUOY_MOVE, ANIM_HATCHWATERPLANET_OPEN);
        }
        if(a->state == BUOY_MOVE)
            a->anim_frame = fx_min(FX(g_buoy.hatch_step - 30), FX(BUOY_HATCH_BCK_FRAMES));
        return true;
    case OBJ_BUOYHEXAGON: buoy_float_think(a); return true;
    case OBJ_BUOYROTATEA:
    case OBJ_BUOYROTATEB:
    case OBJ_BUOYROTATED: {
        const BuoyPartParam* q = buoy_param(a);
        if(q && switch_on(3014)){
            a->target.x = (a->target.x + q->speed + 36000) % 36000;
            fx ang = (fx)((int64_t)a->target.x * FX_TAU / 36000);
            a->front = v3_direction14(v3_rotate_axis14(v3_from_i16(actor_stage_object(a)->front), a->up, ang),
                                      a->front);
        }
        return true;
    }
    case OBJ_BUOYCAGE:
    case OBJ_BUOYFIXATION:
        if(a->state == BUOY_BREAK && a->timer >= 60){
            switch_set(stage_object_switch(actor_stage_object(a), SWITCH_DEAD), true);
            a->active = 0;
        }
        return true;
    case OBJ_BUOYSCREW:
    case OBJ_BUOYSCREWREVERSE:
    case OBJ_BUOYVALVE: buoy_switch_think(a); return true;
    case OBJ_BUOYTORPEDOLAUNCHER: {
        Actor* t = a->value >= 0 && a->value < g_game.actor_count ? &g_game.actors[a->value] : NULL;
        if(t && t->active && t->type == OBJ_BUOYTORPEDO){
            a->timer = 0;
            return true;
        }
        if(!a->health || a->timer >= 180){
            Actor* b = actor_spawn_reuse(OBJ_BUOYTORPEDO, a->position, a->up, a->front);
            if(b){
                b->home = a->position;
                b->target = a->front;
                b->args[0] = a->args[0];
                b->args[1] = a->args[1];
                b->health = 0;
                actor_set_state(b, BUOY_APPEAR, ANIM_TORPEDO_START);
                a->health = 1;
                a->value = (int16_t)actor_index(b);
                a->timer = 0;
            }
        }
        return true;
    }
    case OBJ_BUOYTORPEDO: buoy_torpedo_think(a); return true;
    case OBJ_KILLER: chasing_killer_think(a); return true;
    case OBJ_BUOYAIRBUBBLE:
    case OBJ_BUOYOXYGEN: buoy_bubble_think(a); return true;
    case OBJ_BUOYDOSSUN: dossun_think(a); return true;
    case OBJ_BUOYBIGBIRIKYU: bigbirikyu_think(a); return true;

    default: return false;
    }
}
