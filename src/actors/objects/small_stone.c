static void smallstone_think(Actor* a){
    if(a->state == SS_BROKEN) return;
    if(a->state == SS_IDLE && actor_mario_within(a, FX(2500))){
        for(int i = 0; i < 8; i++){
            v3 dir = v3_rotate_axis14(v3_cross14(a->up, a->front), a->up, FXC(0.7854f) * i);
            v3 at = v3_add(a->position, v3_scale14(dir, FX(300)));
            RayHit hit;
            if(collision_ray(v3_add(at, v3_scale14(a->up, FX(300))), v3_sub(at, v3_scale14(a->up, FX(300))),
                             &hit, 0))
                at = hit.point;
            Actor* m = actor_spawn_reuse(OBJ_SMALLSTONEMEMBER, at, a->up, dir);
            if(!m) break;
            m->value = (int16_t)actor_index(a);
        }
        actor_set_state(a, SS_OUT, -1);
    }
    bool spin =
        g_mario.attack_timer > 0 &&
        v3_length_exact(v3_sub(mario_centre(), v3_add(a->position, v3_scale14(a->up, FX(120))))) < FX(400);
    bool far = !actor_mario_within(a, FX(3500));
    if(a->state != SS_OUT || !(spin || far)) return;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* m = &g_game.actors[i];
        if(!m->active || m->type != OBJ_SMALLSTONEMEMBER || m->value != actor_index(a)) continue;
        if(spin){
            collectibles_pop(OBJ_STARBIT, m->position, m->up, 1);
            actor_defeat(m, false);
        } else m->active = 0;
    }
    actor_set_state(a, spin ? SS_BROKEN : SS_IDLE, -1);
}

static void smallstone_member_think(Actor* a){ actor_block_mario(a, FX(20), FX(60)); }

static void smallstone_init(Actor* a, const StageObject* o){ actor_set_state(a, SS_IDLE, -1); }
