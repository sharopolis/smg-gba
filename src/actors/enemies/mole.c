enum { MOGUCCHI_STROLL = 64, MOGUCCHI_APPEAR, MOGUCCHI_DOWN, MOGUCCHI_DIVE, MOGUCCHI_SCATTER, MOGUCCHI_DIE };
enum { MOLEHILL_APPEAR = 64, MOLEHILL_WAIT, MOLEHILL_CRUMBLE };
#define MOGUCCHI_HILLS_KEPT 4

static int actor_tick_random(const Actor* a, int lo, int hi){
    uint32_t x = (g_game.tick + 1u) * 2654435761u ^ (uint32_t)actor_index(a) * 40503u;
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    return lo + (int)(x % (uint32_t)(hi - lo + 1));
}

static void molehill_drop(Actor* owner, v3 at, int16_t* seq, int16_t* ramp, int keep){
    static const uint8_t* const mole[3] = {figure_mogucchihilla, figure_mogucchihillb, figure_mogucchihillc};
    static const uint8_t* const burrows[3] = {figure_dodoryuhilla, figure_dodoryuhillb, figure_dodoryuhillc};
    if(*ramp < 3) (*ramp)++;
    Actor* h = actor_spawn_reuse(OBJ_GUSTYHILL, at, owner->up, owner->front);
    (*seq)++;
    if(!h) return;
    int pick = (*seq * 7 + actor_index(owner)) % 3;
    figure_init(&h->figure, owner->type == OBJ_MOGUCCHI ? mole[pick] : burrows[pick]);
    h->figure.use_basis = true;
    h->value = (int16_t)actor_index(owner);
    h->args[0] = (int16_t)owner->type;
    h->args[1] = *seq;
    h->args[2] = (int16_t)(64 * 6 / 10 + (64 - 64 * 6 / 10) * (*ramp - 1) / 2);
    h->args[3] = (int16_t)keep;
    h->radius = FX(120) * h->args[2] / 64;
    h->figure.scale = V3(0, 0, 0);
    actor_set_state(h, MOLEHILL_APPEAR, -1);
}

static void molehill_trail(Actor* owner, v3* reserve, int16_t* seq, int16_t* ramp, fx every, int keep){
    if(v3_length_squared64(v3_sub(owner->position, *reserve)) < (uint64_t)every * every) return;
    molehill_drop(owner, *reserve, seq, ramp, keep);
    *reserve = owner->position;
}

static void molehill_crumble(Actor* h){
    if(h->state != MOLEHILL_CRUMBLE) actor_set_state(h, MOLEHILL_CRUMBLE, -1);
}

static void molehill_think(Actor* h){
    Actor* owner = h->value >= 0 && h->value < g_game.actor_count ? &g_game.actors[h->value] : NULL;
    bool alive = owner && owner->active && owner->type == h->args[0];
    int16_t owner_seq = !alive ? 0x7fff : owner->type == OBJ_MOGUCCHI ? owner->args[0] : owner->args[2];
    if(!alive || owner_seq - h->args[1] >= h->args[3]) molehill_crumble(h);
    int s = h->args[2];
    if(h->state == MOLEHILL_APPEAR){
        s = s * fx_min(h->timer, 30) / 30;
        if(h->timer >= 30) actor_set_state(h, MOLEHILL_WAIT, -1);
    } else if(h->state == MOLEHILL_CRUMBLE){
        s = s * fx_max(0, 120 - h->timer) / 120;
        if(h->timer >= 120){
            h->active = 0;
            return;
        }
    }
    h->figure.scale = V3(s, s, s);
    if(h->state == MOLEHILL_CRUMBLE && h->timer >= 40) return;
    Player* p = &g_game.player;
    if(p->action == MA_HIPDROP_LAND && p->action_timer == 0 &&
       v3_length_squared64(v3_sub(h->position, mario_centre())) < (uint64_t)FX(300) * FX(300)){
        h->active = 0;
        return;
    }
    if(g_mario.attack_timer > 0 && actor_touches_mario(h, FX(60))){
        audio_play_effect(SFX_HIPDROP_LAND, 1);
        h->active = 0;
        return;
    }
    actor_block_mario(h, h->radius, FX(100) * s / 64);
}

static void mogucchi_place(Actor* a){
    int rail = a->args[1];
    fx coord = a->home.y >> 8;
    v3 on = rail_position_at(rail, coord);

    RayHit hit;
    if(((g_game.tick + (uint32_t)actor_index(a)) & 1) == 0 || a->state != MOGUCCHI_STROLL){
        GravitySample g = gravity_sample(g_game.stage.header, on);
        if(g.index >= 0) a->up = g.up;
        v3 dir = v3_sub(rail_position_at(rail, coord + FX(20)), on);
        a->front = v3_tangent14(dir, a->up, a->front);
        a->position = collision_ray(on, v3_sub(on, v3_scale14(a->up, FX(1000))), &hit, 0) ? hit.point : on;
        a->velocity = v3_sub(a->position, on);
    } else a->position = v3_add(on, a->velocity);
}

static void mogucchi_init(Actor* a, const StageObject* o){
    int rail = o->rail;
    a->args[1] = (int16_t)rail;
    a->args[0] = a->args[2] = 0;
    fx top = o->args[0] > 0 ? V16(o->args[0]) : V16(5.0f);
    a->home = V3(top, rail_closest_coord(rail, a->position) << 8, top);
    a->radius = FX(120);
    if(rail >= 0) mogucchi_place(a);
    a->target = a->position;
    figure_init(&a->figure, figure_mogucchihole);
    a->figure.use_basis = true;
    a->figure.scale = V3(64, 64, 64);
    actor_set_state(a, MOGUCCHI_STROLL, ANIM_MOGUCCHIHOLE_WALK);
}

static void mogucchi_show(Actor* a, bool mole, int anim){
    a->figure.model = (const FigureHeader*)(mole ? figure_mogucchi : figure_mogucchihole);
    a->anim = anim;
    a->anim_frame = 0;
}

static void mogucchi_think(Actor* a){
    int rail = a->args[1];
    if(rail < 0) return;
    Player* p = &g_game.player;
    int contact = a->state < MOGUCCHI_SCATTER ? actor_mario_contact(a, FX(60)) : CONTACT_NONE;
    switch(a->state){
    case MOGUCCHI_STROLL: {
        if(a->home.x < a->home.z) a->home.x = fx_min(a->home.x + V16(0.1f), a->home.z);
        fx len = rail_length(rail);
        a->home.y += a->home.x;
        if(len > 0) a->home.y %= len << 8;
        mogucchi_place(a);
        molehill_trail(a, &a->target, &a->args[0], &a->args[2], FX(100), MOGUCCHI_HILLS_KEPT);
        if(p->action == MA_HIPDROP_LAND && p->action_timer == 0 && actor_mario_within(a, FX(800))){
            actor_set_state(a, MOGUCCHI_APPEAR, -1);
            mogucchi_show(a, true, ANIM_MOGUCCHI_SWOONSTART);
            audio_play_effect(SFX_MOLEAPPEAR, 2);
            break;
        }
        if(contact == CONTACT_SPIN) break;
        if(contact != CONTACT_NONE && !g_mario.jumping) actor_hurt_mario(a, 1);
        break;
    }
    case MOGUCCHI_APPEAR:
        if(actor_anim_done(a)){
            actor_set_state(a, MOGUCCHI_DOWN, -1);
            mogucchi_show(a, true, ANIM_MOGUCCHI_SWOON);
        }
        break;
    case MOGUCCHI_DOWN:
        if(a->timer >= 30){
            actor_set_state(a, MOGUCCHI_DIVE, -1);
            mogucchi_show(a, true, ANIM_MOGUCCHI_HIDE);
            audio_play_effect(SFX_MOLEHIDE, 2);
        }
        break;
    case MOGUCCHI_DIVE:
        if(actor_anim_done(a)){
            actor_set_state(a, MOGUCCHI_STROLL, -1);
            mogucchi_show(a, false, ANIM_MOGUCCHIHOLE_WALK);
            a->home.x = 0;
            a->target = a->position;
            a->args[2] = 0;
        }
        break;
    case MOGUCCHI_SCATTER:
        a->position =
            v3_add(a->position, v3_add(v3_scale14(a->up, a->velocity.x >> 8), v3_scale14(a->target, 23)));
        a->velocity.x -= V16(1.2f);
        if(a->timer >= 15){
            actor_set_state(a, MOGUCCHI_DIE, -1);
            collectibles_pop(OBJ_COIN, a->position, a->up, 1);
            audio_play_effect(SFX_HIPDROP_LAND, 3);
            a->flags |= ACTOR_HIDDEN;
        }
        return;
    case MOGUCCHI_DIE:
        if(a->timer >= 60) a->active = 0;
        return;
    }
    bool out = a->state == MOGUCCHI_DOWN || a->state == MOGUCCHI_DIVE ||
               (a->state == MOGUCCHI_APPEAR && a->timer >= 10);
    if(out && contact == CONTACT_SPIN){
        v3 away = v3_reject14(v3_sub(a->position, p->figure.position), a->up);
        a->target = v3_direction14(away, a->front);
        a->velocity = V3(V16(50.0f), 0, 0);
        actor_set_state(a, MOGUCCHI_SCATTER, -1);
        mogucchi_show(a, true, ANIM_MOGUCCHI_PUNCHDOWN);
        audio_play_effect(SFX_HIPDROP_LAND, 3);
        mario_spin_hop();
    } else if(out && (contact == CONTACT_TRAMPLE || contact == CONTACT_HIPDROP)){
        a->anim = ANIM_MOGUCCHI_BOUNCE;
        a->anim_frame = 0;
        mario_stomp_bounce(false);
    }
}
