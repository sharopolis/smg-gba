enum { PH_WAIT = 64, PH_MOVE, PH_UNDERWATER };
enum { PH_FORCE_WAIT, PH_FORCE_MOVE, PH_FORCE_RETURN };
static const PhantomFloaterSpec* phantom_floater_spec(const Actor* a){
    for(unsigned i = 0; i < sizeof phantom_floater_specs / sizeof *phantom_floater_specs; i++)
        if(phantom_floater_specs[i].object == a->stage_index) return &phantom_floater_specs[i];
    return NULL;
}
static int32_t phantom_spring_round(int64_t value, int divisor){
    return value < 0 ? -(int32_t)((-value + divisor / 2) / divisor)
                     : (int32_t)((value + divisor / 2) / divisor);
}
static void phantom_spring_value(v3* spring, int acceleration, int damping){
    spring->y += spring->z;
    spring->z += phantom_spring_round((int64_t)acceleration * (spring->x - spring->y), 1000);
    spring->z = phantom_spring_round((int64_t)spring->z * damping, 1000);
}
static void phantom_force_set(Actor* a, int state){
    a->args[0] = state;
    a->args[1] = 1;
}
static bool phantom_floater_carries_player(int set){
    const Player* p = &g_game.player;
    if(!p->on_ground || g_mario_held || p->action == MA_STAR_LAUNCH) return false;
    const ColDynSet* box = &g_col_dyn_sets[set];
    if(!box->on) return false;
    v3 from = v3_add(p->figure.position, v3_scale14(p->up, 30));
    v3 to = v3_sub(p->figure.position, v3_scale14(p->up, 40));
    int shift = g_collision.shift;
    const int32_t* first = (const int32_t*)&from;
    const int32_t* last = (const int32_t*)&to;
    for(int axis = 0; axis < 3; axis++)
        if((fx_min(first[axis], last[axis]) >> shift) > box->hi[axis] ||
           (fx_max(first[axis], last[axis]) >> shift) < box->lo[axis])
            return false;
    return mover_carries_mario(set);
}
static int phantom_ripple_interval(Actor* a){
    uint16_t seed = (uint16_t)a->args[3];
    seed = (uint16_t)(seed * 25173u + 13849u);
    a->args[3] = (int16_t)seed;
    return 20 + seed % 71;
}
static void phantom_floater_place(Actor* a, int set){
    const PhantomFloaterSpec* spec = phantom_floater_spec(a);
    if(!spec || set < 0) return;
    MoverFrame f = mover_frame_of(a->position, a->up, a->front, N14_ONE);

    g_col_dyn_sets[set].on = 0;
    movers_place(set, &f);
    for(int i = 0; i < g_col_dyn_sets[set].count; i++){
        CollisionTriangle* t = &g_col_dyn[g_col_dyn_sets[set].first + i];
        t->attr = COL_NO_CAMERA;
        t->floor_code = 0;
        t->wall_code = 2;
    }
    movers_enable(set, true);
}
static void phantom_floater_init(Actor* a, const StageObject* o){
    const PhantomFloaterSpec* s = phantom_floater_spec(a);
    if(!s){
        actor_scale_from_object(a, o);
        return;
    }
    const PhantomFloaterVariant* v = &phantom_floater_variants[s->variant];
    a->value = -1;
    a->target = V3(-s->rest * 65536, 0, 0);
    a->velocity = V3(0, 0, 0);
    a->args[0] = PH_FORCE_WAIT;
    a->args[1] = 1;
    a->args[3] = (int16_t)(a->stage_index * 17 + 1);
    a->health = phantom_ripple_interval(a);
    actor_set_state(a, PH_WAIT, -1);
    phantom_floater_place(a, mappart_set(a, v->kcl, v->kcl_count));
}
static void phantom_floater_think(Actor* a){
    const PhantomFloaterSpec* s = phantom_floater_spec(a);
    if(!s) return;
    const PhantomFloaterVariant* v = &phantom_floater_variants[s->variant];
    int set = mappart_set(a, v->kcl, v->kcl_count);
    if(set < 0) return;
    bool on = phantom_floater_carries_player(set);

    int step = a->timer - 1;
    if(a->state == PH_WAIT){
        if(fx_abs(a->velocity.y) >= 6554) actor_set_state(a, PH_MOVE, -1);
    } else if(a->state == PH_MOVE){
        if(step == 0){
        }
        if(--a->health <= 0){
            a->health = phantom_ripple_interval(a);
        }
        int threshold = s->variant == 0 ? 580 : s->variant == 1 ? 380 : 180;
        if(on && -a->velocity.x > threshold * 65536) actor_set_state(a, PH_UNDERWATER, -1);
        else if(!on && fx_abs(a->velocity.y) < 6554) actor_set_state(a, PH_WAIT, -1);
    } else if(!on){
        int threshold = s->variant == 0 ? 575 : s->variant == 1 ? 375 : 175;
        if(-a->velocity.x < threshold * 65536) actor_set_state(a, PH_MOVE, -1);
    }

    bool valid_on = actor_has_switch(a, SWITCH_A) && switch_on(actor_switch_key(a, SWITCH_A));
    if(a->args[0] == PH_FORCE_WAIT){
        if(!valid_on && on) phantom_force_set(a, PH_FORCE_MOVE);
    } else {
        bool returning = a->args[0] == PH_FORCE_RETURN;
        if(a->args[1]){
            a->target = V3(returning ? fx_abs(a->velocity.x) : -s->rest * 65536, 0,
                           returning ? 0 : -phantom_spring_round((int64_t)s->rotate_axis * 65536, 10));
            a->args[1] = 0;
        }
        int32_t speed = a->target.z * (valid_on ? 3 : 1);
        a->velocity.y = fx_abs(speed) < 66 ? 0 : speed;
        phantom_spring_value(&a->target, s->rotate_speed, s->rotate_angle);
        if(returning){
            if(fx_abs(a->velocity.x) <= 65 && fx_abs(a->target.z) < 6554){
                a->velocity.x = 0;
                phantom_force_set(a, PH_FORCE_WAIT);
            } else if(!valid_on && on) phantom_force_set(a, PH_FORCE_MOVE);
        } else if(valid_on || !on) phantom_force_set(a, PH_FORCE_RETURN);
    }
    v3 old = a->position;
    a->velocity.x += a->velocity.y;
    int offset = phantom_spring_round(a->velocity.x, 65536);
    a->position = v3_add(a->home, v3_scale14(a->up, offset));
    if(on) mario_nudge(v3_sub(a->position, old));
    phantom_floater_place(a, set);
}

__attribute__((noinline)) static bool phantom_floater_draw(
    uint8_t* framebuffer, Figure* figure, const CameraView* cv){
    const PhantomFloaterVariant* variant = NULL;
    for(unsigned i = 0; i < sizeof phantom_floater_variants / sizeof *phantom_floater_variants; i++)
        if(figure->model == (const FigureHeader*)phantom_floater_variants[i].models[0]){
            variant = &phantom_floater_variants[i];
            break;
        }
    if(!variant) return false;

    Figure* part = figure;
    const FigureHeader* saved_model = part->model;
    const void* saved_pose = part->pose;
    for(int i = 0; i < variant->parts; i++){
        part->model = (const FigureHeader*)variant->models[i];
        part->pose = model_rest_pose(part->model);
        draw_figure(framebuffer, part, cv);
    }
    part->model = saved_model;
    part->pose = saved_pose;
    return true;
}

static const FigureOps phantom_floater_figure_ops = {.draw = phantom_floater_draw};
