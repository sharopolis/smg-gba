static void luigi_init(Actor* a, const StageObject* o){
    actor_set_state(a, LG_WAIT, ANIM_LUIGINPC_WAIT);
    if(actor_arg_or(a, 0, -1) != 1) return;
    RayHit hit;
    if(collision_ray(a->position, v3_add(a->position, v3_scale14(a->front, FX(500))), &hit, 0))
        a->position = v3_sub(hit.point, v3_scale14(a->front, FX(16)));
    actor_set_state(a, LG_TREE_WAIT, ANIM_LUIGINPC_TREEWAIT);
}

static void luigi_tree_hit(Actor* a){
    if(a->state != LG_TREE_WAIT && a->state != LG_TREE_TOUCH) return;
    a->velocity = v3_add(v3_scale14(a->front, -V16(4.0f)), v3_scale14(a->up, V16(20.0f)));
    a->flags &= ~ACTOR_ON_GROUND;
    actor_set_state(a, LG_TREE_FALL, ANIM_LUIGINPC_TREEHIT);
}

static void luigi_think(Actor* a){
    switch(a->state){
    case LG_WAIT:
    case LG_DONE:
    case LG_TALK: talk_actor_think(a); break;
    case LG_TREE_WAIT:
    case LG_TREE_TOUCH: {
        int contact = actor_mario_contact(a, FX(20));
        if((g_mario.attack_timer && actor_touches_mario(a, FX(60))) || contact == CONTACT_TRAMPLE ||
           contact == CONTACT_HIPDROP){
            luigi_tree_hit(a);
        } else if(a->state == LG_TREE_WAIT && contact != CONTACT_NONE){
            actor_set_state(a, LG_TREE_TOUCH, ANIM_LUIGINPC_TREETRAMPLED);
        } else if(a->state == LG_TREE_TOUCH && actor_anim_done(a)){
            actor_set_state(a, LG_TREE_WAIT, ANIM_LUIGINPC_TREEWAIT);
        }
        break;
    }
    case LG_TREE_FALL:
        actor_follow_gravity(a, N14_ONE);
        actor_move(a, V16(1.0f), N14_ONE, N14_ONE);
        if(a->flags & ACTOR_ON_GROUND){
            a->velocity = V3(0, 0, 0);
            actor_set_state(a, LG_TREE_LAND, ANIM_LUIGINPC_TREELAND);
        }
        break;
    case LG_TREE_LAND:
        if(actor_anim_done(a)) actor_set_state(a, LG_WAIT, ANIM_LUIGINPC_WAIT);
        break;
    case LG_STAR:
        if(a->timer == 87) audio_play_effect(SFX_LUIGI_LIFT_UP, 3);
        if(actor_anim_done(a)){
            power_stars_reveal_near(a, FX(1500));
            actor_set_state(a, LG_DONE, ANIM_LUIGINPC_WAIT);
        }
        break;
    }
    actor_block_mario(a, FX(50), FX(170));
}

static void luigi_talk_event(Actor* a, int arg){
    if(arg == 0 && a->state != LG_STAR && a->state != LG_DONE)
        actor_set_state(a, LG_STAR, ANIM_LUIGINPC_TAKEOUTSTAR);
}

static bool luigi_present(const StageObject* o){
    int s = g_game.stage_index;
    return (g_game.scenario == 6 ||
            (game_star_collected(s, 1) && game_star_collected(s, 2) && game_star_collected(s, 3))) &&
           !game_star_collected(s, 6);
}
