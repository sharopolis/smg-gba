static Actor* iceking_demo_actor(void){
    if(!g_iceking.demo || g_iceking.stage != g_game.stage_index || g_iceking.owner >= g_game.actor_count)
        return NULL;
    Actor* a = &g_game.actors[g_iceking.owner];
    return a->active && a->type == OBJ_ICEKING ? a : NULL;
}
static bool iceking_demo_cast(const Actor* owner, const Actor* a){
    return owner->type == OBJ_ICEKING && a->value == actor_index(owner) &&
           (a->type == OBJ_ICEKINGWEAPON || a->type == OBJ_ICEKINGSHOCK || a->type == OBJ_ICEKINGBREAK ||
            a->type == OBJ_ICECHILD);
}
static void iceking_demo_hold(const IceKingPlacedPose* pose){
    if(!g_iceking.held){
        if(g_mario_held) return;
        mario_hold_begin();
        g_iceking.held = 1;
    }
    if(pose){
        Player* p = &g_game.player;
        v3 up = v3_from_i16(pose->up);
        p->up = p->gravity_up = up;
        mario_hold_move(v3_from_i32(pose->pos), v3_from_i16(pose->front));
    }
    mario_set_action(MA_WAIT);
    mario_restart_anim(ANIM_MARIO_BATTLEWAIT);
}
static void iceking_demo_start(int part){
    g_iceking.demo = part + 1;
    g_iceking.demo_tick = 0;
    const IceKingPlacedPose* pose = &iceking_spec.source;
    camera_event_start(iceking_spec.camera[part], 1, v3_from_i32(pose->pos), v3_from_i16(pose->up),
                       v3_from_i16(pose->front));
    iceking_demo_hold(part == 1 ? &iceking_spec.mario_after : part == 2 ? &iceking_spec.mario_angry : NULL);
}
static void iceking_demo_end(void){
    camera_event_cancel();
    g_iceking.demo = 0;
    if(g_iceking.held){
        g_iceking.held = 0;
        mario_hold_release(V3(0, 0, 0));
    }
}
