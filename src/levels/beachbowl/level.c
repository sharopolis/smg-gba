static const PipeMusic beachbowl_pipe_music[] = {{2, MUSIC_SUBAREA}, {0, MUSIC_BEACHBOWL}, {-1, 0}};

static const LevelHooks beachbowl_level = {
    .galaxy_stage = STAGE_BEACHBOWL,
    .hidden_mission = 6,
    .hidden_scenario = 3,
    .features = LEVEL_CHIP_GROUPS_FROM_STAGE,
    .pipe_music = beachbowl_pipe_music,
    .before_actors = switch_objects_tick,
};
