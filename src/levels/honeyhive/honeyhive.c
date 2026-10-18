static EWRAM_BSS HoneyMission g_honey;

static inline bool honey_stage(void){
    if(g_game.stage_index == STAGE_HONEYHIVE_TOWER) return true;
    return g_game.stage_index == STAGE_HONEYHIVE;
}

static bool honey_handles_actor(const Actor* a){
    if(g_game.stage_index == STAGE_BUOYBASE)
        return a->type == OBJ_TAKOHEI || a->type == OBJ_WATERPRESSURE || a->type == OBJ_WATERBULLET;
    if(a->type == OBJ_ROCKCREATOR || a->type == OBJ_ROCK) return false;
    if(honey_stage()) return true;
    return false;
}

static int honey_mission_stage(int stage, int mission){
    if(stage == STAGE_HONEYHIVE && (mission == 2 || mission == 3)) return STAGE_HONEYHIVE_TOWER;
    return stage;
}

static void honey_reset(void){
    g_honey = (HoneyMission){0};
    cosmic_reset();
}

static bool honey_star_hidden(int id){
    if(id == 5 || id == 6) return true;
    if(id >= 1 && id <= 3) return !g_honey.star_out[id];
    return false;
}

static void honey_star_appear_at(int id, v3 from){
    Game* game = &g_game;
    if(id <= 0 || id >= 8 || g_honey.star_out[id]) return;
    g_honey.star_out[id] = 1;
    g_honey.star_from[id] = from;
    for(int i = 0; i < game->actor_count; i++){
        Actor* a = &game->actors[i];
        if(a->type != OBJ_POWERSTAR || a->stage_index < 0 || a->args[0] != id) continue;
        a->active = 1;
        a->timer = 0;
        a->value = 0;
        a->target = from;
        a->position = from;
        g_object_state[a->stage_index] |= OBJECT_STATE_WAS_ACTIVE;
    }
}

static bool honey_star_busy(const Actor* a){ return honey_handles_actor(a) && a->value >= 0; }

static bool honey_own_kind(int type){
    switch(type){
    case OBJ_BEEFLOWER:
    case OBJ_BEEMUSHROOM:
    case OBJ_BOSSBOMB:
    case OBJ_BOSSSTINKBUG:
    case OBJ_COINBLOCK:
    case OBJ_CREEPER:
    case OBJ_FORESTOBSTACLEWOOD:
    case OBJ_FOUNTAINBIG:
    case OBJ_HANACHAN:
    case OBJ_HANACHANBODY:
    case OBJ_HIPDROPROCK:
    case OBJ_HIPDROPSWITCH:
    case OBJ_HONEYQUEEN:
    case OBJ_INVINCIBLE:
    case OBJ_JUMPSTAND:
    case OBJ_KARIPON:
    case OBJ_KOTEBUG:
    case OBJ_LIFEUP:
    case OBJ_LOTUSLEAF:
    case OBJ_PICKETSWITCH:
    case OBJ_PLANT:
    case OBJ_POLE:
    case OBJ_QUESTIONCOIN:
    case OBJ_STINKBUGPARENT:
    case OBJ_TAKOHEI:
    case OBJ_TRAPEZE:
    case OBJ_WATCHTOWERLIFT:
    case OBJ_WATCHTOWERSTEP:
    case OBJ_WATERBULLET:
    case OBJ_WATERPRESSURE:
    case OBJ_YELLOWCHIP: return true;
    default: return false;
    }
}

static void honey_actor_spawned(Actor* a, const StageObject* o){
    if(!honey_stage() && g_game.stage_index != STAGE_BUOYBASE) return;
    if(!honey_handles_actor(a)) return;
    switch(a->type){
    case OBJ_POWERSTAR:
        a->value = -1;
        if(honey_star_hidden(o->args[0]) && !power_star_revealed(a->stage_index)) a->active = 0;
        break;
    case OBJ_TRAPEZE:
        a->state = ACTOR_STATE_WAIT;
        trapeze_init(a);
        break;
    case OBJ_FORESTOBSTACLEWOOD: obstacle_wood_init(a); break;
    case OBJ_CRYSTALCAGES: crystal_cage_small_init(a); break;
    case OBJ_BEEMUSHROOM:
        a->state = o->camera >= 0 && o->args[5] > 0 && o->sw[SWITCH_APPEAR] >= 0 &&
                           !(stream_part_state(a->stage_index) & 1u)
                       ? BEE_MUSHROOM_APPEAR_PENDING
                       : ACTOR_STATE_WAIT;
        if(a->state == BEE_MUSHROOM_APPEAR_PENDING && o->args[0] != 1) a->flags |= ACTOR_HIDDEN;
        break;
    default:
        if(honey_own_kind(a->type)) a->state = ACTOR_STATE_WAIT;
        break;
    }
}
