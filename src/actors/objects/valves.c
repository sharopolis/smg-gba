enum { VALVE_WAIT = 64, VALVE_ADJUST, VALVE_TURN, VALVE_END };

enum {
    VALVE_CUE_TWIST_START,
    VALVE_CUE_SPIN_ATTACK,
    VALVE_CUE_TURN,
    VALVE_CUE_ON,
    VALVE_CUE_ICE_DOOR_START,
    VALVE_CUE_ICE_DOOR_MOVE,
    VALVE_CUE_ICE_DOOR_STOP,
    VALVE_CUE_SHUTTER_C_START,
    VALVE_CUE_SHUTTER_C_MOVE,
    VALVE_CUE_SHUTTER_C_STOP,
    VALVE_CUE_RIDDLE,
    VALVE_CUE_WATER_MOVE,
    VALVE_CUE_LAVA_MOVE
};

static int valve_cue_sound(int source_cue, bool level){
    if(level) return -1;
    switch(source_cue){
    case VALVE_CUE_TWIST_START: return SFX_FF_PV_TWIST_START_0;
    case VALVE_CUE_SPIN_ATTACK: return SFX_FF_PM_SPIN_ATTACK_0;
    case VALVE_CUE_ON: return SFX_FF_OJ_VALVE_SWITCH_ON_0;
    case VALVE_CUE_ICE_DOOR_START: return SFX_FF_OJ_MTL_SHUTTER_S_ST_0;
    case VALVE_CUE_ICE_DOOR_STOP: return SFX_FF_OJ_MTL_SHUTTER_S_ED_0;
    case VALVE_CUE_SHUTTER_C_START: return SFX_FF_OJ_FENCE_SHUTTER_START_0;
    case VALVE_CUE_SHUTTER_C_STOP: return SFX_FF_OJ_FENCE_SHUTTER_STOP_0;
    case VALVE_CUE_RIDDLE: return SFX_BRIDGE_RIDDLE;
    default: return -1;
    }
}
__attribute__((noinline)) static void valve_play_cue(const Actor* owner, int source_cue, bool level,
                                                     v3 position){
    int effect = valve_cue_sound(source_cue, level);

    if(effect >= 0) audio_queue_effect(effect, 3);
    __asm__ volatile("" : : "r"(owner), "r"(source_cue), "r"(level), "r"(position.x) : "memory");
}

static bool valve_type(int type){ return type >= OBJ_VALVESWITCH && type <= OBJ_LAVAUPDOWNPARTS; }
static bool valve_demo_object(const StageObject* o){
    return level_has(LEVEL_VALVE_DEMOS) && o && o->type == OBJ_DEMOGROUP && (o->id == 1022 || o->id == 1073);
}
static bool valve_stream_required(const StageObject* o){
    if(valve_demo_object(o)) return true;

    if((o->type == OBJ_ICEUPDOWNSHUTTERDOORA || o->type == OBJ_SHUTTERDOORC) &&
       (stream_part_state((int)(o - g_game.stage.objects)) & 1u))
        return false;

    return (o->id == 1041 && switch_on(4003)) || ((o->id == 1086 || o->id == 1087) && switch_on(8005));
}
static const FFValveModel* valve_model(const Actor* a){
    return a->value >= 0 && a->value < 5 ? &ffv_models[a->value] : NULL;
}
static bool valve_has_model(const Actor* a){ return valve_model(a) != NULL; }
static int valve_sound_radius(const Actor* a){ return valve_model(a)->sound_radius; }
static const FFValveClip* valve_clip(const Actor* a){
    const FFValveModel* m = valve_model(a);
    if(!m) return NULL;
    return a->type == OBJ_VALVESWITCH && a->args[0] != -1 ? m->reverse : m->normal;
}

static const FFValveJointPose* valve_joint_pose(const Actor* a, unsigned joint, bool collision){
    const FFValveModel* m = valve_model(a);
    const FFValveClip* c = valve_clip(a);
    if(!m || joint >= m->joints) return NULL;
    int frame = collision && a->type == OBJ_VALVESWITCH ? a->args[2] : a->anim_frame >> 8;
    uint16_t id = m->rest[joint];
    if(c){
        frame = fx_clamp(frame, 0, c->frames);
        id = c->row[frame * c->joints + joint];
    }
    return &ffv_pose_dictionary[id];
}
static v3 valve_local_to_world(const Actor* a, v3 v){
    const StageObject* o = actor_stage_object(a);
    v3 up = v3_from_i16(o->up), front = v3_from_i16(o->front), side = v3_cross14(up, front);
    return v3_add(v3_scale14(side, v.x), v3_add(v3_scale14(up, v.y), v3_scale14(front, v.z)));
}
static v3 valve_joint_axis(const Actor* a, const FFValveJointPose* pose, int axis){
    const StageObject* o = actor_stage_object(a);
    v3 local = V3(mul_shift_exact(pose->m[0][axis], o->scale[0], 8),
                  mul_shift_exact(pose->m[1][axis], o->scale[1], 8),
                  mul_shift_exact(pose->m[2][axis], o->scale[2], 8));
    return valve_local_to_world(a, local);
}
static v3 valve_joint_position(const Actor* a, const FFValveJointPose* pose){
    const StageObject* o = actor_stage_object(a);
    v3 local =
        V3(mul_shift_exact(pose->m[0][3], o->scale[0], 16), mul_shift_exact(pose->m[1][3], o->scale[1], 16),
           mul_shift_exact(pose->m[2][3], o->scale[2], 16));
    return v3_add(a->position, valve_local_to_world(a, local));
}
static v3 valve_grip_position(const Actor* a){
    const FFValveModel* m = valve_model(a);
    const FFValveJointPose* p = valve_joint_pose(a, m->collision_joint, true);

    return v3_add(valve_joint_position(a, p), v3_scale14(valve_joint_axis(a, p, 1), 75));
}
static void valve_collision_frame(const Actor* a, v3* pos, v3* up, v3* front, int16_t scale[3]){
    const FFValveModel* m = valve_model(a);
    const FFValveJointPose* p = valve_joint_pose(a, m->collision_joint, true);
    *pos = valve_joint_position(a, p);
    v3 y = valve_joint_axis(a, p, 1), z = valve_joint_axis(a, p, 2);
    *up = v3_direction14(y, a->up);
    *front = v3_direction14(z, a->front);

    if(a->type == OBJ_VALVESWITCH) scale[0] = scale[1] = scale[2] = 256;
    else
        for(int k = 0; k < 3; k++)
            scale[k] = (int16_t)(((int64_t)v3_length_exact(valve_joint_axis(a, p, k)) * 256 + 8192) >> 14);
}

static bool valve_ride_frame(const Actor* a, v3* pos, v3* up, v3* front){
    if(!valve_type(a->type)) return false;
    int16_t scale[3];
    valve_collision_frame(a, pos, up, front, scale);
    return true;
}

static bool valve_collision_one(int i, v3 centre){
    const Actor* a = &g_game.actors[i];
    if(!a->active || !valve_type(a->type)) return false;
    const FFValveModel* m = valve_model(a);
    if(!m) return false;
    v3 pos, up, front;
    int16_t scale[3];
    valve_collision_frame(a, &pos, &up, &front, scale);
    dyncol_add_codes(m->collision, pos, up, front, scale, i, 0, centre, 400, m->codes);
    return true;
}
static void valve_prepare_visual(const Actor* a, Figure* f, unsigned part){
    const FFValveModel* m = valve_model(a);
    const FFValveVisualPart* v = &m->visual[part];
    const FFValveJointPose* pose = valve_joint_pose(a, v->joint, false);
    f->model = (const FigureHeader*)v->model;
    f->pose = model_rest_pose(f->model);
    f->position = valve_joint_position(a, pose);
    f->transform.o = f->position;
    v3 x = valve_joint_axis(a, pose, 0), y = valve_joint_axis(a, pose, 1), z = valve_joint_axis(a, pose, 2);

    for(int k = 0; k < 3; k++) f->transform.m.r[k] = V3(-(&x.x)[k] >> 8, (&z.x)[k] >> 8, (&y.x)[k] >> 8);
    f->transform_dirty = false;
    f->has_shadow = false;
    f->local_yaw = f->local_pitch = f->local_roll = 0;
}

static unsigned valve_visual_parts(const Figure* figure, const Actor** actor){
    bool match = false;
    for(unsigned m = 0; m < 5; m++)
        if(figure->model == (const FigureHeader*)ffv_models[m].visual[0].model){
            match = true;
            break;
        }
    if(!match) return 0;
    for(int i = 0; i < g_game.actor_count; i++){
        const Actor* a = &g_game.actors[i];
        if(&a->figure == figure && a->active && valve_type(a->type)){
            *actor = a;
            return valve_model(a)->parts;
        }
    }
    return 0;
}
static void valve_demo_animate(Actor* a){
    if(!valve_demo_object(actor_stage_object(a))) a->anim_frame += FX_ONE;
}
static void valve_prepare_first(const Actor* a, Figure* f){
    if(valve_type(a->type)) valve_prepare_visual(a, f, 0);
}
static bool valve_draw_figure(uint8_t* framebuffer, Figure* f, const CameraView* view){
    const Actor* a;
    unsigned parts = valve_visual_parts(f, &a);
    if(!parts) return false;
    for(unsigned part = 0; part < parts; part++){
        valve_prepare_visual(a, f, part);
        draw_figure_part(framebuffer, f, view);
    }
    valve_prepare_visual(a, f, 0);
    return true;
}

static void valve_bound_sphere(const Actor* a, v3* centre, fx* radius){
    const StageObject* o = actor_stage_object(a);
    const FFValveModel* model = valve_model(a);
    bool door = a->type == OBJ_ICEUPDOWNSHUTTERDOORA || a->type == OBJ_SHUTTERDOORC;
    *centre =
        door ? valve_joint_position(a, valve_joint_pose(a, model->collision_joint, false)) : a->position;
    int scale = fx_max(o->scale[0], fx_max(o->scale[1], o->scale[2]));
    *radius = model->sound_radius * scale / 256;
    for(unsigned i = 0; i < sizeof(ffv_draw_bounds) / sizeof(ffv_draw_bounds[0]); i++){
        const FFValveDrawBound* bound = &ffv_draw_bounds[i];
        if(bound->source != o->id) continue;
        *radius = bound->radius;
        if(a->type == OBJ_ICEWATERUPDOWNPARTS || a->type == OBJ_LAVAUPDOWNPARTS)
            *centre = V3(bound->centre[0], bound->centre[1], bound->centre[2]);
        return;
    }
}
static bool valve_view_bounds(const Figure* f, const CameraView* view){
    const Actor* a;
    if(!valve_visual_parts(f, &a)) return false;
    v3 centre;
    fx radius;
    valve_bound_sphere(a, &centre, &radius);
    return view_sphere_visible(view, centre, radius);
}
static const FigureOps valve_figure_ops = {.draw = valve_draw_figure, .view_bounds = valve_view_bounds};
static bool valve_unclipped(const Actor* a){
    if(valve_demo_object(actor_stage_object(a))) return true;
    if(!valve_type(a->type)) return false;
    if(g_mario_link.bind == actor_index(a)) return true;
    if(a->state == VALVE_DOOR_MOVE && (a->type == OBJ_ICEUPDOWNSHUTTERDOORA || a->type == OBJ_SHUTTERDOORC))
        return true;

    int radius = valve_model(a)->sound_radius + ACTOR_THINK_RANGE;
    return stream_near(a->position, g_game.player.figure.position, radius);
}

static bool valve_stream_keep(const Actor* a){
    if(!a->active) return false;
    if(a->type == OBJ_ICEWATERUPDOWNPARTS || a->type == OBJ_LAVAUPDOWNPARTS) return true;
    if(a->type == OBJ_VALVESWITCH)
        return a->state == VALVE_ADJUST || a->state == VALVE_TURN || (a->state == VALVE_END && a->timer == 0);
    return (a->type == OBJ_ICEUPDOWNSHUTTERDOORA || a->type == OBJ_SHUTTERDOORC) &&
           a->state == VALVE_DOOR_MOVE;
}
static void valve_set_state(Actor* a, int next){
    if(a->state == next) return;
    bool completed = (a->type == OBJ_VALVESWITCH && a->state == VALVE_TURN && next == VALVE_END) ||
                     ((a->type == OBJ_ICEUPDOWNSHUTTERDOORA || a->type == OBJ_SHUTTERDOORC) &&
                      a->state == VALVE_DOOR_MOVE && next == VALVE_DOOR_DONE);
    if(completed) stream_part_state_set(a->stage_index, stream_part_state(a->stage_index) | 1u);
    a->state = (uint8_t)next;
    a->timer = 0;
}
static void valve_advance_anim(Actor* a){
    const FFValveClip* c = valve_clip(a);
    if(!c) return;
    a->args[2] = (int16_t)(a->anim_frame >> 8);
    if((a->type == OBJ_VALVESWITCH && a->state == VALVE_TURN && a->timer > 1) ||
       (a->type != OBJ_VALVESWITCH && a->state == VALVE_DOOR_MOVE && a->timer > 1))
        a->anim_frame = fx_min(a->anim_frame + 256, c->frames << 8);
}
static void valve_release_player(Actor* a){
    if(g_mario_link.bind != actor_index(a)) return;

    g_mario_link.bind = -1;
    g_mario.jumping = false;
    g_mario.jump_vec = V3(0, 0, 0);
    mario_stop_walk();
    mario_set_action(MA_WAIT);
    mario_set_anim(ANIM_MARIO_WAIT);
}
static bool valve_player_request(const KeyState* keys){
    Player* p = &g_game.player;
    if(!(keys->pressed & KEY_B) || !p->on_ground || g_mario.jumping || p->action == MA_DEATH ||
       g_mario_link.bind >= 0)
        return false;
    RayHit hit;
    if(!collision_ray(v3_add(p->figure.position, v3_scale14(p->up, 40)),
                      v3_sub(p->figure.position, v3_scale14(p->up, 60)), &hit, 0))
        return false;
    int owner = dyncol_owner(hit.tri);
    if(owner < 0 || owner >= g_game.actor_count) return false;
    Actor* a = &g_game.actors[owner];
    if(!a->active || a->type != OBJ_VALVESWITCH || a->state != VALVE_WAIT) return false;

    mario_bind_start(a);
    valve_play_cue(NULL, VALVE_CUE_TWIST_START, false, p->figure.position);
    valve_play_cue(NULL, VALVE_CUE_SPIN_ATTACK, false, p->figure.position);
    valve_set_state(a, VALVE_ADJUST);
    return true;
}
enum {
    VALVE_EV_INIT_WAIT = 1u << 0,
    VALVE_EV_ADJUST_POSE = 1u << 1,
    VALVE_EV_START_ANIM = 1u << 2,
    VALVE_EV_TURN_SOUND = 1u << 3,
    VALVE_EV_ON_SOUND = 1u << 4,
    VALVE_EV_SET_SWITCH = 1u << 5,
    VALVE_EV_INVALIDATE_SENSORS = 1u << 6,
    VALVE_EV_RELEASE_PLAYER = 1u << 7,
    VALVE_DOOR_EV_START_ANIM = 1u << 8,
    VALVE_DOOR_EV_MOVE = 1u << 9,
    VALVE_DOOR_EV_STOP = 1u << 10
};
typedef struct {
    unsigned events;
    int next;
} ValveStep;

static inline ValveStep valve_switch_step(int state, int step, int bck_stopped, int player_bound){
    ValveStep out = {0, state};
    if(state == VALVE_WAIT){
        if(step == 0) out.events = VALVE_EV_INIT_WAIT;
    } else if(state == VALVE_ADJUST){
        out.events = VALVE_EV_ADJUST_POSE;
        if(step == 3) out.next = VALVE_TURN;
    } else if(state == VALVE_TURN){
        if(step == 0) out.events |= VALVE_EV_START_ANIM;
        if(step < 30) out.events |= VALVE_EV_TURN_SOUND;
        if(step == 30) out.events |= VALVE_EV_ON_SOUND;
        if(bck_stopped){
            out.events |= VALVE_EV_SET_SWITCH;
            out.next = VALVE_END;
        }
    } else if(state == VALVE_END && step == 0){
        out.events = VALVE_EV_INVALIDATE_SENSORS;
        if(player_bound) out.events |= VALVE_EV_RELEASE_PLAYER;
    }
    return out;
}

static inline ValveStep valve_door_step(int state, int step, int demo_action, int bck_stopped){
    ValveStep out = {0, state};

    if(state == VALVE_DOOR_WAIT){
        if(demo_action) out.next = VALVE_DOOR_MOVE;
    } else if(state == VALVE_DOOR_MOVE){
        if(step == 0) out.events |= VALVE_DOOR_EV_START_ANIM;
        out.events |= VALVE_DOOR_EV_MOVE;
        if(bck_stopped){
            out.events |= VALVE_DOOR_EV_STOP;
            out.next = VALVE_DOOR_DONE;
        }
    }
    return out;
}
static void valve_switch_think(Actor* a){
    int step = a->timer - 1;
    valve_advance_anim(a);
    const FFValveClip* c = valve_clip(a);
    ValveStep out = valve_switch_step(a->state, step, a->anim_frame >= (c->frames << 8),
                                      g_mario_link.bind == actor_index(a));
    if(out.events & VALVE_EV_INIT_WAIT){
        a->anim_frame = 0;
        a->args[2] = 0;
    }
    if(out.events & VALVE_EV_ADJUST_POSE){
        v3 current = g_game.player.figure.position, target = valve_grip_position(a);
        mario_bind_pose(v3_add(current, v3_scale14(v3_sub(target, current), step * N14_ONE / 3)),
                        g_game.player.up, g_game.player.front);
    }
    if(out.events & VALVE_EV_START_ANIM){
        a->anim_frame = 0;
        mario_restart_anim(ANIM_MARIO_SCREWSWITCHON);
    }
    if(out.events & VALVE_EV_TURN_SOUND) valve_play_cue(a, VALVE_CUE_TURN, true, a->position);
    if(out.events & VALVE_EV_ON_SOUND){
        valve_play_cue(a, VALVE_CUE_ON, false, a->position);
        camera_shake(1);
    }
    if(a->state == VALVE_TURN && g_mario_link.bind == actor_index(a))
        mario_bind_pose(valve_grip_position(a), g_game.player.up, g_game.player.front);
    if(out.events & VALVE_EV_SET_SWITCH) switch_set(actor_switch_key(a, SWITCH_A), true);
    if(out.events & VALVE_EV_RELEASE_PLAYER) valve_release_player(a);
    valve_set_state(a, out.next);
}
static void valve_door_think(Actor* a){
    int step = a->timer - 1;
    valve_advance_anim(a);
    const FFValveClip* clip = valve_clip(a);
    ValveStep out = valve_door_step(a->state, step, false, a->anim_frame >= (clip->frames << 8));
    bool ice = a->type == OBJ_ICEUPDOWNSHUTTERDOORA;
    int stop = ice ? 144 : 174;
    if(out.events & VALVE_DOOR_EV_START_ANIM){
        a->anim_frame = 0;
        valve_play_cue(a, ice ? VALVE_CUE_ICE_DOOR_START : VALVE_CUE_SHUTTER_C_START, false, a->position);
    }
    if(a->state == VALVE_DOOR_MOVE && step < stop)
        valve_play_cue(a, ice ? VALVE_CUE_ICE_DOOR_MOVE : VALVE_CUE_SHUTTER_C_MOVE, true, a->position);
    if(a->state == VALVE_DOOR_MOVE && step == stop){
        valve_play_cue(a, ice ? VALVE_CUE_ICE_DOOR_STOP : VALVE_CUE_SHUTTER_C_STOP, false, a->position);
        valve_play_cue(NULL, VALVE_CUE_RIDDLE, false, a->position);
    }
    valve_set_state(a, out.next);
}

static const FFValveRailSpec* valve_rail_spec(const Actor* a){
    return &ffv_rail_specs[actor_stage_object(a)->id == 1091];
}
static v3 valve_rail_position(const FFValveRailSpec* spec, float coord){
    float rate = coord / spec->length;
    return V3(float_to_int_saturated(spec->p[0][0] + (spec->p[1][0] - spec->p[0][0]) * rate),
              float_to_int_saturated(spec->p[0][1] + (spec->p[1][1] - spec->p[0][1]) * rate),
              float_to_int_saturated(spec->p[0][2] + (spec->p[1][2] - spec->p[0][2]) * rate));
}
enum { VALVE_RAIL_MOVE = 64, VALVE_RAIL_STOP_BEFORE, VALVE_RAIL_STOP_AFTER };
typedef struct {
    float coord, speed, accel;
    int state, step, direction, stop_time, accel_time;
} ValveRailState;
static inline int valve_rail_current_point(float coord, float length, int direction){
    if(coord < .001f) return 0;
    if(length - coord < .001f) return 1;
    return direction > 0 ? 0 : 1;
}
static inline void valve_rail_move(ValveRailState* s, float length, const int16_t point[2][8]){
    if(s->state == VALVE_RAIL_MOVE && (s->direction > 0 ? length - s->coord < .001f : s->coord < .001f)){
        s->direction = -s->direction;
        s->speed = s->accel = 0.0f;
        s->stop_time = point[valve_rail_current_point(s->coord, length, s->direction)][5];
        s->state = VALVE_RAIL_STOP_BEFORE;
        s->step = 0;
    }
    if(s->state == VALVE_RAIL_MOVE){
        if(s->step == 0){
            int current = valve_rail_current_point(s->coord, length, s->direction);
            s->accel_time = point[current][1];
            float target = (float)point[current][0];
            s->accel = s->accel_time > 0 && target >= 0 ? (target - s->speed) / (float)s->accel_time : 0.0f;

            if(s->accel >= .0001f || s->accel <= -.0001f){
            } else if(target >= 0) s->speed = target;
        }
        if((s->accel >= .0001f || s->accel <= -.0001f) && s->step < s->accel_time) s->speed += s->accel;
        if(s->direction > 0) s->coord += s->speed;
        else s->coord -= s->speed;
        if(s->coord < 0) s->coord = 0;
        if(s->coord > length) s->coord = length;
        ++s->step;
    } else if(s->step == s->stop_time){
        s->state = s->state == VALVE_RAIL_STOP_BEFORE ? VALVE_RAIL_STOP_AFTER : VALVE_RAIL_MOVE;
        s->step = 0;
    } else ++s->step;
}
static void valve_rail_think(Actor* a){
    const FFValveRailSpec* spec = valve_rail_spec(a);
    ValveRailState s = {float_from_bits(a->radius),
                        float_from_bits(a->velocity.x),
                        float_from_bits(a->velocity.y),
                        a->state,
                        a->timer - 1,
                        a->args[0],
                        a->args[1],
                        a->args[3]};
    valve_rail_move(&s, spec->length, spec->args);
    a->state = (uint8_t)s.state;
    a->timer = (int16_t)s.step;
    a->args[0] = (int16_t)s.direction;
    a->args[1] = (int16_t)s.stop_time;
    a->args[3] = (int16_t)s.accel_time;
    a->radius = (fx)float_bits(s.coord);
    a->velocity.x = (fx)float_bits(s.speed);
    a->velocity.y = (fx)float_bits(s.accel);
    a->position = valve_rail_position(spec, s.coord);
    if(s.state == VALVE_RAIL_MOVE){
        v3 delta = v3_sub(g_game.player.figure.position, a->position);

        if(v3_dot14(delta, a->up) > -380){
            v3 offset = v3_sub(delta, v3_scale14(a->up, v3_dot14(delta, a->up)));
            int radius = valve_sound_radius(a);
            int length = v3_length_exact(offset);
            if(length > radius) offset = v3_scale14(offset, ratio14(radius, length));
            valve_play_cue(a, a->type == OBJ_ICEWATERUPDOWNPARTS ? VALVE_CUE_WATER_MOVE : VALVE_CUE_LAVA_MOVE,
                           true, v3_add(a->position, offset));
        }
    }
}
static bool valve_init(Actor* a, const StageObject* o){
    if(!valve_type(a->type)) return false;
    a->value = (int16_t)(a->type - OBJ_VALVESWITCH);
    a->anim = -1;
    a->anim_frame = 0;
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    a->timer = 0;
    a->state = VALVE_WAIT;
    a->args[2] = 0;
    if(a->type <= OBJ_SHUTTERDOORC && (stream_part_state(a->stage_index) & 1u)){
        const FFValveClip* c = valve_clip(a);
        a->state = a->type == OBJ_VALVESWITCH ? VALVE_END : VALVE_DOOR_DONE;
        a->anim_frame = c->frames << 8;
        a->args[2] = (int16_t)c->frames;
        a->timer = 1;
    }
    if(a->type >= OBJ_ICEWATERUPDOWNPARTS){
        if(o->id != 1047 && o->id != 1091) return false;
        float coord = 0.0f;
        a->radius = (fx)float_bits(coord);
        a->velocity.x = a->velocity.y = (fx)float_bits(0);
        a->args[0] = 1;
        a->args[1] = 0;
        a->args[3] = 240;
        a->state = VALVE_RAIL_MOVE;
        a->position = valve_rail_position(valve_rail_spec(a), coord);
    }
    return true;
}
static Actor* valve_object_actor(int source){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        const StageObject* o = actor_stage_object(a);
        if(a->active && o && o->id == source) return a;
    }
    return NULL;
}
static void valve_think(Actor* a){
    if(a->type == OBJ_VALVESWITCH) valve_switch_think(a);
    else if(a->type == OBJ_ICEUPDOWNSHUTTERDOORA || a->type == OBJ_SHUTTERDOORC) valve_door_think(a);
    else if(a->type == OBJ_ICEWATERUPDOWNPARTS || a->type == OBJ_LAVAUPDOWNPARTS) valve_rail_think(a);
}
