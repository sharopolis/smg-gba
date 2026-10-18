static void takobarrel_think(Actor* a){
    if(actor_switch_on(a, SWITCH_B)){
        actor_defeat(a, false);
        return;
    }
    actor_block_mario(a, FX(200), FX(400));
}
