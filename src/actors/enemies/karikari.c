static void karikari_blow_out(Actor* a){
    v3 away = v3_tangent14(v3_neg(actor_to_mario(a)), a->up, v3_neg(a->front));
    a->velocity = v3_add(v3_scale14(away, V16(10.0f)), v3_scale14(a->up, V16(38.0f)));
    a->flags &= ~ACTOR_ON_GROUND;
    if(a->state == KARIKARI_CLING && g_mario_cling > 0) g_mario_cling--;
    actor_set_state(a, KARIKARI_BLOWOUT, ANIM_KARIKARI_SPINX);
    audio_play_effect(SFX_ENEMY_HIT, 2);
}

static void karikari_hop(Actor* a, int32_t up, int32_t toward){
    v3 dir = v3_tangent14(actor_to_mario(a), a->up, a->front);
    a->velocity = v3_add(v3_scale14(a->up, up), v3_scale14(dir, toward));
    a->flags &= ~ACTOR_ON_GROUND;
}

static void karikari_think(Actor* a){
    Player* p = game_player();
    bool near = actor_mario_within(a, FX(1000));
    int32_t gravity = V16(2.4f);
    if(a->state != KARIKARI_CLING && a->state != KARIKARI_BLOWOUT && a->state >= KARIKARI_WAIT &&
       a->state != KARIKARI_FALL)
        a->front =
            v3_tangent14(v3_blend14(a->front, v3_tangent14(actor_to_mario(a), a->up, a->front), R14(0.25f)),
                         a->up, a->front);
    switch(a->state){
    case KARIKARI_FALL:
        if(a->flags & ACTOR_ON_GROUND) actor_set_state(a, KARIKARI_LAND, ANIM_KARIKARI_LAND);
        break;
    case KARIKARI_LAND:
        if(a->timer > 90)
            actor_set_state(a, near ? KARIKARI_PURSUE : KARIKARI_WAIT,
                            near ? ANIM_KARIKARI_JUMP : ANIM_KARIKARI_WAIT);
        break;
    case KARIKARI_WAIT:
        if(near) actor_set_state(a, KARIKARI_PREPURSUE, ANIM_KARIKARI_SEARCH);
        else if(a->flags & ACTOR_ON_GROUND) return;
        break;
    case KARIKARI_PREPURSUE:
        if(a->timer == 11) karikari_hop(a, V16(50.0f), 0);
        else if(a->timer > 13 && (a->flags & ACTOR_ON_GROUND))
            actor_set_state(a, KARIKARI_PURSUE, ANIM_KARIKARI_JUMP);
        break;
    case KARIKARI_PURSUE:
        if(a->timer == 11) karikari_hop(a, V16(33.0f), V16(10.0f));
        else if(a->timer > 13 && (a->flags & ACTOR_ON_GROUND)){
            if(g_mario_cling >= 5) actor_set_state(a, KARIKARI_WATCH, ANIM_KARIKARI_GLAD);
            else if(!near) actor_set_state(a, KARIKARI_WAIT, ANIM_KARIKARI_WAIT);
            else actor_set_state(a, KARIKARI_PURSUE, ANIM_KARIKARI_JUMP);
        }
        break;
    case KARIKARI_WATCH:
        gravity = V16(1.4f);
        if(a->timer % 45 == 7) karikari_hop(a, V16(35.0f), 0);
        if(g_mario_cling < 5 || !actor_mario_within(a, FX(500)))
            actor_set_state(a, KARIKARI_PREPURSUE, ANIM_KARIKARI_SEARCH);
        break;
    case KARIKARI_CLING: {
        static const int16_t slot[5][3] = {
            {0, 130, 30}, {-45, 90, 0}, {0, 50, -35}, {0, 95, -30}, {45, 105, 0}};
        const int16_t* s = slot[a->value % 5];
        v3 side = v3_cross14(p->up, p->front);
        a->position =
            v3_add(v3_add(v3_add(p->figure.position, v3_scale14(side, s[0])), v3_scale14(p->up, s[1] - 30)),
                   v3_scale14(p->front, s[2]));
        a->up = p->up;
        a->front = v3_neg(p->front);
        if(g_mario.attack_timer > 0 || g_mario_star_power > 0 || p->action == MA_DEATH) karikari_blow_out(a);
        return;
    }
    case KARIKARI_BLOWOUT: {
        gravity = V16(1.3f);
        a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
        a->velocity = v3_sub(a->velocity, v3_scale14(a->up, gravity));
        RayHit hit;
        if(a->timer >= 60 ||
           (a->timer > 5 &&
            collision_ray(a->position,
                          v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8)),
                          &hit, 0))){
            collectibles_pop(OBJ_STARBIT, a->position, a->up, 1);
            actor_defeat(a, false);
        }
        return;
    }
    }
    actor_follow_gravity(a, R14(0.2f));
    actor_move(a, gravity, R14(0.8f), R14(0.98f));
    switch(actor_mario_contact(a, FX(60))){
    case CONTACT_STAR:
    case CONTACT_SPIN: karikari_blow_out(a); break;
    case CONTACT_TOUCH:
    case CONTACT_TRAMPLE:
        if(g_mario_cling < 5 &&
           v3_length_exact(v3_sub(mario_centre(), actor_centre(a))) < FX(80) + C_BODY_RADIUS){
            a->value = (int16_t)g_mario_cling++;
            actor_set_state(a, KARIKARI_CLING, ANIM_KARIKARI_BITE);
        }
        break;
    }
}

static void karikari_init(Actor* a, const StageObject* o){
    actor_set_state(a, KARIKARI_FALL, ANIM_KARIKARI_WAIT);
}

static void karikari_blown_away(Actor* a, Actor* by){
    if(a->state != KARIKARI_BLOWOUT) karikari_blow_out(a);
}
