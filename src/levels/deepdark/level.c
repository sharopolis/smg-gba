static int8_t g_deep_bgm_cube = -1;

static int deep_music(void){
    g_deep_bgm_cube = -1;
    return g_game.scenario_loaded == 4 ? 255 : MUSIC_MBGM_GALAXY_INTER;
}

static void deep_bgm_tick(const KeyState* keys){
    if(g_game.scenario_loaded == 4) return;
    const Player* p = &g_game.player;
    if(p->action == MA_DEATH || p->action == MA_STAR_GET) return;
    int inside = -1;
    for(unsigned i = 0; i < sizeof deep_bgm_cubes / sizeof *deep_bgm_cubes; i++){
        const DeepBgmCube* c = &deep_bgm_cubes[i];
        if(!(c->scenarios >> (g_game.scenario - 1) & 1)) continue;
        v3 d = v3_sub(p->figure.position, V3(c->base[0], c->base[1], c->base[2]));
        int32_t x = v3_dot14(d, V3(c->axis[0][0], c->axis[0][1], c->axis[0][2]));
        int32_t y = v3_dot14(d, V3(c->axis[1][0], c->axis[1][1], c->axis[1][2]));
        int32_t z = v3_dot14(d, V3(c->axis[2][0], c->axis[2][1], c->axis[2][2]));
        if(x >= -c->half_x && x < c->half_x && y >= 0 && y < c->height && z >= -c->half_z && z < c->half_z){
            inside = (int)i;
            break;
        }
    }
    if(inside == g_deep_bgm_cube) return;
    g_deep_bgm_cube = (int8_t)inside;
    if(inside < 0) return;
    switch(deep_bgm_cubes[inside].bgm){
    case 1:
        if(g_audio.current_music >= 0) audio_play_music(MUSIC_MBGM_GALAXY_05);
        break;
    case 2:
        if(g_audio.current_music >= 0) audio_play_music(MUSIC_MBGM_GALAXY_INTER);
        break;
    default: break;
    }
}

static void deep_camera_arena_clear(CollisionCandidates* candidates){
    int n = 0;
    for(int i = 0; i < candidates->count; i++)
        if(!kamella_camera_transparent_triangle(candidates->index[i]))
            candidates->index[n++] = candidates->index[i];
    candidates->count = n;
}

static bool deep_power_star(Actor* a){
    purple_coin_star_wait_tick(a);
    return purple_coin_star_busy(a);
}

static int deep_actor_unclipped(const Actor* a){
    if(purple_coin_star_busy(a)) return UNCLIPPED_YES;
    if(a->type >= OBJ_AIRBUBBLE) return deep_kind_unclipped(a) ? UNCLIPPED_YES : UNCLIPPED_NO;
    return UNCLIPPED_DEFAULT;
}

static bool deep_fixed_yaw_type(int type){
    return type == OBJ_WATERBAZOOKA || type == OBJ_MOGUCCHISHOOTER || type == OBJ_WBCAPSULE ||
           type == OBJ_MARBLEPLANET || type == OBJ_MARBLECORE || type == OBJ_MARBLEELECTRON ||
           type == OBJ_ITEMAPPEARSTONE;
}

static void deep_actor_figure(const Actor* a, Figure* f){
    if(purple_coin_star_actor(a)) f->local_yaw = purple_coin_star_yaw(a);
    if(deep_fixed_yaw_type(a->type)) f->local_yaw = 0;
}

static int deep_stream_rule(const StageObject* o){
    return o->type == OBJ_GROUPSWITCHWATCHER ? STREAM_SKIP : STREAM_DEFAULT;
}

static const ScenarioTail deep_purple_rail = {
    .first = DEEP_PURPLE_RAIL_FIRST,
    .count = DEEP_PURPLE_RAIL_COUNT,
    .type = OBJ_PURPLECOIN,
    .scenarios = DEEP_PURPLE_RAIL_MASK,
    .zone = 3,
    .ids = deep_purple_rail_groups,
    .ids_step = DEEP_PURPLE_RAIL_COUNT / (sizeof deep_purple_rail_groups / sizeof *deep_purple_rail_groups),
};

static const LevelHooks deep_level = {
    .galaxy_stage = STAGE_DEEPDARK,
    .hidden_mission = 6,
    .hidden_scenario = 1,
    .daredevil_mission = 4,
    .features = LEVEL_FOLLOW_CAMERA | LEVEL_ICE_STEPS,
    .scenario_tail = &deep_purple_rail,
    .music = deep_music,
    .before_player = deep_bgm_tick,
    .before_stream = group_switch_objects_tick,
    .before_actors = deep_actors_pre,
    .camera_arena = kamella_camera_points,
    .camera_arena_clear = deep_camera_arena_clear,
    .actor_init = deep_actor_init,
    .actor_spawned = purple_coin_star_init,
    .actor_think = deep_actor_think,
    .actor_unclipped = deep_actor_unclipped,
    .actor_figure = deep_actor_figure,
    .power_star = deep_power_star,
    .stream_rule = deep_stream_rule,
};
