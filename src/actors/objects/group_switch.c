static bool group_switch_fires(int first, int count, int logic){
    bool any = logic == 0;
    if(!any && logic != -1) return false;
    for(int k = 0; k < count; k++)
        if(switch_on(first + k) == any) return any;
    return !any;
}

static void group_switch_update(Actor* a, bool wait_while_output_on){
    int action = actor_arg_or(a, 2, -1);
    if(wait_while_output_on && action == -1 && actor_switch_on(a, SWITCH_A)) return;
    int b = actor_switch_key(a, SWITCH_B);
    if(b < 0) return;
    bool fire = group_switch_fires(b, actor_arg_or(a, 0, 1), actor_arg_or(a, 1, -1));
    if(action == -1){
        if(fire){
            switch_set(actor_switch_key(a, SWITCH_A), true);
            a->active = 0;
        }
    } else {
        switch_set(actor_switch_key(a, SWITCH_A), fire);
    }
}

static void group_switch_think(Actor* a){ group_switch_update(a, false); }

static void group_switch_actors_tick(void){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(a->active && a->type == OBJ_GROUPSWITCHWATCHER) group_switch_update(a, true);
    }
}
