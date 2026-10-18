enum {
    BLUE_CHIP_COMPLETION_PENDING = 224,
    BLUE_CHIP_COMPLETION_COMPLETE,
    BLUE_CHIP_COMPLETION_END,
    BLUE_CHIP_COMPLETION_EVENT,
    BLUE_CHIP_COMPLETION_POST
};
enum { BLUE_CHIP_COMPLETE_FRAMES = 61, BLUE_CHIP_END_FRAMES = 21, BLUE_CHIP_EVENT_FRAMES = 120 };

enum { BLUE_CHIP_EVENT_CAMERA = 186 };

static bool blue_chip_completion_cast(const Actor* a){
    const StageObject* o = actor_stage_object(a);
    return g_game.stage_index == STAGE_GOODEGG && o && o->zone == 1 && o->id >= 578 && o->id <= 582;
}

static bool blue_chip_completion_cast_shown(void){ return switch_on(3001); }

static bool blue_chip_completion_group(const Actor* a){
    const StageObject* o = actor_stage_object(a);
    return g_game.stage_index == STAGE_GOODEGG && a->type == OBJ_BLUECHIPGROUP && o && o->zone == 1 &&
           o->id == 583;
}

static EWRAM_BSS int16_t g_blue_chip_object;
static Actor* blue_chip_group(void){
    if(g_game.stage_index != STAGE_GOODEGG) return NULL;
    if(!g_blue_chip_object){
        g_blue_chip_object = -1;
        for(uint32_t i = 0; i < g_game.stage.header->object_count; i++){
            const StageObject* o = &g_game.stage.objects[i];
            if(o->type == OBJ_BLUECHIPGROUP && o->zone == 1 && o->id == 583){
                g_blue_chip_object = (int16_t)(i + 1);
                break;
            }
        }
    }
    if(g_blue_chip_object < 0 || !stream_object_maybe_out(g_blue_chip_object - 1)) return NULL;
    Actor* a = g_game.actors;
    for(Actor* end = a + g_game.actor_count; a < end; a++)
        if(a->type == OBJ_BLUECHIPGROUP && a->active && blue_chip_completion_group(a)) return a;
    return NULL;
}

static void blue_chip_completion_request(Actor* a){
    a->state = BLUE_CHIP_COMPLETION_PENDING;
    a->timer = -1;
}

static bool blue_chip_completion_paused(void){
    Actor* a = blue_chip_group();
    return a && a->state >= BLUE_CHIP_COMPLETION_COMPLETE && a->state <= BLUE_CHIP_COMPLETION_EVENT;
}

static int blue_chip_counter_frame(void){
    Actor* a = blue_chip_group();
    if(!a) return -1;
    if(a->state == BLUE_CHIP_COMPLETION_COMPLETE) return fx_clamp(a->timer, 0, BLUE_CHIP_COMPLETE_FRAMES - 1);
    if(a->state == BLUE_CHIP_COMPLETION_END)
        return BLUE_CHIP_COMPLETE_FRAMES + fx_clamp(a->timer, 0, BLUE_CHIP_END_FRAMES - 1);
    return -1;
}

static void gcapture_demo_appear(Actor* a){
    a->flags |= ACTOR_HIDDEN;
    a->state = GC_APPEAR;
    a->timer = 0;
    audio_play_effect(SFX_CAPTURE_APPEAR_SYSTEM, 3);
}

static bool gcapture_appear_tick(Actor* a){
    if(!blue_chip_completion_cast(a) || a->state != GC_APPEAR) return false;
    if(a->timer == 30){
        a->flags &= ~ACTOR_HIDDEN;
        a->anim = ANIM_GCAPTURETARGET_APPEAR;
        a->anim_frame = 0;
        audio_play_effect(SFX_CAPTURE_APPEAR, 3);
    }

    if(a->timer >= 89 && a->anim == ANIM_GCAPTURETARGET_APPEAR){
        a->anim = ANIM_GCAPTURETARGET_WAIT;
        a->anim_frame = 0;
    }
    if(a->timer > 150){
        a->state = GC_WAIT;
        a->timer = -1;
    }
    return true;
}

static void blue_chip_appear_actions(void){
    static const int8_t cast_by_id[] = {3, 4, 2, 0, 1};
    static const int8_t action_cast[] = {0, 1, -1, -1, -1};

    for(int row = 0; row < 5; row++){
        for(int i = 0; i < g_game.actor_count; i++){
            Actor* a = &g_game.actors[i];
            if(!a->active || !blue_chip_completion_cast(a)) continue;
            int cast = cast_by_id[actor_stage_object(a)->id - 578];
            if(action_cast[row] < 0 || action_cast[row] == cast) gcapture_demo_appear(a);
        }
    }
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(a->active && blue_chip_completion_cast(a) && actor_stage_object(a)->id == 581){
            camera_event_start(BLUE_CHIP_EVENT_CAMERA, 1, a->position, a->up, a->front);
            break;
        }
    }
}

static bool blue_chip_completion_tick(const KeyState* keys){
    Actor* group = blue_chip_group();
    if(!group || group->state < BLUE_CHIP_COMPLETION_PENDING || group->state > BLUE_CHIP_COMPLETION_EVENT)
        return false;
    if(group->state == BLUE_CHIP_COMPLETION_PENDING){
        const Player* p = &g_game.player;
        if(camera_event_busy() || p->action == MA_DEATH || p->action == MA_STAR_GET ||
           p->action == MA_STAR_LAUNCH || p->action == MA_TALK)
            return false;
        group->state = BLUE_CHIP_COMPLETION_COMPLETE;
        group->timer = -1;
    }
    group->timer++;
    if(group->state == BLUE_CHIP_COMPLETION_COMPLETE && group->timer >= BLUE_CHIP_COMPLETE_FRAMES){
        group->state = BLUE_CHIP_COMPLETION_END;
        group->timer = 0;
    }
    if(group->state == BLUE_CHIP_COMPLETION_END && group->timer >= BLUE_CHIP_END_FRAMES){
        actor_set_switch_a(group, FX(20000), OBJ_GCAPTURETARGET);
        group->state = BLUE_CHIP_COMPLETION_EVENT;
        group->timer = 0;
        blue_chip_appear_actions();
    }
    if(group->state != BLUE_CHIP_COMPLETION_EVENT) return true;
    if(group->timer >= BLUE_CHIP_EVENT_FRAMES){
        group->state = BLUE_CHIP_COMPLETION_POST;
        camera_event_cancel();
        return false;
    }

    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(!a->active || !blue_chip_completion_cast(a)) continue;
        a->timer = group->timer;
        gcapture_appear_tick(a);
        a->anim_frame += FX_ONE;
    }
    return true;
}
