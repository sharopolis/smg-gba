static void honey_queen_think(Actor* a){
    Player* p = &g_game.player;
    const StageObject* q = actor_stage_object(a);
    if(!q) return;

    actor_set_anim(a, a->state >= HQ_AUDIENCE        ? ANIM_HONEYQUEEN_TALK
                      : a->state == ACTOR_STATE_TALK ? ANIM_HONEYQUEEN_EVENTWAIT
                                                     : ANIM_HONEYQUEEN_WAIT);
    if(a->state >= HQ_AUDIENCE){
        talk_force_actor(a);
        return;
    }
    bool ground = p->on_ground && !g_mario.jumping && p->action != MA_TALK;
    if(q->sw[SWITCH_SLEEP] >= 0){
        if(a->state == ACTOR_STATE_WAIT && switch_on(q->sw[SWITCH_SLEEP]) && ground){
            a->state = HQ_AUDIENCE;
            talk_force_actor(a);
        } else if(a->state == ACTOR_STATE_TALK && switch_on(q->sw[SWITCH_A]) && ground){
            a->state = HQ_THANKS;
            talk_force_actor(a);
        }
        return;
    }
    if(a->state != ACTOR_STATE_WAIT || !ground) return;

    for(int i = 0; i < g_game.actor_count; i++){
        const Actor* m = &g_game.actors[i];
        const StageObject* o = actor_stage_object(m);
        if(!m->active || m->type != OBJ_MESSAGEAREA || !o || o->zone != q->zone) continue;
        v3 d = v3_sub(p->figure.position, m->position);
        fx hx = (FX(500) * (o->scale[0] ? o->scale[0] : 256)) >> 8;
        fx hy = (FX(1000) * (o->scale[1] ? o->scale[1] : 256)) >> 8;
        fx hz = (FX(500) * (o->scale[2] ? o->scale[2] : 256)) >> 8;
        v3 side = v3_cross14(m->up, m->front);
        fx x = v3_dot14(d, side), y = v3_dot14(d, m->up), z = v3_dot14(d, m->front);
        if(fx_abs(x) <= hx && y >= 0 && y <= hy && fx_abs(z) <= hz){
            a->state = HQ_NEIGHBORS;
            talk_force_actor(a);
            break;
        }
    }
}

static v3 honey_queen_vector(const int16_t v[3], v3 side, v3 up, v3 front){
    return v3_add(v3_add(v3_scale14(side, v[0]), v3_scale14(up, v[1])), v3_scale14(front, v[2]));
}

static void honey_queen_collision(Actor* a, v3 mario){
    const HoneyQueenPose(*poses)[10] = honey_queen_wait;
    int ticks = 24, count = 20;
    if(a->anim == ANIM_HONEYQUEEN_EVENTWAIT) poses = honey_queen_eventwait;
    else if(a->anim == ANIM_HONEYQUEEN_TALK) poses = honey_queen_talk;
    else if(a->anim == ANIM_HONEYQUEEN_ITCH){
        poses = honey_queen_itch;
        ticks = 6;
    }
    int first = (a->anim_frame / (ticks << 8)) % count, next = (first + 1) % count;
    int t = (a->anim_frame % (ticks << 8)) * 64 / ticks;
    v3 side = v3_cross14(a->up, a->front);
    int16_t scale[3];
    actor_stage_scale(a, scale);
    for(int k = 0; k < 10; k++){
        const HoneyQueenPose* p = &poses[first][k];
        const HoneyQueenPose* q = &poses[next][k];
        int16_t pos[3], up[3], front[3];
        for(int axis = 0; axis < 3; axis++){
            pos[axis] = p->pos[axis] + ((q->pos[axis] - p->pos[axis]) * t >> 14);
            up[axis] = p->up[axis] + ((q->up[axis] - p->up[axis]) * t >> 14);
            front[axis] = p->front[axis] + ((q->front[axis] - p->front[axis]) * t >> 14);
        }
        v3 at = dyncol_point(pos, a->position, side, a->up, a->front, scale);
        v3 u = v3_direction14(honey_queen_vector(up, side, a->up, a->front), a->up);
        v3 f = v3_tangent14(honey_queen_vector(front, side, a->up, a->front), u, a->front);
        dyncol_add(&honey_colmeshes[honey_queen_mesh[k]], at, u, f, scale, actor_index(a), 0, mario, FX(600));
    }
}

static void honey_queen_talk_ended(Actor* a){
    if(a->state == HQ_AUDIENCE) a->state = ACTOR_STATE_TALK;
    else if(a->state == HQ_THANKS){
        switch_set(actor_switch_key(a, SWITCH_B), true);
        a->state = ACTOR_STATE_DONE;
    } else if(a->state == HQ_NEIGHBORS) a->state = ACTOR_STATE_DONE;
}
