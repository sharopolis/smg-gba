enum {
    BOSS_DEMO_FLAG_CAMERA = 1,
    BOSS_DEMO_FLAG_STAR_CAMERA = 2,
    BOSS_DEMO_FLAG_POSES_MARIO = 4,
    BOSS_DEMO_FLAG_AREA_FORCE = 8
};

static EWRAM_BSS BossDemo g_boss_demo;

static void boss_demo_reset(void){
    g_boss_demo = (BossDemo){0};
    dino_reset();
}

static bool boss_demo_active(void){ return g_boss_demo.owner > 0; }

static Actor* boss_demo_actor(void){
    int slot = g_boss_demo.owner - 1;
    if(slot < 0 || slot >= g_game.actor_count) return NULL;
    Actor* a = &g_game.actors[slot];
    if(!a->active){
        g_boss_demo.owner = 0;
        return NULL;
    }
    return a;
}

static void boss_demo_begin(const Actor* owner, bool camera){
    Player* p = &g_game.player;
    g_boss_demo.owner = (int16_t)(actor_index(owner) + 1);
    g_boss_demo.flags = camera ? BOSS_DEMO_FLAG_CAMERA : 0;
    g_mario_held = true;
    camera_event_cancel();
    p->velocity = V3(0, 0, 0);
    g_mario.jump_vec = V3(0, 0, 0);
    mario_set_anim(ANIM_MARIO_BATTLEWAIT);
}

static void boss_demo_end(v3 pos, v3 up, v3 front){
    g_boss_demo.owner = 0;
    g_boss_demo.flags = BOSS_DEMO_FLAG_AREA_FORCE;
    g_mario_held = false;
    mario_reset(pos, up, v3_tangent14(front, up, g_game.player.front));
    g_gcam.out_valid = false;
    camera_galaxy_snap();
    camera_snap_behind(&g_game.camera, &g_game.player);
}

static bool boss_demo_camera(Camera* cam){
    if(g_boss_demo.star > 0 && (g_boss_demo.flags & BOSS_DEMO_FLAG_STAR_CAMERA)){
        const Actor* s = &g_game.actors[g_boss_demo.star - 1];
        cam->position = g_boss_demo.cam_eye;
        cam->target = s->position;
        cam->front = v3_direction14(v3_sub(cam->target, cam->position), cam->front);
        cam->up = v3_tangent14(s->up, cam->front, cam->up);
        cam->smooth_up = cam->up;
        cam->heading = v3_tangent14(cam->front, cam->smooth_up, cam->heading);
        return true;
    }
    return boss_demo_actor() && (g_boss_demo.flags & BOSS_DEMO_FLAG_CAMERA);
}

static void boss_demo_set_camera(v3 eye, v3 watch, v3 up, int32_t fovy_tan4096){
    Camera* cam = &g_game.camera;
    eye = v3_add(watch, v3_scale(v3_sub(eye, watch), fovy_tan4096 * 256 / 1697));
    cam->position = eye;
    cam->target = watch;
    cam->front = v3_direction14(v3_sub(watch, eye), cam->front);
    cam->up = v3_tangent14(up, cam->front, cam->up);
    cam->smooth_up = cam->up;
    cam->heading = v3_tangent14(cam->front, cam->smooth_up, cam->heading);
}

static const fx boss_camera_zooms[CAMERA_ZOOM_COUNT] = {FXC(0.62f), FX_ONE, FXC(0.8f), FXC(0.46f)};

static bool boss_area_camera(Camera* cam){
    BossCamera camera, *bc = &camera;
    if(boss_demo_active() || !dino_area_camera(bc)) return false;
    BossDemo* bd = &g_boss_demo;
    bool force = (bd->flags & BOSS_DEMO_FLAG_AREA_FORCE) != 0;
    bd->flags &= ~BOSS_DEMO_FLAG_AREA_FORCE;
    int n = g_cam_ticks > 0 ? g_cam_ticks : 1;
    const Player* p = &g_game.player;
    v3 target = v3_add(p->figure.position, v3_scale14(p->up, FX(80)));
    v3 up = p->up, front = p->front;

    v3 to = v3_sub(p->figure.position, bc->reg);
    fx d = v3_length_exact(to);
    v3 want = V3(0, 0, 0);
    if(d > 0 && d < bc->peep_dist){
        v3 away = v3_reject14(to, up);
        if(v3_length_squared64(away) > 64)
            want = v3_scale14(v3_direction14(away, front),
                              (fx)((int64_t)bc->peep_angle * (bc->peep_dist - d) / bc->peep_dist));
    }
    bd->peep = v3_reject14(bd->peep, up);
    bd->peep = force ? want : v3_add(bd->peep, v3_scale14(v3_sub(want, bd->peep), cam_ease(R14(0.15f), n)));
    fx a = v3_length_exact(bd->peep);
    if(a > FXC(0.01f)){
        v3 side = v3_direction14(v3_cross14(up, v3_direction14(bd->peep, front)), front);
        up = v3_rotate_axis14(up, side, a);
        front = v3_rotate_axis14(front, side, a);
    }
    v3 eye = cam->position;
    v3 rel = v3_sub(eye, target);
    if(v3_length_squared64(rel) < 64) rel = v3_scale14(up, FX(100));
    fx h = v3_dot14(rel, up);
    v3 flat = v3_sub(rel, v3_scale14(up, h));
    fx fl = v3_length_exact(flat);
    fx el = fx_atan2_fast(h, fl);
    if(el < bc->angle_min)
        el = force ? bc->angle_min : el + mul14(bc->angle_min - el, cam_ease(bc->constrain14, n));
    v2 sc = fx_sin_cos(el);
    v3 hdir = fl > 0 ? v3_direction14(flat, v3_neg(front)) : v3_neg(front);

    fx ang = angle_between14(bc->reg_up, p->up);
    int32_t rate14 = (int32_t)(((int64_t)ang << 14) / FX_PI);
    rate14 = rate14 < 0 ? 0 : rate14 > N14_ONE ? N14_ONE : rate14;
    fx len = bc->dist_min + mul14(bc->dist_max - bc->dist_min, rate14);
    len = fx_mul(len, boss_camera_zooms[g_game.camera_zoom]);
    v3 pose = v3_add(target, v3_add(v3_scale14(hdir, fx_mul(len, sc.y)), v3_scale14(up, fx_mul(len, sc.x))));

    eye = force ? pose : v3_add(eye, v3_scale14(v3_sub(pose, eye), cam_ease(R14(0.3f), n)));

    v3 out = v3_sub(eye, bc->centre);
    if(bc->radius > 0 && v3_length_squared64(out) < (uint64_t)(bc->radius + FX(150)) * (bc->radius + FX(150)))
        eye = v3_add(bc->centre, v3_set_length(out, bc->radius + FX(150), up));
    v3 f0 = cam->front;
    v3 f1 = v3_direction14(v3_sub(target, eye), f0);
    cam->position = eye;
    cam->target = target;
    cam->up = v3_tangent14(v3_rotate_between14(cam->up, f0, f1), f1, p->up);
    cam->front = f1;
    cam->smooth_up = cam->up;
    cam->heading = v3_tangent14(cam->front, cam->smooth_up, cam->heading);
    g_gcam.out_valid = false;
    return true;
}

static void star_appear_start(Actor* star, v3 from, bool star_camera, int music){
    BossDemo* bd = &g_boss_demo;
    bd->star = (int16_t)(actor_index(star) + 1);
    bd->star_step = 0;
    bd->star_music = (int16_t)music;
    star->target = from;
    star->velocity = V3(0, 0, 0);
    star->active = 1;
    star->position = from;
    star->timer = 0;
    if(star_camera){
        bd->flags |= BOSS_DEMO_FLAG_STAR_CAMERA;
        v3 eye = g_game.camera.position, d = v3_sub(eye, from);
        fx len = v3_length_exact(d);

        if(len > FX(600)){
            eye = v3_add(from, v3_set_length(d, FX(600), star->up));
            fx h = v3_dot14(v3_sub(eye, from), star->up);
            if(h < FX(250)) eye = v3_add(eye, v3_scale14(star->up, FX(250) - h));
        }
        bd->cam_eye = eye;
        g_mario_held = true;
    }
    audio_stop_music();
}

static bool star_appear_busy(const Actor* a){
    return g_boss_demo.star > 0 && actor_index(a) + 1 == g_boss_demo.star;
}

static bool star_appear_done(void){ return g_boss_demo.star == 0; }

static bool star_appear_think(Actor* a){
    BossDemo* bd = &g_boss_demo;
    if(!star_appear_busy(a)) return false;
    int s = bd->star_step++;
    v3 up = a->up;
    v3 from = a->target;
    v3 risen = v3_add(from, v3_scale14(up, FX(300)));
    if(s < 80){
        int32_t t = fx_min(s, 60) * N14_ONE / 60;
        int32_t e = N14_ONE - mul14(N14_ONE - t, N14_ONE - t);
        a->position = v3_add(from, v3_scale14(up, mul14(FX(300), e)));
        a->velocity.x += DEG(10);
    } else {
        int m = s - 80;
        if(m == 0) audio_queue_effect(SFX_POWER_STAR_APPEAR, 5);
        if(m < 120){
            int32_t t = m * N14_ONE / 120;
            v3 d = v3_sub(a->home, risen);
            fx h = fx_clamp(v3_length_exact(v3_reject14(d, up)) / 2, FX(300), FX(5000));
            fx arc = mul14(h, fx_sin(mul14(t, FX_PI)) * 64);
            a->position = v3_add(v3_add(risen, v3_scale14(d, t)), v3_scale14(up, arc));
            a->velocity.x += DEG(20);
        } else {
            if(m == 120){
                a->position = a->home;
                audio_queue_effect(SFX_POWER_STAR_MOVE_END, 5);
            }
            int k = fx_min(m - 120, 90);

            int32_t t = k * N14_ONE / 90;
            int32_t e = N14_ONE - mul14(N14_ONE - t, N14_ONE - t);
            a->velocity.x += DEG(20) - mul14(DEG(17), e);
        }
        if(m >= 210){
            bd->star = 0;
            if(bd->flags & BOSS_DEMO_FLAG_STAR_CAMERA){
                bd->flags &= ~BOSS_DEMO_FLAG_STAR_CAMERA;
                if(!bd->owner) g_mario_held = false;
                g_gcam.out_valid = false;
                camera_galaxy_snap();
            }
            if(bd->star_music >= 0) audio_play_music(bd->star_music);
            else audio_play_music(game_stage_music());
        }
    }
    return true;
}

static void star_appear_figure(const Actor* a, Figure* f){
    if(star_appear_busy(a)) f->local_yaw = a->velocity.x % FX_TAU;
}

static bool boss_power_star_appear(const Actor* boss, v3 from, fx radius, bool star_camera, int music){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* s = &g_game.actors[i];
        if(s->type != OBJ_POWERSTAR || s->active || s->stage_index < 0) continue;
        if(v3_length_exact(v3_sub(s->home, boss->position)) >= radius) continue;
        power_star_reveal(s);
        star_appear_start(s, from, star_camera, music);
        return true;
    }
    return false;
}

static bool boss_demo_cast(const Actor* demo, const Actor* a){
    return star_appear_busy(a) || dino_demo_cast(demo, a);
}
