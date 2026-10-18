enum { GOODEGG_DISK_CAMERA = 104 };

static bool goodegg_launch_star_branch(const StageObject* o){
    if(o->type != OBJ_LAUNCHSTAR || o->zone != 16 || o->id < 399 || o->id > 401 || o->scenarios != 18 ||
       o->args[0] != 450 || o->args[2] != 0 || o->args[3] != 3 || o->rail != 34 + (int)o->id - 399 ||
       o->flags != 0)
        return false;
    for(unsigned k = 0; k < 5; k++)
        if(o->sw[k] >= 0) return false;
    return true;
}

static void goodegg_actors_pre(void){
    dyncol_clear();
    crystal_cage_medium_collision_all();
}

static bool goodegg_camera_hold_height(void){
    return g_gcam.source == CAMSRC_AREA && g_gcam.chunk == GOODEGG_DISK_CAMERA &&
           g_game.player.action == MA_WAIT;
}

static bool goodegg_camera_blend_below(void){
    return g_gcam.active && g_gcam.source == CAMSRC_AREA &&
           (g_gcam.chunk == 96 || g_gcam.chunk == 103 || g_gcam.chunk == 105) &&
           g_gcam.blend_t < g_gcam.blend_total;
}

static bool goodegg_camera_low_anchor(const StageCamera* cp, int source, int chunk){
    const Player* p = &g_game.player;
    return source == CAMSRC_AREA && chunk == GOODEGG_DISK_CAMERA && p->on_ground && p->action == MA_WAIT;
}

static int goodegg_actor_unclipped(const Actor* a){
    return blue_chip_completion_cast(a) && a->state == GC_APPEAR ? UNCLIPPED_YES : UNCLIPPED_DEFAULT;
}

static const PipeMusic goodegg_pipe_music[] = {{4, MUSIC_SUBAREA}, {1, MUSIC_GOODEGG}, {-1, 0}};

static const LevelHooks goodegg_level = {
    .galaxy_stage = STAGE_GOODEGG,
    .intro_music_lead = 20,
    .features = LEVEL_CAGES_BREAK_ON_ATTACK,
    .pipe_music = goodegg_pipe_music,
    .after_map_parts = timer_switches_tick,
    .before_actors = goodegg_actors_pre,
    .camera_hold_height = goodegg_camera_hold_height,
    .camera_blend_below = goodegg_camera_blend_below,
    .camera_low_anchor = goodegg_camera_low_anchor,
    .launch_star_branch = goodegg_launch_star_branch,
    .actor_unclipped = goodegg_actor_unclipped,
};
