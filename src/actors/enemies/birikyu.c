static void birikyu_think(Actor* a){
    int32_t speed = actor_arg_or(a, 0, 10);
    int rail = actor_rail(a);
    if(rail >= 0){
        fx len = rail_length(rail);
        a->value = (int16_t)(a->value + ((a->flags & 0x10) ? -speed : speed));
        if(rail_closed(rail)) a->value = (int16_t)(len > 0 ? (a->value + len) % len : 0);
        else if(a->value >= len || a->value <= 0){
            a->value = (int16_t)fx_clamp(a->value, 0, len);
            a->flags ^= 0x10;
        }
        v3 p = rail_position_at(rail, a->value);
        v3 ahead = rail_position_at(rail, a->value + ((a->flags & 0x10) ? -8 : 8));
        a->front = v3_tangent14(v3_sub(ahead, p), a->up, a->front);
        a->position = p;
        if(actor_mario_contact(a, 0) != CONTACT_NONE && actor_mario_contact(a, 0) != CONTACT_STAR)
            actor_hurt_mario(a, 1);
        return;
    }
    fx ang = (fx)((int32_t)a->timer * speed * FX_ONE / 400) * (actor_arg_or(a, 1, 0) == 1 ? -1 : 1);
    v3 dir = v3_rotate_axis14(a->target, a->up, ang);
    a->position = v3_add(a->home, v3_scale14(dir, FX(400)));
    a->front = v3_cross14(dir, a->up);
    if(actor_mario_contact(a, 0) != CONTACT_NONE && actor_mario_contact(a, 0) != CONTACT_STAR)
        actor_hurt_mario(a, 1);
}

static void birikyu_init(Actor* a, const StageObject* o){
    a->target = v3_cross14(a->up, a->front);
    if(actor_rail(a) >= 0){
        int rail = actor_rail(a);
        fx len = rail_length(rail), best = 0;
        uint64_t bd = UINT64_MAX;
        for(fx d = 0; d < len; d += 20){
            uint64_t q = v3_length_squared64(v3_sub(rail_position_at(rail, d), a->position));
            if(q < bd){
                bd = q;
                best = d;
            }
        }
        a->value = (int16_t)best;
    }
}
