static void killergunner_think(Actor* a){
    switch(a->state){
    case KILLERGUNNER_WAIT:
        if(a->timer > 120) actor_set_state(a, KILLERGUNNER_CHARGE, -1);
        break;
    case KILLERGUNNER_CHARGE:
        if(a->timer > 30) actor_set_state(a, KILLERGUNNER_SHOOT, ANIM_KILLERGUNNERSINGLE_SHOOT);
        break;
    case KILLERGUNNER_SHOOT:
        if(a->timer <= 40){
            Actor shell = *a;
            shell.position = v3_add(a->position, v3_scale14(a->front, FX(700)));
            shell.radius = FX(250);
            if(actor_touches_mario(&shell, 0)) actor_hurt_mario(a, 1);
        }
        if(actor_anim_done(a)) actor_set_state(a, KILLERGUNNER_WAIT, -1);
        break;
    }
    actor_block_mario(a, FX(200), FX(360));
}

static void homingkiller_think(Actor* a){
    Actor* k = a->value > 0 ? &g_game.actors[a->value] : NULL;
    bool out = k && k->active && k->type == OBJ_KILLER && k->state != AS_DEFEATED;
    if(out){
        a->timer = 0;
        return;
    }
    if(a->state == HOMINGKILLER_WAIT && a->timer < 180 && a->health) return;
    a->health = 1;
    Actor* b = actor_spawn_reuse(OBJ_KILLER, a->position, a->up, a->front);
    if(!b) return;
    b->home = a->position;
    b->target = a->front;
    b->value = 0;

    for(int k = 0; k < 4; k++) b->args[k] = a->args[k];
    actor_set_state(b, KILLER_APPEAR, ANIM_HOMINGKILLER_START);
    a->value = (int16_t)actor_index(b);
    actor_set_state(a, HOMINGKILLER_WAIT, -1);
}

static bool killer_near_zero(v3 v){ return fx_abs(v.x) <= 16 && fx_abs(v.y) <= 16 && fx_abs(v.z) <= 16; }

__attribute__((noinline)) static void killer_turn(Actor* a, v3 to, v3 gravity, int radius){
    v3 from = a->front;
    if(killer_near_zero(from) || killer_near_zero(to)) return;
    int cosine, sine;
    if(radius == 3){
        cosine = R14(0.9997f);
        sine = R14(0.02449306024f);
    } else if(radius == 5){
        cosine = R14(0.9995f);
        sine = R14(0.03161882351f);
    } else {
        if(radius < 0 || radius > 20000) return;
        cosine = (10000 - radius) * N14_ONE / 10000;
        uint64_t square = (uint64_t)radius * (20000 - radius) * N14_ONE * N14_ONE / 100000000;
        sine = (int)isqrt32((uint32_t)square);
    }
    if(v3_dot14(from, to) > cosine){
        a->front = v3_direction14(to, from);
        return;
    }
    v3 reject = v3_reject14(to, from);
    if(killer_near_zero(reject)){
        v3 fallback = v3_direction14(v3_cross14(from, gravity), V3(0, 0, 0));
        a->front = v3_direction14(v3_add(from, v3_scale14(fallback, R14(0.02f))), from);
    } else {
        reject = v3_direction14(reject, from);
        a->front = v3_direction14(v3_add(v3_scale14(from, cosine), v3_scale14(reject, sine)), from);
    }
}

static void killer_think(Actor* a){
    Player* p = game_player();
    switch(a->state){
    case KILLER_APPEAR: {
        int32_t t = fx_min(a->timer * N14_ONE / 20, N14_ONE);
        a->position = v3_add(a->home, v3_scale14(a->target, mul14(FX(200), t)));
        if(a->timer >= 72) actor_set_state(a, KILLER_MOVE, ANIM_HOMINGKILLER_MOVE);
        return;
    }
    case KILLER_MOVE:
    case KILLER_CHASE: {
        v3 aim = v3_sub(v3_add(p->figure.position, v3_scale14(p->up, FX(200))), a->position);
        bool front = v3_dot14(aim, a->front) > 0;
        if(a->state == KILLER_MOVE && a->timer >= 35 && front &&
           actor_mario_within(a, FX(actor_arg_or(a, 0, 2000)))){
            actor_set_state(a, KILLER_CHASE, ANIM_HOMINGKILLER_CHASE);
        } else if(a->state == KILLER_CHASE && !actor_mario_within(a, FX(3000))){
            actor_set_state(a, KILLER_MOVE, ANIM_HOMINGKILLER_MOVE);
        }
        v3 g = gravity_sample(g_game.stage.header, a->position).up;
        if(a->state == KILLER_CHASE){
            v3 want = v3_direction14(aim, a->front);
            killer_turn(a, want, v3_scale14(g, -N14_ONE), actor_arg_or(a, 1, 3));
        } else {
            v3 cross = v3_cross14(a->front, g), level = a->front;
            if(fx_abs(cross.x) > 163 || fx_abs(cross.y) > 163 || fx_abs(cross.z) > 163)
                level = v3_direction14(v3_reject14(a->front, g), a->front);
            killer_turn(a, level, v3_scale14(g, -N14_ONE), 5);
        }
        a->up = v3_tangent14(g, a->front, a->up);
        v3 step = v3_scale14(a->front, FX(12));

        bool blast = (a->state == KILLER_MOVE && a->timer > 600) ||
                     (a->value >= 35 && killer_break_cages(a, v3_add(a->position, step)));
        if(a->value < 35) ++a->value;
        a->position = v3_add(a->position, step);
        if(actor_mario_contact(a, 0) != CONTACT_NONE){
            actor_hurt_mario(a, 1);
            blast = true;
        }
        if(blast){
            audio_play_effect(SFX_ENEMY_DEFEAT, 3);
            actor_defeat(a, false);
        }
        return;
    }
    }
}

static void killergunner_init(Actor* a, const StageObject* o){
    actor_set_state(a, KILLERGUNNER_WAIT, ANIM_KILLERGUNNERSINGLE_SHOOT);
    a->anim_frame = FX(1000);
}

static void homingkiller_init(Actor* a, const StageObject* o){ actor_set_state(a, HOMINGKILLER_WAIT, -1); }
