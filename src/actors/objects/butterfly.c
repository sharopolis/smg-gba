static void butterfly_think(Actor* a){
    MarioPhysics* m = &g_mario;
    if(a->state == BF_FLEE){
        a->velocity = v3_scale14(a->velocity, R14(0.97f));
        if(a->timer > 30) actor_set_state(a, BF_FLY, -1);
    } else {
        v3 home = v3_add(a->home, v3_scale14(a->up, FX(80) + ((fx_sin(a->timer * FXC(0.05f)) * 40) >> 8)));
        v3 to = v3_sub(home, a->position);
        a->velocity = v3_add(a->velocity, V3(to.x * 256 / 100, to.y * 256 / 100, to.z * 256 / 100));
        int32_t sp = v3_length_exact(a->velocity);
        if(sp > V16(2.0f)) a->velocity = v3_scale14(v3_direction14(a->velocity, a->front), V16(2.0f));
        if(actor_mario_within(a, FX(300)) && (m->attack_timer > 0 || m->ratio > R14(0.8f))){
            a->velocity = v3_scale14(v3_direction14(v3_neg(actor_to_mario(a)), a->front), V16(12.0f));
            actor_set_state(a, BF_FLEE, -1);
        }
    }
    a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
    if(v3_length_squared64(a->velocity) > 64) a->front = v3_tangent14(a->velocity, a->up, a->front);
    if(!a->health && actor_arg_or(a, 1, -1) == -1 && actor_mario_contact(a, FX(60)) != CONTACT_NONE){
        a->health = 1;
        collectibles_pop(OBJ_STARBIT, a->position, a->up, 1);
    }
}

static void butterfly_init(Actor* a, const StageObject* o){
    actor_set_state(a, BF_FLY, ANIM_BUTTERFLY_FLY);
    a->health = 0;
}

static bool butterfly_idle(const Actor* a){ return a->state == BF_FLY; }
