#define STREAM_SPAWN FX(5200)
#define STREAM_SWEEP 24

_Static_assert(sizeof(ObjectStream) == 8, "ObjectStream must be 8 bytes");
_Static_assert(STAGE_MAX_OBJECTS <= UINT16_MAX, "object count must fit uint16_t");

static EWRAM_BSS ObjectStream g_stream;
static EWRAM_BSS uint8_t g_object_state[STAGE_MAX_OBJECTS + 4];

static bool stream_launch_ready(int object){ return object >= 0 && (g_object_state[object] & 16) != 0; }
static void stream_launch_ready_set(int object, bool ready){
    if(object < 0 || !flight_default_branch(&g_game.stage.objects[object])) return;
    if(ready) g_object_state[object] |= 16;
    else g_object_state[object] &= ~16;
}

static bool stream_object_maybe_out(int object){
    return !g_stream.on || (g_object_state[object] & OBJECT_STATE_OUT) != 0;
}
static bool stream_object_collected(int object){
    return object >= 0 && (g_object_state[object] & (OBJECT_STATE_GONE | OBJECT_STATE_DEAD)) != 0;
}
static unsigned stream_part_state(int object){ return object >= 0 ? g_object_state[object] >> 4 : 0; }
static void stream_part_state_set(int object, unsigned state){
    if(object >= 0) g_object_state[object] = (g_object_state[object] & 15) | (state << 4);
}

static inline bool stream_near(v3 a, v3 b, fx r){
    v3 d = v3_sub(a, b);
    fx x = fx_abs(d.x), y = fx_abs(d.y), z = fx_abs(d.z);
    if(x > r || y > r || z > r) return false;

    fx inner = (fx)(((int64_t)r * 37837) >> 16);
    if(x <= inner && y <= inner && z <= inner) return true;
    return v3_length_squared64(d) <= (uint64_t)((int64_t)r * r);
}

static Actor* actors_stream_free_slot(void){
    Game* game = &g_game;
    if(!g_stream.on) return NULL;
    for(int i = 0; i < game->actor_count; i++){
        Actor* a = &game->actors[i];
        if(actor_slot_reusable(a)) return a;
    }
    return NULL;
}

static inline bool stream_pickup(int type){
    return type == OBJ_COIN || type == OBJ_PURPLECOIN || type == OBJ_STARBIT || type == OBJ_ONEUP ||
           type == OBJ_POWERSTAR;
}

#define STREAM_SPAWN_ITEMS FX(3400)
#define STREAM_KEEP_ITEMS  FX(4400)

static bool stream_decor(int type){
    switch(type){
    case OBJ_COIN:
    case OBJ_STARBIT:
    case OBJ_CUTBUSHGROUP:
    case OBJ_FLOWERGROUP:
    case OBJ_FLOWERBLUEGROUP:
    case OBJ_STARPIECESPOT:
    case OBJ_STARPIECEGROUP:
    case OBJ_BUTTERFLY:
    case OBJ_TREASURESPOT:
    case OBJ_COINFLOWER:
    case OBJ_SMALLSTONE:
    case OBJ_ITEMBUBBLE:
    case OBJ_SIGNBOARD:
    case OBJ_ONEUP:
    case OBJ_PURPLECOIN: return true;
    }
    return false;
}

static bool stream_important(int type){
    if(level_stream_important(type)) return true;
    if(volcano_part_type(type) || valve_type(type)) return true;
    switch(type){
    case OBJ_LAUNCHSTAR:
    case OBJ_SLINGSTAR:
    case OBJ_POWERSTAR:
    case OBJ_TICO:
    case OBJ_KINOPIO:
    case OBJ_LUIGI:
    case OBJ_PENGUINSKATER:
    case OBJ_WATERLEAKPIPE:
    case OBJ_BUOYBABYLAUNCHER:
    case OBJ_BOSSKAMECK2:
    case OBJ_KAMECK:
    case OBJ_JETTURTLE:
    case OBJ_POLESQUARE:
    case OBJ_MARIOLAUNCHER:
    case OBJ_SHUTTERDOORE:
    case OBJ_PHANTOMFLOATERA:
    case OBJ_PHANTOMFLOATERB:
    case OBJ_PHANTOMFLOATERC:
    case OBJ_AIRBUBBLE:
    case OBJ_AIRBUBBLEGENERATOR:
    case OBJ_SYATI:
    case OBJ_TALKSYATI:
    case OBJ_PRIZERING:
    case OBJ_TERESACHIEF:
    case OBJ_ARROWSWITCHMULTI:
    case OBJ_JUMPBRANCH:
    case OBJ_FFMOUNTAINFLOATER:
    case OBJ_FFMOUNTAINNOSLIP:
    case OBJ_FFMOUNTAINSLIPA:
    case OBJ_FFMOUNTAINSLIPB:
    case OBJ_FFMOUNTAINSLIPC:
    case OBJ_FFMOUNTAINSLIPD:
    case OBJ_SHELLFISH:
    case OBJ_CRYSTALCAGEL:
    case OBJ_TICOFAT:
    case OBJ_CRYSTALCAGES:
    case OBJ_CRYSTALCAGEM:
    case OBJ_YELLOWCHIP:
    case OBJ_BLUECHIP:
    case OBJ_YELLOWCHIPGROUP:
    case OBJ_BLUECHIPGROUP:
    case OBJ_GCAPTURETARGET:
    case OBJ_DINOPACKUN:
    case OBJ_OTAKING:
    case OBJ_PLANT:
    case OBJ_BROADBEANMOVESTEPA:
    case OBJ_BROADBEANMOVESTEPB:
    case OBJ_RGTOWERPRESSGROUND:
    case OBJ_PHANTOMSHIPBRIDGE:
    case OBJ_FLIPPANEL:
    case OBJ_FLIPPANELOBSERVER:
    case OBJ_SIDESPIKEMOVESTEP:
    case OBJ_COLLAPSEPLANE:
    case OBJ_EARTHENPIPE:
    case OBJ_GROUPSWITCHWATCHER:
    case OBJ_COCOSAMBO:
    case OBJ_HAMMERHEADPACKUN:
    case OBJ_PACKUNPETIT:
    case OBJ_NEEDLEPLANT:
    case OBJ_OTAROCK:
    case OBJ_TAKOBARREL:
    case OBJ_ROCKCREATOR:
    case OBJ_WANWANROLLING:
    case OBJ_STARPIECESPOT:
    case OBJ_TIMERSWITCH: return true;
    }
    return false;
}

static void stream_release_members(const Actor* parent){
    if(parent->type == OBJ_BUOYBABYLAUNCHER){
        babylauncher_release_child(parent);
        return;
    }
    int child = -1;
    switch(parent->type){
    case OBJ_WATERLEAKPIPE: child = OBJ_WATERLEAKICE; break;
    case OBJ_CUTBUSHGROUP:
    case OBJ_FLOWERGROUP:
    case OBJ_FLOWERBLUEGROUP: child = OBJ_PLANTMEMBER; break;
    case OBJ_SMALLSTONE: child = OBJ_SMALLSTONEMEMBER; break;
    default: return;
    }

    int owner = actor_index(parent);
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* member = &g_game.actors[i];
        if(member->active && member->type == child && member->value == owner) member->active = 0;
    }
}

static bool stream_evict_preserving(v3 mario, v3 burst_origin){
    Game* game = &g_game;
    int best = -1;
    uint64_t best_d = (uint64_t)FX(1500) * FX(1500);
    for(int i = 0; i < game->actor_count; i++){
        Actor* a = &game->actors[i];
        if(!a->active || a->stage_index < 0 || stream_important(a->type) || (a->flags & ACTOR_WAITING) ||
           actor_unclipped(a))
            continue;

        if(v3_length_squared64(v3_sub(a->position, burst_origin)) <= (uint64_t)FX(600) * FX(600)) continue;
        uint64_t d = v3_length_squared64(v3_sub(a->position, mario));
        if(d > best_d){
            best_d = d;
            best = i;
        }
    }
    if(best < 0) return false;
    Actor* a = &game->actors[best];
    stream_release_members(a);
    g_object_state[a->stage_index] &= ~(OBJECT_STATE_OUT | OBJECT_STATE_WAS_ACTIVE);
    a->active = 0;

    a->flags &= ~ACTOR_WAITING;
    a->stage_index = -1;
    return true;
}
static bool stream_evict(v3 mario){ return stream_evict_preserving(mario, mario); }

static void stream_spawn_near(const Stage* stage, v3 mario, uint32_t first, uint32_t count){
    uint32_t n = g_stream.sweep_count;
    uint32_t i = n ? first % n : 0;
    StreamRule level_rule = level_stream_rule();
    for(uint32_t k = 0; k < count && n; k++, i = i + 1 < n ? i + 1 : 0){
        const StageObject* o = &stage->objects[i];
        if(o->type == OBJ_SCENARIOSTARTER) continue;
        int rule = level_rule ? level_rule(o) : STREAM_DEFAULT;
        if(rule == STREAM_SKIP) continue;
        if(g_object_state[i] & (OBJECT_STATE_OUT | OBJECT_STATE_GONE)) continue;
        if(o->scenarios != 0xFF && !(o->scenarios & g_stream.scenario_bit)) continue;
        v3 pos = V3(o->position[0], o->position[1], o->position[2]);

        if(rule != STREAM_REQUIRED &&
           ((o->type == OBJ_LAUNCHSTAR && flight_default_branch(o))
                ? !flight_prefetch_near(pos)
                : !stream_near(pos, mario, stream_decor(o->type) ? STREAM_SPAWN_ITEMS : STREAM_SPAWN)))
            continue;
        const ActorKind* kind = &actor_kinds[o->type];
        if((kind->present && !kind->present(o)) || !volcano_launch_star_available(o)) continue;
        if(o->type == OBJ_FLIPPANEL && !flippanel_available(o)) continue;

        int appear = stage_object_switch(o, SWITCH_APPEAR);
        if(appear >= 0 && !switch_on(appear) && !valve_demo_object(o)) continue;

        int sleep = stage_object_switch(o, SWITCH_SLEEP);
        if(sleep >= 0 && !switch_on(sleep)) continue;
        Actor* a = actors_spawn_object(stage, i);

        if(!a){
            bool room =
                stream_important(o->type)
                    ? stream_evict(mario)
                    : stream_pickup(o->type) &&
                          stream_near(V3(o->position[0], o->position[1], o->position[2]), mario, FX(1500)) &&
                          stream_evict_preserving(mario, V3(o->position[0], o->position[1], o->position[2]));
            if(room) a = actors_spawn_object(stage, i);
        }

        if(!a) continue;
        g_object_state[i] |= OBJECT_STATE_OUT | (a->active ? OBJECT_STATE_WAS_ACTIVE : 0);
    }
}

static bool actors_stream_start(const Stage* stage, int scenario){
    g_stream.on = stage->header->galaxy != 0;
    if(!g_stream.on) return false;
    g_stream.scenario_bit = scenario > 0 ? (uint8_t)(1 << (scenario - 1)) : 1;
    g_stream.sweep_count = level_stream_count(stage);
    g_stream.next = 0;
    for(uint32_t i = 0; i < sizeof g_object_state; i++) g_object_state[i] = 0;
    stream_spawn_near(stage, g_game.player.figure.position, 0, g_stream.sweep_count);
    return true;
}

static void actors_stream_tick(void){
    Game* game = &g_game;
    if(!g_stream.on) return;
    level_before_stream();
    const Stage* stage = &game->stage;
    v3 mario = game->player.figure.position;
    ActorTest level_keep = level_stream_keep();
    for(int i = 0; i < game->actor_count; i++){
        Actor* a = &game->actors[i];
        int s = a->stage_index;
        if(s < 0) continue;
        uint8_t* st = &g_object_state[s];
        if(!a->active && (*st & OBJECT_STATE_WAS_ACTIVE) && !(*st & OBJECT_STATE_DEAD)){
            *st |= OBJECT_STATE_DEAD;
            switch_set(stage_object_switch(&stage->objects[s], SWITCH_DEAD), true);
            if(stream_pickup(a->type)){
                *st = OBJECT_STATE_GONE | OBJECT_STATE_DEAD;

                a->flags &= ~ACTOR_WAITING;
                a->stage_index = -1;
                continue;
            }
        }

        if(level_keep && level_keep(a)) continue;
        if(stream_near(a->active ? a->position : a->home, mario,
                       stream_decor(a->type) ? STREAM_KEEP_ITEMS : STREAM_KEEP))
            continue;
        if(a->active && (((a->type == OBJ_LAUNCHSTAR || a->type == OBJ_SLINGSTAR) && flight_star_keep(a)) ||
                         actor_unclipped(a)))
            continue;
        if(*st & OBJECT_STATE_DEAD) *st = OBJECT_STATE_GONE | OBJECT_STATE_DEAD;
        else *st &= ~(OBJECT_STATE_OUT | OBJECT_STATE_WAS_ACTIVE);
        stream_release_members(a);
        a->active = 0;

        a->flags &= ~ACTOR_WAITING;
        a->stage_index = -1;
    }
    stream_spawn_near(stage, mario, g_stream.next, STREAM_SWEEP);
    uint32_t n = g_stream.sweep_count;
    if(n) g_stream.next = (g_stream.next + STREAM_SWEEP) % n;
}
