static bool fireball_attack_enemies(Actor* a){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* other = &g_game.actors[i];
        if(i == a->value || !other->active || (other->type != OBJ_OTAROCK && other->type != OBJ_HANACHANBODY))
            continue;
        if(other->type == OBJ_OTAROCK && other->state == OTAROCK_DOWN) continue;
        fx radius = other->radius + FX(40);
        if(v3_length_squared64(v3_sub(actor_centre(other), a->position)) > (uint64_t)radius * radius)
            continue;
        if(other->type == OBJ_OTAROCK) actor_set_state(other, OTAROCK_DOWN, ANIM_OTAROCK_DEATH);
        return true;
    }
    return false;
}

static void otarock_ball_arrives(Actor* rock, Actor* ball){
    actor_defeat(ball, false);
    if(rock->state == OTAROCK_DOWN) return;
    actor_set_state(rock, OTAROCK_DOWN, ANIM_OTAROCK_DEATH);
    audio_play_effect(SFX_ENEMY_DEFEAT, 3);
}

static int otarock_live_balls(const Actor* a){
    int n = 0;
    for(int i = 0; i < g_game.actor_count; i++){
        const Actor* b = &g_game.actors[i];
        n += b->active && b->type == OBJ_COCONUTBALL && b->value == actor_index(a) && b->state != AS_DEFEATED;
    }
    return n;
}

static void otarock_think(Actor* a){
    switch(a->state){
    case OTAROCK_HIDDEN:
        if(actor_mario_within(a, FX(3800))) actor_set_state(a, OTAROCK_SHOW, ANIM_OTAROCK_SHOW);
        return;
    case OTAROCK_SHOW:
        if(actor_anim_done(a)){
            a->value = (int16_t)(30 + g_game.tick % 61);
            actor_set_state(a, OTAROCK_WAIT, ANIM_OTAROCK_WAIT);
        }
        break;
    case OTAROCK_WAIT:
        actor_turn_toward(a, actor_to_mario(a), DEG(3));
        if(!actor_mario_within(a, FX(4000)) && otarock_live_balls(a) == 0){
            actor_set_state(a, OTAROCK_HIDE, ANIM_OTAROCK_HIDE);
            break;
        }
        if(a->timer > a->value) actor_set_state(a, OTAROCK_THROW, ANIM_OTAROCK_ATTACK);
        break;
    case OTAROCK_THROW:
        actor_turn_toward(a, actor_to_mario(a), DEG(3));
        if(a->timer == 40){
            v3 mouth = v3_add(a->position, v3_add(v3_scale14(a->up, FX(180)), v3_scale14(a->front, FX(170))));
            if(a->health < 2){
                coconut_ball_throw(a, mouth, v3_add(mario_centre(), v3_scale14(a->up, FX(120))), V16(15.0f));
                a->health++;
            } else {
                fireball_throw(a, mouth, V16(15.0f), 0);
                a->health = 0;
            }
        }
        if(actor_anim_done(a)){
            a->value = (int16_t)(30 + g_game.tick % 61);
            actor_set_state(a, OTAROCK_WAIT, ANIM_OTAROCK_WAIT);
        }
        break;
    case OTAROCK_HIDE:
        if(actor_anim_done(a)) actor_set_state(a, OTAROCK_HIDDEN, -1);
        return;
    case OTAROCK_DOWN:
        if(actor_anim_done(a)){
            actor_set_switch_dead(a, FX(2500));
            actor_defeat(a, false);
        }
        return;
    }
    int c = actor_mario_contact(a, 0);
    if(c == CONTACT_STAR){
        actor_set_state(a, OTAROCK_DOWN, ANIM_OTAROCK_DEATH);
        return;
    }
    if(c != CONTACT_NONE) actor_hurt_mario(a, 1);
}

static void otarock_init(Actor* a, const StageObject* o){
    actor_set_state(a, OTAROCK_HIDDEN, ANIM_OTAROCK_HIDE);
    a->anim_frame = FX(1000);
}

static void otarock_blown_away(Actor* a, Actor* by){
    if(a->state != OTAROCK_DOWN && a->state != OTAROCK_HIDDEN)
        actor_set_state(a, OTAROCK_DOWN, ANIM_OTAROCK_DEATH);
}
