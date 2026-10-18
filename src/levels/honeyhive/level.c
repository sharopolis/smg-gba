static bool honey_camera_hold_height(void){
    return g_gcam.source == CAMSRC_START && g_gcam.chunk == 0 && g_gcam.blend_t < g_gcam.blend_total &&
           (g_game.player.action == MA_WAIT || g_game.player.action == MA_LAND);
}

static bool honey_camera_low_anchor(const StageCamera* cp, int source, int chunk){
    return source == CAMSRC_AREA && cp->type == CAM_XZ_PARA && cp->dist == FX(3000) && !cp->vpan &&
           v3_dot14(gcam_v32(cp->woffset), gcam_v16(cp->zone_up)) < 0;
}

static bool honey_actor_think(Actor* a){
    if(!honey_handles_actor(a)) return false;
    if(a->type == OBJ_TICO || a->type == OBJ_HONEYBEE || a->type == OBJ_KINOPIO) honey_kinopio_think(a);
    else honey_kind_think(a);
    return true;
}

static bool honey_power_star(Actor* a){ return honey_star_busy(a); }

static const PipeMusic honey_pipe_music[] = {{1, MUSIC_SUBAREA}, {0, MUSIC_HONEYHIVE}, {-1, 0}};

static const ScenarioTail honey_purple_rail = {
    .first = HONEY_PURPLE_RAIL_FIRST,
    .count = HONEY_PURPLE_RAIL_COUNT,
    .type = OBJ_PURPLECOIN,
    .scenarios = HONEY_PURPLE_RAIL_MASK,
    .zone = 4,
    .ids = honey_purple_rail_groups,
    .ids_step =
        HONEY_PURPLE_RAIL_COUNT / (sizeof honey_purple_rail_groups / sizeof *honey_purple_rail_groups),
};

static const LevelHooks honey_level = {
    .galaxy_stage = STAGE_HONEYHIVE,
    .intro_music_lead = 7,
    .features = LEVEL_BEE_MUSHROOM_DEMOS,
    .pipe_music = honey_pipe_music,
    .scenario_tail = &honey_purple_rail,
    .mission_stage = honey_mission_stage,
    .reset = honey_reset,
    .before_player = cosmic_tick,
    .before_actors = honey_actors_pre,
    .after_actors = honey_actors_post,
    .after_actor_cameras = cosmic_camera,
    .after_player_pose = cosmic_apply_pose,
    .miss = cosmic_miss,
    .star_get = cosmic_win,
    .camera_hold_height = honey_camera_hold_height,
    .camera_low_anchor = honey_camera_low_anchor,
    .actor_spawned = honey_actor_spawned,
    .actor_think = honey_actor_think,
    .power_star = honey_power_star,
    .star_appear = honey_star_appear_at,
    .add_figures = cosmic_add_figure,
    .draw_hud = cosmic_draw_hud,
};
