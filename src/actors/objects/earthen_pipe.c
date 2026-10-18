enum { EP_WAIT = 64, EP_READY, EP_IN, EP_SHOW, EP_OUT };
enum { EP_SHOWN = 1 };

enum { PIPE_FLAG_WATER = 2 };

static EWRAM_BSS PipeTravel g_pipe;

static void earthenpipe_reset(void){
    g_pipe.out_index = -1;
    g_pipe.phase = g_pipe.input_off = 0;
    for(uint32_t i = 0; i < g_game.stage.header->object_count; i++)
        if(g_game.stage.objects[i].type == OBJ_EARTHENPIPE) stream_part_state_set(i, 0);
}

static v3 pipe_up(const StageObject* o){
    v3 up = v3_from_i16(o->up);
    if(o->args[6] == 1) return up;
    GravitySample g = gravity_sample(g_game.stage.header, V3(o->position[0], o->position[1], o->position[2]));
    return g.index >= 0 ? g.up : up;
}
static v3 pipe_top_at(const StageObject* o, v3 up){
    fx height = FX(100) * (o->scale[1] ? o->scale[1] : 256) / 256;
    return v3_add(V3(o->position[0], o->position[1], o->position[2]), v3_scale14(up, height));
}
static v3 pipe_top(const StageObject* o){ return pipe_top_at(o, pipe_up(o)); }
static int pipe_mode(const StageObject* o){ return o->args[2] < 0 ? 0 : o->args[2]; }
static bool pipe_hidden(int i){
    const StageObject* o = &g_game.stage.objects[i];
    int mode = pipe_mode(o);
    return (mode == 1 || mode == 2) && !(stream_part_state(i) & EP_SHOWN);
}
static bool pipe_in_scenario(const StageObject* o){
    int bit = g_game.scenario > 0 ? 1 << (g_game.scenario - 1) : 1;
    return o->scenarios == 0xff || (o->scenarios & bit) != 0;
}
static int pipe_partner(const Actor* a){
    const StageObject* self = actor_stage_object(a);
    if(!self || self->args[3] < 0) return -1;
    for(uint32_t i = 0; i < g_game.stage.header->object_count; i++){
        const StageObject* o = &g_game.stage.objects[i];
        if((int)i != a->stage_index && o->type == OBJ_EARTHENPIPE && o->args[3] == self->args[3] &&
           pipe_in_scenario(o))
            return (int)i;
    }
    return -1;
}
static void earthenpipe_init(Actor* a, const StageObject* o){
    a->value = -1;
    actor_set_state(a, EP_WAIT, -1);
}
static void pipe_set_player_exit(void){
    Player* p = game_player();
    p->up = p->gravity_up = g_pipe.exit_up;
    mario_hold_move(g_pipe.exit_pos, g_pipe.exit_front);
    camera_snap_behind(&g_game.camera, p);
    camera_galaxy_snap();
}

static void pipe_begin_out(void){
    int music = level_pipe_music(g_game.stage.objects[g_pipe.out_index].args[4]);
    if(music >= 0) audio_play_music(music);
    pipe_set_player_exit();
    mario_restart_anim(ANIM_MARIO_EARTHENPIPEOUT);
    audio_play_effect(SFX_NATIVEPIPE, 4);
    g_pipe.phase = EP_OUT;
    g_pipe.timer = 0;
}

#define PIPE_POSE_SETTLED 0x40000001
#define PIPE_FRAME_PLACED 0x40000000
static void earthenpipe_think(Actor* a){
    const StageObject* o = actor_stage_object(a);
    if(!o) return;
    Player* p = game_player();
    bool exit_pipe = g_pipe.out_index == a->stage_index;
    fx gkey = (fx)(switch_generation() << 8 | gravity_scenario_bit());
    bool cached = !exit_pipe && a->target.y == PIPE_POSE_SETTLED && a->target.x == gkey;
    if(!cached){
        v3 up = pipe_up(o), front = v3_tangent14(v3_from_i16(o->front), up, a->front);
        bool settled = !exit_pipe && front.x == a->front.x && front.y == a->front.y && front.z == a->front.z;
        a->up = up;
        a->front = front;
        a->position = pipe_top_at(o, up);
        a->target = V3(gkey, settled ? PIPE_POSE_SETTLED : 0, 0);
    }
    bool hidden = pipe_hidden(a->stage_index);
    if(g_pipe.out_index == a->stage_index && g_pipe.phase == EP_SHOW){
        int rate = fx_clamp((g_pipe.timer - 20) * N14_ONE / 30, 0, N14_ONE);
        rate = mul14(rate, rate);
        a->position =
            v3_add(V3(o->position[0], o->position[1], o->position[2]),
                   v3_scale14(v3_sub(a->position, V3(o->position[0], o->position[1], o->position[2])), rate));
        hidden = g_pipe.timer < 20;
    } else if(g_pipe.out_index == a->stage_index && g_pipe.hide_timer > 40){
        int rate = fx_clamp((55 - g_pipe.hide_timer) * N14_ONE / 15, 0, N14_ONE);
        rate = mul14(rate, rate);
        a->position =
            v3_add(V3(o->position[0], o->position[1], o->position[2]),
                   v3_scale14(v3_sub(a->position, V3(o->position[0], o->position[1], o->position[2])), rate));
    }
    if(hidden) a->flags |= ACTOR_HIDDEN;
    else a->flags &= ~ACTOR_HIDDEN;
    const int16_t (*tris)[9] = pipe_mode(o) == 3 ? native_earthenpipes_kcl : native_earthenpipe_kcl;
    int count = pipe_mode(o) == 3 ? NATIVE_EARTHENPIPES_KCL_COUNT : NATIVE_EARTHENPIPE_KCL_COUNT;
    int set = mappart_set(a, tris, count);
    if(set >= 0){
        movers_enable(set, !hidden);
        if(hidden) a->target.z = 0;
        else if(!(cached && a->target.z == (PIPE_FRAME_PLACED | set) && g_movers[set].placed)){
            MoverFrame f =
                mover_frame_of(a->position, a->up, a->front, (o->scale[0] ? o->scale[0] : 256) << 6);
            movers_place(set, &f);
            a->target.z = a->target.y == PIPE_POSE_SETTLED ? (PIPE_FRAME_PLACED | set) : 0;
        }
    }
    if(hidden || g_pipe.phase || g_mario_held || p->action == MA_STAR_LAUNCH || p->action == MA_DEATH ||
       p->action == MA_TALK)
        return;
    if(g_pipe.out_index == a->stage_index) return;
    if(stage_object_switch(o, SWITCH_A) >= 0 && !switch_on(stage_object_switch(o, SWITCH_A))) return;
    v3 d = v3_sub(p->figure.position, a->position);
    fx h = v3_dot14(d, a->up);

    v3 body = v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT));
    v3 sensor = v3_sub(a->position, v3_scale14(a->up, FX(50)));
    bool touching = v3_length_squared64(v3_sub(body, sensor)) <= (uint64_t)FX(170) * FX(170);
    if(o->flags & PIPE_FLAG_WATER){
        v3 upper = v3_add(a->position, v3_scale14(a->up, FX(180)));
        v3 lower = v3_add(a->position, v3_scale14(a->up, FX(50)));
        touching = touching || v3_length_squared64(v3_sub(body, upper)) <= (uint64_t)FX(300) * FX(300) ||
                   v3_length_squared64(v3_sub(body, lower)) <= (uint64_t)FX(180) * FX(180);
    }
    if(!touching) return;
    if(p->water_state == WATER_DRY && h < -FX(5) &&
       v3_length_squared64(v3_reject14(d, a->up)) > (uint64_t)FX(50) * FX(50))
        return;
    int partner = pipe_partner(a);
    if(partner < 0) return;
    g_pipe.entry = a->stage_index;
    g_pipe.out_index = (int16_t)partner;
    g_pipe.from_pos = p->figure.position;
    g_pipe.from_front = p->front;
    g_pipe.from_up = p->up;
    g_pipe.entry_front = v3_tangent14(v3_sub(g_game.camera.position, a->position), a->up, a->front);
    const StageObject* target = &g_game.stage.objects[partner];
    g_pipe.exit_pos = pipe_top(target);
    g_pipe.exit_up = pipe_up(target);
    g_pipe.exit_front = v3_tangent14(v3_from_i16(target->front), g_pipe.exit_up, p->front);
    g_pipe.exit_h = target->args[0] < 0 ? 5 : target->args[0];
    g_pipe.exit_v = target->args[1] < 0 ? 20 : target->args[1];
    g_pipe.phase = EP_READY;
    g_pipe.timer = 0;
    g_pipe.hide_timer = 0;
    mario_hold_begin();
    mario_restart_anim(ANIM_MARIO_EARTHENPIPEREADY);
}
static void earthenpipe_global_tick(void){
    if(g_pipe.input_off){
        if(!g_mario.jumping || g_mario.jump_type != JUMP_FORCED || game_player()->water_state != WATER_DRY)
            g_pipe.input_off = 0;
        else g_mario.no_control_timer = 2;
    }
    if(g_pipe.out_index < 0) return;
    Player* p = game_player();
    const StageObject* dst = &g_game.stage.objects[g_pipe.out_index];
    if(g_pipe.phase){
        if(p->action == MA_DEATH){
            g_mario_held = false;
            mario_set_hidden(false);
            g_pipe.phase = 0;
            g_pipe.out_index = -1;
            return;
        }
        ++g_pipe.timer;
        if(g_pipe.phase == EP_READY){
            const StageObject* src = &g_game.stage.objects[g_pipe.entry];
            int t = fx_min(g_pipe.timer * N14_ONE / 24, N14_ONE);
            p->up = p->gravity_up = v3_blend14(g_pipe.from_up, pipe_up(src), t);
            mario_hold_move(v3_add(g_pipe.from_pos, v3_scale14(v3_sub(pipe_top(src), g_pipe.from_pos), t)),
                            v3_blend14(g_pipe.from_front, g_pipe.entry_front, t));
            if(g_pipe.timer >= 24){
                g_pipe.phase = EP_IN;
                g_pipe.timer = 0;
                mario_restart_anim(ANIM_MARIO_EARTHENPIPEIN);
                audio_play_effect(SFX_NATIVEPIPE, 4);
                int music = level_pipe_music(dst->args[4]);
                if(music >= 0 && g_audio.current_music != music) audio_stop_music();
            }
        } else if(g_pipe.phase == EP_IN){
            const StageObject* src = &g_game.stage.objects[g_pipe.entry];
            p->up = p->gravity_up = pipe_up(src);
            mario_hold_move(pipe_top(src), g_pipe.entry_front);
            if(g_pipe.timer >= 74){
                if(pipe_hidden(g_pipe.out_index)){
                    stream_part_state_set(g_pipe.out_index, EP_SHOWN);
                    g_pipe.phase = EP_SHOW;
                    g_pipe.timer = 0;
                    mario_set_hidden(true);
                    pipe_set_player_exit();
                } else pipe_begin_out();
            }
        } else if(g_pipe.phase == EP_SHOW){
            mario_hold_move(g_pipe.exit_pos, g_pipe.exit_front);
            if(g_pipe.timer >= 50){
                mario_set_hidden(false);
                pipe_begin_out();
            }
        } else if(g_pipe.phase == EP_OUT){
            p->up = p->gravity_up = g_pipe.exit_up;
            mario_hold_move(g_pipe.exit_pos, g_pipe.exit_front);
            if(g_pipe.timer >= 74){
                v3 velocity = v3_scale14(g_pipe.exit_up, V16(g_pipe.exit_v));
                if(!water_query(g_pipe.exit_pos, g_pipe.exit_up, 0))
                    velocity = v3_add(velocity, v3_scale14(g_pipe.exit_front, V16(g_pipe.exit_h)));
                mario_hold_release(velocity);
                g_pipe.input_off = 1;
                g_mario.no_control_timer = 2;
                mario_set_action(MA_JUMP);
                mario_restart_anim(ANIM_MARIO_EARTHENPIPEJUMP);
                audio_play_effect(SFX_JUMP, 3);
                const StageObject* src = &g_game.stage.objects[g_pipe.entry];
                switch_set(stage_object_switch(src, SWITCH_B), true);
                g_pipe.phase = 0;
                g_pipe.timer = 0;
                g_pipe.hide_timer = pipe_mode(dst) == 1 ? 1 : 0;
            }
        }
        return;
    }
    if(g_pipe.hide_timer){
        if(++g_pipe.hide_timer >= 55){
            stream_part_state_set(g_pipe.out_index, 0);
            g_pipe.hide_timer = 0;
        }
    }
    if(++g_pipe.timer > 30 && p->on_ground && !g_pipe.hide_timer) g_pipe.out_index = -1;
    else if((pipe_mode(dst) == 4 || p->water_state != WATER_DRY) && !g_pipe.hide_timer &&
            v3_length_squared64(v3_sub(p->figure.position, g_pipe.exit_pos)) > (uint64_t)FX(100) * FX(100))
        g_pipe.out_index = -1;
}

static void earthenpipe_figure(const Actor* a, Figure* f){ f->scale.y = 64; }
