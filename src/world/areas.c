typedef struct {
    bool active, abyss;
    int step;
    v3 centre, rel, axis;
} AreaDeath;

static EWRAM_BSS AreaDeath g_area_death;

static void areas_reset(void){
    g_area_death = (AreaDeath){0};
    g_mario_tower.on = false;
}

static void areas_die(bool abyss){
    Player* p = &g_game.player;
    if(p->action == MA_DEATH) return;
    mario_set_action(MA_DEATH);
    mario_restart_anim(ANIM_MARIO_DIEFALL);
    audio_play_effect(SFX_DIE, 5);
    g_area_death.abyss = abyss;
}

static void areas_black_hole(v3 centre){
    Player* p = &g_game.player;
    AreaDeath* d = &g_area_death;
    areas_die(false);
    d->active = true;
    d->step = 0;
    d->centre = centre;
    d->rel = v3_sub(p->figure.position, centre);
    d->axis = v3_neg(g_game.camera.front);
    p->action_timer = -100;
}

static void areas_black_hole_step(void){
    Player* p = &g_game.player;
    AreaDeath* d = &g_area_death;
    int s = ++d->step;
    int32_t k = s < 180 ? (int32_t)isqrt32((uint32_t)((s << 14) / 180) << 14) : N14_ONE;
    fx ang = fx_min((fx)(((int64_t)s * FXC(0.0018f * 64) * k) >> 20), FXC(0.7f));
    d->rel = v3_rotate_axis14(d->rel, d->axis, ang);
    d->axis = v3_blend14(d->axis, v3_neg(g_game.camera.front), R14(0.01f));
    int f = 180 - s;
    if(f < 0) f = 0;
    d->rel = V3(d->rel.x * f / (1 + f), d->rel.y * f / (1 + f), d->rel.z * f / (1 + f));
    fx scale = fx_max(64 - s * 64 / 500, 45);
    p->figure.scale = V3(scale, scale, scale);
    p->figure.position = v3_add(d->centre, d->rel);
    g_mario.position16 = V3(p->figure.position.x << 8, p->figure.position.y << 8, p->figure.position.z << 8);
    if(s >= 165) mario_set_hidden(true);
}

static void areas_switch(const StageArea* a, bool inside){
    if(a->sw < 0) return;
    if(inside && (a->args[2] == -1 || g_game.player.on_ground)) switch_set(a->sw, a->args[1] == -1);
    else if(!inside && a->args[0] != -1) switch_set(a->sw, false);
}

static void areas_tick(void){
    Game* game = &g_game;
    Player* p = &game->player;
    MarioPhysics* m = &g_mario;
    if(!game->stage.header->galaxy) return;
    if(p->action == MA_DEATH){
        if(g_area_death.active) areas_black_hole_step();
        else if(g_area_death.abyss){
            m->jump_vec =
                v3_add(v3_scale14(m->jump_vec, R14(0.99f)), v3_scale14(game->camera.front, G(0.35f)));
            m->position16 = v3_add(m->position16, m->jump_vec);
            p->figure.position = V3(m->position16.x >> 8, m->position16.y >> 8, m->position16.z >> 8);
        }
        return;
    }
    if(p->action == MA_STAR_LAUNCH) return;
    v3 pos = p->figure.position;
    g_mario_tower.on = false;
    uint8_t scen = gravity_scenario_bit();
    const Stage* st = &game->stage;

    uint32_t n = st->header->area_count;
    const uint16_t* list =
        st->header->area_grid ? bound_grid_list(st->header, st->header->area_grid, pos, &n) : NULL;
    for(uint32_t j = 0; j < n; j++){
        const StageArea* a = &st->areas[list ? list[j] : j];
        if(a->cls == AREA_CAMERA || !(a->scenarios & scen)) continue;
        bool inside = gcam_area_contains(a, pos);
        switch(a->cls){
        case AREA_DEATH:
            if(inside && a->args[0] <= 0){
                areas_die(true);
                return;
            }
            break;
        case AREA_BLACKHOLE:
            if(inside){
                areas_black_hole(gcam_v32(a->centre));
                return;
            }
            break;
        case AREA_RESTART:
            if(inside && (a->args[3] == -1 || p->on_ground)){
                game->restart_id = a->args[0];
                game->restart_zone = a->zone;
            }
            break;
        case AREA_SWITCH: areas_switch(a, inside); break;
        case AREA_TOWER:
            if(inside){
                g_mario_tower.on = true;
                g_mario_tower.base = gcam_v32(a->centre);
                g_mario_tower.axis = gcam_v16(a->axis[1]);
                g_mario_tower.radius = a->extent[0];
            }
            break;
        }
    }

    if(m->jumping && m->air_frames > 300){
        v3 down = v3_neg(p->up);
        fx drop = v3_dot14(v3_sub(pos, m->last_safe_position), down);
        if(drop > FX(28000)){
            RayHit hit;
            bool ground = collision_ray(v3_add(pos, v3_scale14(p->up, FX(100))),
                                        v3_add(pos, v3_scale14(down, FX(2500))), &hit, 0);
            if(!ground || m->void_ticks > 0) areas_die(true);
        }
    }
}
