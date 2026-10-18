enum { LL_WAIT = ACTOR_STATE_WAIT, LL_SHAKE_ON, LL_SHAKE, LL_WAIT_ON };

static void lotus_converge(Actor* a){
    int32_t* off = &a->target.x;
    int32_t* v = &a->velocity.x;
    if(*off < 0) *v = fx_min(*v + 10, 26);
    else if(*off > 0) *v = fx_max(*v - 10, -26);
    else *v = 0;
    if((*off < 0 && *off + *v >= 0) || (*off > 0 && *off + *v <= 0)){
        *off = 0;
        *v = 0;
    }
}

static void lotus_leaf_think(Actor* a){
    bool on = g_mario_link.ride == actor_index(a);
    int32_t* off = &a->target.x;
    int32_t* v = &a->velocity.x;
    switch(a->state){
    case LL_WAIT:
        lotus_converge(a);
        if(on){
            a->state = LL_SHAKE_ON;
            a->timer = 0;
        }
        break;
    case LL_WAIT_ON:
        if(*off > 0) lotus_converge(a);
        else *v = 0;
        if(!on){
            a->state = LL_WAIT;
            a->timer = 0;
        }
        break;
    case LL_SHAKE_ON:
    case LL_SHAKE: {
        if(a->timer == 1){
            a->velocity.y = 384;
            a->velocity.z = 17 << 8;
        }
        int32_t step = a->timer - 1;
        fx angle = (fx)(((int64_t)FX_TAU * (step << 8)) / a->velocity.z);
        int32_t vel = -fx_mul(a->velocity.y, fx_cos(angle));
        a->velocity.z += 166;
        a->velocity.y = mul14(a->velocity.y, R14(0.95f));
        int32_t accel = vel - *v;
        *v = vel;
        if(*off + vel <= -(25 << 8)) *v = 0;
        if(a->state == LL_SHAKE && on){
            a->state = LL_SHAKE_ON;
            a->timer = 0;
        } else if(a->state == LL_SHAKE_ON && !on){
            a->state = LL_SHAKE;
            a->timer = 0;
        } else if(fx_abs(accel) < 1 && fx_abs(vel) < 2 && step > 2){
            *v = 0;
            a->state = a->state == LL_SHAKE_ON ? LL_WAIT_ON : LL_WAIT;
            a->timer = 0;
        }
        break;
    }
    }
    *off += *v;
    a->position = v3_add(a->home, v3_scale14(a->up, *off >> 8));
}
