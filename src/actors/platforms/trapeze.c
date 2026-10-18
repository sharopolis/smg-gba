enum {
    TRAPEZE_FREE = ACTOR_STATE_WAIT,
    TRAPEZE_INVALID,
    TRAPEZE_STOP,
    TRAPEZE_SLIDE_START,
    TRAPEZE_SLIDE,
    TRAPEZE_WAIT,
    TRAPEZE_FRONT_START,
    TRAPEZE_FRONT_END,
    TRAPEZE_BACK_START,
    TRAPEZE_BACK_END,
    TRAPEZE_SLOW_FRONT_START,
    TRAPEZE_SLOW_FRONT_END,
    TRAPEZE_SLOW_BACK_START,
    TRAPEZE_SLOW_BACK_END
};
enum {
    TRAPEZE_STICK_FRONT = 1,
    TRAPEZE_STICK_BACK = 2,
    TRAPEZE_WAS_FRONT = 4,
    TRAPEZE_WAS_BACK = 8,
    TRAPEZE_READY = 128
};

static fx trapeze_length(const Actor* a){
    const StageObject* o = actor_stage_object(a);
    return FX(100) * (o && o->scale[1] ? o->scale[1] : 256) >> 8;
}
static v3 trapeze_front(const Actor* a){
    const StageObject* o = actor_stage_object(a);
    return o ? gcam_v16(o->front) : V3(0, N14_ONE, 0);
}
static v3 trapeze_up(const Actor* a){
    const StageObject* o = actor_stage_object(a);
    return o ? gcam_v16(o->up) : V3(0, 0, N14_ONE);
}
static void trapeze_set_state(Actor* a, int state){
    a->state = state;
    a->timer = -1;
}

static v3 trapeze_read_stick(void){
    unsigned keys = g_game.keys.held;
    int x = ((keys & KEY_RIGHT) != 0) - ((keys & KEY_LEFT) != 0);
    int y = ((keys & KEY_UP) != 0) - ((keys & KEY_DOWN) != 0);
    if(keys & KEY_L) x = y = 0;
    g_mario.stick = (x || y) ? N14_ONE : 0;
    v3 side = v3_cross14(g_game.camera.front, g_game.camera.up);
    side.z = 0;
    v3 front = g_game.camera.front;
    front.z = 0;
    side = v3_direction14(side, V3(0, 0, 0));
    front = v3_direction14(front, V3(0, 0, 0));
    v3 dir = v3_add(v3_scale14(side, x * N14_ONE), v3_scale14(front, y * N14_ONE));
    return g_mario.move_dir = v3_direction14(dir, V3(0, 0, 0));
}

static void trapeze_pendulum_step(v3* pos16, v3* vel, v3* up, v3* front, v3 anchor, fx length, v3 accel,
                                  v3 side_kill){
    v3 old_side = v3_cross14(*up, *front);
    *vel = v3_add(*vel, accel);
    v3 next = v3_sub(v3_add(*pos16, *vel), V3(anchor.x << 8, anchor.y << 8, anchor.z << 8));
    v3 dir = v3_direction14(next, v3_neg(*up));
    if(v3_length_squared64(next) > 1) *up = v3_neg(dir);

    v3 restriction = V3((int64_t)dir.x * length * 256 >> 14, (int64_t)dir.y * length * 256 >> 14,
                        (int64_t)dir.z * length * 256 >> 14);
    *vel = v3_sub(*vel, v3_sub(next, restriction));
    if(side_kill.x || side_kill.y || side_kill.z) *vel = v3_reject14(*vel, side_kill);
    *pos16 = v3_add(*pos16, *vel);
    *vel = v3_scale14(*vel, R14(.995f));
    v3 side = v3_direction14(v3_cross14(*up, *front), old_side);
    *front = v3_direction14(v3_cross14(side, *up), *front);
}
static void trapeze_swing_bar(Actor* a, v3 anchor, fx length){
    trapeze_pendulum_step(&a->figure.extra_rotation, &a->velocity, &a->up, &a->front, anchor, length,
                          v3_scale14(v3_neg(a->figure.up), G(1)), V3(0, 0, 0));
    a->position =
        V3(a->figure.extra_rotation.x >> 8, a->figure.extra_rotation.y >> 8, a->figure.extra_rotation.z >> 8);
}
static bool trapeze_stick_started(const Actor* a, int bit){
    return (a->flags & bit) && !(a->flags & (bit << 2));
}
static void trapeze_mario_anim(Actor* a){
    int anim = ANIM_MARIO_TRAPEZEWAIT, blend = 12;
    switch(a->state){
    case TRAPEZE_SLIDE_START:
        anim = ANIM_MARIO_TRAPEZESLIDEDOWNSTART;
        blend = -1;
        break;
    case TRAPEZE_SLIDE:
        anim = ANIM_MARIO_TRAPEZESLIDEDOWN;
        blend = -1;
        break;
    case TRAPEZE_FRONT_START: anim = ANIM_MARIO_TRAPEZESWINGFRONTSTART; break;
    case TRAPEZE_FRONT_END: anim = ANIM_MARIO_TRAPEZESWINGFRONTEND; break;
    case TRAPEZE_BACK_START: anim = ANIM_MARIO_TRAPEZESWINGBACKSTART; break;
    case TRAPEZE_BACK_END: anim = ANIM_MARIO_TRAPEZESWINGBACKEND; break;
    case TRAPEZE_SLOW_FRONT_START: anim = ANIM_MARIO_TRAPEZESLOWSWINGFRONTSTART; break;
    case TRAPEZE_SLOW_FRONT_END: anim = ANIM_MARIO_TRAPEZESLOWSWINGFRONTEND; break;
    case TRAPEZE_SLOW_BACK_START: anim = ANIM_MARIO_TRAPEZESLOWSWINGBACKSTART; break;
    case TRAPEZE_SLOW_BACK_END: anim = ANIM_MARIO_TRAPEZESLOWSWINGBACKEND; break;
    }
    if(blend < 0) mario_restart_anim(anim);
    else mario_start_anim(ANIM_GROUP_SINGLE, bee_anim(anim), -1, blend);
}
static bool trapeze_jump(Actor* a, v3 stick){
    if(!(g_game.keys.pressed & KEY_A)) return false;
    Player* p = &g_game.player;
    v3 velocity = a->target;
    velocity.z = fx_clamp(velocity.z, G(20), G(50));
    v3 horizontal = velocity;
    horizontal.z = 0;
    int64_t h2 = v3_length_squared64(horizontal);
    if(h2 > (int64_t)300 * 256 * 256) horizontal = v3_set_length(horizontal, G(20), trapeze_front(a));
    else if(h2 < (int64_t)G(15) * G(15) && g_mario.stick)
        horizontal = v3_scale14(trapeze_front(a), v3_dot14(stick, trapeze_front(a)) > 0 ? G(15) : -G(15));
    velocity.x = horizontal.x;
    velocity.y = horizontal.y;
    v3 face = a->value ? v3_neg(trapeze_front(a)) : trapeze_front(a);
    int anim = v3_dot14(face, velocity) >= 0 ? ANIM_MARIO_TRAPEZEFRONTSPIN : ANIM_MARIO_TRAPEZEBACKSPIN;
    p->front = face;
    mario_bind_release(velocity, 13, anim);
    const StageObject* o = actor_stage_object(a);
    if(o && o->camera >= 0) camera_event_end_at_landing();

    audio_play_effect(SFX_TRAPEZE_JUMP, 3);
    trapeze_set_state(a, TRAPEZE_INVALID);
    return true;
}
static void trapeze_swing_mario(Actor* a, v3 stick){
    Player* p = &g_game.player;
    fx length = trapeze_length(a);
    a->args[0] = fx_min(a->args[0] + FX(5), length);
    int old = a->flags & (TRAPEZE_STICK_FRONT | TRAPEZE_STICK_BACK);
    a->flags = (a->flags & ~15) | (old << 2);
    v3 accel = V3(0, 0, 0), anchor_front = trapeze_front(a), side = v3_cross14(trapeze_up(a), anchor_front);
    if(g_mario.stick){
        bool positive = v3_dot14(stick, anchor_front) >= 0;
        accel = v3_scale14(v3_reject14(positive ? anchor_front : v3_neg(anchor_front), side), G(.5f));
        a->flags |= (positive != !!a->value) ? TRAPEZE_STICK_FRONT : TRAPEZE_STICK_BACK;
    }
    accel = v3_add(accel, v3_scale14(v3_neg(a->figure.up), G(.8f)));
    v3 pos = g_mario.position16, up = p->up, front = a->figure.orientation;
    trapeze_pendulum_step(&pos, &a->target, &up, &front, a->home, a->args[0], accel, side);
    a->figure.orientation = front;
    a->radius = v3_dot14(a->target, front) * (a->value ? -1 : 1);
    mario_bind_pose(V3(pos.x >> 8, pos.y >> 8, pos.z >> 8), up, a->value ? v3_neg(front) : front);
    g_mario.position16 = pos;
    if(a->args[0] < length) trapeze_swing_bar(a, p->figure.position, length - a->args[0]);
    else {
        a->position = p->figure.position;
        a->figure.extra_rotation = pos;
        a->up = up;
        a->front = front;
        a->velocity = V3(0, 0, 0);
    }
    int sound_level = fx_clamp(v3_length_exact(a->target) * 100 / G(40), 0, 100);
    if(a->health < 20 && sound_level >= 20) audio_play_effect(SFX_TRAPEZE_CREAK, 1);
    a->health = sound_level;
}
static bool trapeze_grab(Actor* a){
    Player* p = &g_game.player;
    if(p->on_ground || p->action == MA_TALK || p->action == MA_STAR_LAUNCH || p->action == MA_DEATH ||
       g_mario_link.bind >= 0 || g_mario_held)
        return false;
    if(p->figure.position.z > a->home.z) return false;
    v3 line = v3_sub(a->position, a->home), d = v3_sub(p->figure.position, a->home);
    fx line_len = v3_length_exact(line);
    v3 axis = v3_direction14(line, v3_neg(trapeze_up(a)));
    v3 sensor = v3_add(a->home, v3_scale14(axis, fx_clamp(v3_dot14(d, axis), 0, line_len)));
    v3 difference = v3_sub(p->figure.position, sensor);

    if(v3_length_squared64(difference) > (uint64_t)FX(300) * FX(300) ||
       fx_abs(v3_dot14(difference, trapeze_front(a))) > FX(80))
        return false;
    v3 projected = v3_add(v3_scale14(trapeze_up(a), v3_dot14(d, trapeze_up(a))),
                          v3_scale14(trapeze_front(a), v3_dot14(d, trapeze_front(a))));
    a->args[0] = fx_clamp(v3_length_exact(projected), 0, trapeze_length(a));
    fx speed = v3_dot14(g_mario.jump_vec, trapeze_front(a));
    fx prior = v3_dot14(a->target, trapeze_front(a));
    if(fx_abs(speed) < G(3) && prior > G(3)) speed = prior;
    a->target = v3_scale14(trapeze_front(a), speed);
    a->value = v3_dot14(p->front, trapeze_front(a)) < 0;
    v3 up = v3_neg(v3_direction14(projected, V3(0, 0, -N14_ONE)));
    v3 front = fx_abs(projected.z) < FX(1) ? V3(0, 0, -N14_ONE) : trapeze_front(a);
    v3 side = v3_cross14(up, front);
    front = v3_direction14(v3_cross14(side, up), trapeze_front(a));

    a->figure.orientation = front;
    v3 grab_position = v3_add(a->home, v3_scale14(v3_neg(up), a->args[0]));
    up = trapeze_up(a);
    mario_bind_start(a);
    mario_bind_pose(grab_position, up, a->value ? v3_neg(front) : front);
    mario_restart_anim(ANIM_MARIO_TRAPEZEWAIT);
    const StageObject* o = actor_stage_object(a);
    if(o && o->camera >= 0){
        camera_event_start(o->camera, 1, p->figure.position, p->up, p->front);
        camera_event_end_at_landing();
    }

    audio_play_effect(SFX_TRAPEZE_CATCH, 3);
    a->flags &= ~15;
    trapeze_set_state(a, TRAPEZE_SLIDE_START);
    return true;
}
static void trapeze_init(Actor* a){
    a->figure.scale = V3(64, 64, 64);
    a->health = 0;
    a->flags |= TRAPEZE_READY;
    a->position = v3_sub(a->home, V3(0, 0, trapeze_length(a)));
    a->figure.extra_rotation = V3(a->position.x << 8, a->position.y << 8, a->position.z << 8);
    a->figure.up = gravity_sample(g_game.stage.header, a->home).up;
    a->up = V3(0, 0, N14_ONE);
    a->front = trapeze_front(a);
    a->figure.orientation = a->front;
    a->velocity = a->target = V3(0, 0, 0);
}
static void trapeze_think(Actor* a){
    Player* p = &g_game.player;
    const StageObject* placement = actor_stage_object(a);

    if(p->on_ground && g_mario_link.bind != actor_index(a) && placement && placement->camera >= 0 &&
       g_gcam.event_active && g_gcam.event_on_mario && g_gcam.event_first == placement->camera)
        camera_event_landed();
    if(a->state == TRAPEZE_FREE || a->state == TRAPEZE_INVALID){
        trapeze_swing_bar(a, a->home, trapeze_length(a));
        if(a->state == TRAPEZE_FREE && v3_length_squared64(a->velocity) < (uint64_t)G(1) * G(1) &&
           N14_ONE - fx_abs(a->up.z) < R14(.001f))
            trapeze_set_state(a, TRAPEZE_STOP);
        if(a->state == TRAPEZE_INVALID){
            v3 d = v3_sub(p->figure.position, a->position);
            d.z = 0;
            if(p->on_ground || a->timer > 40 || v3_length_squared64(d) > (uint64_t)FX(500) * FX(500))
                trapeze_set_state(a, TRAPEZE_FREE);
        }
    }
    if(a->state == TRAPEZE_FREE || a->state == TRAPEZE_STOP){
        trapeze_grab(a);
        return;
    }
    if(a->state == TRAPEZE_INVALID) return;
    if(g_mario_link.bind != actor_index(a)){
        a->args[0] = 0;
        trapeze_set_state(a, TRAPEZE_FREE);
        return;
    }
    if(a->timer == 0) trapeze_mario_anim(a);
    v3 stick = trapeze_read_stick();
    if(trapeze_jump(a, stick)) return;
    trapeze_swing_mario(a, stick);
    if(a->state == TRAPEZE_WAIT){
        if(a->flags & TRAPEZE_STICK_FRONT) trapeze_set_state(a, TRAPEZE_FRONT_START);
        else if(a->flags & TRAPEZE_STICK_BACK) trapeze_set_state(a, TRAPEZE_BACK_START);
        return;
    }
    if(!(a->flags & (TRAPEZE_STICK_FRONT | TRAPEZE_STICK_BACK)) && fx_abs(a->radius) < G(10) &&
       p->up.z >= R14(.99f)){
        trapeze_set_state(a, TRAPEZE_WAIT);
        return;
    }
    if(a->state == TRAPEZE_SLIDE_START || a->state == TRAPEZE_SLIDE){
        if(a->args[0] == trapeze_length(a)){
            if(a->timer > 20) audio_play_effect(SFX_TRAPEZE_GRAB, 2);
            trapeze_set_state(a, a->radius > 0 ? TRAPEZE_FRONT_START : TRAPEZE_BACK_START);
        } else if(a->state == TRAPEZE_SLIDE_START && a->timer >= 40) trapeze_set_state(a, TRAPEZE_SLIDE);
        return;
    }
    bool sf = trapeze_stick_started(a, TRAPEZE_STICK_FRONT),
         sb = trapeze_stick_started(a, TRAPEZE_STICK_BACK);
    bool front = a->flags & TRAPEZE_STICK_FRONT, back = a->flags & TRAPEZE_STICK_BACK;
    switch(a->state){
    case TRAPEZE_FRONT_START:
    case TRAPEZE_SLOW_FRONT_START:
        if(a->timer >= (a->state == TRAPEZE_SLOW_FRONT_START ? 29 : 44) && !(front && a->radius < 0))
            trapeze_set_state(a, !front && a->radius < G(80) ? TRAPEZE_SLOW_FRONT_END : TRAPEZE_FRONT_END);
        else if(sf && a->state == TRAPEZE_SLOW_FRONT_START) trapeze_set_state(a, TRAPEZE_FRONT_START);
        else if(sb) trapeze_set_state(a, TRAPEZE_BACK_START);
        else if(!front && a->radius < 0)
            trapeze_set_state(a, a->state == TRAPEZE_SLOW_FRONT_START ? TRAPEZE_SLOW_BACK_END
                                                                      : TRAPEZE_BACK_END);
        break;
    case TRAPEZE_BACK_START:
    case TRAPEZE_SLOW_BACK_START:
        if(a->timer >= (a->state == TRAPEZE_SLOW_BACK_START ? 29 : 44) && !(back && a->radius > 0))
            trapeze_set_state(a, !back && a->radius > -G(80) ? TRAPEZE_SLOW_BACK_END : TRAPEZE_BACK_END);
        else if(sb && a->state == TRAPEZE_SLOW_BACK_START) trapeze_set_state(a, TRAPEZE_BACK_START);
        else if(sf) trapeze_set_state(a, TRAPEZE_FRONT_START);
        else if(!back && a->radius > 0)
            trapeze_set_state(a, a->state == TRAPEZE_SLOW_BACK_START ? TRAPEZE_SLOW_FRONT_END
                                                                     : TRAPEZE_FRONT_END);
        break;
    case TRAPEZE_FRONT_END:
    case TRAPEZE_SLOW_FRONT_END:
        if(a->radius <= 0)
            trapeze_set_state(a, a->state == TRAPEZE_SLOW_FRONT_END ? TRAPEZE_SLOW_BACK_START
                                                                    : TRAPEZE_BACK_START);
        else if(sf) trapeze_set_state(a, TRAPEZE_FRONT_START);
        else if(sb && a->state == TRAPEZE_SLOW_FRONT_END) trapeze_set_state(a, TRAPEZE_BACK_START);
        break;
    case TRAPEZE_BACK_END:
    case TRAPEZE_SLOW_BACK_END:
        if(a->radius >= 0)
            trapeze_set_state(a, a->state == TRAPEZE_SLOW_BACK_END ? TRAPEZE_SLOW_FRONT_START
                                                                   : TRAPEZE_FRONT_START);
        else if(sb) trapeze_set_state(a, TRAPEZE_BACK_START);
        else if(sf && a->state == TRAPEZE_SLOW_BACK_END) trapeze_set_state(a, TRAPEZE_FRONT_START);
        break;
    }
}

typedef struct {
    FigureHeader header;
    int16_t points[FIGURE_TRAPEZE_DRAW_POINTS][3];
    FigureTriangle triangles[12];
} TrapezeRopeVisual;
_Static_assert(FIGURE_TRAPEZE_DRAW_POINTS <= FIGURE_PROCEDURAL_MAX_POINTS,
               "rope points must fit the procedural figure limit");
_Static_assert(12 * sizeof(FramePoint) + sizeof(TrapezeRopeVisual) <= sizeof g_figure_points,
               "rope output must not overlap its input");
static const uint16_t trapeze_rope_axis_order[FIGURE_DRAW_AXES][12] = {
#define TRAPEZE_ROPE_ORDER 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
    {TRAPEZE_ROPE_ORDER}, {TRAPEZE_ROPE_ORDER}, {TRAPEZE_ROPE_ORDER}
#undef TRAPEZE_ROPE_ORDER
};
static void trapeze_rope_segment(TrapezeRopeVisual* v, int at, v3 start, v3 end, v3 side, v3 front,
                                 int coord0, int coord1){
    static const int8_t widths[3][4] = {{-12, 19, 19, -19}, {-19, -19, 12, 19}, {19, -7, -19, -7}};
    static const int16_t phase[3] = {0, 358, 256};
    static const uint8_t winding[4][3] = {{0, 1, 2}, {1, 3, 2}, {2, 1, 0}, {2, 3, 1}};
    uint16_t flags = model_triangles((const FigureHeader*)figure_trapezerope)[0].flags;
    for(int plane = 0; plane < 3; plane++){
        v3 lo = v3_add(v3_scale14(side, widths[plane][0]), v3_scale14(front, widths[plane][1]));
        v3 hi = v3_add(v3_scale14(side, widths[plane][2]), v3_scale14(front, widths[plane][3]));
        v3 p[4] = {v3_add(start, lo), v3_add(start, hi), v3_add(end, lo), v3_add(end, hi)};
        int base = plane * 4;
        for(int j = 0; j < 4; j++){
            v->points[base + j][0] = (int16_t)(p[j].x * 4);
            v->points[base + j][1] = (int16_t)(p[j].y * 4);
            v->points[base + j][2] = (int16_t)(p[j].z * 4);
        }
        int16_t tex0 = coord0 * 1536 / 1000 + phase[plane], tex1 = coord1 * 1536 / 1000 + phase[plane];
        for(int t = 0; t < 4; t++){
            FigureTriangle* tri = &v->triangles[plane * 4 + t];
            for(int j = 0; j < 3; j++){
                int k = winding[t][j];
                tri->v[j] = (ModelVertex){base + k, (k & 1) ? 512 : 0, k < 2 ? tex0 : tex1};
            }
            tri->texture = 0;
            tri->flags = flags;
        }
    }
}
__attribute__((noinline)) static void trapeze_add_rope(Figure* visual){
    Game* game = &g_game;
    if(game->figure_count >= MAX_FIGURES) return;
    Actor* a = NULL;
    uint64_t best = (uint64_t)FX(2000) * FX(2000);
    for(int i = 0; i < game->actor_count; i++){
        Actor* candidate = &game->actors[i];
        if(!candidate->active || candidate->type != OBJ_TRAPEZE || !(candidate->flags & TRAPEZE_READY))
            continue;
        uint64_t d = v3_length_squared64(v3_sub(candidate->position, game->player.figure.position));
        if(d < best){
            best = d;
            a = candidate;
        }
    }
    if(!a) return;
    figure_init(visual, figure_trapezerope);
    visual->position = a->position;
    visual->scale = V3(64, 64, 64);
    figure_update_transform(visual);

    visual->local_yaw = actor_index(a);
    game->figures[game->figure_count++] = visual;
}
__attribute__((noinline)) static int trapeze_prepare_rope(Figure* figure, int segment){
    Actor* a = &g_game.actors[figure->local_yaw];
    TrapezeRopeVisual* visual = geometry_figure_draw_tail(sizeof(TrapezeRopeVisual));
    v3 side = v3_cross14(trapeze_up(a), trapeze_front(a));
    v3 bar = v3_sub(a->position, a->home);
    bool bound = g_mario_link.bind == actor_index(a);
    int coord = bound ? a->args[0] : trapeze_length(a);
    bool split = bound && coord < trapeze_length(a);
    int sign = (split ? segment / 2 : segment) ? 1 : -1;
    bool lower = split && (segment & 1);
    v3 offset = v3_add(v3_scale14(side, FX(60) * sign), v3_sub(a->home, figure->position));
    v3 hand = bound ? v3_sub(g_game.player.figure.position, a->home) : bar;
    v3 front = lower ? a->front : bound ? a->figure.orientation : a->front;
    v3 up = lower ? a->up : bound ? g_game.player.up : a->up;
    trapeze_rope_segment(visual, 0, v3_add(lower ? hand : V3(0, 0, 0), offset),
                         v3_add(lower ? bar : hand, offset), v3_cross14(up, front), front, lower ? coord : 0,
                         lower ? trapeze_length(a) : coord);
    FigureHeader* header = &visual->header;
    *header = (FigureHeader){0};
    header->point_count = FIGURE_TRAPEZE_DRAW_POINTS;
    header->triangle_count = 12;
    header->texture_count = 1;
    header->extent = trapeze_length(a) * 4 + 128;
    header->points = (uint32_t)((uintptr_t)visual->points - (uintptr_t)header);
    header->triangles = (uint32_t)((uintptr_t)visual->triangles - (uintptr_t)header);
    header->axes = (uint32_t)((uintptr_t)trapeze_rope_axis_order - (uintptr_t)header);
    header->textures =
        (uint32_t)((uintptr_t)model_textures((const FigureHeader*)figure_trapezerope) - (uintptr_t)header);
    figure->model = header;
    figure->pose = visual->points;
    return split ? 4 : 2;
}

__attribute__((noinline)) static bool trapeze_draw(uint8_t* framebuffer, Figure* figure,
                                                   const CameraView* view){
    if(figure->model != (const FigureHeader*)figure_trapezerope) return false;
    int segments = trapeze_prepare_rope(figure, 0);
    for(int segment = 0; segment < segments; segment++){
        if(segment) trapeze_prepare_rope(figure, segment);
        draw_figure(framebuffer, figure, view);
    }
    figure->model = (const FigureHeader*)figure_trapezerope;
    figure->pose = model_rest_pose(figure->model);
    return true;
}

static const FigureOps trapeze_figure_ops = {.draw = trapeze_draw};
