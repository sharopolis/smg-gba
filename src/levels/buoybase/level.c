static void buoy_actor_spawned(Actor* a, const StageObject* o){
    if(o->type == OBJ_POWERSTAR && o->id == 111 && o->args[0] == 2 &&
       !game_star_collected(g_game.stage_index, 2))
        a->figure.model = (const FigureHeader*)figure_powerstargreen;
    honey_actor_spawned(a, o);
    buoy_cage_star_init(a, o);
}

static bool buoy_power_star(Actor* a){
    bool busy = honey_star_busy(a) || buoy_cage_star_busy(a);
    if(buoy_cage_star_owner(a)) buoy_cage_star_tick(a);
    return busy;
}

static int buoy_actor_unclipped(const Actor* a){
    return metbo_unclipped(a) || jumpguarder_unclipped(a) || buoy_floater_unclipped(a) ||
                   buoy_bubble_unclipped(a) || buoy_cage_star_busy(a) || a->type == OBJ_KILLER ||
                   a->type == OBJ_HOMINGKILLER || a->type == OBJ_BUOYTORPEDO ||
                   a->type == OBJ_BUOYTORPEDOLAUNCHER
               ? UNCLIPPED_YES
               : UNCLIPPED_DEFAULT;
}

static void buoy_actor_figure(const Actor* a, Figure* f){
    if(buoy_no_yaw_kind(a->type)) f->local_yaw = 0;
    if(buoy_cage_star_owner(a)) f->local_yaw = a->velocity.x;
}

static const PipeMusic buoy_pipe_music[] = {{0, MUSIC_BUOYBASE}, {-1, 0}};

static const LevelHooks buoy_level = {
    .galaxy_stage = STAGE_BUOYBASE,
    .hidden_mission = 2,
    .hidden_scenario = 1,
    .features = LEVEL_OCEAN_BOWL | LEVEL_CHIP_GROUPS_FROM_STAGE,
    .pipe_music = buoy_pipe_music,
    .reset = buoy_reset,
    .after_parts_update = buoy_parts_hidden,
    .before_actors = buoy_actors_pre,
    .after_actors = buoy_actors_post,
    .camera_blend_checked = buoy_rise_camera_switch,
    .camera_path_blocked = buoy_camera_path_blocked,
    .actor_init = buoy_actor_init,
    .actor_spawned = buoy_actor_spawned,
    .actor_think = buoy_actor_think,
    .actor_unclipped = buoy_actor_unclipped,
    .actor_figure = buoy_actor_figure,
    .figure_margin = buoy_figure_radius,
    .power_star = buoy_power_star,
};
