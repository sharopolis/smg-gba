enum {
    TERESA_WAIT = 64,
    TERESA_CHASE,
    TERESA_SHAY,
    TERESA_AGGRESSIVE,
    TERESA_ATTACK,
    TERESA_DRIFT,
    TERESA_ASCENSION
};

static void teresa_init(Actor* a, const StageObject* o){
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    a->radius = 85 * o->scale[0] / 256;
    a->velocity = V3(0, 0, 0);
    actor_set_state(a, TERESA_WAIT, ANIM_TERESACHIEF_WAIT);
}

static bool glaring_light_at(v3 p){
    for(unsigned i = 0; i < sizeof deep_glaring_lights / sizeof *deep_glaring_lights; i++){
        const DeepGlaringLight* l = &deep_glaring_lights[i];
        if(!switch_on(l->sw)) continue;
        v3 d = v3_sub(p, V3(l->base[0], l->base[1], l->base[2]));
        v3 up = V3(l->up[0], l->up[1], l->up[2]);
        int32_t h = v3_dot14(d, up);
        if(h < 0 || h > l->height) continue;
        if(v3_length_squared64(v3_reject14(d, up)) < (uint64_t)l->radius * l->radius) return true;
    }
    return false;
}

static bool teresa_should_shy(const Actor* a){
    const Player* p = &g_game.player;
    if(p->action == MA_SQUAT || p->action == MA_SQUAT_SLIDE) return false;
    v3 d = v3_sub(a->position, p->figure.position);
    return v3_dot14(d, p->front) > 0 && actor_mario_within(a, 1500);
}
static void teresa_kill(Actor* a){
    audio_play_effect(SFX_TERESA_DEAD, 4);
    missions_reveal_star_at(DEEP_TERESA_STAR_OBJECT, a->position);
    if(actor_has_switch(a, SWITCH_DEAD)) switch_set(actor_switch_key(a, SWITCH_DEAD), true);
    a->active = 0;
}

static void teresa_move(Actor* a){
    int keep = 150 + 85 * (a->figure.scale.y - 64) / 64;
    RayHit hit;
    v3 down = v3_sub(a->position, v3_scale14(a->up, 2000));
    if(collision_ray(a->position, down, &hit, 0)){
        int32_t h = v3_dot14(v3_sub(a->position, hit.point), a->up);
        int32_t push = fx_clamp((keep - h) * 256 * 2 / 5, -40 * 256, 40 * 256);
        int32_t vu = v3_dot14(a->velocity, a->up);
        if(push > 0 ? vu < push : vu > push)
            a->velocity = v3_add(a->velocity, v3_scale14(a->up, (push - vu) / 4));
    }
    v3 next = v3_add(a->position,
                     V3((a->velocity.x + 128) >> 8, (a->velocity.y + 128) >> 8, (a->velocity.z + 128) >> 8));
    CollisionCandidates cand;
    collision_gather_around(a->position, next, a->radius + 2, &cand);
    for(int i = 0; i < cand.count; i++){
        SphereContact c;
        if(!collision_sphere_triangle(cand.index[i], next, a->radius, &c)) continue;
        next = v3_add(next, v3_scale14(c.normal, c.depth + 1));
        int32_t vn = v3_dot14(a->velocity, c.normal);
        if(vn < 0) a->velocity = v3_sub(a->velocity, v3_scale14(c.normal, mul14(vn, R14(1.2f))));
    }
    a->position = next;
    a->velocity = v3_scale14(a->velocity, R14(.9f));
}
static void teresa_think(Actor* a){
    int step = a->timer - 1;
    a->up = gravity_sample(g_game.stage.header, a->position).up;
    if(a->state == TERESA_ASCENSION){
        teresa_kill(a);
        return;
    }

    if(glaring_light_at(a->position)){
        actor_set_state(a, TERESA_ASCENSION, -1);
        return;
    }
    Player* p = &g_game.player;
    v3 to_mario = actor_to_mario(a);
    int contact = actor_mario_contact(a, FX(120));
    if(contact == CONTACT_SPIN && a->state != TERESA_DRIFT){
        actor_set_state(a, TERESA_DRIFT, ANIM_TERESACHIEF_DRIFT);
    } else if(contact == CONTACT_STAR){
        actor_set_state(a, TERESA_ASCENSION, -1);
        return;
    } else if(contact != CONTACT_NONE && a->state != TERESA_ATTACK && a->state != TERESA_DRIFT &&
              p->action != MA_DEATH){
        actor_hurt_mario(a, 1);
        audio_play_effect(SFX_TERESA_LAUGH, 3);
        actor_set_state(a, TERESA_ATTACK, ANIM_TERESACHIEF_LAUGH);
    }
    switch(a->state){
    case TERESA_WAIT:
        if(teresa_should_shy(a)){
            actor_set_state(a, TERESA_SHAY, ANIM_TERESACHIEF_SHAY);
            audio_play_effect(SFX_TERESA_STOP, 2);
            a->velocity = V3(0, 0, 0);
        } else if(actor_mario_within(a, 1800)){
            actor_set_state(a, TERESA_CHASE, ANIM_TERESACHIEF_CHASE);
            audio_play_effect(SFX_TERESA_LAUGH, 2);
        }
        break;
    case TERESA_CHASE: {
        actor_turn_toward(a, to_mario, DEG(2.5f));
        int rate = fx_min(step, 20) * N14_ONE / 20;
        a->velocity = v3_add(a->velocity, v3_scale14(a->front, mul14(V16(.75f), rate)));
        if(teresa_should_shy(a)){
            actor_set_state(a, TERESA_SHAY, ANIM_TERESACHIEF_SHAY);
            audio_play_effect(SFX_TERESA_STOP, 2);
            a->velocity = V3(0, 0, 0);
        } else if(step >= 240 || !actor_mario_within(a, 3000))
            actor_set_state(a, TERESA_WAIT, ANIM_TERESACHIEF_WAIT);
        break;
    }
    case TERESA_SHAY:
        if(!teresa_should_shy(a)) actor_set_state(a, TERESA_AGGRESSIVE, ANIM_TERESACHIEF_AGGRESSIVE);
        break;
    case TERESA_AGGRESSIVE:
        if(teresa_should_shy(a)){
            actor_set_state(a, TERESA_SHAY, ANIM_TERESACHIEF_SHAY);
            audio_play_effect(SFX_TERESA_STOP, 2);
        } else if(step >= 75) actor_set_state(a, TERESA_WAIT, ANIM_TERESACHIEF_WAIT);
        break;
    case TERESA_ATTACK:
        actor_turn_toward(a, to_mario, DEG(8));
        if(actor_anim_done(a)) actor_set_state(a, TERESA_WAIT, ANIM_TERESACHIEF_WAIT);
        break;
    case TERESA_DRIFT:
        actor_turn_toward(a, to_mario, DEG(8));
        a->velocity = v3_sub(a->velocity,
                             v3_scale14(v3_direction14(v3_reject14(to_mario, a->up), a->front), V16(1.25f)));
        if(step >= 50) actor_set_state(a, TERESA_WAIT, ANIM_TERESACHIEF_WAIT);
        break;
    }
    teresa_move(a);
}
