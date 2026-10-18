static void woodbox_think(Actor* a){
    if(a->state == WB_HIT){
        if(a->timer == 15){
            collectibles_pop(OBJ_COIN, actor_centre(a), a->up, actor_arg_or(a, 0, 1));
            if(actor_arg_or(a, 2, 0) > 0)
                collectibles_pop(OBJ_STARBIT, actor_centre(a), a->up, actor_arg_or(a, 2, 0));
            actor_set_switch_dead(a, FX(300));
            actor_defeat(a, false);
        }
        return;
    }
    int c = actor_mario_contact(a, FX(60));
    if(c == CONTACT_SPIN || c == CONTACT_HIPDROP || c == CONTACT_STAR){
        if(c == CONTACT_SPIN) mario_spin_hop();
        actor_set_state(a, WB_HIT, -1);
        audio_play_effect(SFX_ENEMY_HIT, 3);
        return;
    }
    actor_block_mario(a, FX(100), FX(200));
}

static void woodbox_init(Actor* a, const StageObject* o){ actor_set_state(a, WB_WAIT, -1); }
