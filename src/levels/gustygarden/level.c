static void gusty_camera(void){ dig_planet_camera(); }

static void gusty_actors_pre(void){
    if(g_fluff_cooldown) g_fluff_cooldown--;
    hipdrop_target_update();
    dyncol_clear();
}

static bool gusty_actor_init(Actor* a, const StageObject* o){
    if(a->type == OBJ_POWERSTAR){
        a->value = -1;
        return false;
    }
    return rabbit_actor_init(a, o) || gold_chomp_init(a, o) || fluff_init(a, o) || dig_planet_init(a, o);
}

static bool gusty_power_star(Actor* a){
    if(!gusty_star_busy(a)) return false;
    power_star_fly_home(a);
    return true;
}

static int gusty_actor_unclipped(const Actor* a){
    return rabbit_unclipped(a) || gusty_star_unclipped(a) || dig_planet_unclipped(a) ||
                   (fluff_has_rider() && actor_index(a) == fluff_rider_slot())
               ? UNCLIPPED_YES
               : UNCLIPPED_DEFAULT;
}

static bool gusty_actor_bounds(const Actor* a, v3* centre, fx* extent){
    fx dig = dig_planet_extent(a);
    if(!dig) return false;
    *extent = dig;
    return true;
}

static void gusty_actor_figure(const Actor* a, Figure* f){
    if(dig_planet_no_yaw(a)) f->local_yaw = dig_planet_yaw(a);
}

static bool gusty_stream_important(int type){
    return type == OBJ_FLUFF || type == OBJ_FLUFFROOT || type == OBJ_WANWANROLLINGGOLD ||
           type == OBJ_RABBIT || type == OBJ_TRICKRABBIT || type == OBJ_DODORYU || type == OBJ_MOGUCCHI ||
           type == OBJ_PICKETSWITCHBIG || type == OBJ_WORMEATENPLANET;
}

static const LevelHooks gusty_level = {
    .galaxy_stage = STAGE_GUSTYGARDEN,
    .hidden_mission = 6,
    .hidden_scenario = 3,
    .daredevil_mission = 4,
    .load_before_actors = dyncol_clear,
    .before_actors = gusty_actors_pre,
    .after_camera = gusty_camera,
    .actor_init = gusty_actor_init,
    .actor_unclipped = gusty_actor_unclipped,
    .actor_bounds = gusty_actor_bounds,
    .actor_figure = gusty_actor_figure,
    .power_star = gusty_power_star,
    .star_appear = gusty_star_appear,
    .stream_important = gusty_stream_important,
};
