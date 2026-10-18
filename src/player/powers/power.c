typedef struct {
    uint16_t ticks;
    int16_t owner;
    uint8_t mode, stage;
    int32_t water_distance;
} MarioPowerState;
static EWRAM_BSS MarioPowerState g_power;
static int mario_power_mode(void){ return g_power.mode; }
static bool mario_power_owns(int object){
    return g_power.mode && g_power.stage == g_game.stage_index && g_power.owner == object;
}

static int mario_power_music(int mode){ return mode == 2 ? MUSIC_BGM_FIRE : mode == 3 ? MUSIC_BGM_ICE : -1; }

#define BGM_LEAD_IN_REFRESHES 60
static void mario_power_end(void){
    int mode = g_power.mode;
    if(mario_power_music(mode) >= 0) audio_stop_sub_music();
    g_power = (MarioPowerState){0};
    if(mode == 3 && g_game.player.action == MA_SKATE){
        mario_skate_reset();
        mario_set_action(MA_WAIT);
        mario_restart_anim(ANIM_MARIO_WAIT);
    }
}
static void mario_power_start(int mode, int object){
    if(g_power.mode != mode) mario_power_end();
    bee_end();
    g_power = (MarioPowerState){
        .ticks = 1200, .owner = (int16_t)object, .mode = (uint8_t)mode, .stage = (uint8_t)g_game.stage_index};
    audio_play_sub_music(mario_power_music(mode), BGM_LEAD_IN_REFRESHES);
}
static void mario_power_tick(void){
    if(g_power.ticks && !--g_power.ticks) mario_power_end();
}

static bool mario_power_map_inside(v3 position, fx thickness){
    CollisionCandidates candidates;
    v3 reach = V3(thickness + 2, thickness + 2, thickness + 2);
    collision_gather(v3_sub(position, reach), v3_add(position, reach), &candidates);
    for(int i = 0; i < candidates.count; i++){
        const CollisionTriangle* t = col_tri(candidates.index[i]);
        v3 normal = v3_from_i16(t->normal);
        v3 a = col_vertex(t, 0, position), b = col_vertex(t, 1, position), c = col_vertex(t, 2, position);
        fx distance = -v3_dot14(a, normal);
        if(distance > 2 || distance < -thickness) continue;
        v3 projected = v3_scale14(normal, -distance);
        v3 closest = closest_point_to_origin(a, b, c, projected);
        if(v3_length_squared64(v3_sub(closest, projected)) <= 4) return true;
    }
    return false;
}

static bool mario_power_water(void){
    if(g_power.mode != 3) return false;
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    WaterSample water;
    bool body = water_query(p->figure.position, p->gravity_up, &water);
    if(body && water_query(v3_add(p->figure.position, v3_scale14(p->gravity_up, 110)), p->gravity_up, NULL)){
        v3 raise = v3_scale14(p->gravity_up, 100);
        p->figure.position = v3_add(p->figure.position, raise);
        m->position16 = v3_add(m->position16, V3(raise.x << 8, raise.y << 8, raise.z << 8));
        m->jumping = false;
        m->jump_vec = V3(0, 0, 0);
        if(p->action == MA_SWIM || p->action == MA_WALL_SLIDE || p->action == MA_DAMAGE_FALL){
            mario_swim_reset();
            mario_set_action(MA_FALL);
            mario_restart_anim(ANIM_MARIO_FALL);
        }
    }
    p->water_state = WATER_DRY;
    if(m->stick > R14(.1)) g_power.water_distance = 0;
    v3 dir = horizontal(p->velocity, p->gravity_up);
    dir = v3_length_squared64(dir) > 4 ? v3_direction14(dir, p->front) : p->front;
    for(int i = 0; i < 10; i++){
        v3 check = v3_add(v3_add(p->figure.position, v3_scale14(dir, 100)),
                          v3_scale14(p->gravity_up, 100 - (int)g_power.water_distance));
        bool solid = mario_power_map_inside(check, 40);
        const WaterVolume* found = NULL;
        if(water_query_raw(check, p->gravity_up, &water, &found)){
            if(g_power.water_distance < 210 && !solid){
                WaterSample surface =
                    found ? water_sample_at_volume(found, p->figure.position, p->gravity_up) : water;
                if(surface.depth < 100 && (!p->on_ground || m->walk_index))
                    icestep_create(v3_add(surface.surface, v3_scale14(dir, 170)), p->gravity_up, p->front);
            }
            g_power.water_distance = 0;
            return true;
        }
        if(solid){
            g_power.water_distance = 0;
            return true;
        }
        g_power.water_distance += 40;
    }
    return true;
}
