enum { HIPDROP_SWITCH_OFF = ACTOR_STATE_WAIT, HIPDROP_SWITCH_DOWN, HIPDROP_SWITCH_ON };

static void hipdrop_switch_think(Actor* a){
    switch(a->state){
    case HIPDROP_SWITCH_OFF:
        actor_set_anim(a, a->type == OBJ_PICKETSWITCH ? 0 : ANIM_HIPDROPSWITCH_WAIT);
        if(g_mario_link.hipdrop == actor_index(a)){
            a->state = HIPDROP_SWITCH_DOWN;
            a->timer = 0;
            actor_restart_anim(a, a->type == OBJ_PICKETSWITCH ? ANIM_PICKETSWITCH_SECOND
                                                              : ANIM_HIPDROPSWITCH_ON);
            audio_play_effect(SFX_HIPDROP_LAND, 4);
        }
        break;
    case HIPDROP_SWITCH_DOWN:
        if(a->timer > 0 && actor_anim_frame_done(a, 20)){
            a->state = HIPDROP_SWITCH_ON;
            switch_set(actor_switch_key(a, SWITCH_A), true);
            camera_shake(1);
        }
        break;
    default: break;
    }
}
