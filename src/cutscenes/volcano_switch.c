static Actor* volcano_switch_actor(void){
    if(g_game.stage_index != STAGE_FREEZEFLAME) return NULL;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(a->active && volcano_demo_object(actor_stage_object(a)) && a->state == VOLCANO_DEMO_RUN) return a;
    }
    return NULL;
}
static bool volcano_switch_paused(void){ return volcano_switch_actor() != NULL; }
static bool volcano_switch_casts_present(const FFM2DemoSpec* s){
    for(unsigned i = 0; i < s->action_count; i++){
        if(s->actions[i].type == 0) continue;
        if(!volcano_object_actor(s->actions[i].cast)) return false;
    }
    return true;
}
static bool volcano_switch_tick(const KeyState* keys){
    if(g_game.stage_index != STAGE_FREEZEFLAME) return false;
    Actor* demo = volcano_switch_actor();
    if(!demo && !cutscene_blocks(BLOCK_SCENE_START) && g_game.player.action != MA_DEATH){
        for(int i = 0; i < g_game.actor_count; i++){
            Actor* a = &g_game.actors[i];
            const FFM2DemoSpec* d = volcano_demo_spec(a);
            if(a->active && d && a->state == VOLCANO_DEMO_WAIT && switch_on(d->trigger) &&
               volcano_switch_casts_present(d)){
                a->state = VOLCANO_DEMO_RUN;
                a->timer = 0;
                demo = a;
                break;
            }
        }
    }
    if(!demo) return false;
    const FFM2DemoSpec* d = volcano_demo_spec(demo);
    int step = demo->timer;
    if(step >= d->total){
        stream_part_state_set(demo->stage_index, stream_part_state(demo->stage_index) | 1u);
        demo->state = VOLCANO_DEMO_DONE;
        camera_event_cancel();
        camera_galaxy_snap();
        return false;
    }
    for(unsigned p = 0; p < d->phase_count; p++)
        if(step == d->phase_at[p]){
            if(p) camera_event_cancel();
            camera_event_start(d->camera + p, 1, demo->position, demo->up, demo->front);
        }
    for(unsigned k = 0; k < d->action_count; k++)
        if(step == d->actions[k].at && d->actions[k].type == 3){
            Actor* a = volcano_object_actor(d->actions[k].cast);
            if(a && a->type == OBJ_ANMMODELSWITCHMOVE && a->state != VOLCANO_PART_DONE){
                actor_set_state(a, VOLCANO_PART_MOVE, -1);
                a->anim_frame = 0;
            }
        }

    parts_collision_pre();
    for(unsigned k = 0; k < d->action_count; k++)
        if(d->actions[k].type == 3 && step >= d->actions[k].at){
            Actor* a = volcano_object_actor(d->actions[k].cast);
            if(a && a->active){
                a->timer++;
                volcano_part_switch_move_think(a);
                volcano_part_anim_tick(a);
            }
        }
    parts_collision_post();
    camera_galaxy_update(FX_ONE, keys);
    demo->timer++;
    return true;
}
