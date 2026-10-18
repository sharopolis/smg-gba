enum { CAGE_SMALL_WAIT = 64, CAGE_SMALL_BREAK, CAGE_SMALL_AFTER };

static void crystal_cage_small_init(Actor* a){
    a->figure.model = (const FigureHeader*)figure_honeycrystalcage;
    a->figure.pose = model_rest_pose(a->figure.model);
    a->state = CAGE_SMALL_WAIT;
    a->anim = -1;
    a->anim_frame = 0;
    GravitySample gravity = gravity_sample(g_game.stage.header, a->home);
    a->velocity = gravity.index >= 0 ? gravity.up : a->up;

    RayHit hit;
    fx reach = FX(300) * a->figure.scale.x / 64;
    a->target = a->position;
    if(collision_ray(v3_add(a->position, v3_scale14(a->up, reach)),
                     v3_sub(a->position, v3_scale14(a->up, reach)), &hit, 0))
        a->target = hit.point;

    if(stream_part_state(a->stage_index) || switch_on(actor_switch_key(a, SWITCH_DEAD))){
        a->state = CAGE_SMALL_AFTER;
        a->position = a->target;
        a->figure.model = (const FigureHeader*)figure_honeycrystalwreck;
        a->figure.pose = model_rest_pose(a->figure.model);
    }
}

static void crystal_cage_small_break(Actor* a){
    a->position = a->target;
    a->figure.model = (const FigureHeader*)figure_honeycrystalbreak;
    actor_set_state(a, CAGE_SMALL_BREAK, ANIM_HONEYCRYSTALBREAK_BREAK);

    audio_queue_effect(SFX_CRYSTAL_CAGE_S_BREAK, 3);
    stream_part_state_set(a->stage_index, 1);

    switch_set(actor_switch_key(a, SWITCH_DEAD), true);
}

static void crystal_cage_small_think(Actor* a){
    if(a->state == CAGE_SMALL_BREAK){
        if(a->timer >= 49){
            a->figure.model = (const FigureHeader*)figure_honeycrystalwreck;
            a->figure.pose = model_rest_pose(a->figure.model);
            actor_set_state(a, CAGE_SMALL_AFTER, -1);
            a->anim = -1;
            a->anim_frame = 0;
        }
        return;
    }
    if(a->state != CAGE_SMALL_WAIT) return;
    if(switch_on(actor_switch_key(a, SWITCH_A))){
        crystal_cage_small_break(a);
        return;
    }
    Player* p = &g_game.player;
    if(p->action == MA_DEATH || p->action == MA_STAR_LAUNCH || g_mario_held) return;

    fx h = fx_clamp(v3_dot14(v3_sub(p->figure.position, a->position), a->up), -FX(40), FX(140));
    v3 sensor = v3_add(a->position, v3_scale14(a->up, h));
    v3 d = v3_sub(mario_centre(), sensor);
    fx r = FX(130) * a->figure.scale.x / 64 + (g_mario.attack_timer > 0 ? FX(100) : C_BODY_RADIUS);
    if(v3_length_squared64(d) < (uint64_t)r * r && (g_mario.attack_timer > 0 || g_mario_star_power > 0))
        crystal_cage_small_break(a);
}

static void crystal_cage_small_collision(Actor* a, v3 mario){
    if(a->state != CAGE_SMALL_WAIT) return;
    int16_t scale[3];
    actor_stage_scale(a, scale);
    dyncol_add(&honey_crystal_mesh, a->position, a->up, a->front, scale, actor_index(a), 0, mario, FX(600));
}

static Actor* crystal_cage_breaking_actor(Figure* f){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(f == &a->figure && (a->type == OBJ_CRYSTALCAGES || a->type == OBJ_CRYSTALCAGEM) &&
           a->state == CAGE_SMALL_BREAK)
            return a;
    }
    return NULL;
}
__attribute__((noinline)) static bool crystal_cage_pose_figure(Figure* f, int role){
    Actor* a = crystal_cage_breaking_actor(f);
    if(!a){
        f->local_pitch = f->local_roll = f->local_yaw = 0;
        f->transform_dirty = true;
        figure_update_transform(f);
        return false;
    }
    if(role == 1){
        f->model = (const FigureHeader*)(a->type == OBJ_CRYSTALCAGEM ? figure_eggcrystalmwreck
                                                                     : figure_honeycrystalwreck);
        f->pose = model_rest_pose(f->model);
        f->position = a->target;
        f->basis_up = a->up;
        f->basis_front = a->front;
    } else {
        f->model = (const FigureHeader*)figure_honeycrystalbreak;
        f->pose = figure_clip_pose(f->model, a->anim, a->anim_frame);
        f->position = a->home;
        f->basis_up = a->velocity;

        v3 up = a->velocity;
        v3 support = fx_abs(up.y) > fx_abs(up.x) && fx_abs(up.y) > fx_abs(up.z) ? V3(0, 0, N14_ONE)
                                                                                : V3(0, N14_ONE, 0);
        f->basis_front = v3_tangent14(support, up, a->front);
    }
    f->local_pitch = f->local_roll = f->local_yaw = 0;
    f->transform_dirty = true;
    figure_update_transform(f);
    return true;
}

static bool crystal_cage_model(const FigureHeader* model){
    return model == (const FigureHeader*)figure_honeycrystalcage ||
           model == (const FigureHeader*)figure_honeycrystalbreak ||
           model == (const FigureHeader*)figure_honeycrystalwreck ||
           model == (const FigureHeader*)figure_eggcrystalmintact ||
           model == (const FigureHeader*)figure_eggcrystalmwreck;
}

__attribute__((noinline)) static bool crystal_cage_draw(uint8_t* framebuffer, Figure* f,
                                                        const CameraView* view){
    if(!crystal_cage_model(f->model)) return false;
    bool breaking = crystal_cage_pose_figure(f, 0);
    draw_figure(framebuffer, f, view);
    if(breaking){
        crystal_cage_pose_figure(f, 1);
        draw_figure(framebuffer, f, view);
        crystal_cage_pose_figure(f, 0);
    }
    return true;
}

__attribute__((noinline)) static bool crystal_cage_view_bounds(const Figure* f, const CameraView* view){
    if(!crystal_cage_model(f->model)) return false;
    fx radius = FX(f->model == (const FigureHeader*)figure_eggcrystalmintact ||
                           f->model == (const FigureHeader*)figure_eggcrystalmwreck
                       ? 600
                       : 300) *
                f->scale.x / 64;
    if(f->model == (const FigureHeader*)figure_honeycrystalbreak){
        radius = FX(HONEY_CRYSTAL_BREAK_RADIUS) * f->scale.x / 64;
        Actor* a = crystal_cage_breaking_actor((Figure*)f);
        if(a) radius += v3_length_exact(v3_sub(a->home, a->target));
    }
    return view_sphere_visible(view, f->position, radius);
}

static const FigureOps crystal_cage_figure_ops = {
    .draw = crystal_cage_draw,
    .view_bounds = crystal_cage_view_bounds,
};
