#define GALAXY_MAP_LAUNCH_SHOOT_STEP 58
#define GALAXY_MAP_LAUNCH_STOP_STEP  43
#define GALAXY_MAP_LAUNCH_WHITE_OUT  146
#define GALAXY_MAP_LAUNCH_BGM_START  108
#define GALAXY_MAP_LAUNCH_NEAR_CLIP  250

static void galaxy_map_launch_tick(void){
    Game* game = &g_game;
    int st = g_galaxy_map.step - 1;
    v3 front, up;
    galaxy_map_camera_basis(&front, &up);
    if(g_galaxy_map.nerve == GALAXY_MAP_LAUNCH_APPEAR){
        v3 camz = front, camx = v3_cross14(up, camz);
        if(st == 0){
            fx s5 = sin_turn16(DEG_TO_TURN16(-5)), c5 = cos_turn16(DEG_TO_TURN16(-5));
            g_galaxy_map.star_up = v3_direction14(v3_add(v3_scale(camx, s5), v3_scale(camz, c5)), camz);
            g_galaxy_map.star_front = v3_neg(up);
            g_galaxy_map.star_shown = 1;
            mario_restart_anim(ANIM_MARIO_SPINDRIVERWAIT);
            audio_play_effect(SFX_SPIN, 3);
        }
        int32_t t = step_ratio14(st, AD_PART_APPEAR);
        g_galaxy_map.mario_up = v3_blend14(up, g_galaxy_map.star_up, t);
        g_galaxy_map.mario_front = v3_tangent14(v3_blend14(camz, g_galaxy_map.star_front, t),
                                                g_galaxy_map.mario_up, g_galaxy_map.mario_front);
        if(st + 1 >= AD_PART_APPEAR) galaxy_map_set_nerve(GALAXY_MAP_LAUNCH_START);
        return;
    }
    if(g_galaxy_map.nerve == GALAXY_MAP_LAUNCH_START){
        Player* p = &game->player;
        if(st == 0){
            mario_restart_anim(ANIM_MARIO_LAUNCHSTART);
            g_galaxy_map.mario_up = g_galaxy_map.star_up;
            g_galaxy_map.mario_front = g_galaxy_map.star_front;
        }
        if(st == GALAXY_MAP_LAUNCH_STOP_STEP) p->anim_speed = 0;
        if(st >= GALAXY_MAP_LAUNCH_SHOOT_STEP){
            int k = st - GALAXY_MAP_LAUNCH_SHOOT_STEP;
            if(k == 0){
                mario_restart_anim(ANIM_MARIO_SPACEFLYSTART);
                audio_play_effect(SFX_LAUNCH, 5);
            }
            g_galaxy_map.mario_pos =
                v3_add(g_galaxy_map.star_pos, v3_scale14_wide(g_galaxy_map.star_up, 50 * k));
            if(k < 24){
                int32_t a = (24 - k) * 2 * ((k & 2) ? 1 : -1);
                g_galaxy_map.eye = v3_add(g_galaxy_map.cam_to_pos, v3_scale14_wide(g_galaxy_map.cu, a));
                galaxy_map_camera_commit();
            }
            audio_level_effect(SFX_SPACEFLY);
        }
        if(st + 1 >= AD_PART_START){
            galaxy_map_set_nerve(GALAXY_MAP_LAUNCH_JUMPOUT);
            g_galaxy_map.star_shown = 0;
            g_galaxy_map.fovy_x10 = AD_CAM_FOVY_X10;
            mario_restart_anim(ANIM_MARIO_SPACEFLY);
        }
        return;
    }
    if(g_galaxy_map.nerve == GALAXY_MAP_LAUNCH_JUMPOUT){
        int f = st < AD_CAM_FRAMES ? st : AD_CAM_FRAMES;
        int i = f / AD_CAM_STEP, r = f % AD_CAM_STEP;
        if(i >= (int)(sizeof ad_cam_samples / sizeof ad_cam_samples[0]) - 1){
            i--;
            r = AD_CAM_STEP;
        }
        const int32_t* a = ad_cam_samples[i];
        const int32_t* b = ad_cam_samples[i + 1];
        g_galaxy_map.eye = V3(a[0] + (b[0] - a[0]) * r / AD_CAM_STEP, a[1] + (b[1] - a[1]) * r / AD_CAM_STEP,
                              a[2] + (b[2] - a[2]) * r / AD_CAM_STEP);
        g_galaxy_map.target =
            V3(a[3] + (b[3] - a[3]) * r / AD_CAM_STEP, a[4] + (b[4] - a[4]) * r / AD_CAM_STEP,
               a[5] + (b[5] - a[5]) * r / AD_CAM_STEP);
        g_galaxy_map.up = V3(0, 0, N14_ONE);
        galaxy_map_camera_commit();
        int32_t rate = step_ratio14(st, AD_PART_JUMPOUT);
        fx len = rail_length(0);
        v3 pos = rail_position_at(0, (fx)(((int64_t)len * rate) >> 14));
        int32_t r0 = rate < N14_ONE - 164 ? rate : N14_ONE - 164;
        v3 p0 = rail_position_at(0, (fx)(((int64_t)len * r0) >> 14));
        v3 p1 = rail_position_at(0, (fx)(((int64_t)len * (r0 + 164)) >> 14));
        v3 dir = v3_direction14(v3_sub(p1, p0), V3(0, N14_ONE, 0));
        g_galaxy_map.mario_pos = pos;
        g_galaxy_map.mario_up = dir;
        g_galaxy_map.mario_front = v3_tangent14(V3(0, 0, -N14_ONE), dir, V3(0, N14_ONE, 0));

        g_galaxy_map.mario_spin = (fx)(((int64_t)ease_out14(rate) * FXC(-13.3517f)) >> 14);
        v3 cam_front, cam_up;
        galaxy_map_camera_basis(&cam_front, &cam_up);
        g_galaxy_map.mario_hidden =
            v3_dot14(v3_sub(pos, g_galaxy_map.eye), cam_front) < GALAXY_MAP_LAUNCH_NEAR_CLIP;
        audio_level_effect(SFX_SPACEFLY);
        if(st == GALAXY_MAP_LAUNCH_BGM_START) audio_play_music(MUSIC_SENARIO_SEL);
        if(st == GALAXY_MAP_LAUNCH_WHITE_OUT)
            screen_fade_start(AD_PART_JUMPOUT - GALAXY_MAP_LAUNCH_WHITE_OUT, 1, true);
        if(st + 1 >= AD_PART_JUMPOUT) galaxy_map_set_nerve(GALAXY_MAP_LAUNCH_END);
        return;
    }

    audio_stop_level_effect(SFX_SPACEFLY);
    int galaxy = ad_minis[g_galaxy_map.selected].galaxy;
    g_galaxy_map.vram = 0;
    g_galaxy_map.from_dome = 1;
    g_galaxy_map.mario_hidden = 0;
    game->player.anim_speed = FX_ONE;
    game_open_missions(galaxy);
}
