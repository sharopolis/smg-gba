static void timer_switches_tick(void){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(!a->active || a->type != OBJ_TIMERSWITCH) continue;
        if(a->health < 0 && actor_switch_on(a, SWITCH_B)) a->health = actor_object_arg(a, 0);
        if(a->health <= 0) continue;
        if(--a->health > 0) continue;
        switch_set(actor_switch_key(a, SWITCH_A), true);
        a->active = 0;
    }
}

static void timer_switch_init(Actor* a, const StageObject* o){ a->health = -1; }
