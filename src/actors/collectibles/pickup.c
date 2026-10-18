static void pickup_think(Actor* a){
    Game* game = &g_game;
    Player* p = &game->player;
    collectible_drop_tick(a);
    if(!a->active) return;
    if(!collectible_is_drop(a) && a->flags & 2 && a->timer < 30){
        fx h = (fx)(FX(4) * (30 - a->timer)) / 8;
        a->position = v3_add(a->position, v3_scale14(a->up, h - FX(4)));
    }

    fx pickup_extra = a->type == OBJ_STARBIT ? FX(40) : 0;
    if((a->type == OBJ_COIN || a->type == OBJ_PURPLECOIN) && p->action != MA_HANG)
        pickup_extra = FX(100) - C_BODY_RADIUS;
    if(collectible_drop_ready(a) && !(a->flags & ACTOR_HIDDEN) && p->action != MA_DEATH &&
       actor_touches_mario(a, pickup_extra)){
        shellfish_item_taken(a);
        collectible_dead(a);
        if(a->type == OBJ_COIN){
            game->coins++;
            mario_swim_recover();
            if(p->health < mario_lifeup_max()) p->health++;
            audio_play_effect(SFX_COIN, 2);
        } else if(a->type == OBJ_PURPLECOIN){
            game->purple_coins++;
            purple_coin_star_coin_taken();
            audio_play_effect(SFX_PURPLE_COIN, 2);
        } else if(a->type == OBJ_STARBIT){
            game->star_bits++;
            if(game->star_bits % 50 == 0) game->lives++;
            audio_play_effect(SFX_STARBIT, 1);
        } else {
            game->lives++;
            audio_play_effect(SFX_ONEUP, 4);
        }
    }
}

static void starbit_spawned(Actor* a, const StageObject* o){
    a->figure.model = (const FigureHeader*)starbit_figures[((uint32_t)a->stage_index * 7 + 3) % 6];
}
