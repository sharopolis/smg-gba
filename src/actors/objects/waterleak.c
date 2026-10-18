static Actor* waterleak_child(Actor* a){
    int i = a->args[1];
    if(i < 0 || i >= g_game.actor_count) return NULL;
    Actor* child = &g_game.actors[i];
    return child->active && child->type == OBJ_WATERLEAKICE && child->value == actor_index(a) ? child : NULL;
}
static Actor* waterleak_prepare_child(Actor* a){
    Actor* child = waterleak_child(a);
    if(child) return child;
    child = actor_spawn(OBJ_WATERLEAKICE, a->target, a->up, a->front);
    if(!child && stream_evict(a->position)) child = actor_spawn(OBJ_WATERLEAKICE, a->target, a->up, a->front);
    if(!child) return NULL;
    a->args[1] = (int16_t)actor_index(child);
    child->value = (int16_t)actor_index(a);
    child->state = WL_ICE_DEAD;
    child->flags = ACTOR_HIDDEN;
    child->anim = -1;
    child->timer = 0;
    child->figure.scale = a->figure.scale;
    return child;
}
static const WaterLeakSpec* waterleak_find_spec(int id){
    for(unsigned i = 0; i < WATERLEAK_SPEC_COUNT; i++)
        if(waterleak_specs[i].source == id) return &waterleak_specs[i];
    return NULL;
}
static bool waterleak_init_spec(Actor* a, const StageObject* o, const WaterLeakSpec* spec){
    if(!spec){
        a->active = 0;
        return true;
    }
    a->home = a->position;
    a->args[0] = spec->height;
    a->args[1] = -1;
    a->anim = spec->pose;
    a->state = WL_WAIT;
    a->timer = 0;
    a->anim_frame = 0;
    a->target = v3_add(a->home, v3_scale14(a->up, a->args[0]));
    a->position = v3_add(a->home, v3_scale14(a->up, a->args[0] / 2));
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    waterleak_prepare_child(a);
    return true;
}
static void waterleak_init(Actor* a, const StageObject* o){
    waterleak_init_spec(a, o, waterleak_find_spec(o->id));
}
static bool waterleak_unclipped(const Actor* a){
    return (a->type == OBJ_WATERLEAKICE && a->state != WL_ICE_DEAD) ||
           (a->type == OBJ_WATERLEAKPIPE && a->state == WL_FREEZE);
}
static void waterleak_think(Actor* a){
    if(a->type == OBJ_WATERLEAKICE){
        if(a->value < 0 || a->value >= g_game.actor_count || !g_game.actors[a->value].active ||
           g_game.actors[a->value].type != OBJ_WATERLEAKPIPE){
            a->active = 0;
            return;
        }
        if(a->state == WL_ICE_DEAD){
            a->timer = 0;
            return;
        }
        if(a->state == WL_ICE_BREAK &&
           a->timer >= anim_length_of((const FigureHeader*)figure_waterleakice, ANIM_WATERLEAKICE_END)){
            a->state = WL_ICE_DEAD;
            a->flags |= ACTOR_HIDDEN;
            a->timer = 0;
        }
        return;
    }
    Actor* child = waterleak_prepare_child(a);
    if(!child) return;
    int step = a->timer - 1;
    if(a->state == WL_FREEZE){
        if(step == 0 && child->state == WL_ICE_DEAD){
            child->state = WL_ICE_APPEAR;
            child->flags &= ~ACTOR_HIDDEN;
            child->anim = ANIM_WATERLEAKICE_START;
            child->anim_frame = 0;
            child->timer = 0;
            audio_play_effect(SFX_WATERLEAK_FREEZE, 3);
        } else if(g_mario_link.ride == actor_index(child) && mario_power_mode() == 3){
            a->timer = 0;
        } else if(step == 15){
            child->state = WL_ICE_BREAK;
            child->anim = ANIM_WATERLEAKICE_END;
            child->anim_frame = 0;
            child->timer = 0;
            audio_play_effect(SFX_WATERLEAK_MELT, 3);
            a->state = WL_WAIT;
            a->timer = 0;
        }
        return;
    }

    Player* p = &g_game.player;
    if(mario_power_mode() != 3 || !g_mario.jumping || vertical_speed(g_mario.jump_vec, a->up) >= 0 ||
       v3_dot14(v3_sub(p->figure.position, a->home), a->up) < 0)
        return;
    if(p->action != MA_HIPDROP && p->action != MA_JUMP && p->action != MA_JUMP2 && p->action != MA_JUMP3 &&
       p->action != MA_FALL && p->action != MA_SPIN)
        return;
    v3 delta = v3_sub(mario_centre(), a->target);
    int radius = 120 + C_BODY_RADIUS;
    if(fx_abs(delta.x) > radius || fx_abs(delta.y) > radius || fx_abs(delta.z) > radius ||
       v3_length_squared64(delta) > (uint64_t)radius * radius)
        return;
    a->state = WL_FREEZE;
    a->timer = 0;
}

static bool waterleak_collision_one(int i, v3 centre){
    const Actor* a = &g_game.actors[i];
    if(!a->active || a->type != OBJ_WATERLEAKICE || a->state == WL_ICE_DEAD) return false;
    int16_t scale[3];
    actor_stage_scale(a, scale);
    dyncol_add_codes(&waterleak_mesh, a->position, a->up, a->front, scale, i, 0, centre, 400, waterleak_pa);
    return true;
}
static void waterleak_collision(void){
    for(int i = 0; i < g_game.actor_count; i++) waterleak_collision_one(i, g_game.player.figure.position);
}

static void waterleak_pipe_figure(const Actor* a, Figure* f){ f->position = a->home; }
