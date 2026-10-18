static const WaterLeakSpec* deep_waterleak_spec(int id){
    for(unsigned i = 0; i < sizeof deep_waterleak_specs / sizeof *deep_waterleak_specs; i++)
        if(deep_waterleak_specs[i].source == id) return &deep_waterleak_specs[i];
    return NULL;
}

static bool deep_actor_init(Actor* a, const StageObject* o){
    if(g_game.stage_index != STAGE_DEEPDARK) return false;
    switch(a->type){
    case OBJ_WATERLEAKPIPE: {
        const WaterLeakSpec* spec = waterleak_find_spec(o->id);
        return waterleak_init_spec(a, o, spec ? spec : deep_waterleak_spec(o->id));
    }
    case OBJ_WATERBAZOOKA:
        a->health = 3;
        a->args[0] = 0;
        a->args[1] = a->args[2] = -1;
        a->value = -1;
        a->home = a->position;
        a->target = a->front;
        actor_set_state(a, WATER_BAZOOKA_WAIT, ANIM_WATERBAZOOKA_WAIT);
        return true;
    case OBJ_MARBLEPLANET:
        a->health = 3;
        a->args[0] = (int16_t)marble_star_object();
        a->args[1] = -1;
        actor_set_state(a, MP_WAIT, -1);
        return true;
    case OBJ_ITEMAPPEARSTONE:
        a->value = -1;
        a->home = a->position;
        a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
        actor_set_state(a, ITEM_STONE_WAIT, -1);
        return true;
    case OBJ_WATERPRESSURE:
    case OBJ_PICKETSWITCH:
        a->state = 64;
        a->timer = 0;
        return true;
    default: return false;
    }
}
static bool deep_actor_think(Actor* a){
    if(g_game.stage_index != STAGE_DEEPDARK) return false;
    switch(a->type){
    case OBJ_WATERBAZOOKA: water_bazooka_think(a); return true;
    case OBJ_MOGUCCHISHOOTER: mogucchi_shooter_think(a); return true;
    case OBJ_WBCAPSULE: water_bazooka_capsule_think(a); return true;
    case OBJ_MARBLEPLANET: marble_planet_think(a); return true;
    case OBJ_MARBLEELECTRON: marble_electron_think(a); return true;
    case OBJ_MARBLECORE: marble_core_think(a); return true;
    case OBJ_ITEMAPPEARSTONE: itemstone_think(a); return true;
    case OBJ_WATERPRESSURE: {
        int key = actor_switch_key(a, SWITCH_A);
        if(key >= 0 && !switch_on(key)){
            a->timer = 0;
            return true;
        }
        waterpressure_think(a);
        return true;
    }
    case OBJ_WATERBULLET:
        if(!(a->value & 0xFF)) a->target = gravity_sample(g_game.stage.header, a->position).up;
        waterbullet_think(a);
        return true;
    case OBJ_PICKETSWITCH: hipdrop_switch_think(a); return true;
    default: return false;
    }
}

static bool deep_bazooka_unclipped(const Actor* a){
    return a->type == OBJ_MARBLEELECTRON || a->type == OBJ_MARBLECORE || a->type == OBJ_MARBLEPLANET ||
           a->type == OBJ_WATERBULLET || a->type == OBJ_WATERBAZOOKA || a->type == OBJ_MOGUCCHISHOOTER ||
           a->type == OBJ_WBCAPSULE;
}
static void deep_actors_pre(void){
    if(g_game.stage_index != STAGE_DEEPDARK) return;
    dyncol_clear();
    waterleak_collision();
    v3 mario = g_game.player.figure.position;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(!a->active || a->type != OBJ_PICKETSWITCH) continue;
        v3 d = v3_sub(a->position, mario);
        if(fx_abs(d.x) > FX(3000) || fx_abs(d.y) > FX(3000) || fx_abs(d.z) > FX(3000)) continue;
        int16_t s[3];
        actor_stage_scale(a, s);
        dyncol_add(&honey_colmeshes[HCOL_PICKETSWITCH], a->position, a->up, a->front, s, i, 0, mario,
                   FX(600));
    }
    mario_ride_find();
}

__attribute__((noinline)) static bool deep_kind_unclipped(const Actor* a){
    return (a->type == OBJ_MARIOLAUNCHER && a->state != ML_WAIT) ||
           (a->type == OBJ_BOSSKAMECK2 && a->state > KAMELLA_DORMANT) || a->type == OBJ_BOSSKAMECKBEAM ||
           a->type == OBJ_KAMECKFIREBALL || a->type == OBJ_KAMECKTURTLE || a->type == OBJ_JETTURTLE ||
           a->type == OBJ_MARIOFIREBALL || (a->type == OBJ_AIRBUBBLE && a->state != AIRBUBBLE_WAIT) ||
           (a->type == OBJ_SYATI && a->state >= SY_FADEOUT_START && a->state <= SY_FORCE_KILL) ||
           a->type == OBJ_PRIZERING || a->type == OBJ_TERESACHIEF || deep_bazooka_unclipped(a);
}
