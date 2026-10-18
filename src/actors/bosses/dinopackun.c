enum {
    DINOPACKUN_IDLE = 64,
    DINOPACKUN_EGG_TURN,
    DINOPACKUN_EGG_WALK,
    DINOPACKUN_PUNCHED,
    DINOPACKUN_EGG_BROKEN,
    DINOPACKUN_CRY,
    DINOPACKUN_START,
    DINOPACKUN_TURN,
    DINOPACKUN_WALK,
    DINOPACKUN_FIND,
    DINOPACKUN_CHASE,
    DINOPACKUN_COOLDOWN,
    DINOPACKUN_ATTACKHIT,
    DINOPACKUN_DAMAGE,
    DINOPACKUN_ANGRY,
    DINOPACKUN_LASTDAMAGE,
    DINOPACKUN_DOWN,
    DINOPACKUN_GONE,
    DINOPACKUN_OPENING,
    DINOPACKUN_STAR
};
enum {
    DINO_BALL_WAIT = 64,
    DINO_BALL_SHOOT,
    DINO_BALL_CHARGE,
    DINO_BALL_REVERSE,
    DINO_BALL_REBOUND,
    DINO_BALL_LOCK
};
enum { DINO_PART_ACTIVE = 64 };
enum { DINO_PART_SHELL, DINO_PART_FLOWER, DINO_PART_CHAIN };
enum { DINO_FLAG_CHAIN = 1, DINO_FLAG_FLOWER = 2, DINO_FLAG_HOLD = 4, DINO_FLAG_AIM = 8 };
enum { DINO_TAIL_PULL_NONE, DINO_TAIL_PULL_EGG, DINO_TAIL_PULL_EGG_PUNCHED, DINO_TAIL_PULL_PUNCHED };
#define DINO_NODES    8
#define DINO_FOVY_TAN 1404

typedef struct {
    int16_t boss;
    int16_t cam, cam_step;
    uint16_t lock;
    uint8_t flags;
    int8_t dir;
    int8_t out;
    int16_t home_up[3], home_front[3];
    v3 home;
    v3 centre;
    fx radius;
    v3 node[DINO_NODES];
    int16_t vel[DINO_NODES - 1][3];
    bool area_camera;
} DinoState;
static EWRAM_BSS DinoState g_dino;
static int16_t g_dino_tail_pose[DINO_NODES * 6][3];

static const uint8_t dino_bend[9] = {30, 30, 25, 20, 15, 10, 10, 10, 10};
static const uint8_t dino_part_radius[DINO_NODES] = {0, 80, 70, 60, 55, 50, 45, 50};
static const uint8_t dino_ring_radius[DINO_NODES] = {85, 79, 70, 62, 55, 49, 43, 43};

static void dino_reset(void){
    g_dino.boss = -1;
    g_dino.cam = -1;
    g_dino.flags = 0;
    g_dino.area_camera = false;
}

static inline Actor* dino_actor(int16_t slot, int type, const Actor* boss){
    if(slot < 0 || slot >= g_game.actor_count) return NULL;
    Actor* p = &g_game.actors[slot];
    return p->active && p->type == type && (!boss || p->value == actor_index(boss)) ? p : NULL;
}
static inline Actor* dino_part(const Actor* a, int k, int type){ return dino_actor(a->args[k], type, a); }
static inline Actor* dino_ball(const Actor* a){ return dino_part(a, 3, OBJ_DINOPACKUNTAIL); }

static v3 dino_world(v3 pos, v3 up, v3 front, int32_t r, int32_t f, int32_t u){
    v3 right = v3_cross14(front, up);
    return v3_add(pos, v3_add(v3_add(v3_scale14(right, r), v3_scale14(front, f)), v3_scale14(up, u)));
}
static v3 dino_world_dir(v3 up, v3 front, const int8_t* d){
    return v3_direction14(dino_world(V3(0, 0, 0), up, front, d[0] * 129, d[1] * 129, d[2] * 129), front);
}
static inline v3 dino_home_up(void){ return v3_from_i16(g_dino.home_up); }
static inline v3 dino_home_front(void){ return v3_from_i16(g_dino.home_front); }

static int dino_anim_key(const Actor* a, int* local){
    const AnimClip* c = &figure_clips(a->figure.model)[a->anim];
    int key = (a->anim_frame >> 8) / c->ticks_per_key;
    if(c->loop) key %= c->key_count;
    else if(key >= c->key_count) key = c->key_count - 1;
    if(local) *local = key;
    return c->first_key + key;
}

static v3 dino_head(const Actor* a){
    if(a->health == 0) return v3_add(a->position, v3_scale14(a->up, FX(200)));
    const DinoKey* k = &dino_keys[dino_anim_key(a, NULL)];
    return dino_world(a->position, a->up, a->front, k->head[0] * 8, k->head[1] * 8, k->head[2] * 8);
}

static void dino_set(Actor* a, int st){
    static const int16_t anims[] = {ANIM_DINOPACKUN_OPENINGDEMO,  ANIM_DINOPACKUN_EGGWALK,
                                    ANIM_DINOPACKUN_EGGWALK,      -1,
                                    ANIM_DINOPACKUN_EGGDAMAGETOP, ANIM_DINOPACKUN_CRYDEMO,
                                    ANIM_DINOPACKUN_FIND,         ANIM_DINOPACKUN_TURNRIGHT,
                                    ANIM_DINOPACKUN_WALK,         ANIM_DINOPACKUN_FIND,
                                    ANIM_DINOPACKUN_CHASE,        ANIM_DINOPACKUN_COOLDOWN,
                                    ANIM_DINOPACKUN_ATTACKHIT,    ANIM_DINOPACKUN_DAMAGETOP,
                                    ANIM_DINOPACKUN_ANGRYDEMO,    ANIM_DINOPACKUN_LASTDAMAGETOP,
                                    ANIM_DINOPACKUN_DOWNDEMO,     ANIM_DINOPACKUN_DOWNDEMO,
                                    ANIM_DINOPACKUN_OPENINGDEMO,  ANIM_DINOPACKUN_DOWNDEMO};
    int anim = anims[st - DINOPACKUN_IDLE];
    if(a->health >= 2 && (st == DINOPACKUN_WALK || st == DINOPACKUN_TURN)) anim = ANIM_DINOPACKUN_ANGRYWALK;
    if(a->health >= 2 && st == DINOPACKUN_CHASE) anim = ANIM_DINOPACKUN_ANGRYCHASE;
    if(st == DINOPACKUN_TURN && a->health < 2 && (a->flags & 0x10)) anim = ANIM_DINOPACKUN_TURNLEFT;
    if(st == DINOPACKUN_PUNCHED) anim = -1;
    if(st == DINOPACKUN_DAMAGE)
        anim = g_dino.dir == 2   ? ANIM_DINOPACKUN_DAMAGERIGHT
               : g_dino.dir == 1 ? ANIM_DINOPACKUN_DAMAGELEFT
                                 : ANIM_DINOPACKUN_DAMAGETOP;
    actor_set_state(a, st, anim);
}

static void dino_tail_root(const Actor* a, v3* pos, v3* dir){
    const DinoKey* k = &dino_keys[dino_anim_key(a, NULL)];
    *pos = dino_world(a->position, a->up, a->front, k->tail_root[0] * 8, k->tail_root[1] * 8,
                      k->tail_root[2] * 8);
    *dir = dino_world_dir(a->up, a->front, k->tail_dir);
}

static void dino_chain_from_anim(const Actor* a, Actor* ball){
    DinoState* g = &g_dino;
    int local;
    dino_anim_key(a, &local);
    int first = dino_tail_first[a->anim];
    v3 dir;
    dino_tail_root(a, &g->node[0], &dir);
    for(int k = 1; k <= DINO_NODES; k++){
        v3 p;
        if(first >= 0){
            const int8_t* t = dino_tail_keys[first + local][k - 1];
            p = dino_world(a->position, a->up, a->front, t[0] * 8, t[1] * 8, t[2] * 8);
        } else {
            p = v3_add(g->node[0], v3_scale14(dir, FX(90) * k));
        }
        if(k < DINO_NODES){
            g->node[k] = p;
            g->vel[k - 1][0] = g->vel[k - 1][1] = g->vel[k - 1][2] = 0;
        } else if(ball){
            ball->position = p;
            ball->velocity = V3(0, 0, 0);
        }
    }
}

static void dino_chain_tick(Actor* a){
    DinoState* g = &g_dino;
    Actor* ball = dino_ball(a);
    if(!(g->flags & DINO_FLAG_CHAIN)){
        dino_chain_from_anim(a, ball);
        return;
    }
    v3 root_dir;
    dino_tail_root(a, &g->node[0], &root_dir);

    if(g->out != DINO_TAIL_PULL_NONE){
        static const int16_t out[4][3] = {{0, 0, 0}, {0, -320, 60}, {0, -400, 0}, {0, -300, 100}};
        v3 at = dino_world(a->position, a->up, a->front, out[g->out][0], out[g->out][1], out[g->out][2]);
        int32_t r = g->out == DINO_TAIL_PULL_EGG ? N14_ONE : R14(0.1f);
        g->node[1] = v3_add(g->node[1], v3_scale14(v3_sub(at, g->node[1]), r));
    }
    v3 p[DINO_NODES + 1], nd[DINO_NODES + 1], dv[DINO_NODES + 1];
    for(int k = 0; k < DINO_NODES; k++){
        p[k] = g->node[k];
        dv[k] = V3(0, 0, 0);
    }
    p[DINO_NODES] = ball ? ball->position : v3_add(p[DINO_NODES - 1], v3_scale14(root_dir, FX(120)));
    dv[DINO_NODES] = V3(0, 0, 0);

    for(int i = 1; i <= DINO_NODES; i++){
        v3 d = v3_sub(p[i], p[i - 1]);
        fx len = v3_length_exact(d);
        nd[i - 1] = len > 0 ? v3_direction14(d, root_dir) : root_dir;
        fx link = i == DINO_NODES ? FX(120) : FX(90);
        int32_t k = (link - len) * 115;
        v3 push = v3_scale14(nd[i - 1], k);
        dv[i] = v3_add(dv[i], push);
        dv[i - 1] = v3_sub(dv[i - 1], push);
    }
    nd[DINO_NODES] = nd[DINO_NODES - 1];
    nd[0] = root_dir;

    for(int i = 1; i <= DINO_NODES; i++){
        v3 axis = v3_cross14(nd[i - 1], nd[i]);
        if(v3_length_squared64(axis) < 256) continue;
        axis = v3_direction14(axis, a->up);
        int32_t c = v3_dot14(nd[i - 1], nd[i]);
        int32_t rate = N14_ONE - mul14(c + N14_ONE, 7802);
        int32_t power = mul14(rate, dino_bend[i]) << 8;

        v3 v15 = v3_cross14(nd[i], axis);
        dv[i] = v3_add(dv[i], v3_scale14(v15, power));
        if(i >= 2){
            v3 v14 = v3_cross14(nd[i - 1], axis);
            dv[i - 2] = v3_add(dv[i - 2], v3_scale14(v14, power));
        }
    }

    int32_t inv = (int32_t)((1 << 28) / (g->radius + FX(60)));
    for(int k = 1; k < DINO_NODES; k++){
        if(g->lock & (1 << k)){
            g->vel[k - 1][0] = g->vel[k - 1][1] = g->vel[k - 1][2] = 0;
            continue;
        }
        v3 v = v3_add(v3_from_i16(g->vel[k - 1]), dv[k]);
        v3 out = v3_sub(p[k], g->centre);

        v3 up = V3((out.x * inv) >> 14, (out.y * inv) >> 14, (out.z * inv) >> 14);
        v = v3_sub(v, v3_scale14(up, V16(0.7f)));
        v = v3_scale14(v, R14(0.88f));
        p[k] = v3_add(p[k], V3((v.x + 128) >> 8, (v.y + 128) >> 8, (v.z + 128) >> 8));
        fx floor = g->radius + dino_part_radius[k];
        out = v3_sub(p[k], g->centre);
        if(v3_length_squared64(out) < (uint64_t)((int64_t)floor * floor)){
            p[k] = v3_add(g->centre, v3_set_length(out, floor, up));
            int32_t into = v3_dot14(v, up);
            if(into < 0) v = v3_sub(v, v3_scale14(up, into));
        }
        for(int i = 0; i < 3; i++){
            int32_t c = i == 0 ? v.x : i == 1 ? v.y : v.z;
            g->vel[k - 1][i] = (int16_t)fx_clamp(c, -32000, 32000);
        }
        g->node[k] = p[k];
    }

    if(ball && !(g->lock & 0x100)){
        int32_t r = ball->state == DINO_BALL_WAIT      ? N14_ONE
                    : ball->state == DINO_BALL_REBOUND ? fx_min(ball->timer, 180) * N14_ONE / 180
                                                       : 0;
        if(r) ball->velocity = v3_add(ball->velocity, v3_scale14(dv[DINO_NODES], r));
    }
}

static void dino_chain_pose(const Actor* boss, const Actor* part, Figure* f){
    DinoState* g = &g_dino;
    Actor* ball = dino_ball(boss);
    v3 end = ball ? ball->position : g->node[DINO_NODES - 1];
    v3 origin = part->position;
    f->position = origin;
    f->basis_front = V3(0, N14_ONE, 0);
    f->basis_up = V3(0, 0, N14_ONE);
    f->local_yaw = f->local_pitch = 0;
    v3 ref = boss->up;
    for(int r = 0; r < DINO_NODES; r++){
        v3 next = r + 1 < DINO_NODES ? g->node[r + 1] : end;
        v3 prev = r > 0 ? g->node[r - 1] : g->node[0];
        v3 dir = v3_direction14(v3_sub(next, prev), boss->front);
        v3 side = v3_direction14(v3_cross14(dir, ref), v3_cross14(boss->front, boss->up));
        v3 up = v3_cross14(side, dir);
        ref = up;
        fx rad = dino_ring_radius[r];
        v3 c = v3_sub(g->node[r], origin);
        for(int k = 0; k < 6; k++){
            static const int16_t cs[6][2] = {{14189, 8192},   {0, 16384},  {-14189, 8192},
                                             {-14189, -8192}, {0, -16384}, {14189, -8192}};
            v3 q = v3_add(
                c, v3_add(v3_scale14(side, mul14(rad, cs[k][0])), v3_scale14(up, mul14(rad, cs[k][1]))));
            g_dino_tail_pose[r * 6 + k][0] = (int16_t)fx_clamp(q.x * 4, -32767, 32767);
            g_dino_tail_pose[r * 6 + k][1] = (int16_t)fx_clamp(q.y * 4, -32767, 32767);
            g_dino_tail_pose[r * 6 + k][2] = (int16_t)fx_clamp(q.z * 4, -32767, 32767);
        }
    }
    f->pose = (const int16_t (*)[3])g_dino_tail_pose;
}

static void dino_reset_position(Actor* a){
    DinoState* g = &g_dino;
    a->position = g->home;
    a->up = dino_home_up();
    a->front = dino_home_front();
    a->velocity = V3(0, 0, 0);
    Player* p = &g_game.player;
    v3 at = dino_world(a->position, a->up, a->front, 0, FX(800), -FX(550));
    v3 up = v3_direction14(v3_sub(at, g->centre), a->up);
    p->figure.position = at;
    p->up = p->gravity_up = up;
    p->front = v3_tangent14(v3_sub(a->position, at), up, a->front);
    p->velocity = V3(0, 0, 0);
    g_mario.position16 = V3(at.x << 8, at.y << 8, at.z << 8);
    g_mario.jump_vec = V3(0, 0, 0);
}

static void dino_demo_begin(Actor* a, int st, int cam){
    DinoState* g = &g_dino;
    audio_stop_music();
    boss_demo_begin(a, true);
    g->flags &= ~(DINO_FLAG_CHAIN | DINO_FLAG_HOLD);
    g->cam = (int16_t)cam;
    g->cam_step = 0;
    g->lock = 0;
    g->out = DINO_TAIL_PULL_NONE;
    dino_set(a, st);
    dino_reset_position(a);
}

static void dino_demo_end(Actor* a){
    DinoState* g = &g_dino;
    g->cam = -1;
    g->flags |= DINO_FLAG_CHAIN;
    dino_reset_position(a);
    const Player* p = &g_game.player;
    boss_demo_end(p->figure.position, p->up, p->front);
}

static void dino_demo_camera(void){
    DinoState* g = &g_dino;
    if(g->cam < 0) return;
    int first = dino_cam_index[g->cam][0], frames = dino_cam_index[g->cam][1];
    int f = fx_clamp(g->cam_step, 0, frames), i = f / DINO_CAM_STEP, r = f % DINO_CAM_STEP;
    const DinoCamSample* s0 = &dino_cam_samples[first + i];
    const DinoCamSample* s1 = f < frames ? s0 + 1 : s0;
    int32_t v[6];
    for(int k = 0; k < 6; k++){
        int a0 = k < 3 ? s0->eye[k] : s0->watch[k - 3], a1 = k < 3 ? s1->eye[k] : s1->watch[k - 3];
        v[k] = a0 + (a1 - a0) * r / DINO_CAM_STEP;
    }
    v3 up = dino_home_up(), front = dino_home_front();
    v3 eye = dino_world(g->home, up, front, v[0], v[1], v[2]);
    v3 watch = dino_world(g->home, up, front, v[3], v[4], v[5]);
    boss_demo_set_camera(eye, watch, up, DINO_FOVY_TAN);
    g->cam_step++;
}

static void dino_opening_mario(const Actor* a, int step){
    DinoState* g = &g_dino;
    int n = (int)(sizeof dino_mario_opening / sizeof dino_mario_opening[0]);
    int i = step / DINO_CAM_STEP, r = step % DINO_CAM_STEP;
    if(i >= n - 1){
        i = n - 1;
        r = 0;
    }
    const DinoMarioSample* s0 = &dino_mario_opening[i];
    const DinoMarioSample* s1 = i + 1 < n ? s0 + 1 : s0;
    v3 up = dino_home_up(), front = dino_home_front();
    v3 p0 = dino_world(g->home, up, front, s0->pos[0], s0->pos[1], s0->pos[2]);
    v3 p1 = dino_world(g->home, up, front, s1->pos[0], s1->pos[1], s1->pos[2]);
    Player* p = &g_game.player;
    p->figure.position = v3_add(p0, V3((p1.x - p0.x) * r / DINO_CAM_STEP, (p1.y - p0.y) * r / DINO_CAM_STEP,
                                       (p1.z - p0.z) * r / DINO_CAM_STEP));
    p->up = p->gravity_up = dino_world_dir(up, front, s0->up);
    p->front = v3_tangent14(dino_world_dir(up, front, s0->front), p->up, p->front);
    g_mario.position16 = V3(p->figure.position.x << 8, p->figure.position.y << 8, p->figure.position.z << 8);
    if(step == 0) mario_restart_anim(ANIM_MARIO_SPACEFLY);
    if(step == 28) mario_restart_anim(ANIM_MARIO_LANDSTIFFEN);
    if(step == 56) mario_restart_anim(ANIM_MARIO_JUMPBACK);
}

static void dino_damage_hold(Actor* a){
    DinoState* g = &g_dino;
    if(g->flags & DINO_FLAG_HOLD) return;
    g->flags |= DINO_FLAG_HOLD;
    boss_demo_begin(a, true);
    if(a->health > 0){
        g->dir = (int8_t)dino_damage_side(a);
        a->anim = ANIM_DINOPACKUN_PUNCHEDTAILTOP;
        a->anim_frame = 0;
    }
    v3 watch = v3_add(a->position, v3_scale14(a->up, FX(300)));

    const Camera* cam = &g_game.camera;
    v3 eye = v3_add(cam->target, v3_scale(v3_sub(cam->position, cam->target),
                                          FX_ONE * FX_ONE / boss_camera_zooms[g_game.camera_zoom]));
    v3 d = v3_sub(eye, watch);
    if(v3_length_squared64(d) > (uint64_t)FX(1800) * FX(1800))
        eye = v3_add(watch, v3_set_length(d, FX(1800), a->up));
    g_boss_demo.cam_eye = eye;
}
static void dino_damage_camera(Actor* a){
    DinoState* g = &g_dino;
    if(!(g->flags & DINO_FLAG_HOLD)) return;
    v3 watch = v3_add(a->position, v3_scale14(a->up, FX(300)));
    Actor* ball = dino_ball(a);
    if((g->flags & DINO_FLAG_AIM) && ball)
        watch = v3_add(watch, v3_scale14(v3_sub(ball->position, watch), R14(0.6f)));
    Camera* cam = &g_game.camera;
    v3 t = v3_add(cam->target, v3_scale14(v3_sub(watch, cam->target), R14(0.1f)));
    boss_demo_set_camera(g_boss_demo.cam_eye, t, a->up, 1697);
}
static void dino_demo_end_hold(Actor* a){
    DinoState* g = &g_dino;
    if(!(g->flags & DINO_FLAG_HOLD)) return;
    g->flags &= ~(DINO_FLAG_HOLD | DINO_FLAG_AIM);
    const Player* p = &g_game.player;
    boss_demo_end(p->figure.position, p->up, p->front);
}

static void dino_ball_hit(Actor* a, Actor* ball){
    if(a->state != DINOPACKUN_PUNCHED) return;
    audio_play_effect(SFX_ENEMY_DEFEAT, 5);
    g_dino.flags &= ~DINO_FLAG_AIM;
    a->velocity = V3(0, 0, 0);
    if(a->health == 0){
        dino_set(a, DINOPACKUN_EGG_BROKEN);
        g_dino.lock |= 0x100;
        if(ball){
            actor_set_state(ball, DINO_BALL_LOCK, -1);
            ball->target = ball->velocity;
            ball->velocity = V3(0, 0, 0);
        }
        camera_shake(2);
        return;
    }
    dino_set(a, a->health >= 3 ? DINOPACKUN_LASTDAMAGE : DINOPACKUN_DAMAGE);
    camera_shake(2);
}

static void dinopackun_tail_ball_think(Actor* b){
    Actor* a = dino_actor(b->value, OBJ_DINOPACKUN, NULL);
    if(!a){
        b->active = 0;
        return;
    }
    if(!(g_dino.flags & DINO_FLAG_CHAIN)) return;
    actor_follow_gravity(b, R14(0.3f));
    v3 v = b->velocity;
    v3 head = dino_head(a);
    fx ball_r = FX(135);
    switch(b->state){
    case DINO_BALL_LOCK: b->velocity = V3(0, 0, 0); return;
    case DINO_BALL_WAIT:
    case DINO_BALL_REBOUND:
        v = v3_scale14(v3_sub(v, v3_scale14(b->up, V16(1.0f))), R14(0.9f));
        if(b->state == DINO_BALL_REBOUND && b->timer > 180) actor_set_state(b, DINO_BALL_WAIT, -1);
        break;
    case DINO_BALL_SHOOT:
        if(b->timer > 30 || v3_length_exact(v3_sub(b->position, head)) >= FX(1500)){
            actor_set_state(b, DINO_BALL_CHARGE, -1);
            dino_damage_hold(a);
        }
        break;
    case DINO_BALL_CHARGE:
        v = v3_scale14(v, R14(0.88f));
        if(b->timer > 35) actor_set_state(b, DINO_BALL_REVERSE, -1);
        break;
    case DINO_BALL_REVERSE: {
        v3 to = v3_sub(head, b->position);
        v = v3_scale14(v3_add(v, v3_scale14(v3_direction14(to, b->front), V16(8.0f))), R14(0.98f));
        fx hit_r = (a->health == 0 ? FX(400) : FX(270)) + ball_r;
        if(v3_length_squared64(to) < (uint64_t)((int64_t)hit_r * hit_r) || b->timer > 300){
            v = v3_scale14(v3_direction14(v3_neg(to), b->up), mul14(v3_length_exact(v), R14(0.3f)));
            b->velocity = v;
            actor_set_state(b, DINO_BALL_REBOUND, -1);
            dino_ball_hit(a, b);
            if(b->state == DINO_BALL_LOCK) return;
        }
        break;
    }
    }
    b->position = v3_add(b->position, V3((v.x + 128) >> 8, (v.y + 128) >> 8, (v.z + 128) >> 8));

    RayHit hit;
    int32_t vs = v3_dot14(v, b->up);
    if(vs <= 0 && collision_ray(v3_add(b->position, v3_scale14(b->up, FX(40))),
                                v3_sub(b->position, v3_scale14(b->up, ball_r - vs / 256)), &hit, 0)){
        b->position = v3_add(hit.point, v3_scale14(b->up, ball_r));
        v = v3_reject14(v, b->up);
    }
    b->velocity = v;
    if(v3_length_squared64(v3_reject14(v, b->up)) > 64) b->front = v3_tangent14(v, b->up, b->front);
    b->target.x += v3_length_exact(v3_reject14(v, b->up)) / 135;

    if(b->state == DINO_BALL_WAIT || b->state == DINO_BALL_REBOUND){
        int c = actor_mario_contact(b, FX(60));
        if(b->state == DINO_BALL_WAIT && (c == CONTACT_SPIN || c == CONTACT_STAR) &&
           a->state != DINOPACKUN_PUNCHED && a->state != DINOPACKUN_EGG_BROKEN &&
           a->state != DINOPACKUN_DAMAGE && a->state != DINOPACKUN_LASTDAMAGE && a->state < DINOPACKUN_DOWN &&
           a->state != DINOPACKUN_IDLE && a->state != DINOPACKUN_CRY && a->state != DINOPACKUN_ANGRY){
            v3 away = v3_tangent14(v3_neg(actor_to_mario(b)), b->up, b->front);
            b->velocity = v3_add(v3_scale14(away, V16(120.0f)), v3_scale14(b->up, V16(40.0f)));
            actor_set_state(b, DINO_BALL_SHOOT, -1);
            mario_spin_hop();
            audio_play_effect(SFX_ENEMY_HIT, 4);
            a->velocity = V3(0, 0, 0);
            dino_set(a, DINOPACKUN_PUNCHED);
            g_dino.flags |= DINO_FLAG_AIM;
            g_dino.lock |= 2;
            g_dino.out = a->health == 0 ? DINO_TAIL_PULL_EGG_PUNCHED : DINO_TAIL_PULL_PUNCHED;
        } else if(c != CONTACT_NONE){
            v3 out = v3_tangent14(actor_to_mario(b), b->up, b->front);
            mario_nudge(v3_scale14(out, FX(6)));
        }
    }
}

static void dinopackun_egg_think(Actor* e){
    Actor* a = dino_actor(e->value, OBJ_DINOPACKUN, NULL);
    if(!a){
        e->active = 0;
        return;
    }
    e->up = a->up;
    e->front = a->front;
    switch(e->args[1]){
    case DINO_PART_SHELL: {
        int local;
        dino_anim_key(a, &local);
        int first = dino_egg_first[a->anim];
        if(first < 0){
            e->position = a->position;
            break;
        }
        const DinoEggKey* k = &dino_egg_keys[first + local];
        e->position = dino_world(a->position, a->up, a->front, k->pos[0] * 8, k->pos[1] * 8, k->pos[2] * 8);
        e->front = dino_world_dir(a->up, a->front, k->front);
        e->up = v3_tangent14(dino_world_dir(a->up, a->front, k->up), e->front, a->up);
        break;
    }
    case DINO_PART_FLOWER: {
        int clip = dino_flower_clip[a->anim];
        e->position = a->position;
        if(!(g_dino.flags & DINO_FLAG_FLOWER) || clip < 0){
            e->flags |= ACTOR_HIDDEN;
            break;
        }
        e->flags &= ~ACTOR_HIDDEN;
        e->anim = clip;
        e->anim_frame = a->anim_frame - FX_ONE;
        break;
    }
    default:
        e->position = g_dino.node[DINO_NODES / 2];
        if(a->flags & ACTOR_HIDDEN) e->flags |= ACTOR_HIDDEN;
        break;
    }
}

static void dino_part_figure(const Actor* a, Figure* f){
    if(a->type != OBJ_DINOPACKUNEGG) return;
    const Actor* boss = dino_actor(a->value, OBJ_DINOPACKUN, NULL);
    if(boss && a->args[1] == DINO_PART_CHAIN) dino_chain_pose(boss, a, f);
}

static bool dino_sees(Actor* a){
    bool angry = a->health >= 2;
    return actor_sees_mario(a, angry ? FX(1300) : FX(1000), angry ? DEG(90) : DEG(80), DEG(90));
}

static Actor* dino_spawn_part(Actor* a, int type, int role){
    Actor* p = actor_spawn_reuse(type, a->position, a->up, a->front);
    if(!p && stream_evict_preserving(g_game.player.figure.position, a->position))
        p = actor_spawn_reuse(type, a->position, a->up, a->front);
    if(!p) return NULL;
    p->value = (int16_t)actor_index(a);
    p->args[1] = (int16_t)role;
    actor_set_state(p, type == OBJ_DINOPACKUNTAIL ? DINO_BALL_WAIT : DINO_PART_ACTIVE, -1);
    return p;
}

static void dino_part_model(Actor* p, const uint8_t* model){
    figure_init(&p->figure, model);
    p->figure.use_basis = true;
    p->figure.scale = V3(64, 64, 64);
}

static void dino_init(Actor* a){
    DinoState* g = &g_dino;
    g->boss = (int16_t)actor_index(a);
    g->cam = -1;
    g->lock = 0;
    g->flags = DINO_FLAG_FLOWER;
    g->out = DINO_TAIL_PULL_NONE;
    g->dir = 3;
    g->home = a->position;
    g->home_up[0] = (int16_t)a->up.x;
    g->home_up[1] = (int16_t)a->up.y;
    g->home_up[2] = (int16_t)a->up.z;
    g->home_front[0] = (int16_t)a->front.x;
    g->home_front[1] = (int16_t)a->front.y;
    g->home_front[2] = (int16_t)a->front.z;

    {
        GravitySample s1 =
            gravity_sample(g_game.stage.header, v3_add(a->position, v3_scale14(a->up, FX(50))));
        v3 u1 = s1.index >= 0 ? s1.up : a->up;
        v3 p2 = v3_add(a->position, v3_scale14(a->front, FX(500)));
        GravitySample s2 = gravity_sample(g_game.stage.header, p2);
        v3 u2 = s2.index >= 0 ? s2.up : a->up;

        v3 w = v3_sub(a->position, p2);
        v3 n = v3_reject14(u1, u2);
        int32_t nn = v3_dot14(n, n);
        fx t = nn > 64 ? (fx)(((int64_t)v3_dot14(w, n) << 14) / nn) : FX(850);
        t = fx_clamp(t, FX(200), FX(5000));
        g->centre = v3_sub(a->position, v3_scale14(u1, t));
        g->radius = t;
    }

    Actor* e = dino_spawn_part(a, OBJ_DINOPACKUNEGG, DINO_PART_SHELL);
    a->args[2] = e ? (int16_t)actor_index(e) : -1;
    Actor* c = dino_spawn_part(a, OBJ_DINOPACKUNEGG, DINO_PART_CHAIN);
    if(c){
        dino_part_model(c, figure_dinopackuntailchain);
        c->radius = FX(400);
    }
    a->args[1] = c ? (int16_t)actor_index(c) : -1;
    Actor* b = dino_spawn_part(a, OBJ_DINOPACKUNTAIL, 0);
    a->args[3] = b ? (int16_t)actor_index(b) : -1;
    a->health = 0;
    dino_set(a, DINOPACKUN_IDLE);
    a->anim_frame = 0;
    dino_chain_from_anim(a, b);
    g->area_camera = false;
}

static void dino_star_pieces(Actor* a){ collectibles_pop(OBJ_STARBIT, dino_head(a), a->up, 8); }

static int dino_damage_side(const Actor* a){
    const Actor* ball = dino_ball(a);
    if(!ball) return 3;
    v3 d = v3_direction14(v3_sub(ball->position, dino_head(a)), a->up);
    if(v3_dot14(d, a->up) > R14(0.7f)) return 3;
    return v3_dot14(d, v3_cross14(a->front, a->up)) > 0 ? 2 : 1;
}

static bool dino_area_camera(BossCamera* bc){
    DinoState* g = &g_dino;
    Actor* a = g->area_camera ? dino_actor(g->boss, OBJ_DINOPACKUN, NULL) : NULL;
    if(!a) return false;

    v3 reg = v3_add(a->position, v3_scale14(a->up, FX(300)));
    Actor* ball = dino_ball(a);
    if((g->flags & DINO_FLAG_AIM) && ball)
        reg = v3_add(reg, v3_scale14(v3_sub(ball->position, reg), R14(0.6f)));
    bc->reg = reg;
    bc->reg_up = v3_direction14(v3_sub(reg, g->centre), a->up);
    bc->dist_min = DINO_AREA_DIST_MIN;
    bc->dist_max = DINO_AREA_DIST_MAX;
    bc->angle_min = DINO_AREA_ANGLE_MIN;
    bc->constrain14 = DINO_AREA_CONSTRAIN;
    bc->peep_angle = DINO_AREA_PEEP_ANGLE;
    bc->peep_dist = DINO_AREA_PEEP_DIST;
    bc->centre = g->centre;
    bc->radius = g->radius;
    return true;
}

static void dinopackun_think(Actor* a){
    DinoState* g = &g_dino;
    bool angry = a->health >= 2;
    if(a->state == DINOPACKUN_GONE) return;
    if(a->state < DINOPACKUN_IDLE || g->boss != actor_index(a)) dino_init(a);
    int32_t accel = 0;
    bool body = true;
    switch(a->state){
    case DINOPACKUN_IDLE:
        a->anim_frame = 0;
        body = false;

        if((actor_has_switch(a, SWITCH_A) ? actor_switch_on(a, SWITCH_A) : actor_mario_within(a, FX(3000))) &&
           g_game.player.action != MA_STAR_LAUNCH && g_game.player.action != MA_DEATH){
            dino_demo_begin(a, DINOPACKUN_OPENING, DINO_CAM_OPENING);
            g->lock = 0;
        }
        break;
    case DINOPACKUN_OPENING:
        body = false;
        dino_opening_mario(a, a->timer - 1);
        if(actor_anim_done(a)){
            dino_demo_end(a);
            audio_play_music(MUSIC_MBGM_GALAXY_INTER);
            g->lock = 2;
            g->out = DINO_TAIL_PULL_EGG;
            dino_set(a, DINOPACKUN_EGG_TURN);
        }
        break;
    case DINOPACKUN_EGG_TURN:
        a->front = v3_tangent14(v3_rotate_axis14(a->front, a->up, (a->flags & 0x10) ? DEG(0.3f) : -DEG(0.3f)),
                                a->up, a->front);
        if(a->timer > 120) dino_set(a, DINOPACKUN_EGG_WALK);
        break;
    case DINOPACKUN_EGG_WALK:
        accel = V16(0.5f);
        if(actor_bounce_wall(a, FX(450), N14_ONE / 2)) a->flags ^= 0x10;
        if(a->timer > 300){
            a->flags ^= (g_game.tick & 1) ? 0x10 : 0;
            dino_set(a, DINOPACKUN_EGG_TURN);
        }
        break;
    case DINOPACKUN_PUNCHED: {
        Actor* b = dino_ball(a);
        if(b && v3_length_exact(v3_sub(b->position, a->position)) > FX(800))
            actor_turn_toward(a, v3_sub(b->position, a->position), DEG(6));
        if(!b || (b->state == DINO_BALL_WAIT && a->timer > 60)){
            dino_demo_end_hold(a);
            g->lock = a->health == 0 ? 2 : 0;
            g->out = a->health == 0 ? DINO_TAIL_PULL_EGG : DINO_TAIL_PULL_NONE;
            g->flags &= ~DINO_FLAG_AIM;
            dino_set(a, a->health == 0 ? DINOPACKUN_EGG_TURN : DINOPACKUN_TURN);
        }
        break;
    }
    case DINOPACKUN_EGG_BROKEN:
        body = false;
        if(a->timer == 30){
            Actor* e = dino_part(a, 2, OBJ_DINOPACKUNEGG);
            if(e){
                e->args[1] = DINO_PART_FLOWER;
                dino_part_model(e, figure_dinopackunflower);
            }
            a->anim = ANIM_DINOPACKUN_EGGDAMAGETOP;
            a->anim_frame = 0;
            g->lock = 0;
            g->out = DINO_TAIL_PULL_NONE;
            Actor* b = dino_ball(a);
            if(b){
                b->velocity = b->target;
                actor_set_state(b, DINO_BALL_REBOUND, -1);
            }
            camera_shake(2);
        } else if(a->timer < 30) a->anim_frame = 0;
        if(a->timer > 120){
            dino_demo_end_hold(a);
            a->health = 1;
            dino_demo_begin(a, DINOPACKUN_CRY, DINO_CAM_CRY);
        }
        break;
    case DINOPACKUN_CRY:
        body = false;
        if(actor_anim_done(a)){
            dino_demo_end(a);
            audio_play_music(MUSIC_MBGM_BOSS_03_A);
            dino_star_pieces(a);
            dino_set(a, DINOPACKUN_START);
        }
        break;
    case DINOPACKUN_ANGRY:
        body = false;
        if(actor_anim_done(a)){
            dino_demo_end(a);
            audio_play_music(MUSIC_MBGM_BOSS_03_B);
            dino_star_pieces(a);
            dino_set(a, DINOPACKUN_START);
        }
        break;
    case DINOPACKUN_START:
        if(a->timer > 60) dino_set(a, DINOPACKUN_CHASE);
        break;
    case DINOPACKUN_TURN:
        if(angry) accel = V16(1.3f);
        a->front = v3_tangent14(v3_rotate_axis14(a->front, a->up, (a->flags & 0x10) ? DEG(1) : -DEG(1)),
                                a->up, a->front);
        if(dino_sees(a)){
            dino_set(a, DINOPACKUN_FIND);
            break;
        }
        if(a->timer > (angry ? 30 : 60)) dino_set(a, DINOPACKUN_WALK);
        break;
    case DINOPACKUN_WALK:
        accel = angry ? V16(1.3f) : V16(0.5f);
        if(actor_bounce_wall(a, FX(450), N14_ONE / 2)) a->flags ^= 0x10;
        if(dino_sees(a)){
            dino_set(a, DINOPACKUN_FIND);
            break;
        }
        if(a->timer > 180){
            if(g_game.tick & 1) a->flags ^= 0x10;
            dino_set(a, DINOPACKUN_TURN);
        }
        break;
    case DINOPACKUN_FIND:
        actor_turn_toward(a, actor_to_mario(a), angry ? DEG(4) : DEG(1));
        if(actor_anim_done(a)) dino_set(a, DINOPACKUN_CHASE);
        break;
    case DINOPACKUN_CHASE: {
        int ramp = angry ? 90 : 60;
        actor_turn_toward(a, actor_to_mario(a), angry ? DEG(1.3f) : DEG(1));
        accel = mul14(angry ? V16(1.8f) : V16(1.5f), fx_min(a->timer * N14_ONE / ramp, N14_ONE));
        if(a->timer > (angry ? 300 : 240)) dino_set(a, DINOPACKUN_COOLDOWN);
        break;
    }
    case DINOPACKUN_COOLDOWN:
        if(a->timer > (angry ? 30 : 90)) dino_set(a, DINOPACKUN_TURN);
        break;
    case DINOPACKUN_ATTACKHIT:
        if(actor_anim_done(a)) dino_set(a, DINOPACKUN_TURN);
        break;
    case DINOPACKUN_DAMAGE:
        if(a->timer == 1) g->lock = 0;
        if(actor_anim_done(a)){
            dino_demo_end_hold(a);
            g->out = DINO_TAIL_PULL_NONE;
            a->health++;
            if(a->health == 2){
                g->flags &= ~DINO_FLAG_FLOWER;
                dino_demo_begin(a, DINOPACKUN_ANGRY, DINO_CAM_ANGRY);
            } else {
                dino_star_pieces(a);
                dino_set(a, DINOPACKUN_FIND);
            }
        }
        break;
    case DINOPACKUN_LASTDAMAGE:
        if(a->timer == 1) g->lock = 0;
        if(actor_anim_done(a)){
            dino_demo_end_hold(a);
            g->out = DINO_TAIL_PULL_NONE;
            dino_demo_begin(a, DINOPACKUN_DOWN, DINO_CAM_DOWN);
        }
        break;
    case DINOPACKUN_DOWN:
        body = false;
        if(actor_anim_done(a)){
            audio_play_effect(SFX_ENEMY_DEFEAT, 5);
            a->flags |= ACTOR_HIDDEN;
            Actor* b = dino_ball(a);
            if(b) b->active = 0;
            dino_set(a, DINOPACKUN_STAR);
        }
        break;
    case DINOPACKUN_STAR:
        body = false;
        a->anim_frame = FX(1000);
        if(a->timer == 60){
            boss_power_star_appear(a, a->position, FX(4000), true, BOSS_AFTER_BGM);
        }
        if(a->timer > 60 && star_appear_done()){
            g->cam = -1;
            const Player* p = &g_game.player;
            boss_demo_end(p->figure.position, p->up, p->front);
            g->area_camera = false;
            for(int k = 1; k <= 2; k++){
                Actor* pt = dino_part(a, k, OBJ_DINOPACKUNEGG);
                if(pt) pt->active = 0;
            }
            dino_set(a, DINOPACKUN_GONE);
            a->active = 0;
            g->boss = -1;
            return;
        }
        break;
    }
    if(g->cam >= 0) dino_demo_camera();
    else if(g->flags & DINO_FLAG_HOLD) dino_damage_camera(a);
    if(body){
        actor_push_forward(a, accel);
        actor_follow_gravity(a, R14(0.1f));
        actor_move(a, V16(1.0f), R14(0.9f), R14(0.9f));
    }
    dino_chain_tick(a);

    g->area_camera =
        a->state != DINOPACKUN_IDLE && a->state < DINOPACKUN_DOWN &&
        v3_length_squared64(v3_sub(g_game.player.figure.position, g->centre)) < (uint64_t)FX(3000) * FX(3000);

    if(a->state == DINOPACKUN_IDLE || a->state == DINOPACKUN_OPENING || a->state >= DINOPACKUN_DOWN) return;
    if(a->health == 0){
        actor_block_mario(a, FX(330), FX(560));
        return;
    }
    if(a->state >= DINOPACKUN_DAMAGE || a->state == DINOPACKUN_CRY || a->state == DINOPACKUN_PUNCHED) return;
    Actor head = *a;
    head.position = v3_sub(dino_head(a), v3_scale14(a->up, actor_kinds[a->type].height));
    head.radius = FX(270);
    int c = actor_mario_contact(a, 0);
    if(c == CONTACT_NONE) c = actor_mario_contact(&head, 0);
    if(c == CONTACT_NONE) return;
    if(c == CONTACT_SPIN || c == CONTACT_STAR){
        if(c == CONTACT_SPIN) mario_spin_hop();
    } else {
        actor_hurt_mario(a, 1);
        if(a->state == DINOPACKUN_CHASE || a->state == DINOPACKUN_WALK) dino_set(a, DINOPACKUN_ATTACKHIT);
    }
    actor_block_mario(a, FX(220), FX(500));
}

static bool dino_demo_cast(const Actor* demo, const Actor* a){
    if(demo->type != OBJ_DINOPACKUN) return false;
    return ((a->type == OBJ_DINOPACKUNEGG || a->type == OBJ_DINOPACKUNTAIL) &&
            a->value == actor_index(demo)) ||
           star_appear_busy(a);
}

static void dino_tail_figure(const Actor* a, Figure* f){ f->local_pitch = a->target.x; }

static fx dino_draw_extent(const Actor* a){ return FX(900); }
