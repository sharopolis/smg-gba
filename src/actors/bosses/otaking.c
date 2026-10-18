enum {
    OTAKING_SLEEP = 64,
    OTAKING_WAIT,
    OTAKING_COCO_WAIT,
    OTAKING_COCO,
    OTAKING_FIRE_WAIT,
    OTAKING_FIRE,
    OTAKING_BUBBLE,
    OTAKING_HITBACK,
    OTAKING_DAMAGE,
    OTAKING_POWERUP,
    OTAKING_DOWN,
    OTAKING_GONE
};

static int otaking_live_projectiles(const Actor* a, int type){
    int n = 0;
    for(int i = 0; i < g_game.actor_count; i++){
        const Actor* b = &g_game.actors[i];
        n += b->active && b->type == type && b->value == actor_index(a) && b->state != AS_DEFEATED;
    }
    return n;
}

static bool otaking_can_coco(const Actor* a){
    return otaking_live_projectiles(a, OBJ_COCONUTBALL) == 0 && actor_mario_within(a, FX(4000));
}
static bool otaking_can_fire(const Actor* a){
    return otaking_live_projectiles(a, OBJ_FIREBALL) <= 3 && actor_mario_within(a, FX(4000));
}
static bool otaking_can_bubble(const Actor* a){
    return a->health == 2 && otaking_live_projectiles(a, OBJ_FIREBALL) == 0;
}

static v3 otaking_mouth(const Actor* a){
    return v3_add(a->position, v3_add(v3_scale14(a->up, FX(330)), v3_scale14(a->front, FX(260))));
}

static void otaking_ball_arrives(Actor* a, Actor* ball){
    if(a->state >= OTAKING_DAMAGE){
        actor_defeat(ball, false);
        return;
    }
    if(ball->health <= a->health){
        actor_set_state(a, OTAKING_HITBACK, ANIM_OTAKING_HITBACK);
        ball->velocity = v3_scale14(v3_direction14(v3_sub(mario_centre(), ball->position), a->front),
                                    V16(15.0f) + V16(3.0f) * ball->health);
        actor_set_state(ball, COCONUT_BALL_THROW, -1);
        audio_play_effect(SFX_ENEMY_HIT, 3);
        return;
    }
    actor_defeat(ball, false);
    a->health++;
    audio_play_effect(SFX_ENEMY_DEFEAT, 5);
    if(a->health == 1){
        actor_set_state(a, OTAKING_DAMAGE, ANIM_OTAKING_DAMAGE);
        collectibles_pop_spread(OBJ_STARBIT,
                                collectible_fall_enabled(8) ? v3_add(a->position, v3_scale14(a->up, FX(300)))
                                                            : otaking_mouth(a),
                                a->up, 8, 60, 50);
    } else if(a->health == 2){
        actor_set_state(a, OTAKING_POWERUP, ANIM_OTAKING_CROWNDAMAGE);
        collectibles_pop_spread(OBJ_STARBIT,
                                collectible_fall_enabled(16) ? v3_add(a->position, v3_scale14(a->up, FX(300)))
                                                             : otaking_mouth(a),
                                a->up, 16, 60, 50);
    } else actor_set_state(a, OTAKING_DOWN, ANIM_OTAKING_DOWN);
}

static void otaking_next(Actor* a){
    if(otaking_can_bubble(a)) actor_set_state(a, OTAKING_BUBBLE, ANIM_OTAKING_BUBBLEATTACK);
    else if(a->health > 0 && otaking_can_fire(a) && a->state == OTAKING_COCO)
        actor_set_state(a, OTAKING_FIRE, ANIM_OTAKING_FIREATTACK);
    else if(a->health > 0 && otaking_can_coco(a) && a->state == OTAKING_FIRE)
        actor_set_state(a, OTAKING_COCO, ANIM_OTAKING_ATTACK);
    else if(a->state == OTAKING_COCO && (a->health > 0 || a->args[2] >= 2))
        actor_set_state(a, OTAKING_FIRE_WAIT, ANIM_OTAKING_WAIT);
    else if(a->state == OTAKING_COCO) actor_set_state(a, OTAKING_COCO_WAIT, ANIM_OTAKING_WAIT);
    else actor_set_state(a, OTAKING_WAIT, ANIM_OTAKING_WAIT);
}

static void otaking_think(Actor* a){
    if(a->state == OTAKING_GONE) return;
    if(a->state < OTAKING_SLEEP){
        a->health = 0;
        a->args[2] = 0;
        actor_set_state(a, OTAKING_SLEEP, ANIM_OTAKING_WAIT);
    }

    if(a->state != OTAKING_DOWN && a->state != OTAKING_SLEEP){
        a->velocity.x = fx_min(a->velocity.x + DEG(0.003f) * 16, DEG(1) * 16);
        actor_turn_toward(a, actor_to_mario(a), a->velocity.x / 16);
    }
    switch(a->state){
    case OTAKING_SLEEP:
        if(actor_has_switch(a, SWITCH_A) ? actor_switch_on(a, SWITCH_A) : actor_mario_within(a, FX(4000)))
            actor_set_state(a, OTAKING_FIRE_WAIT, ANIM_OTAKING_WAIT);
        break;
    case OTAKING_WAIT:
    case OTAKING_COCO_WAIT: {
        int t1 = a->state == OTAKING_WAIT ? 75 : 45;
        if(a->timer > t1 && otaking_can_coco(a) && (a->health > 0 || a->args[2] < 2))
            actor_set_state(a, OTAKING_COCO, ANIM_OTAKING_ATTACK);
        else if(a->timer > 2 * t1 && otaking_can_fire(a))
            actor_set_state(a, OTAKING_FIRE, ANIM_OTAKING_FIREATTACK);
        break;
    }
    case OTAKING_FIRE_WAIT:
        if(a->timer > 60 && otaking_can_fire(a)) actor_set_state(a, OTAKING_FIRE, ANIM_OTAKING_FIREATTACK);
        break;
    case OTAKING_COCO:
        if(a->timer == 70){
            v3 at = mario_centre();
            fx h = fx_clamp(v3_dot14(v3_sub(at, a->position), a->up), FX(actor_arg_or(a, 2, 0)),
                            FX(actor_arg_or(a, 0, 450)));
            at = v3_add(v3_sub(at, v3_scale14(a->up, v3_dot14(v3_sub(at, a->position), a->up))),
                        v3_scale14(a->up, h));
            coconut_ball_throw(a, otaking_mouth(a), at, V16(15.0f));
            a->args[2]++;
        }
        if(actor_anim_done(a)) otaking_next(a);
        break;
    case OTAKING_FIRE:
        if(a->timer == 94){
            v3 to_player = v3_tangent14(actor_to_mario(a), a->up, a->front);
            fx fan =
                fx_atan2(v3_dot14(v3_cross14(to_player, a->front), a->up), v3_dot14(to_player, a->front));
            for(int k = 0; k < 3; k++)
                fireball_throw(a, otaking_mouth(a), V16(15.0f), fan + DEG(k == 1 ? -30 : k == 2 ? 30 : 0));
            a->args[2] = 0;
        }
        if(actor_anim_done(a)) otaking_next(a);
        break;
    case OTAKING_BUBBLE:
        if(a->timer == 100){
            for(int k = 0; k < 6; k++){
                fx ang = DEG(60 * k + 30) + DEG((int)((g_game.tick * (k + 3) * 7) % 61) - 30);
                v3 dir = v3_rotate_axis14(a->front, a->up, ang);
                Actor* f = actor_spawn_reuse(
                    OBJ_FIREBALL,
                    v3_add(a->position, v3_add(v3_scale14(a->up, FX(10)), v3_scale14(dir, FX(500)))), a->up,
                    dir);
                if(!f) break;
                f->value = (int16_t)actor_index(a);
                f->velocity = v3_add(v3_scale14(a->up, V16(37.5f)), v3_scale14(dir, V16(15.0f)));
                f->radius = FX(50);
                actor_set_state(f, FIREBALL_ARC, -1);
            }
        }
        if(actor_anim_done(a)) actor_set_state(a, OTAKING_WAIT, ANIM_OTAKING_WAIT);
        break;
    case OTAKING_HITBACK:
        if(actor_anim_done(a)) otaking_next(a);
        break;
    case OTAKING_DAMAGE:
    case OTAKING_POWERUP:
        if(actor_anim_done(a))
            actor_set_state(a, a->health > 0 ? OTAKING_FIRE_WAIT : OTAKING_WAIT, ANIM_OTAKING_WAIT);
        break;
    case OTAKING_DOWN:
        if(actor_anim_done(a)){
            if(!boss_power_star_appear(a, v3_add(a->position, v3_scale14(a->up, FX(100))), FX(5000), true,
                                       BOSS_AFTER_BGM))
                power_stars_reveal_near(a, FX(5000));
            actor_set_switch_dead(a, FX(3000));
            a->flags |= ACTOR_HIDDEN;
            actor_set_state(a, OTAKING_GONE, -1);
            a->active = 0;
        }
        return;
    }
    if(a->state >= OTAKING_DAMAGE) return;
    int c = actor_mario_contact(a, 0);
    if(c != CONTACT_NONE && c != CONTACT_STAR){
        actor_hurt_mario(a, 1);
        if(a->state != OTAKING_HITBACK) actor_set_state(a, OTAKING_HITBACK, ANIM_OTAKING_HITBACK);
    }
}
