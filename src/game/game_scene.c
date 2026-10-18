static const GameScene* const game_scene_table[] = {
    [SCENE_TITLE] = &title_scene,
    [SCENE_PLAY] = &play_scene,
    [SCENE_STAR_GET] = &star_get_scene,
    [SCENE_MISSION_SELECT] = &scenario_select_scene,
    [SCENE_GAME_OVER] = &game_over_scene,
    [SCENE_PAUSE] = &pause_scene,
    [SCENE_GALAXY_SELECT] = &galaxy_select_scene,
    [SCENE_GALAXY_MAP] = &galaxy_map_scene,
};
#define GAME_SCENE_COUNT ((int)(sizeof game_scene_table / sizeof game_scene_table[0]))

static const GameScene* game_scene(int scene){ return game_scene_table[scene]; }

static void game_scenes_release(const GameScene* shown){
    for(int i = 0; i < GAME_SCENE_COUNT; i++){
        const GameScene* s = game_scene_table[i];
        if(s != shown && s->release) s->release();
    }
}
