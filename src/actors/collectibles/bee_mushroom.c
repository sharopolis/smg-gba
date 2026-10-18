static void bee_mushroom_think(Actor* a){
    Player* p = &g_game.player;
    const StageObject* o = actor_stage_object(a);
    if(bee_mushroom_appear_think(a)) return;
    if(a->state == ACTOR_STATE_TAKEN){
        if(!bee_active() && a->timer > 120){
            a->state = ACTOR_STATE_WAIT;
            a->figure.model = (const FigureHeader*)actor_kinds[a->type].figure;
            a->timer = 0;
        }
        return;
    }
    if(a->args[2] && a->state == ACTOR_STATE_WAIT && o->args[2] != 1 && a->args[1] <= 3){
        v3 up = gravity_up_at(a->position, a->up);
        a->velocity = v3_sub(a->velocity, v3_scale14(up, G(0.75f)));
        v3 next = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
        RayHit hit;
        if(v3_dot14(a->velocity, up) < 0 &&
           collision_ray(v3_add(a->position, v3_scale14(up, FX(80))), v3_sub(next, v3_scale14(up, FX(10))),
                         &hit, COL_NO_CAMERA) &&
           v3_dot14(hit.normal, up) > R14(.5f)){
            a->position = v3_add(hit.point, v3_scale14(up, FX(10)));
            fx speed = v3_length_exact(a->velocity);
            if(speed < G(2.0f)) a->velocity = V3(0, 0, 0);
            else {
                a->velocity = a->args[1] < 3 ? V3(-a->velocity.x / 2, -a->velocity.y / 2, -a->velocity.z / 2)
                                             : V3(0, 0, 0);
                a->args[1]++;
            }
        } else a->position = next;
    }
    if(!bee_active() && p->action != MA_DEATH && actor_touches_mario(a, 0)){
        bee_start();
        a->state = ACTOR_STATE_TAKEN;
        a->timer = 0;
        a->figure.model = NULL;
        audio_play_effect(SFX_BEE_MORPH, 4);
    }
}
