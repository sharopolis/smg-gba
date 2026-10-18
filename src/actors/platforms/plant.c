#define PLANT_RINGS             24
#define PLANT_TOP_POINTS        32
#define PLANT_FIRST_TOP_POINT   72
#define PLANT_LEAF_POINTS       26

enum { PL_DEMO = 64, PL_GROW, PL_WAIT, PL_INVALID };

static EWRAM_BSS PlantSlot g_plants[PLANT_SLOTS];
static EWRAM_BSS uint8_t g_plant_grown[(STAGE_MAX_OBJECTS + 7) / 8];

static EWRAM_BSS PlantRide g_plant_ride;

static void plant_reset(void){
    audio_stop_level_effect(SFX_PLANT_GROW);
    g_plant_ride.actor = -1;
    for(int i = 0; i < PLANT_SLOTS; i++){
        g_plants[i].stage_index = -1;
        g_plants[i].pose_grown = -1;
    }
    for(unsigned i = 0; i < sizeof g_plant_grown; i++) g_plant_grown[i] = 0;
}

static uint32_t g_plant_rand = 0x2545F491;
static int plant_rand(int lo, int hi){
    g_plant_rand = g_plant_rand * 1664525u + 1013904223u;
    return lo + (int)((g_plant_rand >> 8) % (uint32_t)(hi - lo + 1));
}

static v3 plant_dir_at(int rail, fx coord){
    fx len = rail_length(rail);
    fx a = fx_clamp(coord - FX(15), 0, len), b = fx_clamp(coord + FX(15), 0, len);
    if(b - a < FX(2)) a = fx_clamp(b - FX(30), 0, len);
    return v3_direction14(v3_sub(rail_position_at(rail, b), rail_position_at(rail, a)), V3(0, 0, N14_ONE));
}

static PlantSlot* plant_slot(Actor* a){
    for(int i = 0; i < PLANT_SLOTS; i++)
        if(g_plants[i].stage_index == a->stage_index) return &g_plants[i];
    return NULL;
}

static void plant_init_slot(Actor* a, PlantSlot* ps){
    int rail = actor_rail(a);
    ps->stage_index = a->stage_index;
    ps->length = rail_length(rail);
    ps->pose_grown = -1;
    int n = (int)((ps->length - FX(300)) / FX(200)) + 2;
    if(n > PLANT_LEAVES) n = PLANT_LEAVES;
    if(n < 0) n = 0;
    ps->leaves = (int16_t)n;
    g_plant_rand = 0x2545F491u ^ (uint32_t)a->stage_index;
    v3 base = V3(0, N14_ONE, 0);
    fx leaf_pos = ps->length - FX(100);
    for(int k = 0; k < n; k++){
        int32_t ratio = N14_ONE * k / n;
        v3 raild = plant_dir_at(rail, leaf_pos);
        v3 grow = base;
        int32_t dot = v3_dot14(grow, raild);
        if(fx_abs(dot) > R14(0.7f)){
            grow = dot > 0 ? v3_cross14(V3(0, 0, N14_ONE), raild) : v3_cross14(raild, V3(0, 0, N14_ONE));
            grow = v3_direction14(grow, base);
        }

        ps->leaf_size[k] = N14_ONE + (fx_sin(ratio * FX_TAU / (4 * N14_ONE)) << 5);
        ps->leaf_coord[k] = leaf_pos;
        ps->leaf_dir[k] = grow;
        leaf_pos -= FX(100) + (FX(200) * ratio >> 14);
        base = v3_rotate_axis14(base, V3(0, 0, N14_ONE), FXC(0.01745f) * plant_rand(90, 270));
    }
}

static void plant_init(Actor* a, const StageObject* o){
    a->value = -1;
    if(actor_rail(a) < 0) return;
    PlantSlot* ps = plant_slot(a);
    for(int i = 0; !ps && i < PLANT_SLOTS; i++)
        if((g_plants[i].stage_index < 0) || g_plants[i].stage_index == a->stage_index) ps = &g_plants[i];
    if(!ps) return;
    plant_init_slot(a, ps);
    a->value = (int16_t)(ps - g_plants);
    a->figure.model = (const FigureHeader*)figure_plant;
    a->figure.use_basis = true;
    a->figure.scale = V3(64, 64, 64);
    a->figure.pose = (const int16_t (*)[3])ps->pose;

    if(a->stage_index >= 0 && g_plant_grown[a->stage_index >> 3] >> (a->stage_index & 7) & 1){
        a->velocity = V3(ps->length, 0, 0);
        actor_set_state(a, PL_WAIT, -1);
    } else {
        a->velocity = V3(0, V16(plant_rand(15, 100)), plant_rand(5, 30));
        actor_set_state(a, actor_object_arg(a, 2) == 1 ? PL_DEMO : PL_GROW, -1);
    }
    a->target = a->home;
}

static fx plant_nearest_coord(int rail, fx grown, v3 p, uint64_t* d2_out){
    fx best = 0;
    uint64_t best_d = UINT64_MAX;
    for(fx c = FX(90); c <= grown - FX(90); c += FX(30)){
        uint64_t d = v3_length_squared64(v3_sub(rail_position_at(rail, c), p));
        if(d < best_d){
            best_d = d;
            best = c;
        }
    }
    *d2_out = best_d;
    return best;
}

static void plant_release(v3 velocity){
    g_plant_ride.actor = -1;
    mario_hold_release(velocity);
    mario_restart_anim(ANIM_MARIO_GROWPLANTJUMP);
}

static void plant_ride_tick(Actor* a){
    Player* p = game_player();
    int rail = actor_rail(a);
    mario_update_gravity();
    fx len = a->velocity.x;

    bool b = (g_game.keys.held & KEY_B) != 0;
    g_plant_ride.timer++;
    int32_t sp = g_plant_ride.speed;
    int32_t rot_deg = 0;
    if(g_plant_ride.mode == 1 || g_plant_ride.mode == 2){
        if(g_plant_ride.mode == 2 && b){
            sp += V16(1.0f);
            if(sp < V16(10.0f)) sp = V16(10.0f);
        }
        sp -= V16(0.3f);
        if(sp > V16(35.0f)) sp = V16(35.0f);
        if(sp <= 0){
            sp = 0;
            g_plant_ride.mode = 3;
            mario_restart_anim(ANIM_MARIO_GROWPLANTHANGDOWN);
        } else {
            g_plant_ride.coord += sp >> 8;
            rot_deg = g_plant_ride.mode == 1 ? 15 << 8 : fx_min(sp * 13 / 10, 30 << 8);
            if(g_plant_ride.mode == 1 &&
               g_plant_ride.timer >= anim_length_of(p->figure.model, ANIM_MARIO_GROWPLANTCATCH)){
                g_plant_ride.mode = 2;
                mario_restart_anim(ANIM_MARIO_GROWPLANTHANGUP);
            }
            if(g_plant_ride.mode == 2 && g_plant_ride.coord >= len){
                v3 tip = plant_dir_at(rail, len);
                int jump = actor_object_arg(a, 0);
                int32_t v = V16(jump > 0 ? jump : 30);
                mario_hold_move(rail_position_at(rail, len), p->front);

                mario_update_gravity();
                int offset = actor_object_arg(a, 1);
                plant_release(v3_add(v3_scale14(tip, v), v3_scale14(p->up, V16(offset > 0 ? offset : 0))));
                actor_set_state(a, PL_INVALID, -1);
                return;
            }
        }
    } else if(g_plant_ride.mode == 3){
        if(b && !g_plant_ride.grabbed_top){
            sp = V16(10.0f);
            g_plant_ride.mode = 2;
            mario_restart_anim(ANIM_MARIO_GROWPLANTHANGUP);
        } else {
            if(!b) g_plant_ride.grabbed_top = false;
            sp = fx_min(sp + V16(0.3f), V16(20.0f));
            g_plant_ride.coord -= sp >> 8;
            rot_deg = fx_min(sp * 13 / 10, 20 << 8);
            if(g_plant_ride.coord <= FX(10)){
                mario_hold_move(rail_position_at(rail, 0), p->front);
                plant_release(V3(0, 0, 0));
                actor_set_state(a, PL_INVALID, -1);
                return;
            }
        }
    }
    g_plant_ride.speed = sp;
    g_plant_ride.coord = fx_clamp(g_plant_ride.coord, 0, len);
    v3 up = plant_dir_at(rail, g_plant_ride.coord);
    g_plant_ride.front =
        v3_tangent14(v3_rotate_axis14(g_plant_ride.front, up, (fx)((int64_t)rot_deg * 1144 >> 16)), up,
                     v3_cross14(up, V3(N14_ONE, 0, 0)));
    g_plant_ride.up = up;
    g_plant_ride.pos = rail_position_at(rail, g_plant_ride.coord);

    p->up = up;
    mario_hold_move(g_plant_ride.pos, g_plant_ride.front);
    if(p->action == MA_DEATH) plant_release(V3(0, 0, 0));
}

static void plant_think(Actor* a){
    PlantSlot* ps = a->value >= 0 ? &g_plants[a->value] : NULL;
    if(!ps || ps->stage_index != a->stage_index) return;
    int rail = actor_rail(a);
    Player* p = game_player();
    switch(a->state){
    case PL_DEMO:
        if(a->timer == 60) audio_play_effect(SFX_PLANT_BUD, 2);
        if(a->timer >= 90) actor_set_state(a, PL_GROW, -1);
        break;
    case PL_GROW: {
        if(--a->velocity.z <= 0){
            a->velocity.y += V16(plant_rand(15, 30));
            a->velocity.z = plant_rand(5, 30);
        }
        a->velocity.x += a->velocity.y >> 8;
        a->velocity.y = a->velocity.y * 95 / 100;
        a->velocity.y = fx_clamp(a->velocity.y, V16(15.0f), V16(100.0f));
        if(a->timer == 1) audio_play_effect(SFX_PLANT_APPEAR, 2);
        audio_level_effect(SFX_PLANT_GROW);
        if(a->velocity.x >= ps->length){
            a->velocity.x = ps->length;
            if(a->stage_index >= 0)
                g_plant_grown[a->stage_index >> 3] |= (uint8_t)(1 << (a->stage_index & 7));
            actor_set_state(a, PL_WAIT, -1);
            audio_stop_level_effect(SFX_PLANT_GROW);
        }
        break;
    }
    case PL_INVALID:
        if(a->timer >= 60 || p->on_ground) actor_set_state(a, PL_WAIT, -1);
        break;
    }

    a->position = rail_position_at(rail, a->velocity.x / 2);
    if(g_plant_ride.actor == actor_index(a)){
        plant_ride_tick(a);
        return;
    }

    if(a->state != PL_WAIT && a->state != PL_GROW) return;
    if(a->state == PL_GROW && p->on_ground) return;
    if(g_plant_ride.actor >= 0 || g_mario_held || p->action == MA_STAR_LAUNCH || p->action == MA_DEATH)
        return;
    if(p->on_ground && !(g_mario.attack_timer > 0)) return;
    v3 centre = mario_centre();
    if(!actor_mario_within(a, a->velocity.x / 2 + FX(300))) return;
    uint64_t d2;
    fx c = plant_nearest_coord(rail, a->velocity.x, centre, &d2);
    if(d2 > (uint64_t)FX(140) * FX(140)) return;

    g_plant_ride.actor = (int16_t)actor_index(a);
    g_plant_ride.timer = 0;
    g_plant_ride.coord = c;
    g_plant_ride.grabbed_top = false;
    g_plant_ride.front = v3_tangent14(p->front, plant_dir_at(rail, c), V3(N14_ONE, 0, 0));
    int32_t upward = v3_dot14(g_mario.jump_vec, p->up);
    if(c >= a->velocity.x - FX(150)){
        g_plant_ride.coord = a->velocity.x - FX(150);
        g_plant_ride.speed = 0;
        g_plant_ride.mode = 3;
        g_plant_ride.grabbed_top = true;
    } else if(upward >= -V16(2.0f)){
        g_plant_ride.speed = fx_clamp(upward, V16(15.0f), V16(35.0f));
        g_plant_ride.mode = 1;
    } else {
        g_plant_ride.speed = 0;
        g_plant_ride.mode = 3;
    }
    mario_hold_begin();
    mario_restart_anim(ANIM_MARIO_GROWPLANTCATCH);
    audio_play_effect(SFX_PLANT_CATCH_SOURCE, 2);
}

static void plant_pose(const Actor* a, Figure* f){
    PlantSlot* ps = a->value >= 0 ? &g_plants[a->value] : NULL;
    if(!ps || ps->stage_index != a->stage_index) return;
    int rail = actor_rail(a);
    fx grown = a->velocity.x;
    int32_t growth = ps->length > 0 ? (int32_t)(((int64_t)grown << 14) / ps->length) : N14_ONE;
    v3 origin = a->home;
    f->position = origin;
    f->basis_front = V3(0, N14_ONE, 0);
    f->basis_up = V3(0, 0, N14_ONE);
    f->local_yaw = f->local_pitch = 0;
    const int16_t (*rest)[3] = (const int16_t (*)[3])model_rest_pose(f->model);
#define POSE_SET(i, w)                                               \
    do {                                                             \
        v3 q_ = v3_sub((w), origin);                                 \
        ps->pose[i][0] = (int16_t)fx_clamp(q_.x * 4, -32767, 32767); \
        ps->pose[i][1] = (int16_t)fx_clamp(q_.y * 4, -32767, 32767); \
        ps->pose[i][2] = (int16_t)fx_clamp(q_.z * 4, -32767, 32767); \
    } while(0)

    if(ps->pose_grown != grown){
        v3 side_ref = V3(N14_ONE, 0, 0);
        for(int r = 0; r < PLANT_RINGS; r++){
            fx c = grown * r / (PLANT_RINGS - 1);
            v3 pos = rail_position_at(rail, c);
            v3 up = plant_dir_at(rail, c);
            v3 front = v3_tangent14(V3(0, N14_ONE, 0), up, side_ref);
            v3 side = v3_cross14(front, up);
            int32_t t = N14_ONE - (N14_ONE - R14(0.45f)) * r / (PLANT_RINGS - 1);
            fx w = mul14(FX(10), t);
            POSE_SET(r * 3 + 0, v3_add(pos, v3_scale14(front, w)));
            POSE_SET(r * 3 + 1, v3_add(pos, v3_sub(v3_scale14(side, w), v3_scale14(front, w))));
            POSE_SET(r * 3 + 2, v3_sub(pos, v3_add(v3_scale14(side, w), v3_scale14(front, w))));
        }

        const int16_t (*leaf_rest)[3] =
            (const int16_t (*)[3])model_rest_pose((const FigureHeader*)figure_plantleaves);
        for(int k = 0; k < PLANT_LEAVES; k++){
            int fig = k / PLANT_LEAVES_PER_FIGURE, first = (k % PLANT_LEAVES_PER_FIGURE) * PLANT_LEAF_POINTS;
            int16_t (*out)[3] = ps->leaf_pose[fig];
            if(k >= ps->leaves || ps->leaf_coord[k] > grown){
                for(int i = 0; i < PLANT_LEAF_POINTS; i++)
                    out[first + i][0] = out[first + i][1] = out[first + i][2] = 0;
                continue;
            }
            fx c = mul14(ps->leaf_coord[k], growth);
            v3 axis = plant_dir_at(rail, c);
            int32_t t2 = mul14(growth, growth);
            v3 lup = v3_direction14(v3_add(v3_scale14(axis, N14_ONE - t2), v3_scale14(V3(0, 0, N14_ONE), t2)),
                                    axis);
            v3 lfront = v3_tangent14(ps->leaf_dir[k], lup, axis);
            v3 lside = v3_cross14(lfront, lup);
            v3 at = v3_sub(v3_add(rail_position_at(rail, c), v3_scale14(lfront, FX(10))), origin);
            int32_t scale = mul14(ps->leaf_size[k], growth);
            for(int i = 0; i < PLANT_LEAF_POINTS; i++){
                const int16_t* q = leaf_rest[first + i];
                v3 lp =
                    v3_add(v3_add(v3_scale14(lside, q[0]), v3_scale14(lfront, q[1])), v3_scale14(lup, q[2]));
                lp = v3_add(V3(at.x * 4, at.y * 4, at.z * 4), v3_scale14(lp, scale));
                out[first + i][0] = (int16_t)fx_clamp(lp.x, -32767, 32767);
                out[first + i][1] = (int16_t)fx_clamp(lp.y, -32767, 32767);
                out[first + i][2] = (int16_t)fx_clamp(lp.z, -32767, 32767);
            }
        }

        {
            v3 tip = rail_position_at(rail, grown);
            v3 up = plant_dir_at(rail, grown);
            v3 front = v3_tangent14(V3(0, N14_ONE, 0), up, side_ref);
            v3 side = v3_cross14(front, up);
            for(int i = 0; i < PLANT_TOP_POINTS; i++){
                const int16_t* q = rest[PLANT_FIRST_TOP_POINT + i];
                v3 lp = v3_add(v3_add(v3_scale14(side, q[0]), v3_scale14(front, q[1])), v3_scale14(up, q[2]));
                POSE_SET(PLANT_FIRST_TOP_POINT + i,
                         v3_add(tip, v3_scale14(V3(lp.x >> 2, lp.y >> 2, lp.z >> 2), growth)));
            }
        }
        ps->pose_grown = grown;
    }
    int leaf_figures = (ps->leaves + PLANT_LEAVES_PER_FIGURE - 1) / PLANT_LEAVES_PER_FIGURE;
    for(int k = 0; k < PLANT_LEAF_FIGURES; k++){
        Figure* lf = &ps->leaf_figure[k];
        if(!lf->model){
            figure_init(lf, figure_plantleaves);
            lf->use_basis = true;
            lf->scale = V3(64, 64, 64);
        }
        lf->hidden = k >= leaf_figures;
        lf->position = origin;
        lf->basis_front = V3(0, N14_ONE, 0);
        lf->basis_up = V3(0, 0, N14_ONE);
        lf->pose = (const int16_t (*)[3])ps->leaf_pose[k];
        lf->transform_dirty = true;
    }

    Figure* sf = &ps->seed_figure;
    if(!sf->model){
        figure_init(sf, figure_plantseed);
        sf->use_basis = true;
        sf->scale = V3(64, 64, 64);
    }
    sf->position = origin;
    sf->basis_up = a->up;
    sf->basis_front = a->front;
    {
        int anim = ANIM_PLANTSEED_WAIT;
        fx frame = 0;
        if(a->state == PL_DEMO && a->timer >= 60){
            anim = ANIM_PLANTSEED_BUD;
            frame = FX(a->timer - 60);
        } else if(a->state != PL_DEMO){
            anim = ANIM_PLANTSEED_GROWUP;
            frame = FX(1000);
        }
        sf->pose = figure_clip_pose(sf->model, anim, frame);
    }
    sf->transform_dirty = true;
    ps->drawn_tick = g_game.tick;
#undef POSE_SET
    f->pose = (const int16_t (*)[3])ps->pose;
}

static void plant_extra_figures(void){
    Game* game = &g_game;
    for(int i = 0; i < PLANT_SLOTS; i++){
        PlantSlot* ps = &g_plants[i];
        if(ps->stage_index < 0 || ps->drawn_tick != g_game.tick) continue;
        Figure* extra[PLANT_LEAF_FIGURES + 1] = {&ps->seed_figure};
        for(int k = 0; k < PLANT_LEAF_FIGURES; k++) extra[k + 1] = &ps->leaf_figure[k];
        for(int k = 0; k <= PLANT_LEAF_FIGURES && game->figure_count < MAX_FIGURES; k++){
            Figure* f = extra[k];
            if(!f->model || f->hidden) continue;
            figure_update_transform(f);
            game->figures[game->figure_count++] = f;
        }
    }
}

static void plant_ride_mario_pose(Figure* figure){
    if(g_plant_ride.actor < 0 || !g_mario_held) return;
    figure->basis_up = g_plant_ride.up;
    figure->basis_front = g_plant_ride.front;
    figure->transform_dirty = true;
}

static int plant_draw_cost(const Actor* a){
    return PLANT_LEAF_FIGURES * (16 + ((const FigureHeader*)figure_plantleaves)->triangle_count) + 16 +
           ((const FigureHeader*)figure_plantseed)->triangle_count;
}
