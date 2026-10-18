#define GALAXY_MAP_SELECT_FRAMES 60
#define GALAXY_MAP_ZOOM_FRAMES   20

static EWRAM_BSS GalaxyMap g_galaxy_map;

static inline fx sin_turn16(int32_t turns){ return fx_sin((fx)(((int64_t)turns * FX_TAU) >> 16)); }
static inline fx cos_turn16(int32_t turns){ return fx_cos((fx)(((int64_t)turns * FX_TAU) >> 16)); }

static int32_t step_ratio14(int step, int frames){
    return step >= frames ? N14_ONE : step <= 0 ? 0 : (step << 14) / frames;
}
static int32_t ease_in_out14(int32_t t){ return (N14_ONE - (cos_turn16((t * 32768) >> 14) << 6)) >> 1; }
static int32_t ease_out14(int32_t t){ return sin_turn16((t * 16384) >> 14) << 6; }
static int32_t ease_in14(int32_t t){ return N14_ONE - (cos_turn16((t * 16384) >> 14) << 6); }

static v3 v3_lerp14(v3 a, v3 b, int32_t t){
    return V3(a.x + (fx)(((int64_t)(b.x - a.x) * t) >> 14), a.y + (fx)(((int64_t)(b.y - a.y) * t) >> 14),
              a.z + (fx)(((int64_t)(b.z - a.z) * t) >> 14));
}

static v3 v3_scale14_wide(v3 dir, int32_t length){
    return V3((fx)(((int64_t)dir.x * length) >> 14), (fx)(((int64_t)dir.y * length) >> 14),
              (fx)(((int64_t)dir.z * length) >> 14));
}

static v3 galaxy_map_offset_position(v3 at, int32_t ox, int32_t oy, int32_t oz, v3 toward, v3 up){
    v3 front = v3_tangent14(v3_direction14(toward, V3(0, N14_ONE, 0)), up, V3(0, N14_ONE, 0));
    v3 side = v3_cross14(up, front);
    return v3_add(
        at, v3_add(v3_scale14_wide(side, ox), v3_add(v3_scale14_wide(up, oy), v3_scale14_wide(front, oz))));
}

static void galaxy_map_handle_axes(v3* x, v3* y, v3* z){
    v3 up = g_galaxy_map.handle_up;
    v3 front = v3_tangent14(g_galaxy_map.handle_front, up, g_galaxy_map.handle_front);
    v3 side = v3_cross14(up, front);
    fx s = sin_turn16(g_galaxy_map.handle_rot), c = cos_turn16(g_galaxy_map.handle_rot);

    *x = v3_sub(v3_scale(side, c), v3_scale(front, s));
    *z = v3_add(v3_scale(side, s), v3_scale(front, c));
    *y = up;
}

static const int32_t galaxy_map_orbit_radius[AD_MINI_COUNT] = {4000, 6200, 8100, 10300, 12000};

static void galaxy_map_orbit_basis(int i, v3* u, v3* v){
    v3 hx, hy, hz;
    galaxy_map_handle_axes(&hx, &hy, &hz);
    if(i < 4){
        *u = hx;
        *v = hz;
        return;
    }

    fx s20 = sin_turn16(DEG_TO_TURN16(20)), c20 = cos_turn16(DEG_TO_TURN16(20)),
       s45 = sin_turn16(DEG_TO_TURN16(45)), c45 = cos_turn16(DEG_TO_TURN16(45));
    *u = v3_sub(v3_scale(hx, c45), v3_scale(hz, s45));

    *v = v3_add(v3_add(v3_scale(hx, fx_mul(c20, s45)), v3_scale(hy, -s20)), v3_scale(hz, fx_mul(c20, c45)));
}

static v3 galaxy_map_orbit_point(int i, int32_t turns, v3 u, v3 v){
    fx c = cos_turn16(turns), s = sin_turn16(turns);
    return v3_scale14_wide(v3_add(v3_scale(u, c), v3_scale(v, s)), galaxy_map_orbit_radius[i]);
}

static v3 galaxy_map_galaxy_position(int i){
    v3 u, v;
    galaxy_map_orbit_basis(i, &u, &v);
    return galaxy_map_orbit_point(i, g_galaxy_map.orbit[i], u, v);
}

static void galaxy_map_load_vram(void){
    hud_lifeup_restore();
    hud_copy_tiles(0, dome_tiles, (int)(dome_tiles_SIZE / 32));
    const uint16_t* pal = (const uint16_t*)dome_palette;
    for(uint32_t i = 0; i < dome_palette_SIZE / 2; i++) OBJ_PALETTE[i] = pal[i];
    for(int i = 0; i < SLOT_COUNT; i++) g_hud.slot_art[i] = -1;
    g_galaxy_map.slot_art = 0xFF;
    g_galaxy_map.vram = 1;
}

static void galaxy_map_restore_vram(void){
    hud_copy_tiles(0, hud_tiles, (int)(hud_tiles_SIZE / 32));
    const uint16_t* pal = (const uint16_t*)hud_palette;
    for(uint32_t i = 0; i < hud_palette_SIZE / 2; i++) OBJ_PALETTE[i] = pal[i];
    for(int i = 0; i < SLOT_COUNT; i++) g_hud.slot_art[i] = -1;
    g_galaxy_map.vram = 0;
}

static void galaxy_map_sprite(int x, int y, int tile, int bank, int obj){
    if(g_oam_count >= 128 || x <= -64 || x >= 240 || y <= -64 || y >= 160) return;
    OamEntry* e = &g_oam[g_oam_count++];
    e->attr0 = (uint16_t)((y & 0xFF) | (obj >> 2) << 14);
    e->attr1 = (uint16_t)((x & 0x1FF) | (obj & 3) << 14);
    e->attr2 = (uint16_t)((512 + tile) | bank << 12);
}

static void galaxy_map_art(int art, int x, int y){
    const AdArt* a = &ad_arts[art];
    int base = 0;
    if(a->slot){
        if(g_galaxy_map.slot_art != art){
            hud_copy_tiles(AD_SLOT_BASE, dome_slot + a->data, a->tiles);
            g_galaxy_map.slot_art = (uint8_t)art;
        }
        base = AD_SLOT_BASE;
    }
    for(int i = 0; i < a->count; i++){
        const AdPiece* p = &ad_pieces[a->first + i];
        galaxy_map_sprite(x + p->x, y + p->y, base + p->tile, a->bank, p->obj);
    }
}

static void galaxy_map_set_nerve(int nerve){
    g_galaxy_map.nerve = (uint8_t)nerve;
    g_galaxy_map.step = 0;
}

static bool galaxy_map_selectable(int i){ return ad_minis[i].galaxy >= 0; }

static uint8_t galaxy_map_nearest_colour(int r, int g, int b){
    int best = 1, bd = 1 << 30;
    for(int i = 1; i < 256; i++){
        uint16_t c = game_palette[i];
        int dr = (c & 31) - r, dg = (c >> 5 & 31) - g, db = (c >> 10 & 31) - b;
        int d = dr * dr + dg * dg + db * db;
        if(d < bd){
            bd = d;
            best = i;
        }
    }
    return (uint8_t)best;
}

static void galaxy_map_open(void){
    Game* game = &g_game;
    const Camera* c = &game->camera;
    game->scene = SCENE_GALAXY_MAP;
    galaxy_map_set_nerve(GALAXY_MAP_SELECT_START);
    v3 back = V3(-c->front.x, -c->front.y, 0);
    g_galaxy_map.handle_front = v3_direction14(back, V3(0, N14_ONE, 0));
    v3 side = v3_cross14(V3(0, 0, N14_ONE), g_galaxy_map.handle_front);
    g_galaxy_map.handle_up = v3_rotate_axis14(V3(0, 0, N14_ONE), side, (fx)(40 * FX_PI / 180));
    g_galaxy_map.handle_rot = 0;
    g_galaxy_map.handle_speed = 0;
    static const int16_t start_rot[AD_MINI_COUNT] = {0, 210, 80, 330, 130};
    for(int i = 0; i < AD_MINI_COUNT; i++){
        g_galaxy_map.orbit[i] = DEG_TO_TURN16(230) * i;
        g_galaxy_map.spin[i] = DEG_TO_TURN16(start_rot[i]);
        g_galaxy_map.zoom[i] = GALAXY_MAP_ZOOM_FRAMES;
        g_galaxy_map.zoom_in[i] = 0;
    }
    g_galaxy_map.pointed = 0;
    g_galaxy_map.selected = -1;
    g_galaxy_map.cam_start = c->position;
    g_galaxy_map.cam_from_pos = c->position;
    g_galaxy_map.cam_from_target = V3(0, 0, 0);
    g_galaxy_map.cam_from_up = V3(0, 0, N14_ONE);
    g_galaxy_map.eye = c->position;
    g_galaxy_map.target = V3(0, 0, 0);
    g_galaxy_map.up = V3(0, 0, N14_ONE);
    galaxy_map_camera_commit();
    g_galaxy_map.fovy_x10 = 600;

    g_galaxy_map.cam_to_pos =
        galaxy_map_offset_position(V3(0, 0, 0), 0, 0, -22000, v3_neg(c->position), V3(0, 0, N14_ONE));
    g_galaxy_map.cam_to_target = V3(0, 0, 0);
    g_galaxy_map.cam_to_up = V3(0, 0, N14_ONE);
    g_galaxy_map.mario_pos = V3(0, 0, 0);
    g_galaxy_map.mario_up = game->player.up;
    g_galaxy_map.mario_front = game->player.front;
    g_galaxy_map.mario_spin = 0;
    g_galaxy_map.mario_hidden = 0;
    g_galaxy_map.star_shown = 0;
    g_galaxy_map.ring_colour = galaxy_map_nearest_colour(0x13 >> 3, 0xB1 >> 3, 0xFF >> 3);
    galaxy_map_load_vram();
    screen_fade_start(30, -1, true);
    audio_play_effect(SFX_MENU_OPEN, 4);
}

static void galaxy_map_camera_tick(int32_t ease){
    g_galaxy_map.eye = v3_lerp14(g_galaxy_map.cam_from_pos, g_galaxy_map.cam_to_pos, ease);
    g_galaxy_map.target = v3_lerp14(g_galaxy_map.cam_from_target, g_galaxy_map.cam_to_target, ease);
    g_galaxy_map.up = v3_blend14(g_galaxy_map.cam_from_up, g_galaxy_map.cam_to_up, ease);
    galaxy_map_camera_commit();
}

static void galaxy_map_camera_commit(void){
    g_galaxy_map.cf = v3_direction14(v3_sub(g_galaxy_map.target, g_galaxy_map.eye), V3(0, N14_ONE, 0));
    g_galaxy_map.cu = v3_tangent14(g_galaxy_map.up, g_galaxy_map.cf, V3(0, 0, N14_ONE));
    g_galaxy_map.cl = v3_cross14(g_galaxy_map.cu, g_galaxy_map.cf);
}

static void galaxy_map_camera_basis(v3* front, v3* up){
    *front = g_galaxy_map.cf;
    *up = g_galaxy_map.cu;
}

static void galaxy_map_mario_face_camera(void){
    v3 front, up;
    galaxy_map_camera_basis(&front, &up);
    g_galaxy_map.mario_up = up;
    g_galaxy_map.mario_front = front;
}

static v3 galaxy_map_mario_zoom_position(void){
    v3 sel = galaxy_map_galaxy_position(g_galaxy_map.selected);
    v3 hx, hy, hz;
    galaxy_map_handle_axes(&hx, &hy, &hz);
    return galaxy_map_offset_position(sel, 400, 2550, -7600, v3_sub(sel, g_galaxy_map.eye), hy);
}

static void galaxy_map_confirm_targets(void){
    v3 sel = galaxy_map_galaxy_position(g_galaxy_map.selected);
    v3 hx, hy, hz;
    galaxy_map_handle_axes(&hx, &hy, &hz);
    v3 zoom_out = galaxy_map_offset_position(V3(0, 0, 0), 0, 0, -22000, v3_neg(g_galaxy_map.cam_start),
                                             V3(0, 0, N14_ONE));
    g_galaxy_map.cam_to_target = v3_add(sel, v3_scale14_wide(hy, 200));
    g_galaxy_map.cam_to_pos = galaxy_map_offset_position(sel, 0, 3000, -8500, v3_sub(sel, zoom_out), hy);
    g_galaxy_map.cam_to_up = hy;
}

static void galaxy_map_tick(const KeyState* keys){
    Game* game = &g_game;
    int st = g_galaxy_map.step++;
    mario_anim_tick();
    switch(g_galaxy_map.nerve){
    case GALAXY_MAP_SELECT_START:
        galaxy_map_camera_tick(ease_in_out14(step_ratio14(st, GALAXY_MAP_SELECT_FRAMES)));
        galaxy_map_mario_face_camera();
        galaxy_map_galaxies_tick(true);
        if(st >= GALAXY_MAP_SELECT_FRAMES){
            g_galaxy_map.cam_from_pos = g_galaxy_map.cam_to_pos;
            g_galaxy_map.cam_from_target = g_galaxy_map.cam_to_target;
            g_galaxy_map.cam_from_up = g_galaxy_map.cam_to_up;
            galaxy_map_set_nerve(GALAXY_MAP_SELECT);
        }
        break;
    case GALAXY_MAP_SELECT:
        galaxy_map_camera_tick(N14_ONE);
        galaxy_map_mario_face_camera();

        {
            int32_t want = keys->held & KEY_L ? -DEG_TO_TURN16(2) : keys->held & KEY_R ? DEG_TO_TURN16(2) : 0;
            int32_t d = want - g_galaxy_map.handle_speed, lim = DEG_TO_TURN16(0.2f);
            if(want == 0) g_galaxy_map.handle_speed = g_galaxy_map.handle_speed * 95 / 100;
            else g_galaxy_map.handle_speed += d > lim ? lim : d < -lim ? -lim : d;
            g_galaxy_map.handle_rot += g_galaxy_map.handle_speed;
        }
        galaxy_map_point_move(keys);
        galaxy_map_galaxies_tick(true);
        if(keys->pressed & KEY_B){
            audio_play_effect(SFX_MENU_SELECT, 4);
            screen_fade_start(20, 1, true);
            galaxy_map_set_nerve(GALAXY_MAP_SELECT_CANCEL);
        } else if((keys->pressed & (KEY_A | KEY_START)) && g_galaxy_map.pointed >= 0 &&
                  galaxy_map_selectable(g_galaxy_map.pointed)){
            audio_play_effect(SFX_MENU_SELECT, 4);
            g_galaxy_map.selected = g_galaxy_map.pointed;
            g_galaxy_map.cam_from_pos = g_galaxy_map.eye;
            g_galaxy_map.cam_from_target = g_galaxy_map.target;
            g_galaxy_map.cam_from_up = g_galaxy_map.up;
            galaxy_map_confirm_targets();
            galaxy_map_set_nerve(GALAXY_MAP_CONFIRM_START);
        }
        break;
    case GALAXY_MAP_SELECT_CANCEL:
        galaxy_map_camera_tick(N14_ONE);
        galaxy_map_galaxies_tick(true);
        if(st >= 20){
            Player* p = &game->player;
            galaxy_map_restore_vram();
            game->scene = SCENE_PLAY;
            p->up = V3(0, 0, N14_ONE);
            mario_hold_move(V3(0, 0, -C_BODY_HEIGHT), g_galaxy_map.mario_front);
            mario_hold_release(V3(0, 0, 0));
            camera_snap_behind(&game->camera, p);
            camera_galaxy_snap();
            screen_fade_start(20, -1, true);
        }
        break;
    case GALAXY_MAP_CONFIRM_START: {
        int32_t e = ease_out14(step_ratio14(st, GALAXY_MAP_CONFIRM_FRAMES));
        galaxy_map_confirm_targets();
        galaxy_map_camera_tick(e);
        galaxy_map_mario_face_camera();
        g_galaxy_map.mario_pos = v3_lerp14(V3(0, 0, 0), galaxy_map_mario_zoom_position(), e);
        galaxy_map_galaxies_tick(false);
        if(st >= GALAXY_MAP_CONFIRM_FRAMES) galaxy_map_set_nerve(GALAXY_MAP_CONFIRM);
        break;
    }
    case GALAXY_MAP_CONFIRM:
        galaxy_map_confirm_targets();
        galaxy_map_camera_tick(N14_ONE);
        galaxy_map_mario_face_camera();
        g_galaxy_map.mario_pos = galaxy_map_mario_zoom_position();
        galaxy_map_galaxies_tick(false);
        if(st < 20) break;
        if(keys->pressed & (KEY_A | KEY_START)){
            audio_play_effect(SFX_MENU_SELECT, 4);
            audio_stop_music();
            g_galaxy_map.star_pos = g_galaxy_map.mario_pos;
            galaxy_map_set_nerve(GALAXY_MAP_LAUNCH_APPEAR);
        } else if(keys->pressed & KEY_B){
            audio_play_effect(SFX_MENU_SELECT, 4);
            g_galaxy_map.cam_from_pos = g_galaxy_map.eye;
            g_galaxy_map.cam_from_target = g_galaxy_map.target;
            g_galaxy_map.cam_from_up = g_galaxy_map.up;
            g_galaxy_map.cam_to_pos = galaxy_map_offset_position(
                V3(0, 0, 0), 0, 0, -22000, v3_neg(g_galaxy_map.cam_start), V3(0, 0, N14_ONE));
            g_galaxy_map.cam_to_target = V3(0, 0, 0);
            g_galaxy_map.cam_to_up = V3(0, 0, N14_ONE);
            g_galaxy_map.cam_start = g_galaxy_map.cam_to_pos;
            galaxy_map_set_nerve(GALAXY_MAP_CONFIRM_CANCEL);
        }
        break;
    case GALAXY_MAP_CONFIRM_CANCEL: {
        int32_t e = ease_in14(step_ratio14(st, GALAXY_MAP_CONFIRM_FRAMES));
        galaxy_map_camera_tick(e);
        galaxy_map_mario_face_camera();
        g_galaxy_map.mario_pos = v3_lerp14(g_galaxy_map.mario_pos, V3(0, 0, 0), e);
        galaxy_map_galaxies_tick(false);
        if(st >= GALAXY_MAP_CONFIRM_FRAMES){
            g_galaxy_map.cam_from_pos = g_galaxy_map.cam_to_pos;
            g_galaxy_map.cam_from_target = g_galaxy_map.cam_to_target;
            g_galaxy_map.cam_from_up = g_galaxy_map.cam_to_up;
            g_galaxy_map.mario_pos = V3(0, 0, 0);
            g_galaxy_map.selected = -1;
            galaxy_map_set_nerve(GALAXY_MAP_SELECT);
        }
        break;
    }
    default: galaxy_map_launch_tick(); break;
    }
    game->fade_target = game->fade;
}

static void galaxy_map_point_move(const KeyState* keys){
    int d = keys->pressed & (KEY_RIGHT | KEY_DOWN) ? 1 : keys->pressed & (KEY_LEFT | KEY_UP) ? -1 : 0;
    if(!d) return;
    g_galaxy_map.pointed = (int8_t)((g_galaxy_map.pointed + d + AD_MINI_COUNT) % AD_MINI_COUNT);
    audio_play_effect(SFX_MENU_MOVE, 1);
}

static void galaxy_map_galaxies_tick(bool moving){
    for(int i = 0; i < AD_MINI_COUNT; i++){
        g_galaxy_map.spin[i] += DEG_TO_TURN16(0.4f);
        if(moving && g_galaxy_map.selected < 0) g_galaxy_map.orbit[i] -= DEG_TO_TURN16(0.05f);
        bool want = g_galaxy_map.selected >= 0
                        ? i == g_galaxy_map.selected
                        : i == g_galaxy_map.pointed && g_galaxy_map.nerve == GALAXY_MAP_SELECT;
        if(want != g_galaxy_map.zoom_in[i] && g_galaxy_map.zoom[i] >= GALAXY_MAP_ZOOM_FRAMES){
            g_galaxy_map.zoom_in[i] = want;
            g_galaxy_map.zoom[i] = 0;
        }
        if(g_galaxy_map.zoom[i] < GALAXY_MAP_ZOOM_FRAMES) g_galaxy_map.zoom[i]++;
    }
}

static int32_t galaxy_map_galaxy_scale(int i){
    int32_t t = step_ratio14(g_galaxy_map.zoom[i], GALAXY_MAP_ZOOM_FRAMES);
    if(g_galaxy_map.zoom_in[i]){
        int32_t r;
        if(t < N14_ONE / 2) r = sin_turn16(t * 2) << 6;
        else {
            int32_t u = (t - N14_ONE / 2) * 2;
            int32_t cosv = cos_turn16(u * 3) << 6;
            r = N14_ONE + mul14(mul14(N14_ONE - cosv, N14_ONE - u), -1311);
        }
        return 166 + (int32_t)((90 * (int64_t)r) >> 14);
    }
    return 256 - (int32_t)((90 * (int64_t)ease_out14(t)) >> 14);
}

static void galaxy_map_apply_pose(Figure* figure){
    figure->position = g_galaxy_map.mario_pos;
    figure->use_basis = true;
    figure->basis_up = g_galaxy_map.mario_up;
    figure->basis_front = g_galaxy_map.mario_front;
    figure->local_yaw = g_galaxy_map.mario_spin;
    figure->local_pitch = 0;
    figure->local_roll = 0;
    figure->has_shadow = false;
    figure->hidden = g_galaxy_map.mario_hidden;
    figure->transform_dirty = true;
}

static bool galaxy_map_return_from_missions(void){
    if(!g_galaxy_map.from_dome) return false;
    g_galaxy_map.from_dome = 0;
    game_reload_keep_health(STAGE_ASTRODOME, 0, 0);
    return true;
}

static void galaxy_map_clear_return(void){ g_galaxy_map.from_dome = 0; }

static void galaxy_map_view(CameraView* out, v3* front_out, v3* up_out){
    v3 f = g_galaxy_map.cf, u = g_galaxy_map.cu;
    *front_out = f;
    *up_out = u;
    v3 left = g_galaxy_map.cl;
    m3 m = {{v3_n14_to_fx(left), v3_n14_to_fx(u), v3_n14_to_fx(f)}};
    out->view = m;
    out->direction = m.r[2];

    int32_t cy = g_galaxy_map.fovy_x10 == 600 ? 1774 : g_galaxy_map.fovy_x10 == 450 ? VIEW_CLIP_Y : 3835;
    int32_t cx = cy * VIEW_CLIP_X / VIEW_CLIP_Y;
    m3 to_clip = {{{-cx, 0, 0}, {0, -cy, 0}, {0, 0, 1024}}};
    out->to_screen.m = m3_mul(&to_clip, &m);
    out->to_screen.o = m3_transform_vector(&out->to_screen.m, v3_neg(g_galaxy_map.eye));
}

static bool galaxy_map_project(v3 p, int* sx, int* sy, int32_t* depth){
    v3 f = g_galaxy_map.cf, u = g_galaxy_map.cu, left = g_galaxy_map.cl;
    v3 d = v3_sub(p, g_galaxy_map.eye);
    int32_t z = v3_dot14(d, f);
    if(z < 256) return false;
    int32_t focal = g_galaxy_map.fovy_x10 == 600 ? 139 : g_galaxy_map.fovy_x10 == 450 ? 193 : 299;

    int32_t inv = (focal << 14) / z;
    *sx = 120 - ((v3_dot14(d, left) * inv) >> 14);
    *sy = 80 - ((v3_dot14(d, u) * inv) >> 14);
    *depth = z;
    return true;
}

static void galaxy_map_line(uint8_t* fb, int x0, int y0, int x1, int y1, unsigned colour){
    int dx = fx_abs(x1 - x0), dy = -fx_abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1,
        err = dx + dy;
    if(dx > 200 || dy < -200) return;
    for(;;){
        if((unsigned)x0 < FRAME_WIDTH && (unsigned)y0 < FRAME_HEIGHT) frame_put(fb, x0, y0, colour);
        if(x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if(e2 >= dy){
            err += dy;
            x0 += sx;
        }
        if(e2 <= dx){
            err += dx;
            y0 += sy;
        }
    }
}

static void galaxy_map_draw_orbits(uint8_t* fb){
    if(g_galaxy_map.selected >= 0) return;
    for(int i = 0; i < AD_MINI_COUNT; i++){
        v3 u, v;
        galaxy_map_orbit_basis(i, &u, &v);
        int px = 0, py = 0;
        bool have = false;
        for(int k = 0; k <= 64; k++){
            int x, y;
            int32_t z;
            bool on = galaxy_map_project(galaxy_map_orbit_point(i, k * 1024, u, v), &x, &y, &z);
            x >>= 1;
            y >>= 1;
            if(on && have) galaxy_map_line(fb, px, py, x, y, g_galaxy_map.ring_colour);
            px = x;
            py = y;
            have = on;
        }
    }
}

static void galaxy_map_draw_figure(uint8_t* fb, const uint8_t* model, v3 pos, v3 up, v3 front,
                                   int32_t scale256, const CameraView* view){
    Figure f;
    figure_init(&f, model);
    f.position = pos;
    f.use_basis = true;
    f.basis_up = up;
    f.basis_front = v3_tangent14(front, up, front);
    int32_t s = 64 * scale256 / 256;
    f.scale = V3(s, s, s);
    figure_update_transform(&f);
    v3 vp = xform_point(&view->to_screen, f.transform.o);
    if(vp.z < FX(100) * 4) return;
    draw_figure(fb, &f, view);
}

static void galaxy_map_draw(uint8_t* fb){
    Game* game = &g_game;
    CameraView view;
    v3 front, up;
    bool jumpout =
        g_galaxy_map.nerve == GALAXY_MAP_LAUNCH_JUMPOUT || g_galaxy_map.nerve == GALAXY_MAP_LAUNCH_END;
    galaxy_map_view(&view, &front, &up);

    Camera c = game->camera;
    c.front = front;
    c.up = up;
    camera_update_angles(&c);
    const uint8_t* sky = game->stage.sky;
    if(jumpout && stage_blobs[STAGE_GOODEGG]){
        const StageHeader* h = (const StageHeader*)stage_blobs[STAGE_GOODEGG];
        sky = (const uint8_t*)h + h->sky;
    }
    draw_sky(fb, sky, c.orientation.x, c.orientation.y);
    int saved_view = game->view_distance;
    game->view_distance = VIEW_MODE_DEFAULT;
    if(jumpout){
        galaxy_map_draw_figure(fb, figure_astrodomedemoastrogalaxy,
                               V3(AD_DEMO_OBS_X, AD_DEMO_OBS_Y, AD_DEMO_OBS_Z), V3(0, 0, N14_ONE),
                               V3(0, N14_ONE, 0), 256, &view);
    } else {
        galaxy_map_draw_orbits(fb);

        int order[AD_MINI_COUNT], n = 0;
        int32_t depth[AD_MINI_COUNT];
        v3 pos[AD_MINI_COUNT];
        for(int i = 0; i < AD_MINI_COUNT; i++){
            if(g_galaxy_map.selected >= 0 && i != g_galaxy_map.selected) continue;
            pos[i] = galaxy_map_galaxy_position(i);
            depth[i] = v3_dot14(v3_sub(pos[i], g_galaxy_map.eye), front);
            int k = n++;
            while(k > 0 && depth[order[k - 1]] < depth[i]){
                order[k] = order[k - 1];
                k--;
            }
            order[k] = i;
        }
        v3 hx, hy, hz;
        galaxy_map_handle_axes(&hx, &hy, &hz);
        for(int k = 0; k < n; k++){
            int i = order[k];

            fx s = sin_turn16(g_galaxy_map.spin[i]), cs = cos_turn16(g_galaxy_map.spin[i]);
            v3 gf = v3_direction14(v3_add(v3_scale(hx, s), v3_scale(hz, cs)), hz);
            galaxy_map_draw_figure(fb, ad_minis[i].figure, pos[i], hy, gf, galaxy_map_galaxy_scale(i), &view);
        }
        if(g_galaxy_map.star_shown)
            galaxy_map_draw_figure(fb, figure_launchstar, g_galaxy_map.star_pos, g_galaxy_map.star_up,
                                   g_galaxy_map.star_front, 256, &view);
    }
    Figure* m = &game->player.figure;
    if(!m->hidden){
        v3 vp = xform_point(&view.to_screen, m->transform.o);
        if(vp.z >= FX(40) * 4) draw_figure(fb, m, &view);
        if(g_galaxy_map.nerve < GALAXY_MAP_LAUNCH_APPEAR){
            Figure bubble;
            capture_bubble_init(
                &bubble, v3_add(g_galaxy_map.mario_pos, v3_scale14(g_galaxy_map.mario_up, C_BODY_HEIGHT)),
                front, up, 20);
            vp = xform_point(&view.to_screen, bubble.transform.o);
            if(vp.z >= FX(40) * 4) draw_figure(fb, &bubble, &view);
        }
    }
    game->view_distance = saved_view;
}

static void galaxy_map_digits(int x, int y, int value){
    char buf[4];
    int n = 0;
    if(value < 0) value = 0;
    if(value > 999) value = 999;
    do {
        buf[n++] = (char)('0' + value % 10);
        value /= 10;
    } while(value);
    int x16 = x << 4;
    for(int i = n - 1; i >= 0; i--, x16 += AD_DIGIT_ADV16)
        galaxy_map_art(AD_ART_DIGIT0 + buf[i] - '0', x16 >> 4, y);
}

static void galaxy_map_draw_hud(void){
    Game* game = &g_game;
    if(!g_galaxy_map.vram || g_galaxy_map.nerve >= GALAXY_MAP_LAUNCH_APPEAR) return;
    int st = g_galaxy_map.step;
    bool select = g_galaxy_map.nerve == GALAXY_MAP_SELECT;

    if(select && g_galaxy_map.pointed >= 0){
        int x, y;
        int32_t z;
        if(galaxy_map_project(galaxy_map_galaxy_position(g_galaxy_map.pointed), &x, &y, &z))
            galaxy_map_art(AD_ART_HAND, x + 10 + AD_HAND_DX, y + 10 + AD_HAND_DY);
    }
    if(g_galaxy_map.nerve == GALAXY_MAP_CONFIRM && st >= 10){
        int x = fx_max(AD_START_X, AD_BACK_X + ad_arts[AD_ART_BACK].w + 2);
        galaxy_map_art(AD_ART_HAND, x + AD_START_W - 24, AD_START_Y + AD_START_H / 2);
        galaxy_map_art(AD_ART_START, x, AD_START_Y);
        galaxy_map_art(AD_ART_BACK, AD_BACK_X, AD_BACK_Y);
    }
    if(select) galaxy_map_art(AD_ART_BACK, AD_BACK_X, AD_BACK_Y);

    if(g_galaxy_map.nerve != GALAXY_MAP_SELECT_START || st > 20){
        galaxy_map_art(AD_ART_COUNTER, AD_COUNTER_X, AD_COUNTER_Y);
        galaxy_map_digits(AD_COUNTER_X + AD_COUNTER_DIGITS_DX, AD_COUNTER_Y + AD_COUNTER_DIGITS_DY,
                          game->stars_total);
    }
    int g = g_galaxy_map.selected >= 0 ? g_galaxy_map.selected : select ? g_galaxy_map.pointed : -1;
    if(g < 0) return;
    const AdMini* mg = &ad_minis[g];

    if(select){
        int plate = mg->galaxy == GALAXY_GOODEGG     ? AD_ART_PLATE_GOODEGG
                    : mg->galaxy == GALAXY_HONEYHIVE ? AD_ART_PLATE_HONEYHIVE
                                                     : AD_ART_PLATE_UNKNOWN;
        int tip_dx = plate == AD_ART_PLATE_GOODEGG     ? AD_PLATE_GOODEGG_TIP_DX
                     : plate == AD_ART_PLATE_HONEYHIVE ? AD_PLATE_HONEYHIVE_TIP_DX
                                                       : AD_PLATE_UNKNOWN_TIP_DX;
        int tip_dy = plate == AD_ART_PLATE_GOODEGG     ? AD_PLATE_GOODEGG_TIP_DY
                     : plate == AD_ART_PLATE_HONEYHIVE ? AD_PLATE_HONEYHIVE_TIP_DY
                                                       : AD_PLATE_UNKNOWN_TIP_DY;
        int x, y;
        int32_t z;
        if(galaxy_map_project(v3_add(galaxy_map_galaxy_position(g), V3(0, 0, 1500)), &x, &y, &z))
            galaxy_map_art(plate, x - tip_dx, y - tip_dy);
    }

    int info = mg->galaxy == GALAXY_GOODEGG     ? AD_ART_INFO_GOODEGG
               : mg->galaxy == GALAXY_HONEYHIVE ? AD_ART_INFO_HONEYHIVE
                                                : AD_ART_INFO_HATENA;
    if(mg->galaxy >= 0){
        const GalaxyInfo* gi = &galaxies[mg->galaxy];
        int x16 = AD_STAR_ON_X << 4, k = 0;
        for(int m = 0; m < gi->mission_count && k < 8; m++){
            if(mission_kinds[mg->galaxy][m] != MISSION_NORMAL) continue;
            bool got = game->stars_collected[mg->galaxy] >> gi->star_ids[m] & 1;
            galaxy_map_art(got ? AD_ART_STAR_ON : AD_ART_STAR_OFF, x16 >> 4,
                           got ? AD_STAR_ON_Y : AD_STAR_OFF_Y);
            x16 += AD_STAR_ADV16;
            k++;
        }
    }
    galaxy_map_art(info, AD_INFO_X, AD_INFO_Y);
}

static void galaxy_map_release(void){
    if(g_galaxy_map.vram) galaxy_map_restore_vram();
}

static const GameScene galaxy_map_scene = {
    .tick = galaxy_map_tick,
    .draw = galaxy_map_draw,
    .draw_hud = galaxy_map_draw_hud,
    .pose_player = galaxy_map_apply_pose,
    .release = galaxy_map_release,
};
