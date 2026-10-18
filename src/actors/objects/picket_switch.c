enum { BIG_PICKET_WAIT = 64, BIG_PICKET_DROP, BIG_PICKET_DONE };

static const int16_t big_picket_vertices[12][3] = {
    {-143, 0, 0},  {-143, 150, 0}, {-72, 150, -129}, {-72, 150, 129}, {-71, 0, -128}, {-71, 0, 128},
    {71, 0, -128}, {71, 0, 128},   {72, 150, -129},  {72, 150, 129},  {143, 0, 0},    {143, 150, 0}};
static const uint16_t big_picket_triangles[16][4] = {
    {8, 6, 4, 0},  {8, 4, 2, 0},  {4, 0, 1, 0},   {4, 1, 2, 0},  {1, 0, 5, 0},   {1, 5, 3, 0},
    {3, 5, 7, 0},  {3, 7, 9, 0},  {7, 10, 11, 0}, {7, 11, 9, 0}, {11, 10, 6, 0}, {8, 11, 6, 0},
    {1, 3, 11, 0}, {3, 9, 11, 0}, {1, 11, 2, 0},  {8, 2, 11, 0}};
static const HoneyColGroup big_picket_group = {{-143, 0, -129}, {143, 150, 129}, 0, 16};
static const HoneyColMesh big_picket_mesh = {big_picket_vertices, big_picket_triangles, &big_picket_group, 1};

static const int16_t big_picket_heights[5] = {-75, -116, -132, -133, -147};

static int big_picket_pose_key(const Actor* a){
    if(a->state == BIG_PICKET_WAIT) return 0;
    if(a->state == BIG_PICKET_DONE) return 4;
    return fx_clamp((a->anim_frame >> 8) / 2, 0, 4);
}

static void big_picket_init(Actor* a, const StageObject* o){
    a->radius = 0;
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    bool done = switch_on(stage_object_switch(o, SWITCH_A)) || stream_part_state(a->stage_index) == 1;
    actor_set_state(a, done ? BIG_PICKET_DONE : BIG_PICKET_WAIT, ANIM_PICKETSWITCHBIG_SECOND);
}

static int16_t g_hipdrop_target = -1;
static void hipdrop_target_update(void){
    Player* p = &g_game.player;
    g_hipdrop_target = -1;
    if(p->action != MA_HIPDROP_LAND || p->action_timer != 0) return;
    RayHit hit;
    v3 feet = p->figure.position;
    if(collision_ray(v3_add(feet, v3_scale14(p->up, FX(40))), v3_sub(feet, v3_scale14(p->up, FX(60))), &hit,
                     0))
        g_hipdrop_target = (int16_t)dyncol_owner(hit.tri);
}

static void big_picket_think(Actor* a){
    Player* p = &g_game.player;
    const StageObject* o = actor_stage_object(a);
    v3 base;
    if(a->state == BIG_PICKET_WAIT){
        a->anim_frame = 0;
        if(g_hipdrop_target == actor_index(a)){
            actor_set_state(a, BIG_PICKET_DROP, ANIM_PICKETSWITCHBIG_SECOND);
            audio_play_effect(SFX_HIPDROP_LAND, 3);
            camera_shake(2);
        }
    } else if(a->state == BIG_PICKET_DROP){
        if(a->timer >= 8){
            if(o) switch_set(stage_object_switch(o, SWITCH_A), true);
            stream_part_state_set(a->stage_index, 1);
            actor_set_state(a, BIG_PICKET_DONE, -1);
        }
    } else {
        a->anim_frame = FX(7) << 8;
    }
    int16_t scale[3] = {(int16_t)(a->figure.scale.x * 4), (int16_t)(a->figure.scale.y * 4),
                        (int16_t)(a->figure.scale.z * 4)};
    base = v3_add(a->position, v3_scale14(a->up, big_picket_heights[big_picket_pose_key(a)]));
    dyncol_add(&big_picket_mesh, base, a->up, a->front, scale, actor_index(a), 0, p->figure.position,
               FX(600));
}
