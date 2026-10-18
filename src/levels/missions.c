enum { COMET_NONE, COMET_RED, COMET_PURPLE };
typedef struct {
    int16_t stage;
    uint8_t mission, comet;
    uint16_t seconds;
} CometInfo;
static const CometInfo comet_infos[] = {
    {STAGE_GOODEGG, 4, COMET_RED, 240},      {STAGE_GOODEGG, 5, COMET_PURPLE, 0},
    {STAGE_BEACHBOWL, 5, COMET_PURPLE, 0},   {STAGE_HONEYHIVE, 5, COMET_PURPLE, 0},
    {STAGE_GUSTYGARDEN, 5, COMET_PURPLE, 0}, {STAGE_FREEZEFLAME, 5, COMET_PURPLE, 0},
    {STAGE_DEEPDARK, 5, COMET_PURPLE, 0},
};

static struct {
    uint8_t comet;
    bool started, timed_out, purple_done;
    int32_t limit, left;
    int16_t timeup;
} g_mission EWRAM_BSS;

static void missions_stage_start(void){
    g_mission.comet = COMET_NONE;
    g_mission.started = g_mission.timed_out = g_mission.purple_done = false;
    g_mission.timeup = 0;
    for(unsigned i = 0; i < sizeof g_power_star_revealed; i++) g_power_star_revealed[i] = 0;
    for(unsigned i = 0; i < sizeof comet_infos / sizeof comet_infos[0]; i++)
        if(comet_infos[i].stage == level_current()->galaxy_stage &&
           comet_infos[i].mission == g_game.scenario){
            g_mission.comet = comet_infos[i].comet;
            g_mission.limit = g_mission.left = comet_infos[i].seconds * 60;
        }
    hud_comet_reset();
}

static void missions_kill_mario(void){
    Player* p = &g_game.player;
    if(p->action == MA_DEATH) return;
    if(g_mario_held) mario_hold_release(V3(0, 0, 0));
    p->health = 0;
    mario_set_action(MA_DEATH);
    mario_restart_anim(ANIM_MARIO_DIEOVER);
    audio_play_effect(SFX_DIE, 5);
}

static void missions_tick(void){
    if(g_game.scene != SCENE_PLAY || scenario_intro_active()) return;
    g_mission.started = true;
    if(g_mission.comet == COMET_RED){
        hud_time_limit_tick(g_mission.left, g_mission.limit);
        if(hud_time_limit_running() && g_mission.left > 0 && g_game.player.action != MA_STAR_GET)
            g_mission.left--;
        if(g_mission.left <= 0 && !g_mission.timed_out){
            g_mission.timed_out = true;
            missions_kill_mario();
        }
    } else if(g_mission.comet == COMET_PURPLE){
        hud_purple_coins_tick(g_game.purple_coins);
        if(purple_coin_star_stage()){
            if(g_game.purple_coins >= 100 && !g_mission.purple_done && purple_coin_star_request())
                g_mission.purple_done = true;
            return;
        }
        if(g_game.purple_coins >= 100 && !g_mission.purple_done){
            g_mission.purple_done = true;
            for(uint32_t i = 0; i < g_game.stage.header->object_count; i++){
                const StageObject* o = &g_game.stage.objects[i];
                if(o->type == OBJ_POWERSTAR && (o->flags & 1) &&
                   (o->scenarios & (1 << (g_game.scenario - 1))))
                    g_power_star_revealed[i >> 3] |= (uint8_t)(1 << (i & 7));
            }
            for(int i = 0; i < g_game.actor_count; i++){
                Actor* b = &g_game.actors[i];
                if(b->type == OBJ_POWERSTAR && !b->active && b->stage_index >= 0 &&
                   power_star_revealed(b->stage_index))
                    power_star_reveal(b);
            }
        }
    }
}

static void missions_draw_hud(void){
    if(!g_mission.started || talk_active()) return;
    if(g_mission.comet == COMET_RED) hud_draw_time_limit(g_mission.left);
    else if(g_mission.comet == COMET_PURPLE) hud_draw_purple_coins();
}
