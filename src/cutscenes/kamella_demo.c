static bool kamella_demo_active(void){ return g_kamella.demo; }
static void kamella_demo_begin(Actor* a, int state, int anim){
    actor_set_state(a, state, anim);
    mario_set_anim(ANIM_MARIO_BATTLEWAIT);
    if(state == KAMELLA_START) audio_play_music(MUSIC_MBGM_BOSS_04);
    g_kamella.demo = true;
    g_mario_held = true;
    a->velocity = V3(0, 0, 0);
}
static void kamella_demo_end(void){
    g_kamella.demo = false;
    g_mario_held = false;
    g_gcam.out_valid = false;
    g_gcam.snap = true;
    Player* p = &g_game.player;
    mario_reset(p->figure.position, p->up, p->front);
    camera_galaxy_snap();
}
