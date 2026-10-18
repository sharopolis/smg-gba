enum { DOSSUN_UPPER = 96, DOSSUN_SIGN, DOSSUN_FALL, DOSSUN_GROUND, DOSSUN_RISE };
#define DOSSUN_FALLSTART_FRAMES 24
static fx dossun_dist(const Actor* a){ return a->args[0] > 0 ? FX(a->args[0]) : FX(1000); }
static int dossun_wait(const Actor* a){
    int w = a->args[1] > 0 ? a->args[1] : 180;
    return w > DOSSUN_FALLSTART_FRAMES ? w - DOSSUN_FALLSTART_FRAMES : 0;
}
static void dossun_init(Actor* a){
    a->home = a->position;
    a->position = v3_add(a->home, v3_scale14(a->up, dossun_dist(a)));
    actor_set_state(a, DOSSUN_UPPER, -1);
}

static fx dossun_ease(int step, int total, bool inout){
    if(total <= 0) return N14_ONE;
    fx angle = (fx)((int64_t)fx_clamp(step, 0, total) * (inout ? FX_PI : FX_PI / 2) / total);
    fx c = fx_sin_cos(angle).y << 6;
    return inout ? (N14_ONE - c) / 2 : N14_ONE - c;
}
static bool dossun_mario_below(const Actor* a){
    Player* p = game_player();
    v3 d = v3_sub(p->figure.position, a->home);
    v3 side = v3_cross14(a->up, a->front);
    fx sx = a->figure.scale.x * 256, sz = a->figure.scale.z * 256;
    fx h = v3_dot14(d, a->up);

    return fx_abs(v3_dot14(d, side)) < mul14(FX(419), sx) &&
           fx_abs(v3_dot14(d, a->front)) < mul14(FX(297), sz) && h >= -FX(20) &&
           h < v3_dot14(v3_sub(a->position, a->home), a->up);
}
static void dossun_think(Actor* a){
    fx dist = dossun_dist(a);
    int fall = (int)(dist / 30), rise = (int)(dist / 10);
    v3 top = v3_add(a->home, v3_scale14(a->up, dist));
    switch(a->state){
    case DOSSUN_UPPER:
        a->position = top;
        if(a->timer >= dossun_wait(a)) actor_set_state(a, DOSSUN_SIGN, -1);
        break;
    case DOSSUN_SIGN:
        if(a->timer >= DOSSUN_FALLSTART_FRAMES) actor_set_state(a, DOSSUN_FALL, -1);
        break;
    case DOSSUN_FALL:
        a->position =
            v3_add(a->home, v3_scale14(a->up, mul14(dist, N14_ONE - dossun_ease(a->timer, fall, false))));
        if(dossun_mario_below(a)) actor_hurt_mario(a, 1);
        if(a->timer >= fall){
            a->position = a->home;
            actor_set_state(a, DOSSUN_GROUND, -1);
            camera_shake(8);
        }
        break;
    case DOSSUN_GROUND:
        a->position = a->home;
        if(a->timer >= 120) actor_set_state(a, DOSSUN_RISE, -1);
        break;
    case DOSSUN_RISE:
        a->position = v3_add(a->home, v3_scale14(a->up, mul14(dist, dossun_ease(a->timer, rise, true))));
        if(a->timer >= rise) actor_set_state(a, DOSSUN_UPPER, -1);
        break;
    default: actor_set_state(a, DOSSUN_UPPER, -1); break;
    }
}
static void bigbirikyu_init(Actor* a, const StageObject* o){
    a->value = 0;
    if(o->rail < 0) return;
    fx len = rail_length(o->rail), best = 0;
    uint64_t bd = UINT64_MAX;
    for(fx d = 0; d < len; d += FX(20)){
        uint64_t q = v3_length_squared64(v3_sub(rail_position_at(o->rail, d), a->position));
        if(q < bd){
            bd = q;
            best = d;
        }
    }
    a->value = (int16_t)best;
    a->position = rail_position_at(o->rail, best);
}
static void bigbirikyu_think(Actor* a){
    const StageObject* o = actor_stage_object(a);
    if(o && o->rail >= 0){
        fx len = rail_length(o->rail);
        fx speed = a->args[0] > 0 ? FX(a->args[0]) : FX(10);
        fx d = (a->value + speed) % len;
        a->value = (int16_t)d;
        v3 p = rail_position_at(o->rail, d), ahead = rail_position_at(o->rail, (d + FX(8)) % len);
        a->front = v3_tangent14(v3_sub(ahead, p), a->up, a->front);
        a->position = p;
    }
    int c = actor_mario_contact(a, 0);
    if(c != CONTACT_NONE && c != CONTACT_STAR) actor_hurt_mario(a, 1);
}
