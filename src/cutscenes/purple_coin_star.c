#define PURPLE_COIN_STAR_SAMPLE_COUNT 292
enum {
    PURPLE_COIN_STAR_EVENT_START = 1,
    PURPLE_COIN_STAR_EVENT_APPEAR_SOUND = 2,
    PURPLE_COIN_STAR_EVENT_END_SOUND = 4,
    PURPLE_COIN_STAR_EVENT_END_DEMO = 8,
    PURPLE_COIN_STAR_EVENT_EXIST_LEVEL = 16
};
typedef struct {
    int32_t height_q16;
    uint16_t yaw_q12, events;
} PurpleCoinStarSample;
static const PurpleCoinStarSample purple_coin_star_samples[PURPLE_COIN_STAR_SAMPLE_COUNT] = {
    {0, 715, 17},          {514660, 1430, 16},    {1028967, 2145, 16},   {1542569, 2860, 16},
    {2055113, 3574, 16},   {2566249, 4289, 16},   {3075627, 5004, 16},   {3582896, 5719, 16},
    {4087710, 6434, 16},   {4589723, 7149, 16},   {5088589, 7864, 16},   {5583969, 8579, 16},
    {6075521, 9294, 16},   {6562910, 10008, 16},  {7045801, 10723, 16},  {7523862, 11438, 16},
    {7996768, 12153, 16},  {8464193, 12868, 16},  {8925816, 13583, 16},  {9381323, 14298, 16},
    {9830400, 15013, 16},  {10272740, 15728, 16}, {10708039, 16442, 16}, {11136000, 17157, 16},
    {11556328, 17872, 16}, {11968737, 18587, 16}, {12372942, 19302, 16}, {12768668, 20017, 16},
    {13155643, 20732, 16}, {13533602, 21447, 16}, {13902285, 22161, 16}, {14261440, 22876, 16},
    {14610822, 23591, 16}, {14950190, 24306, 16}, {15279311, 25021, 16}, {15597961, 0, 16},
    {15905921, 715, 16},   {16202980, 1430, 16},  {16488934, 2145, 16},  {16763588, 2860, 16},
    {17026752, 3574, 16},  {17278247, 4289, 16},  {17517901, 5004, 16},  {17745549, 5719, 16},
    {17961035, 6434, 16},  {18164211, 7149, 16},  {18354938, 7864, 16},  {18533086, 8579, 16},
    {18698532, 9294, 16},  {18851163, 10008, 16}, {18990874, 10723, 16}, {19117571, 11438, 16},
    {19231164, 12153, 16}, {19331578, 12868, 16}, {19418743, 13583, 16}, {19492599, 14298, 16},
    {19553096, 15013, 16}, {19600192, 15728, 16}, {19633856, 16442, 16}, {19654063, 17157, 16},
    {19660800, 17872, 16}, {19660800, 18587, 16}, {19660800, 19302, 16}, {19660800, 20017, 16},
    {19660800, 20732, 16}, {19660800, 21447, 16}, {19660800, 22161, 16}, {19660800, 22876, 16},
    {19660800, 23591, 16}, {19660800, 24306, 16}, {19660800, 25021, 16}, {19660800, 0, 16},
    {19660800, 715, 16},   {19660800, 1430, 16},  {19660800, 2145, 16},  {19660800, 2860, 16},
    {19660800, 3574, 16},  {19660800, 4289, 16},  {19660800, 5004, 16},  {19660800, 5719, 16},
    {19660800, 6434, 16},  {19660800, 7864, 2},   {20011620, 9294, 0},   {20362087, 10723, 0},
    {20711849, 12153, 0},  {21060553, 13583, 0},  {21407849, 15013, 0},  {21753387, 16442, 0},
    {22096816, 17872, 0},  {22437790, 19302, 0},  {22775963, 20732, 0},  {23110989, 22161, 0},
    {23442529, 23591, 0},  {23770241, 25021, 0},  {24093790, 715, 0},    {24412841, 2145, 0},
    {24727062, 3574, 0},   {25036128, 5004, 0},   {25339713, 6434, 0},   {25637496, 7864, 0},
    {25929163, 9294, 0},   {26214400, 10723, 0},  {26492900, 12153, 0},  {26764359, 13583, 0},
    {27028480, 15013, 0},  {27284968, 16442, 0},  {27533537, 17872, 0},  {27773902, 19302, 0},
    {28005788, 20732, 0},  {28228923, 22161, 0},  {28443042, 23591, 0},  {28647885, 25021, 0},
    {28843200, 715, 0},    {29028742, 2145, 0},   {29204270, 3574, 0},   {29369551, 5004, 0},
    {29524361, 6434, 0},   {29668481, 7864, 0},   {29801700, 9294, 0},   {29923814, 10723, 0},
    {30034628, 12153, 0},  {30133952, 13583, 0},  {30221607, 15013, 0},  {30297421, 16442, 0},
    {30361229, 17872, 0},  {30412875, 19302, 0},  {30452211, 20732, 0},  {30479098, 22161, 0},
    {30493406, 23591, 0},  {30495012, 25021, 0},  {30483803, 715, 0},    {30459674, 2145, 0},
    {30422531, 3574, 0},   {30372284, 5004, 0},   {30308858, 6434, 0},   {30232183, 7864, 0},
    {30142199, 9294, 0},   {30038856, 10723, 0},  {29922112, 12153, 0},  {29791936, 13583, 0},
    {29648303, 15013, 0},  {29491200, 16442, 0},  {29320623, 17872, 0},  {29136576, 19302, 0},
    {28939072, 20732, 0},  {28728136, 22161, 0},  {28503799, 23591, 0},  {28266103, 25021, 0},
    {28015098, 715, 0},    {27750844, 2145, 0},   {27473411, 3574, 0},   {27182874, 5004, 0},
    {26879323, 6434, 0},   {26562852, 7864, 0},   {26233566, 9294, 0},   {25891578, 10723, 0},
    {25537011, 12153, 0},  {25169995, 13583, 0},  {24790669, 15013, 0},  {24399181, 16442, 0},
    {23995687, 17872, 0},  {23580352, 19302, 0},  {23153348, 20732, 0},  {22714854, 22161, 0},
    {22265060, 23591, 0},  {21804161, 25021, 0},  {21332361, 715, 0},    {20849871, 2145, 0},
    {20356910, 3574, 0},   {19853702, 5004, 0},   {19340480, 6434, 0},   {18817485, 7864, 0},
    {18284962, 9294, 0},   {17743163, 10723, 0},  {17192348, 12153, 0},  {16632782, 13583, 0},
    {16064737, 15013, 0},  {15488488, 16442, 0},  {14904320, 17872, 0},  {14312519, 19302, 0},
    {13713380, 20732, 0},  {13107200, 22161, 0},  {12494283, 23591, 0},  {11874936, 25021, 0},
    {11249473, 715, 0},    {10618208, 2145, 0},   {9981462, 3574, 0},    {9339561, 5004, 0},
    {8692830, 6434, 0},    {8041601, 7864, 0},    {7386209, 9294, 0},    {6726989, 10723, 0},
    {6064283, 12153, 0},   {5398430, 13583, 0},   {4729776, 15013, 0},   {4058667, 16442, 0},
    {3385449, 17872, 0},   {2710473, 19302, 0},   {2034089, 20732, 0},   {1356647, 22161, 0},
    {678500, 23591, 0},    {0, 25021, 20},        {0, 694, 16},          {0, 2081, 16},
    {0, 3447, 16},         {0, 4792, 16},         {0, 6116, 16},         {0, 7419, 16},
    {0, 8700, 16},         {0, 9961, 16},         {0, 11201, 16},        {0, 12419, 16},
    {0, 13617, 16},        {0, 14794, 16},        {0, 15951, 16},        {0, 17087, 16},
    {0, 18202, 16},        {0, 19297, 16},        {0, 20371, 16},        {0, 21425, 16},
    {0, 22459, 16},        {0, 23474, 16},        {0, 24468, 16},        {0, 25442, 16},
    {0, 661, 16},          {0, 1597, 16},         {0, 2513, 16},         {0, 3410, 16},
    {0, 4288, 16},         {0, 5147, 16},         {0, 5988, 16},         {0, 6810, 16},
    {0, 7614, 16},         {0, 8399, 16},         {0, 9167, 16},         {0, 9918, 16},
    {0, 10650, 16},        {0, 11366, 16},        {0, 12064, 16},        {0, 12746, 16},
    {0, 13411, 16},        {0, 14059, 16},        {0, 14692, 16},        {0, 15308, 16},
    {0, 15909, 16},        {0, 16495, 16},        {0, 17065, 16},        {0, 17621, 16},
    {0, 18162, 16},        {0, 18688, 16},        {0, 19201, 16},        {0, 19700, 16},
    {0, 20185, 16},        {0, 20657, 16},        {0, 21116, 16},        {0, 21563, 16},
    {0, 21997, 16},        {0, 22419, 16},        {0, 22830, 16},        {0, 23229, 16},
    {0, 23617, 16},        {0, 23994, 16},        {0, 24361, 16},        {0, 24718, 16},
    {0, 25065, 16},        {0, 25402, 16},        {0, 25731, 16},        {0, 314, 16},
    {0, 625, 16},          {0, 928, 16},          {0, 1223, 16},         {0, 1511, 16},
    {0, 1792, 16},         {0, 2066, 16},         {0, 2333, 16},         {0, 2595, 16},
    {0, 2851, 16},         {0, 3101, 16},         {0, 3347, 16},         {0, 3588, 16},
    {0, 3825, 16},         {0, 4058, 16},         {0, 4287, 16},         {0, 4513, 16},
    {0, 4737, 16},         {0, 4958, 16},         {0, 5177, 16},         {0, 5395, 16},
    {0, 5611, 16},         {0, 5826, 16},         {0, 6041, 16},         {0, 6255, 24},
};
enum {
    PURPLE_COIN_STAR_HIDDEN = 208,
    PURPLE_COIN_STAR_PENDING,
    PURPLE_COIN_STAR_RISE,
    PURPLE_COIN_STAR_MOVE,
    PURPLE_COIN_STAR_WAIT
};

static bool purple_coin_star_stage(void){
    return g_game.stage_index == STAGE_DEEPDARK && g_game.scenario == 5;
}
static bool purple_coin_star_object(const StageObject* o){
    return o->type == OBJ_POWERSTAR && o->id == 666 && o->args[0] == 5 && o->scenarios == 0x10 &&
           (o->flags & 1);
}
static bool purple_coin_star_actor(const Actor* a){
    return purple_coin_star_stage() && a->type == OBJ_POWERSTAR && a->stage_index >= 0 &&
           (uint32_t)a->stage_index < g_game.stage.header->object_count &&
           purple_coin_star_object(&g_game.stage.objects[a->stage_index]);
}
static bool purple_coin_star_busy(const Actor* a){
    return purple_coin_star_actor(a) && a->state >= PURPLE_COIN_STAR_PENDING &&
           a->state <= PURPLE_COIN_STAR_MOVE;
}
static void purple_coin_star_init(Actor* a, const StageObject* o){
    if(!purple_coin_star_stage() || !purple_coin_star_object(o)) return;
    a->state = power_star_revealed(a->stage_index) ? PURPLE_COIN_STAR_WAIT : PURPLE_COIN_STAR_HIDDEN;
    a->timer = 0;
    a->value = 0;
    a->target = a->home;
    if(a->state == PURPLE_COIN_STAR_HIDDEN){
        a->active = 0;
        a->flags |= ACTOR_HIDDEN | ACTOR_WAITING;
    }
}

__attribute__((noinline)) static bool purple_coin_star_request(void){
    if(!purple_coin_star_stage() || g_game.purple_coins < 100) return false;
    const Stage* s = &g_game.stage;
    int object = -1;
    for(uint32_t i = 0; i < s->header->object_count; i++)
        if(purple_coin_star_object(&s->objects[i])){
            if(object >= 0) return false;
            object = (int)i;
        }
    if(object < 0 || object >= STAGE_MAX_OBJECTS) return false;
    if(power_star_revealed(object)) return true;
    Actor* a = NULL;
    for(int i = 0; i < g_game.actor_count; i++)
        if(g_game.actors[i].stage_index == object && purple_coin_star_actor(&g_game.actors[i])){
            a = &g_game.actors[i];
            break;
        }
    if(!a){
        a = actors_spawn_object(s, (unsigned)object);
        if(!a && stream_evict_preserving(g_game.player.figure.position, g_game.player.figure.position))
            a = actors_spawn_object(s, (unsigned)object);
    }
    if(!a) return false;

    GravitySample gravity = gravity_sample(s->header, a->home);
    a->up = gravity.up;
    a->front = v3_tangent14(a->front, a->up, V3(0, N14_ONE, 0));
    a->target = a->home;
    a->position = a->home;
    a->state = PURPLE_COIN_STAR_PENDING;
    a->timer = 0;
    a->value = 0;
    a->active = 1;
    a->flags |= ACTOR_HIDDEN | ACTOR_WAITING;
    g_power_star_revealed[object >> 3] |= (uint8_t)(1u << (object & 7));
    g_object_state[object] |= OBJECT_STATE_OUT | OBJECT_STATE_WAS_ACTIVE;
    return true;
}
static void purple_coin_star_coin_taken(void){
    if(purple_coin_star_stage() && g_game.purple_coins == 100) purple_coin_star_request();
}
static Actor* purple_coin_star_demo_actor(void){
    if(!purple_coin_star_stage()) return NULL;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(a->active && purple_coin_star_busy(a)) return a;
    }
    return NULL;
}
static bool purple_coin_star_paused(void){
    Actor* a = purple_coin_star_demo_actor();
    return a && (a->state == PURPLE_COIN_STAR_RISE || a->state == PURPLE_COIN_STAR_MOVE);
}
static bool purple_coin_star_can_demo(void){
    int action = g_game.player.action;
    return action != MA_DEATH && action != MA_STAR_GET && action != MA_STAR_LAUNCH && action != MA_TALK &&
           !g_mario_held && !cutscene_blocks(BLOCK_DEMO_START | BLOCK_SCENE_START);
}

__attribute__((noinline)) static bool purple_coin_star_tick(const KeyState* keys){
    Actor* a = purple_coin_star_demo_actor();
    if(!a) return false;
    if(a->state == PURPLE_COIN_STAR_PENDING){
        if(!purple_coin_star_can_demo()) return false;
        const StageObject* o = &g_game.stage.objects[a->stage_index];
        if(o->camera < 0 || (uint32_t)o->camera >= g_game.stage.header->camera_count) return false;
        a->state = PURPLE_COIN_STAR_RISE;
        a->timer = 0;
        a->flags &= ~ACTOR_HIDDEN;
        camera_event_start(o->camera, 1, a->home, a->up, a->front);
    }
    unsigned index = a->state == PURPLE_COIN_STAR_RISE ? (unsigned)a->timer : 81u + (unsigned)a->timer;
    if(index >= PURPLE_COIN_STAR_SAMPLE_COUNT) return false;
    const PurpleCoinStarSample* sample = &purple_coin_star_samples[index];

    v3 rise = V3((int32_t)((int64_t)a->up.x * sample->height_q16 / (INT64_C(1) << 30)),
                 (int32_t)((int64_t)a->up.y * sample->height_q16 / (INT64_C(1) << 30)),
                 (int32_t)((int64_t)a->up.z * sample->height_q16 / (INT64_C(1) << 30)));
    a->position = v3_add(a->home, rise);
    a->value = (int16_t)sample->yaw_q12;
    camera_event_target(a->position, a->up, a->front);
    if(sample->events & PURPLE_COIN_STAR_EVENT_APPEAR_SOUND) audio_queue_effect(SFX_POWER_STAR_APPEAR, 5);
    if(sample->events & PURPLE_COIN_STAR_EVENT_END_SOUND)
        audio_queue_effect(water_query(a->position, a->up, NULL) ? SFX_POWER_STAR_MOVE_END_WATER
                                                                 : SFX_POWER_STAR_MOVE_END,
                           5);
    g_game.player.velocity = V3(0, 0, 0);
    mario_ride_carry();
    if(sample->events & PURPLE_COIN_STAR_EVENT_END_DEMO){
        a->position = a->home;
        a->state = PURPLE_COIN_STAR_WAIT;
        a->timer = 0;
        a->flags &= ~ACTOR_WAITING;
        camera_event_cancel();
    } else if(a->state == PURPLE_COIN_STAR_RISE && a->timer == 80){
        a->state = PURPLE_COIN_STAR_MOVE;
        a->timer = 0;
    } else a->timer++;
    return true;
}
static void purple_coin_star_wait_tick(Actor* a){
    if(!purple_coin_star_actor(a) || a->state != PURPLE_COIN_STAR_WAIT) return;
    if(a->timer >= 120) a->timer = 0;
}
static fx purple_coin_star_yaw(const Actor* a){
    int yaw = a->value;
    if(a->state == PURPLE_COIN_STAR_WAIT) yaw = (yaw + (a->timer * 25736) / 120) % 25736;
    return yaw >> 4;
}
