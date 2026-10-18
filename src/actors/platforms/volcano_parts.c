static bool volcano_part_type(int type){
    switch(type){
    case OBJ_MARIOFIREBALL:
    case OBJ_CANDLESTANDICEVOLCANO:
    case OBJ_ANMMODELSWITCHMOVE:
    case OBJ_SIMPLESEESAWOBJ:
    case OBJ_LAVAFLOATER:
    case OBJ_RAILMOVEOBJ: return true;
    default: return false;
    }
}
static const FFM2Spec* volcano_part_spec(const Actor* a){
    return a->value >= 0 && (unsigned)a->value < FFM2_SPEC_COUNT ? &ffm2_specs[a->value] : NULL;
}
static const FFM2Model* volcano_part_model(const Actor* a){
    if(a->type == OBJ_MARIOFIREBALL) return &ffm2_models[FFM2_MODEL_MARIOFIREBALL];
    if(a->type == OBJ_CANDLESTANDICEVOLCANO) return &ffm2_models[FFM2_MODEL_CANDLESTANDICEVOLCANO];
    const FFM2Spec* s = volcano_part_spec(a);
    return s ? &ffm2_models[s->model] : NULL;
}
static bool volcano_demo_object(const StageObject* o){
    if(!level_has(LEVEL_VOLCANO_DEMOS) || !o || o->type != OBJ_DEMOGROUP) return false;
    for(unsigned i = 0; i < FFM2_DEMO_COUNT; i++)
        if(o->id == ffm2_demos[i].source) return true;
    return false;
}
static const FFM2DemoSpec* volcano_demo_spec(const Actor* a){
    const StageObject* o = actor_stage_object(a);
    if(!volcano_demo_object(o)) return NULL;
    for(unsigned i = 0; i < FFM2_DEMO_COUNT; i++)
        if(o->id == ffm2_demos[i].source) return &ffm2_demos[i];
    return NULL;
}
static Actor* volcano_object_actor(unsigned source){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        const StageObject* o = actor_stage_object(a);
        if(a->active && o && o->id == source) return a;
    }
    return NULL;
}

__attribute__((noinline)) static void volcano_part_play_cue(const Actor* a, const char* cue, bool level){
    __asm__ volatile("" : : "r"(a), "r"(cue), "r"(level) : "memory");
}
static v3 volcano_part_local_to_world(const Actor* a, v3 local){
    return v3_add(v3_scale14(v3_cross14(a->up, a->front), local.x),
                  v3_add(v3_scale14(a->up, local.y), v3_scale14(a->front, local.z)));
}
static const FFM2JointPose* volcano_part_joint_pose(const Actor* a, unsigned joint){
    const FFM2Model* model = volcano_part_model(a);
    uint16_t id = model->rest[joint];
    if(model->normal){
        int frame = a->anim_frame >> 8;
        if(a->type == OBJ_MARIOFIREBALL) frame %= model->normal->frames;
        frame = fx_clamp(frame, 0, model->normal->frames);
        id = model->normal->row[frame * model->joints + joint];
    }
    return &ffm2_pose_dictionary[id];
}
static v3 volcano_part_joint_axis(const Actor* a, const FFM2JointPose* p, int axis){
    const StageObject* o = actor_stage_object(a);
    int sx = o ? o->scale[0] : 256, sy = o ? o->scale[1] : 256, sz = o ? o->scale[2] : 256;
    return volcano_part_local_to_world(a, V3(mul_shift_exact(p->m[0][axis], sx, 8),
                                             mul_shift_exact(p->m[1][axis], sy, 8),
                                             mul_shift_exact(p->m[2][axis], sz, 8)));
}
static v3 volcano_part_joint_position(const Actor* a, const FFM2JointPose* p){
    const StageObject* o = actor_stage_object(a);
    int sx = o ? o->scale[0] : 256, sy = o ? o->scale[1] : 256, sz = o ? o->scale[2] : 256;
    return v3_add(a->position, volcano_part_local_to_world(a, V3(mul_shift_exact(p->m[0][3], sx, 16),
                                                                 mul_shift_exact(p->m[1][3], sy, 16),
                                                                 mul_shift_exact(p->m[2][3], sz, 16))));
}
static bool volcano_part_ride_frame(const Actor* a, v3* position, v3* up, v3* front){
    if(!volcano_part_type(a->type) || a->type == OBJ_MARIOFIREBALL) return false;
    const FFM2Model* model = volcano_part_model(a);
    if(!model) return false;
    const FFM2JointPose* pose = volcano_part_joint_pose(a, model->collision_joint);
    *position = volcano_part_joint_position(a, pose);
    *up = v3_direction14(volcano_part_joint_axis(a, pose, 1), a->up);
    *front = v3_direction14(volcano_part_joint_axis(a, pose, 2), a->front);
    return true;
}

static bool volcano_part_collision_one(int i, v3 centre){
    {
        const Actor* a = &g_game.actors[i];
        if(!a->active || !volcano_part_type(a->type)) return false;
        const FFM2Model* model = volcano_part_model(a);
        if(!model || !model->collision) return false;

        const FFM2JointPose* pose = volcano_part_joint_pose(a, model->collision_joint);
        int16_t scale[3];
        v3 position = volcano_part_joint_position(a, pose), axis[3];
        int smax = 0;
        for(int k = 0; k < 3; k++){
            axis[k] = volcano_part_joint_axis(a, pose, k);
            int bound =
                (int)((((int64_t)fx_abs(axis[k].x) + fx_abs(axis[k].y) + fx_abs(axis[k].z)) * 256 + 8192) >>
                      14) +
                1;
            if(bound > smax) smax = bound;
        }
        if(dyncol_mesh_beyond(model->collision, position, smax, centre, 400)){
            icestep_cache_invalidate();
            return true;
        }
        v3 up = v3_direction14(axis[1], a->up), front = v3_direction14(axis[2], a->front);
        for(int k = 0; k < 3; k++)
            scale[k] = (int16_t)(((int64_t)v3_length_exact(axis[k]) * 256 + 8192) >> 14);
        dyncol_add_codes(model->collision, position, up, front, scale, i, 0, centre, 400, model->codes);
    }
    return true;
}
static void volcano_part_prepare_visual(const Actor* a, Figure* f, unsigned part){
    const FFM2Model* model = volcano_part_model(a);
    const FFM2VisualPart* visual = &model->visual[part];
    const FFM2JointPose* pose = volcano_part_joint_pose(a, visual->joint);
    f->model = (const FigureHeader*)visual->model;
    f->pose = model_rest_pose(f->model);
    f->position = volcano_part_joint_position(a, pose);
    f->transform.o = f->position;
    v3 x = volcano_part_joint_axis(a, pose, 0), y = volcano_part_joint_axis(a, pose, 1),
       z = volcano_part_joint_axis(a, pose, 2);
    for(int k = 0; k < 3; k++) f->transform.m.r[k] = V3(-(&x.x)[k] >> 8, (&z.x)[k] >> 8, (&y.x)[k] >> 8);
    f->transform_dirty = false;
    f->has_shadow = false;
    f->local_yaw = f->local_pitch = f->local_roll = 0;
}
static const Actor* volcano_part_figure_owner(const Figure* f){
    uintptr_t at = (uintptr_t)f, first = (uintptr_t)&g_game.actors[0].figure;
    if(at < first) return NULL;
    uintptr_t delta = at - first;
    if(delta % sizeof(Actor)) return NULL;
    unsigned slot = delta / sizeof(Actor);
    if(slot >= (unsigned)g_game.actor_count) return NULL;
    const Actor* a = &g_game.actors[slot];
    return a->active && volcano_part_type(a->type) ? a : NULL;
}

__attribute__((noinline)) static void volcano_part_draw_sorted(uint8_t* fb, const Actor* a, Figure* f,
                                                               const CameraView* view,
                                                               const FFM2Model* model){
    uint8_t order[32];
    fx depth[32];
    unsigned n = 0;
    for(unsigned part = 0; part < model->parts && part < 32; part++){
        const FFM2VisualPart* v = &model->visual[part];
        volcano_part_prepare_visual(a, f, part);
        xform t = xform_compose(&f->transform, &view->to_screen);
        v3 centre = V3((v->lo[0] + v->hi[0]) / 2, (v->lo[1] + v->hi[1]) / 2, (v->lo[2] + v->hi[2]) / 2);
        int64_t h[3] = {(v->hi[0] - v->lo[0]) / 2 + 1, (v->hi[1] - v->lo[1]) / 2 + 1,
                        (v->hi[2] - v->lo[2]) / 2 + 1};
        fx margin[3];
        for(int k = 0; k < 3; k++)
            margin[k] =
                (fx)((h[0] * fx_abs(t.m.r[k].x) + h[1] * fx_abs(t.m.r[k].y) + h[2] * fx_abs(t.m.r[k].z)) >>
                     8) +
                4;
        v3 c = v3_add(t.o, m3_transform_vector(&t.m, centre));
        if(c.x - c.z > margin[0] + margin[2] || -c.x - c.z > margin[0] + margin[2] ||
           c.y - c.z > margin[1] + margin[2] || -c.y - c.z > margin[1] + margin[2])
            continue;
        unsigned i = n++;
        while(i && depth[i - 1] < c.z){
            depth[i] = depth[i - 1];
            order[i] = order[i - 1];
            i--;
        }
        depth[i] = c.z;
        order[i] = (uint8_t)part;
    }
    for(unsigned i = 0; i < n; i++){
        volcano_part_prepare_visual(a, f, order[i]);
        draw_figure(fb, f, view);
    }
}
static bool volcano_part_draw_figure(uint8_t* fb, Figure* f, const CameraView* view){
    const Actor* a = volcano_part_figure_owner(f);
    if(!a) return false;
    if(a->type == OBJ_CANDLESTANDICEVOLCANO){
        if(a->velocity.x <= 0 && a->velocity.y <= 0) return false;
        draw_figure(fb, f, view);
        candle_effect_draw(fb, view, a);
        return true;
    }
    const FFM2Model* model = volcano_part_model(a);
    if(model->visual[0].lo[0] != model->visual[0].hi[0]){
        volcano_part_draw_sorted(fb, a, f, view, model);
        volcano_part_prepare_visual(a, f, 0);
        return true;
    }
    for(unsigned part = 0; part < model->parts; part++){
        volcano_part_prepare_visual(a, f, part);
        draw_figure_part(fb, f, view);
    }
    volcano_part_prepare_visual(a, f, 0);
    return true;
}
static fx volcano_part_extent(const Actor* a){
    if(!volcano_part_type(a->type)) return 0;
    const FFM2Model* m = volcano_part_model(a);
    if(!m) return 0;
    const StageObject* o = actor_stage_object(a);
    int scale = o ? fx_max(o->scale[0], fx_max(o->scale[1], o->scale[2])) : 256;

    if(a->type == OBJ_CANDLESTANDICEVOLCANO) return 500 * scale / 256;

    int offset = 0;
    for(unsigned j = 0; j < m->joints; j++)
        offset =
            fx_max(offset, v3_length_exact(v3_sub(
                               volcano_part_joint_position(a, volcano_part_joint_pose(a, j)), a->position)));
    return m->sound_radius * scale / 256 + offset;
}
static bool volcano_part_view_bounds(const Figure* f, const CameraView* view){
    const Actor* a = volcano_part_figure_owner(f);
    if(!a) return false;
    return view_sphere_visible(view, a->position, volcano_part_extent(a));
}
static const FigureOps volcano_part_figure_ops = {.draw = volcano_part_draw_figure,
                                                  .view_bounds = volcano_part_view_bounds};
static bool volcano_part_unclipped(const Actor* a){
    if(volcano_demo_object(actor_stage_object(a)) || a->type == OBJ_MARIOFIREBALL) return true;
    if(!volcano_part_type(a->type)) return false;
    return a->state == VOLCANO_PART_MOVE || candlestand_stream_keep(a) ||
           stream_near(a->position, g_game.player.figure.position,
                       ACTOR_THINK_RANGE + volcano_part_extent(a));
}
static bool volcano_part_stream_required(const StageObject* o){
    if(volcano_demo_object(o) || (o->type == OBJ_TIMERSWITCH && (o->id == 37 || o->id == 175))) return true;
    for(unsigned i = 0; i < FFM2_DEMO_COUNT; i++)
        if(switch_on(ffm2_demos[i].trigger))
            for(unsigned j = 0; j < ffm2_demos[i].action_count; j++)
                if(o->id == ffm2_demos[i].actions[j].cast) return true;
    return false;
}
static bool volcano_launch_star_available(const StageObject* o){
    if(!level_has(LEVEL_VOLCANO_DEMOS) || o->type != OBJ_LAUNCHSTAR || o->id != 34) return true;

    Actor* demo = volcano_object_actor(33);
    return demo && demo->state != VOLCANO_DEMO_WAIT;
}
static bool volcano_part_init(Actor* a, const StageObject* o){
    if(volcano_demo_object(o)){
        a->figure.model = NULL;
        a->anim = -1;
        a->state = (stream_part_state(a->stage_index) & 1u) ? VOLCANO_DEMO_DONE : VOLCANO_DEMO_WAIT;
        a->timer = 0;
        return true;
    }
    if(!volcano_part_type(a->type) || a->type == OBJ_MARIOFIREBALL) return false;
    if(a->type == OBJ_CANDLESTANDICEVOLCANO){
        candlestand_init(a, o, false);
        return true;
    }
    a->value = -1;
    for(unsigned i = 0; i < FFM2_SPEC_COUNT; i++)
        if(ffm2_specs[i].source == o->id){
            a->value = i;
            break;
        }
    const FFM2Spec* spec = volcano_part_spec(a);
    if(!spec){
        a->active = 0;
        return true;
    }
    a->figure.model = (const FigureHeader*)ffm2_models[spec->model].visual[0].model;
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    a->target = V3(0, 0, 0);
    a->anim = -1;
    a->anim_frame = 0;
    a->state = VOLCANO_PART_WAIT;
    a->timer = 0;
    if(a->type == OBJ_ANMMODELSWITCHMOVE && (stream_part_state(a->stage_index) & 1u)){
        a->state = VOLCANO_PART_DONE;
        a->anim_frame = volcano_part_model(a)->normal->frames << 8;
    } else if(a->type == OBJ_LAVAFLOATER) a->state = VOLCANO_PART_MOVE;
    else if(a->type == OBJ_SIMPLESEESAWOBJ) a->state = VOLCANO_PART_STAY;
    else if(a->type == OBJ_RAILMOVEOBJ){
        volcano_part_rail_init(a);
    }
    return true;
}
static void volcano_part_switch_move_think(Actor* a){
    const FFM2Spec* spec = volcano_part_spec(a);
    const FFM2Clip* clip = volcano_part_model(a)->normal;
    int step = a->timer - 1;
    if(a->state == VOLCANO_PART_WAIT){
        if(spec->demo_group < 0 && (actor_switch_key(a, SWITCH_A) < 0 || actor_switch_on(a, SWITCH_A)))
            actor_set_state(a, VOLCANO_PART_MOVE, -1);
    } else if(a->state == VOLCANO_PART_MOVE){
        const char* start = NULL;
        const char* move = NULL;
        const char* end = NULL;
        int stop = -1, riddle = 0;
        switch(spec->model){
        case FFM2_MODEL_ICEVOLCANOAPPEARSTEPA:
            start = "SE_OJ_VOL_APPEAR_STEP_ST";
            move = "SE_OJ_LV_VOL_APPEAR_STEP_MV";
            end = "SE_OJ_VOL_APPEAR_STEP_ED";
            stop = 218;
            riddle = 2;
            break;
        case FFM2_MODEL_ICEVOLCANOAPPEARSTEPB:
            start = "SE_OJ_IV_STEP_B_START";
            move = "SE_OJ_LV_IV_STEP_B_MVOE";
            end = "SE_OJ_IV_STEP_B_END";
            stop = 210;
            break;
        case FFM2_MODEL_ICEVOLCANOCLIMBINGWALL:
            start = "SE_OJ_VOL_CLIMB_WALL_ST";
            move = "SE_OJ_LV_VOL_CLIMB_WALL_MV";
            end = "SE_OJ_VOL_CLIMB_WALL_ED";
            stop = 458;
            riddle = 2;
            break;
        case FFM2_MODEL_SHUTTERDOORD:
            start = "SE_OJ_FENCE_SHUTTER_S_ST";
            move = "SE_OJ_LV_FENCE_SHUTTER_S_MV";
            end = "SE_OJ_FENCE_SHUTTER_S_ED";
            stop = 180;
            break;
        case FFM2_MODEL_SHUTTERDOORC:
            start = "SE_OJ_FENCE_SHUTTER_START";
            move = "SE_OJ_LV_FENCE_SHUTTER_MOVE";
            end = "SE_OJ_FENCE_SHUTTER_STOP";
            stop = 174;
            riddle = 2;
            break;
        }
        if(step == 0){
            a->anim_frame = 0;
            if(start) volcano_part_play_cue(a, start, false);
        }
        if(move && (stop <= 0 || step < stop)) volcano_part_play_cue(a, move, true);
        if(end && step == stop){
            volcano_part_play_cue(a, end, false);
            if(riddle == 2) volcano_part_play_cue(a, "SE_SY_READ_RIDDLE_S", false);
        }
        if(step >= clip->frames){
            a->anim_frame = clip->frames << 8;
            stream_part_state_set(a->stage_index, stream_part_state(a->stage_index) | 1u);
            actor_set_state(a, VOLCANO_PART_DONE, -1);
        }
    } else if(a->state == VOLCANO_PART_DONE) a->anim_frame = clip->frames << 8;
}
static void volcano_part_float_think(Actor* a){
    const FFM2Spec* s = volcano_part_spec(a);
    float speed = float_from_bits(a->target.x), depth = float_from_bits(a->target.y);
    bool on = mario_ride_actor() == actor_index(a);
    const StageObject* o = actor_stage_object(a);
    v3 up = v3_from_i16(o->up);
    if(on){
        if(depth >= s->condition) speed = float_converge(speed, 0, .05f);
        else speed += .0001f * s->speed;
    } else {
        if(depth <= 0) speed = float_converge(speed, 0, .05f);
        else speed -= .0001f * s->angle;
    }
    speed = float_clamp(speed * .98f, -50, 50);
    depth = float_clamp(depth + speed, 0, s->condition);
    a->target.x = (int32_t)float_bits(speed);
    a->target.y = (int32_t)float_bits(depth);
    a->velocity = v3_scale14(up, -float_to_int_saturated(speed * 256));
    a->position = v3_sub(a->home, v3_scale14(up, float_to_int_saturated(depth)));
}
static void volcano_part_seesaw_think(Actor* a){
    const FFM2Spec* s = volcano_part_spec(a);
    bool on = mario_ride_actor() == actor_index(a), hip = mario_hipdrop_actor() == actor_index(a);
    float speed = float_from_bits(a->target.x), angle = float_from_bits(a->target.y);
    const StageObject* o = actor_stage_object(a);
    v3 up0 = v3_from_i16(o->up), axis = v3_from_i16(o->front),
       off = v3_sub(g_game.player.figure.position, a->position);
    if(hip){
        a->state = VOLCANO_PART_HIPDROP;
        a->timer = 0;
        a->health = 1;
    }
    if(a->state == VOLCANO_PART_STAY){
        if(on) actor_set_state(a, VOLCANO_PART_MOVE_START, -1);
        return;
    }
    if(a->state == VOLCANO_PART_MOVE_START && (speed > .1f || speed < -.1f)){
        actor_set_state(a, VOLCANO_PART_MOVE, -1);
        return;
    }
    v3 grav = v3_neg(gravity_sample(g_game.stage.header, a->position).up);
    int sign = v3_dot14(v3_cross(off, grav), axis) < 0 ? -1 : 1;
    float distance = .001f * (float)fx_abs(v3_dot14(off, v3_cross14(a->up, a->front)));
    if(a->state == VOLCANO_PART_HIPDROP && a->health)
        speed += (float)sign * (40 * distance / (.1f * s->inertia));
    if(on) speed += (float)sign * (distance / (.1f * s->inertia));
    if(a->state != VOLCANO_PART_MOVE_START && a->timer > 2){
        int force = v3_dot14(v3_cross14(a->up, up0), axis);
        if(fx_abs(force) < 16 && speed >= -.1f && speed <= .1f) speed = 0;
        else speed += (float)(force < 0 ? -1 : 1) * (.01f * s->restore) / (.1f * s->inertia);
        speed *= .99f;
    }
    speed = float_clamp(speed, -.01f * s->speed, .01f * s->speed);
    float next = angle + speed;
    v3 nextup = v3_rotate_degrees14(up0, axis, next);
    int dot = v3_dot14(nextup, v3_neg(grav));

    if(s->angle > 0 && s->angle < 180 && dot <= sin14_degrees((float)s->angle + 90)) speed *= -.5f;
    else {
        angle = next;
        a->up = v3_direction14(nextup, up0);
        a->front = v3_tangent14(axis, a->up, axis);
    }
    if(a->state == VOLCANO_PART_HIPDROP){
        if(!a->health) actor_set_state(a, VOLCANO_PART_MOVE, -1);
        a->health = 0;
    }
    if(speed == 0 && !on) actor_set_state(a, VOLCANO_PART_STAY, -1);
    a->target.x = (int32_t)float_bits(speed);
    a->target.y = (int32_t)float_bits(angle);
}

static const FFM2Rail* volcano_part_rail(const Actor* a){
    return actor_rail(a) == 19 ? &ffm2_rail_19 : actor_rail(a) == 20 ? &ffm2_rail_20 : NULL;
}
static int volcano_part_rail_segment(const FFM2Rail* r, float coord){
    int p = 0;
    while(p < 3 && coord >= r->coord[p + 1]) p++;
    return p;
}
static v3 volcano_part_rail_position(const FFM2Rail* r, float coord){
    int p = volcano_part_rail_segment(r, coord);
    if(p == 3) p = 2;
    float t = (coord - r->coord[p]) / (r->coord[p + 1] - r->coord[p]);
    t = float_clamp(t, 0, 1);
    v3 out;
    for(int k = 0; k < 3; k++)
        (&out.x)[k] =
            float_to_int_saturated(r->position[p][k] + (r->position[p + 1][k] - r->position[p][k]) * t);
    return out;
}
static void volcano_part_rail_init(Actor* a){
    const FFM2Rail* r = volcano_part_rail(a);
    if(!r){
        a->state = VOLCANO_PART_DONE;
        return;
    }
    float best = 0, bestd = 3.402823466e38f;
    for(int p = 0; p < 3; p++){
        float dot = 0, len = 0;
        for(int k = 0; k < 3; k++){
            float d = r->position[p + 1][k] - r->position[p][k];
            dot += ((float)(&a->home.x)[k] - r->position[p][k]) * d;
            len += d * d;
        }
        float t = float_clamp(dot / len, 0, 1), ds = 0;
        for(int k = 0; k < 3; k++){
            float d =
                r->position[p][k] + t * (r->position[p + 1][k] - r->position[p][k]) - (float)(&a->home.x)[k];
            ds += d * d;
        }
        if(ds < bestd){
            bestd = ds;
            best = r->coord[p] + t * (r->coord[p + 1] - r->coord[p]);
        }
    }
    a->radius = (int32_t)float_bits(best);
    a->velocity = V3(0, 0, 0);
    a->args[0] = 1;
    a->args[1] = 0;
    a->args[2] = (int16_t)volcano_part_rail_segment(r, best);
    a->args[3] = 0;
    a->timer = 0;
    a->state = VOLCANO_PART_MOVE;
    a->position = volcano_part_rail_position(r, best);
}
static void volcano_part_rail_think(Actor* a){
    const FFM2Rail* r = volcano_part_rail(a);
    if(!r) return;
    float coord = float_from_bits(a->radius), speed = float_from_bits(a->velocity.x),
          accel = float_from_bits(a->velocity.y);
    int point = volcano_part_rail_segment(r, coord), step = a->timer - 1;

    if(coord >= r->coord[3]){
        coord = 0;
        speed = accel = 0;
        point = 0;
        a->timer = 1;
        step = 0;
    } else if(point != a->args[2]){
        a->timer = 1;
        step = 0;
    }
    a->args[2] = (int16_t)point;
    if(step == 0){
        a->args[3] = r->accel[point];
        accel = a->args[3] > 0 ? (r->speed[point] - speed) / (float)a->args[3] : 0;
        if(accel >= -.0001f && accel <= .0001f) speed = r->speed[point];
    }
    if((accel < -.0001f || accel > .0001f) && step < a->args[3]) speed += accel;
    coord = float_clamp(coord + speed, 0, r->coord[3]);
    a->radius = (int32_t)float_bits(coord);
    a->velocity.x = (int32_t)float_bits(speed);
    a->velocity.y = (int32_t)float_bits(accel);
    a->position = volcano_part_rail_position(r, coord);
    volcano_part_play_cue(a, "SE_OJ_LV_VOL_MOVE_STEP_FLOW", true);
}
static bool volcano_part_anim_tick(Actor* a){
    if(!volcano_part_type(a->type)) return false;
    if(a->type == OBJ_ANMMODELSWITCHMOVE){
        if(a->state == VOLCANO_PART_MOVE && a->timer > 0)
            a->anim_frame = fx_min(a->anim_frame + 256, volcano_part_model(a)->normal->frames << 8);
        return true;
    }
    if(a->type != OBJ_MARIOFIREBALL) return true;
    a->anim_frame += 256;
    return true;
}
static void volcano_part_animate(Actor* a){ volcano_part_anim_tick(a); }
static void volcano_part_prepare_first(const Actor* a, Figure* f){
    if(volcano_part_type(a->type) && a->type != OBJ_CANDLESTANDICEVOLCANO && f->model)
        volcano_part_prepare_visual(a, f, 0);
}
static void volcano_part_think(Actor* a){
    if(volcano_demo_object(actor_stage_object(a))) return;
    switch(a->type){
    case OBJ_CANDLESTANDICEVOLCANO: candlestand_think(a); break;
    case OBJ_MARIOFIREBALL: mario_fireball_think(a); break;
    case OBJ_ANMMODELSWITCHMOVE: volcano_part_switch_move_think(a); break;
    case OBJ_LAVAFLOATER: volcano_part_float_think(a); break;
    case OBJ_SIMPLESEESAWOBJ: volcano_part_seesaw_think(a); break;
    case OBJ_RAILMOVEOBJ: volcano_part_rail_think(a); break;
    }
}
