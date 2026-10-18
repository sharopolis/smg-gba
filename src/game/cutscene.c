#define BLOCK_SCENARIO_INTRO                                                                        \
    (BLOCK_CAMERA_INPUT | BLOCK_POINTER_INPUT | ALLOW_RAIL_FLIGHT_INPUT | BLOCK_PAUSE | BLOCK_HUD | \
     BLOCK_DEMO_START)
#define RAIL_FLIGHT_INPUT (BLOCK_CAMERA_INPUT | BLOCK_POINTER_INPUT)

#define CUTSCENES(X)                                                                                         \
    X(scenario_intro_tick(keys), scenario_intro_playing(), NULL, false, false, false, NULL,                  \
      BLOCK_SCENARIO_INTRO | BLOCK_COLLECTIBLES)                                                             \
    X(false, scenario_intro_welcoming(), NULL, false, false, false, NULL, BLOCK_SCENARIO_INTRO)              \
    X(volcano_switch_tick(keys), volcano_switch_paused(), NULL, false, false, false, NULL,                   \
      BLOCK_CAMERA_INPUT)                                                                                    \
    X(valve_doors_tick(keys), valve_doors_playing(), NULL, false, false, false, NULL, 0)                     \
    X(blue_chip_completion_tick(keys), blue_chip_completion_paused(), NULL, false, false, false,             \
      blue_chip_counter_draw,                                                                                \
      BLOCK_CAMERA_INPUT | BLOCK_POINTER_UPDATE | BLOCK_DEMO_START | BLOCK_INPUT_EXCEPT_START |              \
          BLOCK_EVENT_PLAYER_FOCUS)                                                                          \
    X(floater_rise_tick(keys), floater_rise_active(), NULL, false, false, false, NULL, BLOCK_CAMERA_FRAMING) \
    X(life_mushroom_tick(keys), life_mushroom_paused(), NULL, false, false, false, hud_lifeup_draw,          \
      BLOCK_CAMERA_INPUT | BLOCK_PAUSE | BLOCK_DEMO_START)                                                   \
    X(purple_coin_star_tick(keys), purple_coin_star_paused(), NULL, false, false, false, NULL,               \
      BLOCK_CAMERA_INPUT | BLOCK_COLLECTIBLES | BLOCK_MARIO_ANIM)                                            \
    X(false, life_mushroom_anim_paused(), NULL, false, false, false, NULL, BLOCK_MARIO_ANIM)                 \
    X(false, bee_mushroom_appear_actor() != NULL, bee_mushroom_appear_actor(), false, false,                 \
      bee_mushroom_appear_camera_target(pos, up, front, move), NULL,                                         \
      BLOCK_CAMERA_INPUT | BLOCK_POINTER_UPDATE | BLOCK_HUD | BLOCK_DEMO_START | BLOCK_MARIO_ANIM |          \
          BLOCK_PLAYER | BLOCK_INPUT | BLOCK_CAMERA_PLAYER_FOCUS)                                            \
    X(false, iceking_demo_actor() != NULL, iceking_demo_actor(), iceking_demo_cast(demo, a), false, false,   \
      NULL, BLOCK_DEMO_START)                                                                                \
    X(false, kamella_demo_active(), NULL, false, false, false, NULL,                                         \
      BLOCK_GROUND_ANIM | BLOCK_GALAXY_CAMERA)                                                               \
    X(false, boss_demo_active(), boss_demo_actor(), boss_demo_cast(demo, a),                                 \
      boss_demo_camera(cam) || boss_area_camera(cam), false, NULL, BLOCK_GROUND_ANIM)                        \
    X(false, talk_active(), NULL, false, false, false, NULL,                                                 \
      BLOCK_CAMERA_INPUT | BLOCK_POINTER_INPUT | BLOCK_HUD | BLOCK_COLLECTIBLES | BLOCK_SCENE_START)         \
    X(false, mario_talk_paused(), NULL, false, false, false, NULL, BLOCK_COLLECTIBLES)                       \
    X(false, cosmic_bound(), NULL, false, false, false, NULL,                                                \
      BLOCK_CAMERA_INPUT | BLOCK_HUD | BLOCK_COLLECTIBLES | BLOCK_DEMO_START | BLOCK_PLAYER)                 \
    X(false, astro_dome_bound(), NULL, false, false, false, NULL, BLOCK_CAMERA_INPUT)                        \
    X(false, camera_event_busy(), NULL, false, false, false, NULL, BLOCK_SCENE_START)

static bool cutscene_rail_flight(void){
    const Player* p = &g_game.player;
    return p->action == MA_STAR_LAUNCH && g_flight.phase == FLIGHT_SHOOT;
}

static bool cutscene_tick(const KeyState* keys){
#define CUTSCENE_TICK(tick, running, actor, cast, camera, target, draw_hud, blocks) \
    if(tick) return true;
    CUTSCENES(CUTSCENE_TICK)
#undef CUTSCENE_TICK
    return false;
}

__attribute__((always_inline)) static inline bool cutscene_blocks(unsigned query){
#define CUTSCENE_BLOCKS(tick, running, actor, cast, camera, target, draw_hud, blocks)                        \
    if(((blocks) & query) &&                                                                 \
       !(((blocks) & ALLOW_RAIL_FLIGHT_INPUT) && !((blocks) & query & ~RAIL_FLIGHT_INPUT) && \
         cutscene_rail_flight()) &&                                                          \
       (running))                                                                            \
        return true;
    CUTSCENES(CUTSCENE_BLOCKS)
#undef CUTSCENE_BLOCKS
    return false;
}

static bool cutscene_draw_hud(void){
#define CUTSCENE_DRAW_HUD(tick, running, actor, cast, camera, target, draw_hud, blocks) \
    if((draw_hud) != NULL && (running)){                                \
        void (*draw)(void) = draw_hud;                                  \
        draw();                                                         \
        return true;                                                    \
    }
    CUTSCENES(CUTSCENE_DRAW_HUD)
#undef CUTSCENE_DRAW_HUD
    return false;
}

static Actor* cutscene_actor(void){
    Actor* demo;
#define CUTSCENE_ACTOR(tick, running, actor, cast, camera, target, draw_hud, blocks) \
    if((demo = (actor)) != NULL) return demo;
    CUTSCENES(CUTSCENE_ACTOR)
#undef CUTSCENE_ACTOR
    return NULL;
}

static bool cutscene_cast(const Actor* demo, const Actor* a){
#define CUTSCENE_CAST(tick, running, actor, cast, camera, target, draw_hud, blocks) \
    if(cast) return true;
    CUTSCENES(CUTSCENE_CAST)
#undef CUTSCENE_CAST
    return false;
}

static bool cutscene_camera(Camera* cam){
#define CUTSCENE_CAMERA(tick, running, actor, cast, camera, target, draw_hud, blocks) \
    if(camera) return true;
    CUTSCENES(CUTSCENE_CAMERA)
#undef CUTSCENE_CAMERA
    return false;
}

static bool cutscene_camera_target(v3* pos, v3* up, v3* front, v3* move){
#define CUTSCENE_CAMERA_TARGET(tick, running, actor, cast, camera, target, draw_hud, blocks) \
    if(target) return true;
    CUTSCENES(CUTSCENE_CAMERA_TARGET)
#undef CUTSCENE_CAMERA_TARGET
    return false;
}
