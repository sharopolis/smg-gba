static void punchingkinoko_think(Actor* a){
    v3 h = a->target, v = a->velocity;
    v = v3_sub(v, v3_scale14(V3(h.x << 8, h.y << 8, h.z << 8), R14(0.008f)));
    switch(a->state){
    case PUNCHING_KINOKO_WAIT: v = v3_scale14(v, R14(0.94f)); break;
    case PUNCHING_KINOKO_PUNCHED:
        v = v3_scale14(v, R14(0.99f));
        if(a->timer >= 5) actor_set_state(a, PUNCHING_KINOKO_BRAKE, -1);
        break;
    case PUNCHING_KINOKO_BRAKE:
        v = v3_scale14(v, R14(0.9f));
        if(a->timer >= 40) actor_set_state(a, PUNCHING_KINOKO_SWING, -1);
        break;
    case PUNCHING_KINOKO_SWING:
        v = v3_scale14(v, R14(0.97f));
        if(v3_length_exact(v) < V16(40.0f) / 10 && v3_length_exact(h) < FX(50))
            actor_set_state(a, PUNCHING_KINOKO_WAIT, -1);
        break;
    case PUNCHING_KINOKO_CRUSHED:
        a->figure.scale.z = 64 * 6 / 10;
        if(a->timer >= 180){
            actor_set_state(a, PUNCHING_KINOKO_CRUSHED_END, -1);
            a->figure.scale.z = 64;
        }
        break;
    case PUNCHING_KINOKO_CRUSHED_END:
        if(a->timer >= 60) actor_set_state(a, PUNCHING_KINOKO_WAIT, -1);
        break;
    }
    v = v3_reject14(v, a->up);
    h = v3_add(h, V3(v.x >> 8, v.y >> 8, v.z >> 8));
    int32_t len = v3_length_exact(h);
    if(len > FX(600)) h = v3_scale14(v3_direction14(h, a->front), FX(600));
    a->target = h;
    a->velocity = v;

    v3 head_pos = v3_add(v3_add(a->position, v3_scale14(a->up, FX(200))), h);
    Actor head = *a;
    head.position = v3_sub(head_pos, v3_scale14(a->up, actor_kinds[a->type].height));
    head.radius = FX(70);
    int c = actor_mario_contact(&head, FX(60));
    int32_t speed = v3_length_exact(v) >> 8;
    if(c == CONTACT_SPIN && a->state != PUNCHING_KINOKO_CRUSHED){
        v3 away = v3_tangent14(v3_neg(actor_to_mario(&head)), a->up, a->front);
        a->velocity = v3_add(a->velocity, v3_scale14(away, V16(50.0f)));
        actor_set_state(a, PUNCHING_KINOKO_PUNCHED, -1);
        audio_play_effect(SFX_ENEMY_HIT, 3);
    } else if(c == CONTACT_HIPDROP){
        actor_set_state(a, PUNCHING_KINOKO_CRUSHED, -1);
        audio_play_effect(SFX_HIPDROP_LAND, 2);
    } else if(c == CONTACT_TRAMPLE){
        mario_stomp_bounce(g_game.keys.held & KEY_A);
        if(a->state == PUNCHING_KINOKO_CRUSHED_END) mario_stomp_bounce(true);
        a->velocity = v3_add(a->velocity, v3_scale14(game_player()->front, V16(5.0f)));
    } else if(c == CONTACT_TOUCH && a->state == PUNCHING_KINOKO_SWING && speed >= 15){
        mario_damage(head_pos, 0);
        if(speed >= 30){
            int lost = fx_min(10, g_game.star_bits);
            g_game.star_bits -= lost;
            collectibles_pop(OBJ_STARBIT, mario_centre(), game_player()->up, fx_min(lost, 4));
        }
        a->velocity = v3_scale14(v3_neg(a->velocity), R14(0.3f * 1.6f));
        actor_set_state(a, PUNCHING_KINOKO_WAIT, -1);
    }
    actor_block_mario(a, FX(40), FX(160));
}

static void punchingkinoko_figure(const Actor* a, Figure* f){
    v3 lean = v3_add(v3_scale14(a->up, FX(200)), a->target);
    f->basis_up = v3_direction14(lean, a->up);
    f->basis_front = v3_tangent14(a->front, f->basis_up, a->front);
}

static void punchingkinoko_init(Actor* a, const StageObject* o){
    actor_set_state(a, PUNCHING_KINOKO_WAIT, -1);
    a->target = V3(0, 0, 0);
}

static bool punchingkinoko_idle(const Actor* a){
    return a->state == PUNCHING_KINOKO_WAIT && !(a->target.x | a->target.y | a->target.z);
}
