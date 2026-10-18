enum { ASTRO_DOME_BIND_NONE, ASTRO_DOME_BIND_TRY, ASTRO_DOME_BIND_TRACTION, ASTRO_DOME_BIND_HOLD };

#define ASTRO_DOME_TRY_FRAMES      30
#define ASTRO_DOME_TRACTION_FRAMES 90

typedef struct {
    uint8_t bind;
    int16_t bind_step;
    v3 bind_from, bind_eye;
    v3 bind_up0, bind_front0;
} AstroDomeCapture;

static EWRAM_BSS AstroDomeCapture g_astro_dome_capture;

static bool astro_dome_bound(void){
    return g_astro_dome_capture.bind != ASTRO_DOME_BIND_NONE && g_game.stage_index == STAGE_ASTRODOME;
}

static bool astro_dome_bluestar_usable(const Actor* a){
    return a->active && a->type == OBJ_ASTRODOMEBLUESTAR && !(a->flags & ACTOR_HIDDEN) &&
           g_astro_dome_capture.bind == ASTRO_DOME_BIND_NONE && g_game.scene == SCENE_PLAY &&
           g_game.player.on_ground;
}

static bool astro_dome_pull_preview(void){
    return g_game.stage_index == STAGE_ASTRODOME && g_astro_dome_capture.bind == ASTRO_DOME_BIND_NONE &&
           g_game.player.on_ground;
}

static void astro_dome_bluestar_think(Actor* a){
    Player* p = &g_game.player;
    if(!a->anim && !a->value){
        a->anim = ANIM_GCAPTURETARGET_WAIT;
        a->value = 1;
    }
    v3 star = actor_centre(a);
    if(g_astro_dome_capture.bind != ASTRO_DOME_BIND_NONE && !g_mario_held)
        g_astro_dome_capture.bind = ASTRO_DOME_BIND_NONE;
    if(g_astro_dome_capture.bind == ASTRO_DOME_BIND_NONE){
        a->flags &= ~ACTOR_HIDDEN;
        Actor* sel = pointer_pull_selected();
        bool pointed = sel == a || pointer_is_pointing(a, FX(200));
        if(pointed){
            pointer_set_hand(pointer_held() ? 1 : 0);
            audio_level_effect(SFX_CAPTURE_POINT);
        }
        if(pointed && pointer_pressed() && p->action != MA_DEATH && astro_dome_bluestar_usable(a)){
            g_astro_dome_capture.bind = ASTRO_DOME_BIND_TRY;
            g_astro_dome_capture.bind_step = 0;
            a->anim = ANIM_GCAPTURETARGET_REACT;
            a->anim_frame = 0;
            audio_stop_level_effect(SFX_CAPTURE_POINT);
            audio_play_effect(SFX_CAPTURE_ON, 2);
            mario_hold_begin();
            mario_set_action(MA_TALK);
            p->on_ground = false;
            mario_restart_anim(ANIM_MARIO_WAIT);
            g_astro_dome_capture.bind_from = p->figure.position;
            g_astro_dome_capture.bind_eye = g_game.camera.position;
            g_astro_dome_capture.bind_up0 = p->up;
            g_astro_dome_capture.bind_front0 = p->front;
        }
        return;
    }
    int step = ++g_astro_dome_capture.bind_step;
    if(g_astro_dome_capture.bind == ASTRO_DOME_BIND_TRY){
        audio_level_effect(SFX_CAPTURE_PULL);
        if(step >= ASTRO_DOME_TRY_FRAMES){
            g_astro_dome_capture.bind = ASTRO_DOME_BIND_TRACTION;
            g_astro_dome_capture.bind_step = 0;
            mario_restart_anim(ANIM_MARIO_SPACESTRUGGLE);
        }
        return;
    }
    if(g_astro_dome_capture.bind == ASTRO_DOME_BIND_TRACTION){
        audio_level_effect(SFX_CAPTURE_PULL);

        v3 at = v3_lerp14(g_astro_dome_capture.bind_from, v3_sub(star, v3_scale14(p->up, C_BODY_HEIGHT)),
                          ease_in_out14(step_ratio14(step, ASTRO_DOME_TRACTION_FRAMES)));
        mario_hold_move(at, p->front);
        if(step >= ASTRO_DOME_TRACTION_FRAMES){
            g_astro_dome_capture.bind = ASTRO_DOME_BIND_HOLD;
            g_astro_dome_capture.bind_step = 0;
            a->flags |= ACTOR_HIDDEN;
            audio_stop_level_effect(SFX_CAPTURE_PULL);
            mario_restart_anim(ANIM_MARIO_SPACEWAIT);
        }
        return;
    }
    if(g_astro_dome_capture.bind == ASTRO_DOME_BIND_HOLD){
        const Camera* c = &g_game.camera;
        int32_t t = step_ratio14(step, GALAXY_MAP_CONFIRM_FRAMES);
        v3 up = v3_blend14(g_astro_dome_capture.bind_up0, c->up, t);
        p->up = up;
        p->front = v3_tangent14(v3_blend14(g_astro_dome_capture.bind_front0, c->front, t), up, p->front);
        if(step >= GALAXY_MAP_CONFIRM_FRAMES){
            g_astro_dome_capture.bind = ASTRO_DOME_BIND_NONE;
            galaxy_map_open();
        }
    }
}

static bool astro_dome_add_bubble(Figure* bubble){
    if(!astro_dome_bound() || g_astro_dome_capture.bind < ASTRO_DOME_BIND_TRACTION ||
       g_game.figure_count >= MAX_FIGURES)
        return false;
    const Player* p = &g_game.player;
    const Camera* c = &g_game.camera;
    capture_bubble_init(bubble, v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT)), c->front, c->up,
                        g_astro_dome_capture.bind == ASTRO_DOME_BIND_TRACTION ? g_astro_dome_capture.bind_step
                                                                              : 20);
    g_game.figures[g_game.figure_count++] = bubble;
    return true;
}

static void astro_dome_camera(void){
    if(!astro_dome_bound() || g_game.scene != SCENE_PLAY) return;
    Camera* cam = &g_game.camera;
    cam->position = g_astro_dome_capture.bind_eye;
    cam->target = v3_add(g_game.player.figure.position, v3_scale14(g_game.player.up, C_BODY_HEIGHT));
    cam->front = v3_direction14(v3_sub(cam->target, cam->position), cam->front);
    cam->up = v3_tangent14(V3(0, 0, N14_ONE), cam->front, cam->up);
    cam->smooth_up = cam->up;
    camera_update_angles(cam);
}

static bool astro_dome_door_think(Actor* a){
    Game* game = &g_game;
    const Player* p = &game->player;
    bool terrace = game->stage_index == STAGE_OBSERVATORY && a->args[1] == 0;
    bool exit = game->stage_index == STAGE_ASTRODOME;
    if(!terrace && !exit) return false;
    if(a->state == 0){
        if(p->action == MA_DEATH || g_mario_held) return true;
        bool inside;
        if(exit){
            v3 m = p->figure.position;
            inside = m.x >= AD_EXIT_MIN_X && m.x <= AD_EXIT_MAX_X && m.y >= AD_EXIT_MIN_Y &&
                     m.y <= AD_EXIT_MAX_Y && m.z >= AD_EXIT_MIN_Z && m.z <= AD_EXIT_MAX_Z;
        } else inside = actor_touches_mario(a, 0);
        if(inside){
            a->state = 1;
            a->timer = 0;
            game->fade_target = 16;
        }
    } else if(a->timer > 20){
        a->state = 0;
        game->fade_target = 0;
        galaxy_map_clear_return();
        g_astro_dome_capture.bind = ASTRO_DOME_BIND_NONE;
        if(exit) game_reload_keep_health(STAGE_OBSERVATORY, 1, 0);
        else game_reload_keep_health(STAGE_ASTRODOME, 1, 0);
    }
    return true;
}
