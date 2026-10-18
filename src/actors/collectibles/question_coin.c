static void questioncoin_think(Actor* a){
    if(actor_mario_contact(a, 0) == CONTACT_NONE) return;
    audio_play_effect(SFX_QUESTION_COIN, 3);
    actor_set_switch_dead(a, FX(3000));
    a->active = 0;
}
