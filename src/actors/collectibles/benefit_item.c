static void benefit_think(Actor* a){
    if(a->timer < 60 && a->type == OBJ_INVINCIBLE) return;
    if(actor_mario_contact(a, FX(20)) == CONTACT_NONE) return;
    if(a->type == OBJ_INVINCIBLE){
        g_mario_star_power = 1200;
        audio_play_effect(SFX_ONEUP, 4);
    } else mario_lifeup_pickup();
    a->active = 0;
}

static void lifeup_spawned(Actor* a, const StageObject* o){
    if(mario_lifeup_dark()) a->active = 0;
}
