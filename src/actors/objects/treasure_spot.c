static void treasurespot_think(Actor* a){
    if(a->state == TS_BLOOM){
        if(actor_anim_done(a)) actor_set_state(a, TS_END, -1);
        return;
    }
    if(a->state == TS_END) return;
    if(g_mario.attack_timer == 0) a->health = 0;
    if(g_mario.attack_timer > 0 && !a->health){
        a->health = 1;
        if(actor_mario_within(a, FX(300)) && g_game.star_bits > 0){
            g_game.star_bits--;
            collectibles_pop(OBJ_COIN, v3_add(a->position, v3_scale14(a->up, FX(60))), a->up, 1);
            audio_play_effect(SFX_STARBIT, 2);
            if(a->type == OBJ_COINFLOWER) actor_set_state(a, TS_BLOOM, ANIM_COINFLOWER_BLOOM);
            else actor_defeat(a, false);
        }
    }
}

static void treasurespot_init(Actor* a, const StageObject* o){ actor_set_state(a, TS_WAIT, -1); }

static void coinflower_init(Actor* a, const StageObject* o){
    actor_set_state(a, TS_WAIT, ANIM_COINFLOWER_BUD);
}
