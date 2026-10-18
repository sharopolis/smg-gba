static void dome_think(Actor* a){
    Game* game = &g_game;
    Player* p = &game->player;
    if(astro_dome_door_think(a)) return;

    if(a->state == 0 && p->action != MA_DEATH && !g_mario_held && actor_touches_mario(a, 0)){
        a->state = 1;
        a->timer = 0;
        audio_stop_music();
        game->fade_target = 16;
    } else if(a->state == 1 && a->timer > 20){
        a->state = 0;
        game->fade_target = 0;
        galaxy_select_open();
    }
}
