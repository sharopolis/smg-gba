typedef struct {
    int32_t pos[3];
    int32_t half_x, height, half_z;
    bool rot90, cylinder;
} PullBackArea;
static const PullBackArea pull_back_areas[] = {
    {{1650, -17150, -200}, 25000, 16280, 25000, false, false},
    {{-5800, -36450, 19900}, 32500, 82727, 591, false, false},
    {{-6250, -35400, -16900}, 32500, 82727, 591, false, false},
    {{22400, -32800, 800}, 27500, 70000, 500, true, false},
    {{-17550, -32800, 2350}, 27500, 70000, 500, true, false},
    {{250, 17100, -200}, 25000, 1000, 25000, false, false},
    {{150, -2700, 300}, 2500, 2500, 0, false, true},
};

static bool observatory_pull_back(v3 pos){
    int32_t gx = -pos.x, gy = pos.z, gz = pos.y;
    for(int i = 0; i < (int)(sizeof pull_back_areas / sizeof pull_back_areas[0]); i++){
        const PullBackArea* a = &pull_back_areas[i];
        int32_t dx = gx - a->pos[0], dy = gy - a->pos[1], dz = gz - a->pos[2];
        if(dy < 0 || dy >= a->height) continue;
        if(a->cylinder){
            if((int64_t)dx * dx + (int64_t)dz * dz < (int64_t)a->half_x * a->half_x) return true;
            continue;
        }
        if(a->rot90){
            int32_t t = dx;
            dx = dz;
            dz = t;
        }
        if(dx >= -a->half_x && dx < a->half_x && dz >= -a->half_z && dz < a->half_z) return true;
    }
    return false;
}

static void observatory_load(void){ g_game.coins = 0; }

static const LevelHooks observatory_level = {
    .galaxy_stage = STAGE_OBSERVATORY,
    .features = LEVEL_RAISED_COUNTERS,
    .load_before_player = observatory_load,
    .pull_back = observatory_pull_back,
};
