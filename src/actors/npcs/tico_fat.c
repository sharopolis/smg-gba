static void ticofat_receive_piece(Actor* a){
    if(a->state > TICOFAT_EAT) return;
    if(a->health > 0) a->health--;
    int wanted = actor_arg_or(a, 1, 10);
    if(a->value >= wanted) return;
    a->value++;
    fx scale = 64 + 64 * 9 * a->value / (10 * wanted);
    a->figure.scale = V3(scale, scale, scale);
    audio_play_effect(SFX_STARBIT, 1);
    if(a->value == wanted) actor_set_state(a, TICOFAT_FULL, ANIM_TICOFAT_FATJOY);
    else if(a->state != TICOFAT_EAT) actor_set_state(a, TICOFAT_EAT, ANIM_TICOFAT_EAT);
    else a->anim_frame = 0;
}

static void ticofat_think(Actor* a){
    int wanted = actor_arg_or(a, 1, 10);
    switch(a->state){
    case TICOFAT_WAIT:
    case TICOFAT_EAT: {
        talk_actor_think(a);
        if(a->args[3] && !talk_active() &&
           actor_mario_within(a, FX(280) * a->figure.scale.x / 64 + C_BODY_HEIGHT) &&
           pointer_is_pointing(a, FX(110) * a->figure.scale.x / 64) && pointer_shoot_held() &&
           g_game.star_bits > 0 && a->value + a->health < wanted && a->timer % 5 == 0){
            if(pointer_gift_starbit(a)) a->health++;
        }
        if(a->state == TICOFAT_EAT && actor_anim_done(a)) actor_set_state(a, TICOFAT_WAIT, ANIM_TICOFAT_WAIT);
        actor_turn_toward(a, actor_to_mario(a), DEG(4));
        break;
    }
    case TICOFAT_FULL: talk_force_actor(a); break;
    case TICOFAT_TRANSFORM:
        if(actor_anim_done(a)){
            collectibles_pop(OBJ_ONEUP, a->position, a->up, 1);
            a->active = 0;
        }
        break;
    case TICOFAT_FLY: {
        int rail = actor_rail(a);
        v3 end = rail >= 0                                   ? rail_position_at(rail, rail_length(rail))
                 : (a->target.x | a->target.y | a->target.z) ? a->target
                                                             : v3_add(a->home, v3_scale14(a->up, FX(3000)));
        int32_t t = fx_min(a->timer * N14_ONE / 120, N14_ONE);
        a->position = v3_add(v3_add(a->home, v3_scale14(v3_sub(end, a->home), t)),
                             v3_scale14(a->up, mul14(FX(800), mul14(4 * t, N14_ONE - t))));
        if(t >= N14_ONE){
            a->position = end;
            actor_set_switch_a(a, FX(3000), -1);
            a->active = 0;
        }
        break;
    }
    }
}

static void ticofat_pointer_camera(void){
    if(!pointer_active() || talk_active()) return;
    for(int i = 0; i < g_game.actor_count; i++){
        const Actor* a = &g_game.actors[i];
        if(!a->active || a->type != OBJ_TICOFAT || !a->args[3] || a->state > TICOFAT_EAT ||
           !actor_mario_within(a, FX(280) * a->figure.scale.x / 64 + C_BODY_HEIGHT))
            continue;
        Camera* c = &g_game.camera;
        fx distance = FX(1300) + FX(380) * a->value / actor_arg_or(a, 1, 10);
        c->position = v3_add(a->position, v3_add(v3_scale14(a->up, FX(100)), v3_scale14(a->front, distance)));
        c->target = a->position;
        c->front = v3_direction14(v3_sub(c->target, c->position), v3_neg(a->front));
        c->up = v3_tangent14(a->up, c->front, a->up);
        camera_update_angles(c);
        return;
    }
}

static fx ticofat_pointer_radius(const Actor* a){ return FX(110) * a->figure.scale.x / 64; }

static bool ticofat_pointer_blocks_shot(const Actor* a){
    return a->args[3] && a->state <= TICOFAT_EAT &&
           actor_mario_within(a, FX(280) * a->figure.scale.x / 64 + C_BODY_HEIGHT);
}

static void ticofat_init(Actor* a, const StageObject* o){
    actor_set_state(a, TICOFAT_WAIT, ANIM_TICOFAT_WAIT);
    a->value = 0;
    a->health = 0;
    a->args[3] = OBJECT_ARG(o, 2) != -1;
}

static bool ticofat_idle(const Actor* a){ return a->state == TICOFAT_WAIT; }

static void ticofat_talk_event(Actor* a, int arg){
    if(arg == 1 && a->state == TICOFAT_FULL){
        if(actor_arg_or(a, 0, -1) == 0) actor_set_state(a, TICOFAT_TRANSFORM, ANIM_TICOFAT_METAMORPHOSIS);
        else actor_set_state(a, TICOFAT_FLY, ANIM_TICOFAT_SPIN);
    }
    if(arg == 0) a->args[3] = 1;
}

static int ticofat_talk_node(const Actor* a, int node){
    if(a->state == TICOFAT_FULL) return TALK_TICOFAT_FULL;
    if(a->args[3]) return TALK_TICOFAT_REPEAT;
    return node;
}

static int ticofat_talk_count(const Actor* a){ return fx_max(actor_arg_or(a, 1, 10) - a->value, 0); }
