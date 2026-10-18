enum { RACE_OFF, RACE_ARMED, RACE_INTRO1, RACE_INTRO2, RACE_COUNTDOWN, RACE_RUN, RACE_LOST, RACE_WON };
static EWRAM_BSS struct {
    Figure figure;
    uint16_t step, replay;
    uint8_t phase, clip;
    bool hidden, retry;
} g_cosmic;
static bool g_cosmic_miss_pending;

static v3 cosmic_pos(const int32_t* p){ return V3(p[0], p[1], p[2]); }
static bool cosmic_bound(void){
    return (g_cosmic.phase >= RACE_INTRO1 && g_cosmic.phase <= RACE_COUNTDOWN) || g_cosmic.phase == RACE_LOST;
}

static void cosmic_reset(void){
    bool retry = g_cosmic_miss_pending;
    g_cosmic_miss_pending = false;
    g_cosmic.phase =
        level_current()->galaxy_stage == STAGE_HONEYHIVE && g_game.scenario == 4 ? RACE_ARMED : RACE_OFF;
    g_cosmic.retry = retry && g_cosmic.phase != RACE_OFF;
    g_cosmic.step = g_cosmic.replay = 0;
    g_cosmic.hidden = true;
    figure_init(&g_cosmic.figure, figure_ghostmario);
    g_cosmic.figure.use_basis = true;
    g_cosmic.figure.scale = V3(64, 64, 64);
}

static void cosmic_miss(void){ g_cosmic_miss_pending = g_cosmic.phase != RACE_OFF; }

static void cosmic_phase(int phase){
    g_cosmic.phase = phase;
    g_cosmic.step = 0;
}
static void cosmic_player_at(const int32_t* pos, const int16_t* up, const int16_t* front){
    mario_reset(cosmic_pos(pos), v3_from_i16(up), v3_from_i16(front));
    mario_set_action(MA_TALK);
}
static void cosmic_ghost_at(const int32_t* pos, const int16_t* up, const int16_t* front){
    g_cosmic.figure.position = cosmic_pos(pos);
    g_cosmic.figure.basis_up = v3_from_i16(up);
    g_cosmic.figure.basis_front = v3_from_i16(front);
}

static void cosmic_lose(void){
    cosmic_phase(RACE_LOST);
    cosmic_player_at(cosmic_mario_lose_pos, cosmic_mario_lose_up, cosmic_mario_lose_front);
    audio_play_music(MUSIC_RACE_LOSE);
}

static void cosmic_tick(const KeyState* keys){
    if(g_cosmic.phase == RACE_OFF || g_cosmic.phase == RACE_WON || g_game.player.action == MA_DEATH) return;
    switch(g_cosmic.phase){
    case RACE_ARMED:
        if(switch_on(2012)){
            cosmic_player_at(cosmic_mario_demo_pos, cosmic_mario_demo_up, cosmic_mario_demo_front);
            cosmic_ghost_at(cosmic_ghost_demo_pos, cosmic_ghost_demo_up, cosmic_ghost_demo_front);
            cosmic_phase(RACE_INTRO1);
        }
        return;
    case RACE_INTRO1:
        if(g_cosmic.step == 60) g_cosmic.hidden = false;
        if(g_cosmic.step >= 240) cosmic_phase(RACE_INTRO2);
        break;
    case RACE_INTRO2:
        if(g_cosmic.step >= 240){
            cosmic_player_at(cosmic_mario_start_pos, cosmic_mario_start_up, cosmic_mario_start_front);
            cosmic_ghost_at(cosmic_ghost_start_pos, cosmic_ghost_start_up, cosmic_ghost_start_front);
            cosmic_phase(RACE_COUNTDOWN);
        }
        break;
    case RACE_COUNTDOWN:
        if(g_cosmic.step % 60 == 0) audio_play_effect(SFX_RACE_COUNT, 4);
        if(g_cosmic.step >= 180){
            mario_reset(cosmic_pos(cosmic_mario_start_pos), v3_from_i16(cosmic_mario_start_up),
                        v3_from_i16(cosmic_mario_start_front));
            cosmic_phase(RACE_RUN);
            g_mario.dash_chance = 5;
            audio_play_effect(SFX_RACE_GO, 4);
            camera_galaxy_snap();
        }
        break;
    case RACE_RUN: {
        const CosmicFrame* r = &cosmic_replay[fx_min(g_cosmic.replay, COSMIC_REPLAY_FRAMES - 1)];
        g_cosmic.figure.position = V3(r->pos[0] * 4, r->pos[1] * 4, r->pos[2] * 4);
        g_cosmic.figure.basis_up = v3_from_i16(r->up);
        g_cosmic.figure.basis_front = v3_from_i16(r->front);
        g_cosmic.clip = r->clip;

        g_cosmic.figure.pose = figure_clip_pose(g_cosmic.figure.model, r->clip, r->frame * 32);
        if(g_game.player.action == MA_STAR_GET){
            cosmic_phase(RACE_WON);
            break;
        }
        for(int i = 0; i < g_game.actor_count; i++){
            const Actor* star = &g_game.actors[i];
            if(!star->active || star->type != OBJ_POWERSTAR) continue;
            v3 sensor = v3_add(g_cosmic.figure.position, v3_scale14(g_cosmic.figure.basis_up, FX(80)));
            if(stream_near(sensor, star->position, FX(140))){
                g_cosmic.figure.position = star->position;
                cosmic_lose();
                break;
            }
        }
        if(g_cosmic.phase == RACE_RUN && ++g_cosmic.replay >= COSMIC_REPLAY_FRAMES) cosmic_lose();
        break;
    }
    case RACE_LOST:
        if(g_cosmic.step >= 180) missions_kill_mario();
        break;
    }

    if((g_cosmic.phase == RACE_INTRO1 || g_cosmic.phase == RACE_INTRO2) && g_cosmic.retry &&
       (keys->pressed & KEY_A)){
        g_cosmic.hidden = false;
        cosmic_player_at(cosmic_mario_start_pos, cosmic_mario_start_up, cosmic_mario_start_front);
        cosmic_ghost_at(cosmic_ghost_start_pos, cosmic_ghost_start_up, cosmic_ghost_start_front);
        cosmic_phase(RACE_COUNTDOWN);
    }
    g_cosmic.step++;
}

static void cosmic_apply_pose(void){
    if(!cosmic_bound()) return;
    int mario_clip = g_cosmic.phase == RACE_COUNTDOWN
                         ? (g_game.keys.held & KEY_R ? COSMIC_RACESTARTCROUCH : COSMIC_RACESTART)
                         : COSMIC_WATCH;
    g_game.player.figure.pose = figure_clip_pose(g_cosmic.figure.model, mario_clip, g_cosmic.step * FX_ONE);
}

static void cosmic_camera(void){
    if(!cosmic_bound()) return;
    int camera = g_cosmic.phase == RACE_INTRO1      ? 0
                 : g_cosmic.phase == RACE_INTRO2    ? 1
                 : g_cosmic.phase == RACE_COUNTDOWN ? 2
                                                    : 3;
    const CosmicCamera* sample = &cosmic_camera_samples[camera][fx_min(g_cosmic.step, 239)];
    Camera* c = &g_game.camera;
    v3 watch = cosmic_pos(sample->watch), offset = v3_sub(cosmic_pos(sample->eye), watch);
    if(camera == 3)
        watch = v3_add(watch, v3_sub(g_cosmic.figure.position, cosmic_pos(cosmic_ghost_demo_pos)));
    fx length = v3_length_exact(offset);
    if(length) offset = v3_scale14(offset, fx_min(length * 7 / 10, FX(1000)) * N14_ONE / length);
    c->position = v3_add(watch, offset);
    c->target = watch;
    c->front = v3_direction14(v3_sub(watch, c->position), c->front);
    c->up = v3_tangent14(v3_from_i16(sample->up), c->front, g_game.player.up);
    camera_update_angles(c);
}

static void cosmic_win(void){
    if(g_cosmic.phase != RACE_RUN) return;
    cosmic_phase(RACE_WON);
    g_cosmic.step = (uint16_t)g_game.tick;
    cosmic_ghost_at(cosmic_mario_lose_pos, cosmic_mario_lose_up, cosmic_mario_lose_front);
}

static void cosmic_add_figure(void){
    if(g_cosmic.hidden || g_cosmic.phase < RACE_INTRO1 || g_game.figure_count >= MAX_FIGURES) return;
    Figure* f = &g_cosmic.figure;
    if(g_cosmic.phase != RACE_RUN){
        int clip = g_cosmic.phase == RACE_INTRO1      ? COSMIC_APPEARGHOST
                   : g_cosmic.phase == RACE_INTRO2    ? COSMIC_WATCH
                   : g_cosmic.phase == RACE_COUNTDOWN ? COSMIC_RACESTARTGHOST
                   : g_cosmic.phase == RACE_WON       ? COSMIC_DIEEVENT
                                                      : COSMIC_WINGHOST;
        int frame = g_cosmic.phase == RACE_INTRO1 ? fx_max(g_cosmic.step - 60, 0)
                    : g_cosmic.phase == RACE_WON  ? (uint16_t)((uint16_t)g_game.tick - g_cosmic.step)
                                                  : g_cosmic.step;
        f->pose = figure_clip_pose(f->model, clip, frame * FX_ONE);
    }
    f->transform_dirty = true;
    figure_update_transform(f);
    g_game.figures[g_game.figure_count++] = f;
}

static void cosmic_draw_hud(void){
    int art = -1;
    if(g_cosmic.phase == RACE_COUNTDOWN) art = HUD_ART_RACE3 + fx_min(g_cosmic.step / 60, 2);
    else if(g_cosmic.phase == RACE_RUN && g_cosmic.step < 60) art = HUD_ART_RACEGO;
    else if(g_cosmic.phase == RACE_LOST && g_cosmic.step < 120) art = HUD_ART_RACELOSE;
    if(art >= 0){
        hud_load_text(SLOT_BANNER, art);
        hud_art_centred(art, HUD_RACE_CY, SLOT_BANNER);
    }
}
