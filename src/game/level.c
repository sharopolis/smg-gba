static const LevelHooks* const level_table[STAGE_COUNT] = {
    [STAGE_OBSERVATORY] = &observatory_level, [STAGE_HONEYHIVE] = &honey_level,
    [STAGE_GOODEGG] = &goodegg_level,         [STAGE_HONEYHIVE_TOWER] = &honey_level,
    [STAGE_BEACHBOWL] = &beachbowl_level,     [STAGE_FREEZEFLAME] = &freezeflame_level,
    [STAGE_BUOYBASE] = &buoy_level,           [STAGE_DEEPDARK] = &deep_level,
    [STAGE_GUSTYGARDEN] = &gusty_level,       [STAGE_ASTRODOME] = &astro_dome_level,
};

static const LevelHooks level_none = {.galaxy_stage = -1};

static const LevelHooks* level_hooks(int stage){
    return (unsigned)stage < STAGE_COUNT ? level_table[stage] : &level_none;
}

static inline const LevelHooks* level_current(void){ return g_level; }

static inline bool level_has(unsigned feature){ return g_level->features & feature; }

static void level_enter(int stage){ g_level = level_hooks(stage); }

static void levels_reset(void){
    for(int i = 0; i < STAGE_COUNT; i++){
        const LevelHooks* level = level_table[i];
        if(level->galaxy_stage == i && level->reset) level->reset();
    }
}

static int level_mission_stage(int stage, int mission){
    const LevelHooks* level = level_hooks(stage);
    return level->mission_stage ? level->mission_stage(stage, mission) : stage;
}

static int level_pipe_music(int pipe){
    for(const PipeMusic* m = g_level->pipe_music; m && m->pipe >= 0; m++)
        if(m->pipe == pipe) return m->music;
    return -1;
}

static uint32_t level_stream_count(const Stage* stage){
    uint32_t count = stage->header->object_count;
    const ScenarioTail* tail = g_level->scenario_tail;
    if(!tail || (g_stream.scenario_bit & tail->scenarios) || count != (uint32_t)tail->first + tail->count)
        return count;
    for(unsigned k = 0; k < tail->count; k++){
        const StageObject* o = &stage->objects[tail->first + k];
        if(o->type != tail->type || o->scenarios != tail->scenarios || o->zone != tail->zone ||
           o->id != tail->ids[k / tail->ids_step])
            return count;
    }
    return tail->first;
}

static void level_load_before_player(void){
    if(g_level->load_before_player) g_level->load_before_player();
}

static void level_load_before_actors(void){
    if(g_level->load_before_actors) g_level->load_before_actors();
}

static void level_before_player(const KeyState* keys){
    if(g_level->before_player) g_level->before_player(keys);
}

static void level_after_parts_update(void){
    if(g_level->after_parts_update) g_level->after_parts_update();
}

static void level_after_map_parts(void){
    if(g_level->after_map_parts) g_level->after_map_parts();
}

static void level_before_stream(void){
    if(g_level->before_stream) g_level->before_stream();
}

static void level_before_actors(void){
    if(g_level->before_actors) g_level->before_actors();
}

static void level_after_actors(void){
    if(g_level->after_actors) g_level->after_actors();
}

static void level_after_camera(void){
    if(g_level->after_camera) g_level->after_camera();
}

static void level_after_actor_cameras(void){
    if(g_level->after_actor_cameras) g_level->after_actor_cameras();
}

static void level_after_player_pose(void){
    if(g_level->after_player_pose) g_level->after_player_pose();
}

static void level_miss(void){
    if(g_level->miss) g_level->miss();
}

static void level_star_get(void){
    if(g_level->star_get) g_level->star_get();
}

static bool level_pull_back(v3 position){ return g_level->pull_back && g_level->pull_back(position); }

static bool level_pull_preview(void){ return g_level->pull_preview && g_level->pull_preview(); }

static bool level_pull_target(const Actor* a){ return g_level->pull_target && g_level->pull_target(a); }

static bool level_camera_hold_height(void){
    return g_level->camera_hold_height && g_level->camera_hold_height();
}

static bool level_camera_blend_below(void){
    return g_level->camera_blend_below && g_level->camera_blend_below();
}

static bool level_camera_low_anchor(const StageCamera* cp, int source, int chunk){
    return g_level->camera_low_anchor && g_level->camera_low_anchor(cp, source, chunk);
}

static bool level_camera_arena(v3* points, int* count, v3* up, v3* front){
    return g_level->camera_arena && g_level->camera_arena(points, count, up, front);
}

static void level_camera_arena_clear(CollisionCandidates* candidates){
    if(g_level->camera_arena_clear) g_level->camera_arena_clear(candidates);
}

static bool level_camera_blend_checked(int old_chunk, int new_chunk){
    return g_level->camera_blend_checked && g_level->camera_blend_checked(old_chunk, new_chunk);
}

static bool level_camera_path_blocked(v3 from, v3 to){
    return g_level->camera_path_blocked && g_level->camera_path_blocked(from, to);
}

static bool level_player_request(const KeyState* keys){
    return g_level->player_request && g_level->player_request(keys);
}

static bool level_actor_init(Actor* a, const StageObject* o){
    return g_level->actor_init && g_level->actor_init(a, o);
}

static void level_actor_spawned(Actor* a, const StageObject* o){
    if(g_level->actor_spawned) g_level->actor_spawned(a, o);
}

static bool level_actor_think(Actor* a){ return g_level->actor_think && g_level->actor_think(a); }

static int level_actor_unclipped(const Actor* a){
    return g_level->actor_unclipped ? g_level->actor_unclipped(a) : UNCLIPPED_DEFAULT;
}

static bool level_actor_bounds(const Actor* a, v3* centre, fx* extent){
    return g_level->actor_bounds && g_level->actor_bounds(a, centre, extent);
}

static void level_actor_figure(const Actor* a, Figure* f){
    if(g_level->actor_figure) g_level->actor_figure(a, f);
}

static fx level_figure_margin(const Figure* figure){
    return g_level->figure_margin ? g_level->figure_margin(figure) : 0;
}

static bool level_power_star(Actor* a){ return g_level->power_star && g_level->power_star(a); }

static void level_star_appear(int id, v3 from){
    if(g_level->star_appear) g_level->star_appear(id, from);
}

static StreamRule level_stream_rule(void){ return g_level->stream_rule; }

static bool level_stream_important(int type){
    return g_level->stream_important && g_level->stream_important(type);
}

static ActorTest level_stream_keep(void){ return g_level->stream_keep; }

static void level_add_figures(void){
    if(g_level->add_figures) g_level->add_figures();
}

static bool level_player_figure(Figure* figure){
    return g_level->player_figure && g_level->player_figure(figure);
}

static void level_draw_hud(void){
    if(g_level->draw_hud) g_level->draw_hud();
}
