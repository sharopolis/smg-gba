typedef struct {
    const NativeWarpPod* path;
    uint16_t step;
    uint8_t phase, from;
} WarpPodState;
static EWRAM_BSS WarpPodState g_warppod;

static v3 warppod_v(const int16_t p[3]){ return V3(p[0], p[1], p[2]); }

static v3 warppod_position(const int16_t p[3]){
    v3 v = warppod_v(p);
    return v;
}

static void warppod_reset(void){
    g_warppod = (WarpPodState){0};
    audio_stop_level_effect(SFX_WARPPOD_MOVE);
}

static Actor* warppod_actor(int row){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(a->active && a->type == OBJ_WARPPOD && a->args[0] == row) return a;
    }
    return NULL;
}

static void warppod_think(Actor* a){
    if(a->state){
        if(a->timer < 60 || v3_length_squared64(v3_sub(a->position, g_game.player.figure.position)) <
                                (uint64_t)FX(200) * FX(200))
            return;
        a->state = 0;
        a->anim = ANIM_WARPPOD_ACTIVE;
        a->anim_frame = 0;
    }
    if(g_warppod.phase || g_game.scene != SCENE_PLAY || g_mario_held || talk_active() ||
       g_game.player.action == MA_DEATH || !actor_touches_mario(a, FX(40)))
        return;
    int row = a->args[0];
    if(row < 0 || row >= (int)(sizeof native_warppods / sizeof native_warppods[0])) return;
    g_warppod.path = &native_warppods[row];
    g_warppod.phase = 1;
    g_warppod.from = (uint8_t)row;
    g_warppod.step = 0;
    mario_hold_begin();
    g_game.player.on_ground = false;
    mario_set_action(MA_TALK);
    mario_restart_anim(ANIM_MARIO_WARPPODSTART);
    audio_play_effect(SFX_WARPPOD_IN, 2);
}

static void warppod_tick(void){
    WarpPodState* w = &g_warppod;
    if(!w->phase) return;
    Player* p = &g_game.player;
    const NativeWarpPod* path = w->path;
    const NativeWarpPod* to = &native_warppods[path->pair];
    if(p->action == MA_DEATH || !g_mario_held){
        warppod_reset();
        return;
    }
    if(w->phase == 1){
        int elapsed = ++w->step;
        v3 target = warppod_position(path->start);
        v3 move = v3_scale(v3_sub(target, p->figure.position), elapsed * FX_ONE / 60);
        mario_hold_move(v3_add(p->figure.position, move), p->front);
        if(elapsed >= 60){
            w->phase = 2;
            w->step = 0;
        }
    } else if(w->phase == 2){
        int progress = (int)w->step * 64 * FX_ONE / path->ticks;
        int sample = progress >> 8;
        if(sample >= 64) sample = 64, progress = 64 * FX_ONE;
        v3 target = warppod_position(path->path[sample]);
        if(sample < 64)
            target = v3_add(
                target, v3_scale(v3_sub(warppod_position(path->path[sample + 1]), target), progress & 255));
        v3 delta = v3_sub(target, p->figure.position);
        v3 front = v3_direction14(delta, p->front);
        mario_hold_move(v3_add(p->figure.position, V3(delta.x / 2, delta.y / 2, delta.z / 2)), front);
        audio_level_effect(SFX_WARPPOD_MOVE);
        if(++w->step > path->ticks){
            w->phase = 3;
            w->step = 0;
            mario_hold_move(warppod_position(to->start), warppod_v(to->front));
            p->up = p->gravity_up = warppod_v(to->up);
            mario_restart_anim(ANIM_MARIO_WARPPODEND);
            audio_stop_level_effect(SFX_WARPPOD_MOVE);
            audio_play_effect(SFX_WARPPOD_OUT, 2);
            Actor* dest = warppod_actor(path->pair);
            if(dest){
                dest->state = 1;
                dest->timer = 0;
                dest->anim = ANIM_WARPPOD_WAIT;
                dest->anim_frame = 0;
            }
        }
    } else {
        if(++w->step >= 49){
            v3 velocity =
                v3_add(v3_scale14(warppod_v(to->up), V16(12)), v3_scale14(warppod_v(to->front), V16(5)));
            mario_hold_release(velocity);
            w->phase = 0;
        }
    }
}

static void warppod_camera(void){
    if(!g_warppod.phase || !g_warppod.path) return;
    const NativeWarpPod* path = g_warppod.path;
    Camera* c = &g_game.camera;
    c->position = warppod_position(path->eye);
    c->target = v3_add(g_game.player.figure.position, warppod_v(path->offset));
    c->front = v3_direction14(v3_sub(c->target, c->position), g_game.player.front);
    c->up = v3_tangent14(V3(0, 0, N14_ONE), c->front, g_game.player.up);
    camera_update_angles(c);
}

static void warppod_init(Actor* a, const StageObject* o){ actor_set_state(a, 0, ANIM_WARPPOD_ACTIVE); }
