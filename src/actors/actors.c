static void (*const actor_module_resets[])(void) = {
    icestep_stage_reset, kamella_reset, boss_demo_reset, pole_square_reset, iceking_reset,
    warppod_reset, gcapture_reset, mario_link_reset, fluff_reset,
};

static void (*const actor_module_starts[])(void) = {
    parts_update, chip_reset, earthenpipe_reset, plant_reset, missions_stage_start,
};

static void actor_modules_run(void (*const* steps)(void), unsigned count){
    for(unsigned i = 0; i < count; i++) steps[i]();
}

static void actor_scale_from_object(Actor* a, const StageObject* o){
    int scale = OBJECT_SCALE_PERCENT(o);
    if(scale != 100){
        fx s = 64 * scale / 100;
        a->figure.scale = V3(s, s, s);
        a->radius = a->radius * scale / 100;
    }
}

static void actor_blow_away(Actor* v, Actor* by){
    const ActorKind* kind = &actor_kinds[v->type];
    if(kind->blown_away) kind->blown_away(v, by);
}

static void actor_init_by_kind(Actor* a, const StageObject* o){
    const ActorKind* kind = &actor_kinds[a->type];
    if(!(kind->flags & KIND_OWN_SCALE)) actor_scale_from_object(a, o);
    if(kind->init) kind->init(a, o);
}

static Actor* actors_spawn_object(const Stage* stage, uint32_t i){
    const StageObject* o = &stage->objects[i];
    Actor* a = actor_spawn(o->type, V3(o->position[0], o->position[1], o->position[2]), v3_from_i16(o->up),
                           v3_from_i16(o->front));
    if(!a) return NULL;
    a->stage_index = (int16_t)i;
    a->target = V3(o->target[0], o->target[1], o->target[2]);
    for(int k = 0; k < 4; k++) a->args[k] = o->args[k];
    a->flags = o->flags;
    if(!level_actor_init(a, o)) actor_init_by_kind(a, o);
    const ActorKind* kind = &actor_kinds[a->type];
    if(kind->spawned) kind->spawned(a, o);
    level_actor_spawned(a, o);
    return a;
}

static void actors_load(const Stage* stage, int scenario){
    Game* game = &g_game;
    game->actor_count = 0;
    actor_modules_run(actor_module_resets, sizeof actor_module_resets / sizeof actor_module_resets[0]);
    levels_reset();
    level_load_before_actors();
    actor_modules_run(actor_module_starts, sizeof actor_module_starts / sizeof actor_module_starts[0]);

    if(actors_stream_start(stage, scenario)) return;
    uint8_t mask = scenario > 0 ? (uint8_t)(1 << (scenario - 1)) : 0x01;
    for(uint32_t i = 0; i < stage->header->object_count; i++){
        const StageObject* o = &stage->objects[i];
        if(o->scenarios != 0xFF && !(o->scenarios & mask)) continue;
        if(!actors_spawn_object(stage, i)) break;
    }
}

static bool actor_resting(const Actor* a, const ActorKind* kind){
    if(!(kind->flags & KIND_RESTS) || (kind->idle && !kind->idle(a))) return false;
    if(((g_game.tick + (uint32_t)actor_index(a)) & 7) == 0) return false;
    v3 d = actor_to_mario(a);
    return fx_abs(d.x) > FX(1500) || fx_abs(d.y) > FX(1500) || fx_abs(d.z) > FX(1500);
}

static void actor_think(Actor* a){
    a->timer++;
    if(a->state == AS_DEFEATED){
        if(a->timer > 20) collectible_dead(a);
        return;
    }
    if(pointer_actor_hook(a)) return;
    const ActorKind* kind = &actor_kinds[a->type];
    if(!(kind->flags & KIND_OWN_THINK) && (level_actor_think(a) || actor_resting(a, kind))) return;
    if(kind->think) kind->think(a);
}

static void actor_animate(Actor* a){
    const ActorKind* kind = &actor_kinds[a->type];
    if(kind->animate) kind->animate(a);
    else a->anim_frame += FX_ONE;
}

static const fx actor_spin_rates[4] = {0, FXC(0.13963f), FXC(0.2618f), FXC(0.05236f)};

static inline bool plantmember_idle(const Actor* a, v3 d){
    if(a->type != OBJ_PLANTMEMBER || a->state != PM_WAIT) return false;

    fx reach = fx_max(FX(300), a->radius + C_BODY_RADIUS + FX(50) + actor_kinds[OBJ_PLANTMEMBER].height +
                                   C_BODY_HEIGHT + 2);
    return fx_abs(d.x) > reach || fx_abs(d.y) > reach || fx_abs(d.z) > reach;
}

__attribute__((noinline)) static bool actor_unclipped_by_state(const Actor* a){
    return penguin_unclipped(a) || a->type == OBJ_ICESTEP || collectible_is_pop(a) ||
           a->type == OBJ_FIREBALL || a->type == OBJ_COCONUTBALL ||
           (a->type == OBJ_TICOFAT && a->state >= TICOFAT_FULL) ||
           (a->type == OBJ_GCAPTURETARGET && g_mario_held && actor_index(a) == g_gcapture_held) ||
           (a->type == OBJ_STARBIT && (a->flags & (PTR_SHOT | PTR_HOMING)));
}

__attribute__((noinline)) static bool actor_unclipped_ice_kinds(const Actor* a){
    return waterleak_unclipped(a) || iceking_unclipped(a) || a->type == OBJ_ICEKINGWEAPON;
}
_Static_assert(OBJ_WATERLEAKICE - OBJ_ICEKING == 12, "IceKing to WaterLeakIce must span 12 types");

enum { UNCLIPPED_BY_STATE, UNCLIPPED_ALWAYS, UNCLIPPED_WHILE_BREAKING, UNCLIPPED_LAUNCH, UNCLIPPED_PARTS };

static const uint8_t actor_unclipped_reason[OBJ_TYPE_COUNT] = {
    [OBJ_HOMINGKILLER] = UNCLIPPED_ALWAYS,
    [OBJ_KILLER] = UNCLIPPED_ALWAYS,
    [OBJ_CRYSTALCAGEM] = UNCLIPPED_WHILE_BREAKING,
    [OBJ_TIMERSWITCH] = UNCLIPPED_ALWAYS,
    [OBJ_ROCK] = UNCLIPPED_ALWAYS,
    [OBJ_WANWAN] = UNCLIPPED_ALWAYS,
    [OBJ_LAUNCHSTAR] = UNCLIPPED_LAUNCH,
    [OBJ_SLINGSTAR] = UNCLIPPED_LAUNCH,
    [OBJ_DEMOGROUP] = UNCLIPPED_PARTS,
    [OBJ_MARIOFIREBALL] = UNCLIPPED_PARTS,
    [OBJ_CANDLESTANDICEVOLCANO] = UNCLIPPED_PARTS,
    [OBJ_ANMMODELSWITCHMOVE] = UNCLIPPED_PARTS,
    [OBJ_SIMPLESEESAWOBJ] = UNCLIPPED_PARTS,
    [OBJ_LAVAFLOATER] = UNCLIPPED_PARTS,
    [OBJ_RAILMOVEOBJ] = UNCLIPPED_PARTS,
    [OBJ_VALVESWITCH] = UNCLIPPED_PARTS,
    [OBJ_ICEUPDOWNSHUTTERDOORA] = UNCLIPPED_PARTS,
    [OBJ_SHUTTERDOORC] = UNCLIPPED_PARTS,
    [OBJ_ICEWATERUPDOWNPARTS] = UNCLIPPED_PARTS,
    [OBJ_LAVAUPDOWNPARTS] = UNCLIPPED_PARTS,
};

__attribute__((noinline)) static bool actor_launch_unclipped(const Actor* a){
    return a->state == STAR_BOUND || a->state == STAR_COOLDOWN || actor_unclipped_by_state(a) ||
           flight_resident_wait_clock(a);
}
__attribute__((always_inline)) static inline bool actor_unclipped(const Actor* a){
    int level = level_actor_unclipped(a);
    if(level != UNCLIPPED_DEFAULT) return level == UNCLIPPED_YES;
    if((unsigned)(a->type - OBJ_ICEKING) <= (unsigned)(OBJ_WATERLEAKICE - OBJ_ICEKING))
        return actor_unclipped_ice_kinds(a);
    unsigned reason = actor_unclipped_reason[a->type];
    if(reason != UNCLIPPED_BY_STATE){
        if(reason == UNCLIPPED_PARTS)
            return volcano_part_unclipped(a) || valve_unclipped(a) || actor_unclipped_by_state(a);
        if(reason == UNCLIPPED_LAUNCH) return actor_launch_unclipped(a);
        if(reason == UNCLIPPED_ALWAYS || a->state == CAGE_MEDIUM_BREAK) return true;
    }
    return actor_unclipped_by_state(a);
}

#define ACTOR_HALF_RATE_RANGE FX(2400)
static EWRAM_BSS uint32_t g_actor_drawn[(MAX_ACTORS + 31) / 32];
static const uint8_t actor_half_rate_kind[OBJ_TYPE_COUNT] = {
    [OBJ_KURIBO] = 1,        [OBJ_KURIBOMINI] = 1, [OBJ_TAKOBO] = 1,  [OBJ_PACKUNPETIT] = 1,
    [OBJ_STINKBUGSMALL] = 1, [OBJ_KARIPON] = 1,    [OBJ_KOTEBUG] = 1, [OBJ_BUTTERFLY] = 1,
    [OBJ_COIN] = 1,          [OBJ_PURPLECOIN] = 1, [OBJ_STARBIT] = 1, [OBJ_ONEUP] = 1,
    [OBJ_LIFEUP] = 1,        [OBJ_TICO] = 1,       [OBJ_KINOPIO] = 1, [OBJ_ROSETTA] = 1,
    [OBJ_BUTLER] = 1,        [OBJ_HONEYBEE] = 1,
};
static bool actors_half_rate_allowed(void){
    const Player* p = &g_game.player;
    return !g_mario_held && p->action != MA_STAR_LAUNCH && p->action != MA_TALK && p->action != MA_DEATH;
}

static bool actor_half_rate_skip(const Actor* a, int i, v3 d){
    if(a->timer < 32 || (a->timer & 1)) return false;
    if(!actor_half_rate_kind[a->type]) return false;
    fx m = fx_max(fx_max(fx_abs(d.x), fx_abs(d.y)), fx_abs(d.z));
    if(m <= ACTOR_HALF_RATE_RANGE || m > ACTOR_THINK_RANGE) return false;
    if(g_actor_drawn[i >> 5] >> (i & 31) & 1) return false;
    if(a->state == AS_DEFEATED) return false;
    return !actor_unclipped(a);
}

static void actors_world_tick(void){
    warppod_tick();
    parts_update();
    earthenpipe_global_tick();
    level_after_map_parts();
    missions_tick();
    actors_stream_tick();
}

static void actors_tick(void){
    Game* game = &g_game;
    v3 mario = game->player.figure.position;
    actors_world_tick();
    level_before_actors();
    Actor* appearance_demo = cutscene_actor();
    bool half_rate = !appearance_demo && actors_half_rate_allowed();
    for(int i = 0; i < game->actor_count; i++){
        Actor* a = &game->actors[i];
        if(!a->active) continue;
        if(appearance_demo && a != appearance_demo && !cutscene_cast(appearance_demo, a)) continue;
        if((g_debug_options & DEBUG_FREEZE_ENEMIES) && (actor_kinds[a->type].flags & KIND_ENEMY)) continue;
        v3 d = v3_sub(a->position, mario);

        if(a != appearance_demo &&
           (fx_abs(d.x) > ACTOR_THINK_RANGE || fx_abs(d.y) > ACTOR_THINK_RANGE ||
            fx_abs(d.z) > ACTOR_THINK_RANGE) &&
           !actor_unclipped(a))
            continue;
        if(half_rate && actor_half_rate_skip(a, i, d)) a->timer++;
        else if(plantmember_idle(a, d)) a->timer++;
        else actor_think(a);
        actor_animate(a);
    }
    level_after_actors();
}

static const uint8_t actor_draw_low[OBJ_TYPE_COUNT] = {
    [OBJ_COIN] = 1,           [OBJ_STARBIT] = 1,         [OBJ_PURPLECOIN] = 1,       [OBJ_COINGROUP] = 1,
    [OBJ_FLOWERGROUP] = 1,    [OBJ_FLOWERBLUEGROUP] = 1, [OBJ_CUTBUSHGROUP] = 1,     [OBJ_PLANTMEMBER] = 1,
    [OBJ_BUTTERFLY] = 1,      [OBJ_SMALLSTONE] = 1,      [OBJ_SMALLSTONEMEMBER] = 1, [OBJ_STARPIECESPOT] = 1,
    [OBJ_STARPIECEGROUP] = 1, [OBJ_TREASURESPOT] = 1,    [OBJ_COINFLOWER] = 1,
};
#define ACTOR_DRAW_MARIO_NEAR  FX(1000)
#define ACTOR_DRAW_PICKUP_NEAR FX(500)
#define ACTOR_DRAW_TIER        (1 << 20)

static int actor_draw_cost(const Actor* a, fx z, int lod_k){
    const FigureHeader* m = a->figure.model;
    fx height = fx_mul((fx)m->extent, a->figure.scale.z);
    int tris =
        m->lod_triangle_count && lod_k && z > height * lod_k ? m->lod_triangle_count : m->triangle_count;
    int r = fx_min(386 * fx_abs(height) / fx_max(z, 256), 64);
    tris += r * r >> 4;
    const ActorKind* kind = &actor_kinds[a->type];
    if(kind->draw_cost) tris += kind->draw_cost(a);
    return 16 + tris;
}

static void actors_collect_figures(const CameraView* view){
    Game* game = &g_game;
    v3 cam = game->camera.position;
    v3 mario = game->player.figure.position;
    int near_idx[VIEW_MAX_ACTORS];
    int32_t near_d[VIEW_MAX_ACTORS];
    fx near_z[VIEW_MAX_ACTORS];
    int n_near = 0;
    const ViewMode* mode = view_mode();
    const int base_drawn = mode->actors;
    const int max_drawn = fx_min(base_drawn + mode->actors_extra, VIEW_MAX_ACTORS);
    const fx range = mode->actor_range;
    for(int i = 0; i < game->actor_count; i++){
        Actor* a = &game->actors[i];
        if(!a->active || !a->figure.model || (a->flags & ACTOR_HIDDEN)) continue;
        const ActorKind* kind = &actor_kinds[a->type];
        if(kind->hidden && kind->hidden(a)) continue;
        v3 draw_centre = a->position;
        fx level_extent = 0;
        bool level_bounds = level_actor_bounds(a, &draw_centre, &level_extent);
        fx map_radius = level_figure_margin(&a->figure);

        if(!kind->draw_extent && !level_bounds && !map_radius){
            v3 d0 = v3_sub(a->position, cam);
            if(fx_abs(d0.x) > range || fx_abs(d0.y) > range || fx_abs(d0.z) > range) continue;
        }
        v3 d = v3_sub(draw_centre, cam);

        fx extent = level_bounds ? level_extent : kind->draw_extent ? kind->draw_extent(a) : FX(200);
        bool near_first = false;
        v3 mario_delta = V3(0, 0, 0);
        if(kind->flags & KIND_NEAR_FIRST){
            mario_delta = v3_sub(a->position, game->player.figure.position);
            near_first = v3_length_squared64(mario_delta) <= (uint64_t)FX(1200) * FX(1200);
        }

        fx draw_range = range + (level_bounds || (kind->flags & KIND_EXTENT_IN_RANGE) ? extent : map_radius);
        if(fx_abs(d.x) > draw_range || fx_abs(d.y) > draw_range || fx_abs(d.z) > draw_range) continue;

        if(v3_dot14(d, game->camera.front) < -extent - map_radius) continue;

        v3 vp = xform_point(&view->to_screen, draw_centre);
        {
            const FigureHeader* fm = a->figure.model;
            fx r = fx_max(extent, (fx)(((int64_t)fm->extent * fx_abs(a->figure.scale.x) * 7) >> 10)) +
                   map_radius + FX(100);
            if(fx_abs(vp.x) - vp.z > r * 8 || fx_abs(vp.y) - vp.z > r * 11 ||
               vp.z - r * 4 > mode->figure_depth)
                continue;
        }
        int32_t dist =
            near_first ? -3 * range + fx_abs(mario_delta.x) + fx_abs(mario_delta.y) + fx_abs(mario_delta.z)
                       : fx_abs(d.x) + fx_abs(d.y) + fx_abs(d.z);
        {
            v3 m = v3_sub(a->position, mario);
            bool near_mario =
                near_first || (kind->flags & KIND_ALWAYS_NEAR) ||
                (fx_abs(m.x) <= ACTOR_DRAW_MARIO_NEAR && fx_abs(m.y) <= ACTOR_DRAW_MARIO_NEAR &&
                 fx_abs(m.z) <= ACTOR_DRAW_MARIO_NEAR &&
                 v3_length_squared64(m) <= (uint64_t)ACTOR_DRAW_MARIO_NEAR * ACTOR_DRAW_MARIO_NEAR);
            if(actor_draw_low[a->type]){
                if(!near_mario ||
                   v3_length_squared64(m) > (uint64_t)ACTOR_DRAW_PICKUP_NEAR * ACTOR_DRAW_PICKUP_NEAR)
                    dist += 2 * ACTOR_DRAW_TIER;
                else dist += ACTOR_DRAW_TIER;
            } else if(!near_mario) dist += ACTOR_DRAW_TIER;
        }
        int k;
        if(n_near < VIEW_MAX_ACTORS){
            k = n_near++;
        } else {
            if(near_d[VIEW_MAX_ACTORS - 1] <= dist) continue;
            k = VIEW_MAX_ACTORS - 1;
        }
        while(k > 0 && near_d[k - 1] > dist){
            near_d[k] = near_d[k - 1];
            near_idx[k] = near_idx[k - 1];
            near_z[k] = near_z[k - 1];
            k--;
        }
        near_d[k] = dist;
        near_idx[k] = i;
        near_z[k] = vp.z;
    }
    for(int w = 0; w < (MAX_ACTORS + 31) / 32; w++) g_actor_drawn[w] = g_actor_far_lod[w] = 0;
    int drawn = 0, units = 0;
    for(int n = 0; n < n_near && game->figure_count < MAX_FIGURES; n++){
        int i = near_idx[n];
        if(near_d[n] < ACTOR_DRAW_TIER){
            units += actor_draw_cost(&game->actors[i], near_z[n], mode->figure_lod);
        } else {
            bool low = near_d[n] >= 2 * ACTOR_DRAW_TIER, extra = drawn >= base_drawn;
            if(drawn >= max_drawn || (extra && low)) continue;
            int cost = actor_draw_cost(&game->actors[i], near_z[n],
                                       extra ? mode->figure_lod >> 1 : mode->figure_lod);
            if((low || extra) && mode->actor_budget && units + cost > mode->actor_budget) continue;
            units += cost;
            if(extra) g_actor_far_lod[i >> 5] |= 1u << (i & 31);
        }
        drawn++;
        g_actor_drawn[i >> 5] |= 1u << (i & 31);
        Actor* a = &game->actors[i];
        Figure* f = &a->figure;
        f->position = a->position;
        f->basis_up = a->up;
        f->basis_front = a->front;
        f->local_pitch = 0;
        const ActorKind* kind = &actor_kinds[a->type];
        if(kind->adjust_figure) kind->adjust_figure(a, f);
        if(a->state == AS_DEFEATED){
            fx s = fx_max(kind->scale - a->timer * 3, 4);
            f->scale = V3(s, s, s / 2);
        }
        const FigureHeader* model = a->figure.model;
        if(kind->pose_figure) kind->pose_figure(a, f);
        else if(model->clip_count) f->pose = figure_clip_pose(model, a->anim, a->anim_frame);

        f->local_yaw = kind->spin ? (fx)((game->tick * actor_spin_rates[kind->spin] +
                                          (kind->flags & KIND_SPIN_SYNCED ? 0 : i * 97)) %
                                         FX_TAU)
                                  : 0;
        level_actor_figure(a, f);
        if(kind->flags & KIND_YAW_FROM_VALUE) f->local_yaw = a->value;
        if(kind->flags & KIND_YAW_FROM_VELOCITY) f->local_yaw = a->velocity.x;
        if(kind->orient_figure) kind->orient_figure(a, f);
        f->transform_dirty = true;
        f->has_shadow = false;
        figure_update_transform(f);
        if(kind->prepare_draw) kind->prepare_draw(a, f);
        game->figures[game->figure_count++] = f;
    }
    plant_extra_figures();
}
