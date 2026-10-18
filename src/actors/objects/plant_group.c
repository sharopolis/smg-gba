static const uint8_t* plantgroup_figure(int group_type){
    return group_type == OBJ_CUTBUSHGROUP  ? figure_cutbush
           : group_type == OBJ_FLOWERGROUP ? figure_flower
                                           : figure_flowerblue;
}

static void plantgroup_think(Actor* a){
    int n = fx_min(actor_arg_or(a, 0, 7), 7);
    bool near = actor_mario_within(a, FX(2500));
    if(a->state == PG_IDLE && near){
        int items = actor_arg_or(a, 1, 0);
        for(int i = 0; i < n; i++){
            v3 at = a->position;
            if(i){
                v3 dir = v3_rotate_axis14(v3_cross14(a->up, a->front), a->up, FXC(1.0472f) * (i - 1));
                at = v3_add(at, v3_scale14(dir, FX(160)));
                RayHit hit;
                if(collision_ray(v3_add(at, v3_scale14(a->up, FX(300))),
                                 v3_sub(at, v3_scale14(a->up, FX(300))), &hit, 0))
                    at = hit.point;
            }
            Actor* m = actor_spawn_reuse(OBJ_PLANTMEMBER, at, a->up, a->front);
            if(!m) break;
            m->figure.model = (const FigureHeader*)plantgroup_figure(a->type);
            m->value = (int16_t)actor_index(a);

            bool has = ((i * 3 + 1) % n) < items && !(a->health >> i & 1);
            m->args[0] = has ? (actor_arg_or(a, 2, 0) == 1 ? 2 : 1) : 0;
            m->args[1] = (int16_t)i;
            m->anim = 1;
            m->anim_frame = (fx)(i * 37) << 8;
            actor_set_state(m, PM_WAIT, -1);
        }
        actor_set_state(a, PG_OUT, -1);
    } else if(a->state == PG_OUT && !actor_mario_within(a, FX(3500))){
        for(int i = 0; i < g_game.actor_count; i++){
            Actor* m = &g_game.actors[i];
            if(m->active && m->type == OBJ_PLANTMEMBER && m->value == actor_index(a)) m->active = 0;
        }
        actor_set_state(a, PG_IDLE, -1);
    }
}

static void plantmember_think(Actor* a){
    Actor* g = &g_game.actors[a->value];
    if(a->state == PM_SHAKE && actor_anim_done(a)){
        actor_set_state(a, PM_WAIT, 1);
    }
    if(a->state == PM_SHAKE && a->timer < 20) return;
    MarioPhysics* m = &g_mario;
    bool moving = m->ratio > R14(0.15f) || m->jumping;
    bool spin = m->attack_timer > 0 && actor_mario_within(a, FX(300));
    if(!(spin || (moving && actor_touches_mario(a, FX(50))))) return;
    actor_set_state(a, PM_SHAKE, 0);
    if(!spin && a->args[0]){
        collectibles_pop(a->args[0] == 2 ? OBJ_STARBIT : OBJ_COIN,
                         v3_add(a->position, v3_scale14(a->up, FX(60))), a->up, 1);
        g->health |= (int16_t)(1 << a->args[1]);
        a->args[0] = 0;
    }
}

static void plantgroup_init(Actor* a, const StageObject* o){
    actor_set_state(a, PG_IDLE, -1);
    a->health = 0;
}
