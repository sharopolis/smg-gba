#define CAMERA_DISTANCE FX(846)
#define CAMERA_HEIGHT   FX(308)
#define CAMERA_LOOK_UP  FX(100)

static inline fx camera_zoomed(fx v){ return fx_mul(v, camera_zooms[g_game.camera_zoom]); }

static void camera_set_zoom(Camera* camera, int zoom){
    fx old = camera_zooms[g_game.camera_zoom], scale = camera_zooms[zoom];
    camera->height = (fx)(((int64_t)camera->height * scale) / old);
    g_game.camera_zoom = zoom;
    camera->distance = camera_zoomed(CAMERA_DISTANCE);
}

static void camera_build_view(const Camera* camera, CameraView* out){
    v3 f = camera->front, u = camera->up;
    v3 left = v3_cross14(u, f);
    m3 m = {{v3_n14_to_fx(left), v3_n14_to_fx(u), v3_n14_to_fx(f)}};
    out->view = m;
    out->direction = m.r[2];

    static const m3 to_clip = {{{-VIEW_CLIP_X, 0, 0}, {0, -VIEW_CLIP_Y, 0}, {0, 0, 1024}}};
    out->to_screen.m = m3_mul(&to_clip, &m);
    out->to_screen.o = m3_transform_vector(&out->to_screen.m, v3_neg(camera->position));
}

static void camera_update_angles(Camera* camera){
    v3 f = camera->front;

    camera->orientation.x = fx_atan2_small(-(f.x >> 6), f.y >> 6);
    fx horizontal = (fx)isqrt64((uint64_t)((int64_t)f.x * f.x + (int64_t)f.y * f.y)) >> 6;
    camera->orientation.y = fx_atan2_small(f.z >> 6, horizontal);
    camera->orientation.z = 0;
}

static EWRAM_BSS struct {
    v3 heading, up;
    fx pitch;
} g_manual_camera;

static int32_t g_follow_col = FX(4000);

static void camera_snap_behind(Camera* camera, const Player* player){
    g_game.camera_orbit_held = false;
    g_follow_col = FX(4000);
    camera->smooth_up = player->up;
    camera->heading = player->front;
    camera->target = v3_add(player->figure.position, v3_scale14(player->up, CAMERA_LOOK_UP));
    camera->distance = camera_zoomed(CAMERA_DISTANCE);
    camera->height = camera_zoomed(CAMERA_HEIGHT);
    camera->position = v3_add(v3_sub(camera->target, v3_scale14(camera->heading, camera->distance)),
                              v3_scale14(camera->smooth_up, camera->height));
    camera->front = v3_direction14(v3_sub(camera->target, camera->position), camera->heading);
    camera->up = v3_tangent14(camera->smooth_up, camera->front, camera->smooth_up);
    camera_update_angles(camera);
}

static int g_cam_ticks;
static uint32_t g_cam_tick_acc;

static void camera_count_ticks(fx dt){
    g_cam_tick_acc += (uint32_t)dt * 60;
    int n = (int)(g_cam_tick_acc >> 8);
    g_cam_tick_acc &= 255;
    g_cam_ticks = n > 8 ? 8 : n;
}

static int32_t cam_ease(int32_t r, int n){
    int32_t keep = N14_ONE;
    for(int i = 0; i < n; i++) keep = mul14(keep, N14_ONE - r);
    return N14_ONE - keep;
}

static CamRot cam_rotation_towards(v3 a, v3 b, int32_t frac, fx max_angle, v3 hint){
    CamRot r = {V3(0, 0, N14_ONE), 0};
    v3 k = v3_cross14(a, b);
    int32_t c = v3_dot14(a, b);
    int32_t s = v3_length_exact(k);
    if(c > N14_ONE - 3) return r;
    fx angle = fx_atan2_fast(s, c);
    if(s < 400) r.axis = v3_tangent14(hint, a, V3(N14_ONE, 0, 0));
    else r.axis = v3_direction14(k, hint);
    angle = (fx)(((int64_t)angle * frac) >> 14);
    r.angle = fx_min(angle, max_angle);
    return r;
}

static inline v3 cam_rotate(v3 v, CamRot r){ return r.angle ? v3_rotate_axis14(v, r.axis, r.angle) : v; }

static v3 cam_slerp14(v3 a, v3 b, int32_t t, v3 hint){
    if(t >= N14_ONE) return b;
    if(v3_dot14(a, b) > -R14(0.5f)) return v3_blend14(a, b, t);
    CamRot r = cam_rotation_towards(a, b, t, FX_PI, hint);
    return v3_direction14(cam_rotate(a, r), b);
}

__attribute__((noinline)) static void camera_manual_prepare(uint16_t held, uint16_t pressed, bool allowed){
    if(!allowed || !(held & KEY_L) || (held & KEY_R)) return;
    Camera* cam = &g_game.camera;
    const Player* p = &g_game.player;
    if(held & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)){
        if(!g_game.camera_orbit_held){
            g_manual_camera.up = p->up;
            v3 d = v3_sub(cam->position, cam->target);
            g_manual_camera.heading = v3_tangent14(v3_neg(d), p->up, p->front);
            fx h = v3_dot14(d, p->up);
            fx flat = v3_length_exact(v3_reject14(d, p->up));
            g_manual_camera.pitch = fx_clamp(fx_atan2_fast(h, flat), FXC(0.0873f), FXC(1.309f));
            g_game.camera_orbit_held = true;
        }
    } else if((pressed & KEY_L) && g_game.camera_orbit_held){
        g_game.camera_orbit_held = false;
        camera_manual_resume_source();
    }
}

__attribute__((noinline)) static void camera_manual_update(uint16_t held, bool allowed){
    if(!allowed || !g_game.camera_orbit_held) return;
    Camera* cam = &g_game.camera;
    const Player* p = &g_game.player;
    int n = g_cam_ticks;
    v3 side = v3_cross14(g_manual_camera.up, g_manual_camera.heading);
    CamRot rot =
        cam_rotation_towards(g_manual_camera.up, p->up, cam_ease(R14(0.12f), n), n * FXC(0.105f), side);
    g_manual_camera.up = v3_direction14(cam_rotate(g_manual_camera.up, rot), p->up);
    g_manual_camera.heading =
        v3_tangent14(cam_rotate(g_manual_camera.heading, rot), g_manual_camera.up, p->front);
    if((held & KEY_L) && !(held & KEY_R)){
        int yaw = ((held & KEY_RIGHT) != 0) - ((held & KEY_LEFT) != 0);
        int pitch = ((held & KEY_UP) != 0) - ((held & KEY_DOWN) != 0);
        g_manual_camera.heading = v3_direction14(
            v3_rotate_axis14(g_manual_camera.heading, g_manual_camera.up, yaw * n * FXC(0.037f)),
            g_manual_camera.heading);
        g_manual_camera.pitch =
            fx_clamp(g_manual_camera.pitch + pitch * n * FXC(0.024f), FXC(0.0873f), FXC(1.309f));
    }

    bool ground_view =
        p->on_ground && (p->action == MA_WAIT || p->action == MA_SKID || p->action == MA_SQUAT ||
                         p->action == MA_SQUAT_SLIDE || p->action == MA_LAND || p->action == MA_WALL_PUSH);
    v3 display_up = ground_view ? p->up : g_manual_camera.up;
    v3 target = v3_add(p->figure.position, v3_scale14(display_up, CAMERA_LOOK_UP));
    v3 dir = v3_add(v3_scale14(v3_neg(g_manual_camera.heading), fx_cos(g_manual_camera.pitch) << 6),
                    v3_scale14(g_manual_camera.up, fx_sin(g_manual_camera.pitch) << 6));
    v3 eye = v3_add(target, v3_scale14(dir, camera_zoomed(FX(900))));
    if(ground_view){
        fx height = v3_dot14(v3_sub(eye, target), p->up);
        if(height < FX(100)) eye = v3_add(eye, v3_scale14(p->up, FX(100) - height));
    }
    RayHit hit;
    if(collision_ray(target, eye, &hit, COL_NO_CAMERA)){
        int32_t clear_t = hit.t > 1600 ? hit.t - 1600 : hit.t / 2;
        eye = v3_add(target, v3_scale14(v3_sub(eye, target), clear_t));
        if(eye.x == target.x && eye.y == target.y && eye.z == target.z)
            eye = v3_add(hit.point, v3_scale14(hit.normal, FX(4)));
    }
    cam->position = eye;
    cam->target = target;
    cam->front = v3_direction14(v3_sub(target, eye), g_manual_camera.heading);
    cam->up = v3_tangent14(display_up, cam->front, display_up);
    cam->smooth_up = g_manual_camera.up;
    cam->heading = g_manual_camera.heading;
    camera_update_angles(cam);
}

static void camera_update(fx dt, const KeyState* keys){
    if(mario_launcher_camera(dt, keys)){
        camera_update_angles(&g_game.camera);
        return;
    }
    camera_count_ticks(dt);
    if(cutscene_camera(&g_game.camera)){
        camera_update_angles(&g_game.camera);
        return;
    }
    KeyState automatic = *keys;
    if(g_game.camera_orbit_held){
        automatic.held &= ~KEY_L;
        automatic.pressed &= ~KEY_L;
    }
    keys = &automatic;
    if(!camera_galaxy_update(dt, keys)) camera_follow_update(keys);
    camera_visibility_update();
}

static void camera_follow_update(const KeyState* keys){
    Game* game = &g_game;
    Player* player = &game->player;
    Camera* camera = &game->camera;
    int n = g_cam_ticks;

    v3 side = v3_cross14(camera->smooth_up, camera->heading);
    CamRot rot =
        cam_rotation_towards(camera->smooth_up, player->up, cam_ease(R14(0.12f), n), n * FXC(0.105f), side);
    if(rot.angle){
        camera->smooth_up = v3_direction14(cam_rotate(camera->smooth_up, rot), player->up);
        camera->heading = cam_rotate(camera->heading, rot);
        camera->position = v3_add(camera->target, cam_rotate(v3_sub(camera->position, camera->target), rot));
    }

    v3 focus = v3_add(player->figure.position, v3_scale14(camera->smooth_up, CAMERA_LOOK_UP));
    camera->target =
        v3_add(camera->target, v3_scale14(v3_sub(focus, camera->target), cam_ease(R14(0.22f), n)));
    v3 lag = v3_sub(camera->target, focus);
    if(v3_length_squared64(lag) > (uint64_t)FX(400) * FX(400))
        camera->target = v3_add(focus, v3_set_length(lag, FX(400), camera->heading));

    if(keys->pressed & KEY_L) camera->recentre_ticks = 36;
    bool orbit = (keys->held & KEY_L) && (keys->held & (KEY_LEFT | KEY_RIGHT));
    if(orbit){
        int turn = ((keys->held & KEY_RIGHT) != 0) - ((keys->held & KEY_LEFT) != 0);
        camera->heading = v3_rotate_axis14(camera->heading, camera->smooth_up, turn * n * FXC(0.037f));
        camera->recentre_ticks = 0;
    } else if(camera->recentre_ticks > 0){
        camera->recentre_ticks = fx_max(camera->recentre_ticks - n, 0);
        camera->heading =
            cam_slerp14(camera->heading, v3_tangent14(player->front, camera->smooth_up, camera->heading),
                        cam_ease(R14(0.1f), n), camera->smooth_up);
    } else if(!player->figure.hidden){
        v3 to_target = v3_sub(camera->target, camera->position);
        v3 want =
            v3_tangent14(v3_direction14(to_target, camera->heading), camera->smooth_up, camera->heading);
        camera->heading = cam_slerp14(camera->heading, want, cam_ease(R14(0.1f), n), camera->smooth_up);
    }
    camera->heading = v3_tangent14(camera->heading, camera->smooth_up, player->front);

    if(keys->held & KEY_L){
        int lift = ((keys->held & KEY_UP) != 0) - ((keys->held & KEY_DOWN) != 0);

        camera->height =
            fx_clamp(camera->height + lift * n * FX(8), camera_zoomed(FX(156)), camera_zoomed(FX(450)));
    }

    v3 want = v3_add(v3_sub(camera->target, v3_scale14(camera->heading, camera->distance)),
                     v3_scale14(camera->smooth_up, camera->height));

    RayHit hit;
    v3 from = camera->target;
    v3 d = v3_sub(want, from);
    int32_t len = v3_length_exact(d);
    int32_t allowed = len;
    if(collision_ray(from, want, &hit, COL_NO_CAMERA)){
        allowed = (int32_t)(((int64_t)len * fx_max(hit.t - 1600, 0)) >> 14);

        if(allowed < FX(300)) allowed = fx_min(FX(300), len);
    }
    if(allowed < g_follow_col) g_follow_col += mul14(allowed - g_follow_col, cam_ease(R14(0.25f), n));
    else g_follow_col += mul14(allowed - g_follow_col, cam_ease(R14(0.08f), n));
    if(g_follow_col < len) want = v3_add(from, v3_set_length(d, g_follow_col, camera->heading));
    camera->position =
        v3_add(camera->position, v3_scale14(v3_sub(want, camera->position), cam_ease(R14(0.3f), n)));

    camera->front = v3_direction14(v3_sub(camera->target, camera->position), camera->heading);
    camera->up = v3_tangent14(camera->smooth_up, camera->front, camera->smooth_up);
    camera_update_angles(camera);
}
