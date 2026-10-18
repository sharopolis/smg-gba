static const LevelHooks astro_dome_level = {
    .galaxy_stage = STAGE_ASTRODOME,
    .features = LEVEL_FIXED_CAMERA | LEVEL_HUB_ROOM,
    .after_actor_cameras = astro_dome_camera,
    .pull_preview = astro_dome_pull_preview,
    .pull_target = astro_dome_bluestar_usable,
    .player_figure = astro_dome_add_bubble,
};
