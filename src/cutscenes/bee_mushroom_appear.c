static Actor* bee_mushroom_appear_actor(void){
    if(g_game.player.action != MA_TALK || !level_has(LEVEL_BEE_MUSHROOM_DEMOS)) return NULL;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(a->active && a->type == OBJ_BEEMUSHROOM &&
           (a->state == BEE_MUSHROOM_APPEAR_SHOT1 || a->state == BEE_MUSHROOM_APPEAR_SHOT2))
            return a;
    }
    return NULL;
}

static bool bee_mushroom_appear_camera_target(v3* pos, v3* up, v3* front, v3* move){
    Actor* a = bee_mushroom_appear_actor();
    if(!a || a->state != BEE_MUSHROOM_APPEAR_SHOT1) return false;
    *pos = a->position;
    *up = a->up;
    *front = a->front;
    *move = V3((a->velocity.x >> 8) * g_cam_ticks, (a->velocity.y >> 8) * g_cam_ticks,
               (a->velocity.z >> 8) * g_cam_ticks);
    return true;
}

static bool bee_mushroom_appear_think(Actor* a){
    Player* p = &g_game.player;
    const StageObject* o = actor_stage_object(a);
    if(a->state == BEE_MUSHROOM_APPEAR_PENDING){
        if(camera_event_busy() || p->action == MA_TALK || p->action == MA_DEATH || p->action == MA_STAR_GET ||
           p->action == MA_STAR_LAUNCH)
            return true;
        a->value = p->action;
        a->target.x = p->action_timer;
        p->action = MA_TALK;
        stream_part_state_set(a->stage_index, stream_part_state(a->stage_index) | 1u);
        a->state = BEE_MUSHROOM_APPEAR_SHOT1;
        a->timer = 0;
        a->anim = ANIM_BEEMUSHROOM_APPEAR;
        a->anim_frame = 0;
        a->args[0] = o->args[0] == 1 ? 59 : 0;
        a->args[1] = 0;
        a->args[2] = 1;
        camera_event_start(o->camera, 1, a->position, a->up, a->front);
        if(a->args[0]){
            audio_play_effect(SFX_MORPH_APPEAR, 3);
            a->flags &= ~ACTOR_HIDDEN;
            a->velocity = v3_scale14(gravity_up_at(a->position, a->up), G(10.0f));
            a->position = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
        }
        return true;
    }
    if(a->state == BEE_MUSHROOM_APPEAR_SHOT1 || a->state == BEE_MUSHROOM_APPEAR_SHOT2){
        if(a->args[0] > 0){
            if(--a->args[0])
                a->position =
                    v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
            else a->velocity = V3(0, 0, 0);
        }
        if(a->state == BEE_MUSHROOM_APPEAR_SHOT1){
            if(a->timer >= 60) a->flags &= ~ACTOR_HIDDEN;

            if(a->timer >= 119){
                a->state = BEE_MUSHROOM_APPEAR_SHOT2;
                a->timer = 0;
                camera_event_start(o->camera + 1, 1, a->position, a->up, a->front);
            }
        } else if(a->timer >= 180){
            camera_event_cancel();
            if(p->action == MA_TALK){
                p->action = (MarioAction)a->value;
                p->action_timer = a->target.x;
            }
            a->state = ACTOR_STATE_WAIT;
            a->timer = 0;
        }
        return true;
    }
    return false;
}

static void bee_mushroom_appear_figure(const Actor* a, Figure* f){
    if(a->type != OBJ_BEEMUSHROOM ||
       !(a->state == BEE_MUSHROOM_APPEAR_SHOT1 || a->state == BEE_MUSHROOM_APPEAR_SHOT2) || a->args[0] <= 0)
        return;
    fx angle = a->args[0] * FX_TAU / 30;
    v3 rotated = v3_rotate_axis14(a->front, gravity_up_at(a->position, a->up), angle);
    f->position = v3_add(f->position, v3_scale14(rotated, FX(a->args[0] * 2 + 30)));
}
