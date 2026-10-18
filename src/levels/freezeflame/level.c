static bool freezeflame_actor_init(Actor* a, const StageObject* o){
    return volcano_part_init(a, o) || valve_init(a, o) || valve_doors_init(a, o);
}

static bool freezeflame_actor_think(Actor* a){ return a->type == OBJ_GROUPSWITCHWATCHER; }

static void freezeflame_switches_tick(void){
    group_switch_actors_tick();
    timer_switches_tick();
}

static int freezeflame_actor_unclipped(const Actor* a){
    return a->type == OBJ_GROUPSWITCHWATCHER ? UNCLIPPED_YES : UNCLIPPED_DEFAULT;
}

static bool freezeflame_actor_bounds(const Actor* a, v3* centre, fx* extent){
    if(!valve_type(a->type)) return false;
    valve_bound_sphere(a, centre, extent);
    return true;
}

static const uint16_t freezeflame_timer_switch_ids[] = {1196, 1197, 1198};

static const ScenarioTail freezeflame_timer_switches = {
    .first = 441,
    .count = 3,
    .type = OBJ_TIMERSWITCH,
    .scenarios = 0x0C,
    .zone = 5,
    .ids = freezeflame_timer_switch_ids,
    .ids_step = 1,
};

static int freezeflame_stream_rule(const StageObject* o){
    return valve_stream_required(o) || volcano_part_stream_required(o) || o->type == OBJ_TIMERSWITCH ||
                   o->type == OBJ_GROUPSWITCHWATCHER
               ? STREAM_REQUIRED
               : STREAM_DEFAULT;
}

static bool freezeflame_stream_keep(const Actor* a){
    return valve_stream_keep(a) || candlestand_stream_keep(a);
}

static const LevelHooks freezeflame_level = {
    .galaxy_stage = STAGE_FREEZEFLAME,
    .hidden_mission = 6,
    .hidden_scenario = 1,
    .features = LEVEL_ICE_STEPS | LEVEL_VOLCANO_DEMOS | LEVEL_VALVE_DEMOS,
    .scenario_tail = &freezeflame_timer_switches,
    .after_map_parts = freezeflame_switches_tick,
    .before_actors = parts_collision_pre,
    .after_actors = parts_collision_post,
    .player_request = valve_player_request,
    .actor_init = freezeflame_actor_init,
    .actor_think = freezeflame_actor_think,
    .actor_unclipped = freezeflame_actor_unclipped,
    .actor_bounds = freezeflame_actor_bounds,
    .stream_rule = freezeflame_stream_rule,
    .stream_keep = freezeflame_stream_keep,
};
