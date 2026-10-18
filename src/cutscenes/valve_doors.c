enum { VALVE_DOORS_WAIT = 64, VALVE_DOORS_REQUEST, VALVE_DOORS_RUN, VALVE_DOORS_DONE };
typedef struct {
    int health;
    int super_kinoko_collected;
    int damaging;
    int swimming;
    int sink_timer;
    int current_floor_sink;
    int enable_nerve_change;
    int demo_active;
    int player_dead;
    int stage_clear;
} ValveDoorsContext;

static inline int valve_doors_player_at_risk(const ValveDoorsContext* c){
    if(!c->super_kinoko_collected){
        if(c->health == 0) return 1;
        if(c->health == 1 && c->damaging && !c->swimming) return 1;
    }
    if(c->sink_timer != 0 && c->current_floor_sink) return 1;
    return !c->enable_nerve_change;
}

static inline int valve_doors_allowed(const ValveDoorsContext* c){
    if(c->demo_active) return 0;
    if(c->player_dead) return 0;
    if(c->stage_clear) return 0;
    return !valve_doors_player_at_risk(c);
}
static bool valve_doors_can_start(const Actor* executor){
    const Player* p = &g_game.player;
    bool dead = p->action == MA_DEATH;
    bool rush = g_mario_held || g_mario_link.bind >= 0 || p->action == MA_STAR_LAUNCH ||
                p->action == MA_STAR_GET || p->action == MA_TALK || cutscene_blocks(BLOCK_PLAYER);
    bool damaging = p->action == MA_DAMAGE || p->action == MA_DAMAGE_FALL || p->action == MA_FIRE_DANCE ||
                    p->action == MA_FIRE_RUN || p->action == MA_FREEZE;

    bool sink = p->ground_code == 17 || p->ground_code == 18 || p->ground_code == 25 || p->ground_code == 31;
    if(sink) return false;
    ValveDoorsContext context = {p->health,
                                 g_mario_max_health > 3,
                                 damaging,
                                 g_swim.active,
                                 0,
                                 0,
                                 !dead && !rush,
                                 cutscene_blocks(BLOCK_DEMO_START),
                                 dead,
                                 g_game.scene == SCENE_STAR_GET};
    if(!valve_doors_allowed(&context)) return false;
    const StageObject* o = actor_stage_object(executor);
    bool ice = o->id == 1022;
    return valve_object_actor(ice ? 1041 : 1086) != NULL && (ice || valve_object_actor(1087) != NULL);
}
static bool valve_doors_init(Actor* a, const StageObject* o){
    if(!valve_demo_object(o)) return false;
    a->figure.model = NULL;
    a->anim = -1;
    a->state = VALVE_DOORS_WAIT;
    a->timer = 0;
    a->args[0] = 0;
    a->args[1] = -1;
    a->args[2] = o->camera;
    a->value = o->id == 1022 ? 240 : 180;
    return true;
}
static Actor* valve_doors_actor(void){
    if(g_game.stage_index != STAGE_FREEZEFLAME) return NULL;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(a->active && valve_demo_object(actor_stage_object(a)) && a->state == VALVE_DOORS_RUN) return a;
    }
    return NULL;
}
static bool valve_doors_playing(void){ return valve_doors_actor() != NULL; }
static inline int valve_doors_advance(int* part, int* step, int second_length){
    ++*step;
    int length = *part == 0 ? 60 : second_length;
    if(*step >= length){
        ++*part;
        *step = 0;
    }
    return *part >= 2;
}
static bool valve_doors_tick(const KeyState* keys){
    if(g_game.stage_index != STAGE_FREEZEFLAME) return false;
    Actor* demo = valve_doors_actor();
    if(!demo){
        for(int i = 0; i < g_game.actor_count; i++){
            Actor* a = &g_game.actors[i];
            const StageObject* o = actor_stage_object(a);
            if(!a->active || !valve_demo_object(o) || a->state == VALVE_DOORS_DONE) continue;
            if(a->state == VALVE_DOORS_WAIT && switch_on(o->sw[SWITCH_APPEAR])){
                a->state = VALVE_DOORS_REQUEST;

                stream_spawn_near(&g_game.stage, g_game.player.figure.position, 0, g_stream.sweep_count);
            }
            if(a->state == VALVE_DOORS_REQUEST && valve_doors_can_start(a)){
                demo = a;
                a->state = VALVE_DOORS_RUN;
                a->timer = -1;
                a->args[0] = 0;
                a->args[1] = -1;
                break;
            }
        }
    }
    if(!demo) return false;
    const StageObject* o = actor_stage_object(demo);
    bool ice = o->id == 1022;
    int target = ice ? 1041 : 1086;
    Actor* cast = valve_object_actor(target);

    if(!cast || (!ice && !valve_object_actor(1087))) return true;

    ++demo->timer;

    int part = demo->args[0], partstep = demo->args[1];
    bool end = valve_doors_advance(&part, &partstep, ice ? 180 : 120);
    demo->args[0] = (int16_t)part;
    demo->args[1] = (int16_t)partstep;
    if(end){
        camera_event_cancel();
        demo->state = VALVE_DOORS_DONE;
        return false;
    }
    if(demo->args[1] == 0){
        camera_event_start(demo->args[2] + demo->args[0], 1, cast->position, cast->up, cast->front);
        if(demo->args[0] == 1){
            valve_set_state(cast, VALVE_DOOR_MOVE);
            if(!ice) valve_set_state(valve_object_actor(1087), VALVE_DOOR_MOVE);
        }
    }

    ++cast->timer;
    valve_door_think(cast);
    if(!ice){
        Actor* other = valve_object_actor(1087);
        ++other->timer;
        valve_door_think(other);
    }
    parts_collision_update();
    return true;
}
