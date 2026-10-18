enum {
    BABYLAUNCHER_WAIT_AFTER_SWITCH,
    BABYLAUNCHER_WAIT,
    BABYLAUNCHER_WAIT_FOR_LAUNCH,
    BABYLAUNCHER_LAUNCH,
    BABYLAUNCHER_APPEAR,
    BABYLAUNCHER_DISAPPEAR
};
enum { BABYLAUNCHER_STATE_BASE = 220 };
static Actor* babylauncher_child(const Actor* a){
    Actor* b = actor_at_slot(a->value, OBJ_BUOYBEGOBABY);
    return b && b->args[3] == actor_index(a) && b->args[2] == 0 ? b : NULL;
}
static void babylauncher_release_child(const Actor* a){
    Actor* b = babylauncher_child(a);
    if(!b) return;
    b->active = 0;
    b->flags = 0;
    b->args[3] = -1;
    b->args[2] = -1;
}
static bool babylauncher_ensure_child(Actor* a){
    if(babylauncher_child(a)) return true;
    Actor* b = actor_spawn_reuse(OBJ_BUOYBEGOBABY, a->position, a->up, a->front);
    if(!b) return false;
    a->value = actor_index(b);
    b->args[3] = actor_index(a);
    b->args[2] = 0;
    b->active = 0;
    b->flags = ACTOR_WAITING;
    b->target = b->front;
    b->home = a->position;
    b->value = 0;
    return true;
}
static void babylauncher_init(Actor* a, const StageObject* o){
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    a->value = -1;
    a->velocity = V3(0, 0, 0);
    a->target = gravity_sample(g_game.stage.header, a->position).up;

    actor_set_state(a,
                    BABYLAUNCHER_STATE_BASE + (stage_object_switch(o, SWITCH_APPEAR) >= 0
                                                   ? BABYLAUNCHER_WAIT_AFTER_SWITCH
                                                   : BABYLAUNCHER_WAIT),
                    0);
    if(a->state == BABYLAUNCHER_STATE_BASE + BABYLAUNCHER_WAIT) a->flags |= ACTOR_HIDDEN;
    else a->flags &= ~ACTOR_HIDDEN;
    babylauncher_ensure_child(a);
}
static void babylauncher_launch_child(Actor* a, Actor* b){
    v3 side = v3_cross14(a->up, a->front);
    v3 radial = v3_direction14(v3_reject14(side, a->target), V3(0, 0, 0));
    b->up = a->target;
    b->front = radial;
    b->target = radial;
    b->position = v3_add(a->position, v3_scale14(side, 100));
    b->velocity = v3_add(v3_scale14(side, V16(10)), v3_scale14(a->target, V16(30)));
    b->active = 1;
    b->flags = ACTOR_WAITING;
    b->health = 0;
    b->value = 0;
    b->figure.scale = V3(64, 64, 64);
    actor_set_state(b, BEGOMAN_BABY_AFTER_LAUNCH, ANIM_BEGOMANBABY_FLY);
    audio_play_effect(SFX_BABY_LAUNCHER_CHILD_APPEAR, 2);
}
enum {
    BABYLAUNCHER_DO_HIDE = 1,
    BABYLAUNCHER_DO_SHOW = 2,
    BABYLAUNCHER_DO_EMIT = 4,
    BABYLAUNCHER_DO_LAUNCH_SOUND = 8,
    BABYLAUNCHER_DO_START_ANIM = 16,
    BABYLAUNCHER_DO_STOP_ANIM = 32,
    BABYLAUNCHER_DO_ANIM_FROM_90 = 64,
    BABYLAUNCHER_DO_DELETE_EFFECT = 128,
    BABYLAUNCHER_DO_LAUNCH_CHILD = 256,
    BABYLAUNCHER_DO_APPEAR_LEVEL = 512
};
typedef struct {
    int phase, step;
    unsigned actions;
} BabyLauncherUpdate;
static inline BabyLauncherUpdate babylauncher_step(int phase, int step, int delay, bool child_dead){
    BabyLauncherUpdate r = {phase, step, 0};
    switch(phase){
    case BABYLAUNCHER_WAIT_AFTER_SWITCH:
        if(step > delay) r.phase = BABYLAUNCHER_LAUNCH;
        break;
    case BABYLAUNCHER_WAIT:
        if(!step) r.actions = BABYLAUNCHER_DO_HIDE;
        if(child_dead) r.phase = BABYLAUNCHER_WAIT_FOR_LAUNCH;
        break;
    case BABYLAUNCHER_WAIT_FOR_LAUNCH:
        if(step > 180) r.phase = BABYLAUNCHER_APPEAR;
        break;
    case BABYLAUNCHER_LAUNCH:
        if(!step) r.actions = BABYLAUNCHER_DO_SHOW | BABYLAUNCHER_DO_EMIT | BABYLAUNCHER_DO_LAUNCH_SOUND;
        if(step > 0){
            r.actions |= BABYLAUNCHER_DO_LAUNCH_CHILD;
            r.phase = BABYLAUNCHER_DISAPPEAR;
        }
        break;
    case BABYLAUNCHER_APPEAR:
        r.actions = BABYLAUNCHER_DO_APPEAR_LEVEL;
        if(!step) r.actions |= BABYLAUNCHER_DO_SHOW | BABYLAUNCHER_DO_EMIT | BABYLAUNCHER_DO_START_ANIM;
        if(step > 90){
            r.actions |= BABYLAUNCHER_DO_STOP_ANIM;
            r.phase = BABYLAUNCHER_LAUNCH;
        }
        break;
    case BABYLAUNCHER_DISAPPEAR:
        if(!step)
            r.actions = BABYLAUNCHER_DO_SHOW | BABYLAUNCHER_DO_START_ANIM | BABYLAUNCHER_DO_ANIM_FROM_90 |
                        BABYLAUNCHER_DO_DELETE_EFFECT;
        if(step > 90) r.phase = BABYLAUNCHER_WAIT;
        break;
    }
    r.step = r.phase != phase ? 0 : step + 1;
    return r;
}
static void babylauncher_think(Actor* a){
    const StageObject* o = actor_stage_object(a);
    if(!o) return;
    if(!babylauncher_ensure_child(a)) return;
    Actor* b = babylauncher_child(a);
    int kill = stage_object_switch(o, SWITCH_B);
    if(kill >= 0 && switch_on(kill)){
        if(b->active) begoman_baby_kill(b, false);
        a->active = 0;
        babylauncher_release_child(a);
        return;
    }
    int phase = a->state - BABYLAUNCHER_STATE_BASE, step = a->timer - 1;
    BabyLauncherUpdate r = babylauncher_step(phase, step, actor_arg_or(a, 2, 0), !b->active);
    if(r.actions & BABYLAUNCHER_DO_HIDE) a->flags |= ACTOR_HIDDEN;
    if(r.actions & BABYLAUNCHER_DO_SHOW) a->flags &= ~ACTOR_HIDDEN;

    if(phase == BABYLAUNCHER_APPEAR) a->anim_frame = fx_min(step, 91) * FX_ONE;
    else if(phase == BABYLAUNCHER_DISAPPEAR) a->anim_frame = (90 + fx_min(step, 89)) * FX_ONE;
    else a->anim_frame = 90 * FX_ONE;
    if(r.actions & BABYLAUNCHER_DO_LAUNCH_SOUND) audio_play_effect(SFX_BABY_LAUNCHER_LAUNCH, 2);
    if(r.actions & BABYLAUNCHER_DO_LAUNCH_CHILD) babylauncher_launch_child(a, b);
    if(r.phase != phase) actor_set_state(a, BABYLAUNCHER_STATE_BASE + r.phase, -1);
}
