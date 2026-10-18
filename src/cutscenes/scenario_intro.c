#define INTRO_TITLE_LEAD 60
#define INTRO_BAR_ROWS   8
#define INTRO_BAR_FRAMES 30

typedef struct {
    bool active, had, flying;
    int8_t camera;
    int16_t frame;
    int8_t view_mode;
    v3 eye, watch;
    v3 start_pos, start_up, start_front;
    int16_t starter, welcome;
} ScenarioIntro;

static EWRAM_BSS ScenarioIntro g_intro;

static bool scenario_intro_active(void){ return g_intro.active || g_intro.flying || g_intro.welcome > 0; }
static bool scenario_intro_playing(void){ return g_intro.active || g_intro.flying; }
static bool scenario_intro_welcoming(void){ return g_intro.welcome > 0; }

static void scenario_intro_sample(int frame){
    const IntroCamera* c = &intro_cameras[g_intro.camera];
    int f = frame < 0 ? 0 : frame > c->frames ? c->frames : frame;
    int i = f / INTRO_STEP, r = f % INTRO_STEP;
    if(i >= c->count - 1){
        i = c->count - 2;
        r = INTRO_STEP;
    }
    const IntroSample* a = &intro_samples[c->first + i];
    const IntroSample* b = a + 1;
    for(int k = 0; k < 3; k++){
        (&g_intro.eye.x)[k] = a->eye[k] + (b->eye[k] - a->eye[k]) * r / INTRO_STEP;
        (&g_intro.watch.x)[k] = a->watch[k] + (b->watch[k] - a->watch[k]) * r / INTRO_STEP;
    }
}

static void scenario_intro_start(int stage, int mission){
    if(scenario_intro_active() && g_intro.view_mode >= 0) g_game.view_distance = g_intro.view_mode;
    g_intro = (ScenarioIntro){0};
    g_intro.camera = -1;
    g_intro.view_mode = -1;
    g_intro.starter = -1;
    g_intro.start_pos = g_game.player.figure.position;
    g_intro.start_up = g_game.player.up;
    g_intro.start_front = g_game.player.front;
    if(mission <= 0) return;
    for(uint32_t i = 0; i < g_game.stage.header->object_count; i++){
        const StageObject* o = &g_game.stage.objects[i];
        if(o->type == OBJ_SCENARIOSTARTER && (o->scenarios & (1 << (mission - 1)))){
            g_intro.starter = i;
            break;
        }
    }
    stage = level_hooks(stage)->galaxy_stage;
    for(int i = 0; i < INTRO_CAMERA_COUNT; i++)
        if(intro_cameras[i].stage == stage && intro_cameras[i].mission == mission) g_intro.camera = (int8_t)i;
    if(g_intro.camera < 0){
        scenario_intro_opening_done();
        return;
    }
    g_intro.active = g_intro.had = true;

    g_intro.view_mode = -1;
    if(g_game.view_distance == VIEW_MODE_DEFAULT){
        g_intro.view_mode = (int8_t)g_game.view_distance;
        g_game.view_distance = 1;
    }
    audio_play_music(MUSIC_STARTDEMO);
    mario_set_hidden(true);
    g_game.fade = 0;
    scenario_intro_sample(0);
}

static void scenario_intro_finish(bool resume_stage_music){
    Game* game = &g_game;
    audio_stop_level_effect(SFX_SPACEFLY);
    g_intro.active = g_intro.flying = false;
    g_intro.welcome = 0;
    if(g_intro.view_mode >= 0) game->view_distance = g_intro.view_mode;
    g_intro.view_mode = -1;
    mario_set_hidden(false);
    if(resume_stage_music) audio_play_stage_music(game_stage_music());
    else audio_stop_music();

    camera_snap_behind(&game->camera, &game->player);
    g_gcam.out_valid = false;
    g_gcam.snap = true;
    g_gcam.chunk = -2;
    game->fade = 0;
}

static void scenario_intro_end(void){ scenario_intro_finish(true); }

static int scenario_starter_bgm_lead(void){
    int lead = level_current()->intro_music_lead;
    return g_game.scenario == 1 && lead ? lead : -1;
}

static void scenario_intro_opening_done(void){
    int starter = g_intro.starter;

    bool quiet_flight = starter >= 0;
    scenario_intro_finish(!quiet_flight);
    if(starter >= 0 && flight_start_scenario(starter)){
        g_intro.flying = g_intro.had = true;
        if(scenario_starter_bgm_lead() < 0) audio_play_stage_music(game_stage_music());
    } else if(quiet_flight) audio_play_stage_music(game_stage_music());
}

static void scenario_starter_tick(int step, int duration){
    int lead = scenario_starter_bgm_lead();
    if(lead >= 0 && step == duration - lead) audio_play_stage_music(game_stage_music());
}

static void scenario_starter_released(void){
    bool was_flying = g_intro.flying;
    g_intro.flying = false;
    g_intro.welcome = 90;
    if(was_flying) audio_play_stage_music(game_stage_music());
}

static bool scenario_intro_tick(const KeyState* keys){
    if(g_intro.welcome > 0) g_intro.welcome--;
    if(g_intro.flying){
        if(g_flight.step > 1 && (keys->pressed & KEY_A)){
            camera_event_cancel();
            mario_reset(g_intro.start_pos, g_intro.start_up, g_intro.start_front);
            scenario_intro_end();
            g_intro.welcome = 90;
        }
        return false;
    }
    if(!g_intro.active) return false;
    Game* game = &g_game;
    const IntroCamera* c = &intro_cameras[g_intro.camera];
    g_intro.frame++;

    if(g_intro.frame > 1 && (keys->pressed & (KEY_A | KEY_START))){
        scenario_intro_opening_done();
        return false;
    }
    if(g_intro.frame >= c->frames){
        scenario_intro_opening_done();
        return false;
    }
    scenario_intro_sample(g_intro.frame);

    Player* p = &game->player;
    v3 keep = p->figure.position;
    p->figure.position = g_intro.watch;
    actors_world_tick();
    p->figure.position = keep;
    return true;
}

static void scenario_intro_camera(void){
    if(!g_intro.active) return;
    Camera* cam = &g_game.camera;
    cam->position = g_intro.eye;
    cam->target = g_intro.watch;
    cam->front = v3_direction14(v3_sub(g_intro.watch, g_intro.eye), cam->front);

    cam->up = v3_tangent14(V3(0, 0, N14_ONE), cam->front, cam->up);
    cam->smooth_up = cam->up;
    camera_update_angles(cam);
}

static void scenario_intro_draw_frame(uint8_t* framebuffer){
    if(!scenario_intro_active()) return;
    int rows = INTRO_BAR_ROWS;
    int left = g_intro.active   ? intro_cameras[g_intro.camera].frames - INTRO_TITLE_LEAD - g_intro.frame
               : g_intro.flying ? 0
                                : g_intro.welcome - 90;
    if(left < 0){
        int t = -left;
        rows = t >= INTRO_BAR_FRAMES ? 0 : INTRO_BAR_ROWS - INTRO_BAR_ROWS * t / INTRO_BAR_FRAMES;
    }
    for(int y = 0; y < rows; y++){
        uint32_t* top = (uint32_t*)(framebuffer + y * FRAME_STRIDE);
        uint32_t* bot = (uint32_t*)(framebuffer + (FRAME_HEIGHT - 1 - y) * FRAME_STRIDE);
        for(int x = 0; x < FRAME_WIDTH / 4; x++) top[x] = bot[x] = 0;
    }
}

static bool scenario_intro_title_visible(int stage_tick){
    if(g_intro.had){
        if(g_intro.flying) return g_flight.step >= 10;
        if(!g_intro.active) return false;
        return g_intro.frame < intro_cameras[g_intro.camera].frames - INTRO_TITLE_LEAD;
    }
    return stage_tick >= 10 && stage_tick < 200;
}
