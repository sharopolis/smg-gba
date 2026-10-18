static void signboard_think(Actor* a){
    if(actor_touches_mario(a, FX(60))) g_game.hint = HINT_TALK;
    actor_block_mario(a, FX(80), FX(200));
}
