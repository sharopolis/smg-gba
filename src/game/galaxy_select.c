static const int8_t galaxy_select_order[] = {GALAXY_GOODEGG, GALAXY_HONEYHIVE, GALAXY_BEACHBOWL,
                                             GALAXY_FREEZEFLAME, GALAXY_DEEPDARK, GALAXY_GUSTYGARDEN,
                                             GALAXY_BUOYBASE};
#define GALAXY_SELECT_COUNT ((int)(sizeof galaxy_select_order / sizeof galaxy_select_order[0]))

static void galaxy_select_open(void){
    Game* game = &g_game;
    game->scene = SCENE_GALAXY_SELECT;
    game->selected_galaxy = 0;
    game->scene_timer = 0;
    audio_play_effect(SFX_MENU_OPEN, 4);
}

static void galaxy_select_tick(const KeyState* keys){
    Game* game = &g_game;
    int n = GALAXY_SELECT_COUNT;
    if(keys->pressed & (KEY_LEFT | KEY_RIGHT)){
        int direction = keys->pressed & KEY_LEFT ? -1 : 1;
        game->selected_galaxy = (game->selected_galaxy + direction + n) % n;
        audio_play_effect(SFX_MENU_MOVE, 1);
    }
    if(keys->pressed & KEY_B){
        game->scene = SCENE_PLAY;
        game_reload_keep_health(STAGE_OBSERVATORY, 1, 0);
    } else if(keys->pressed & (KEY_A | KEY_START)){
        audio_play_effect(SFX_MENU_SELECT, 4);
        game_open_missions(galaxy_select_order[game->selected_galaxy]);
    }
}

static void galaxy_select_draw_hud(void){
    Game* game = &g_game;
    int first = (game->selected_galaxy / 2) * 2;
    if(first + 2 > GALAXY_SELECT_COUNT) first = fx_max(0, GALAXY_SELECT_COUNT - 2);
    for(int i = 0; i < 2 && first + i < GALAXY_SELECT_COUNT; i++){
        const GalaxyInfo* gi = &galaxies[galaxy_select_order[first + i]];
        const HudArt* a = &hud_arts[gi->plate_art];
        int x = 120 + (2 * i - 1) * 56 - a->w / 2;
        int y = HUD_PLATE_CY - a->h / 2 -
                (first + i == game->selected_galaxy ? 4 + ((game->tick >> 3) & 1) : 0);
        hud_art(gi->plate_art, x, y, i ? SLOT_SCENARIO : SLOT_GALAXY);
    }
}

static const GameScene galaxy_select_scene = {.tick = galaxy_select_tick, .draw_hud = galaxy_select_draw_hud};
