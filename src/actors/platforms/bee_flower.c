enum {
    BEE_FLOWER_WAIT = ACTOR_STATE_WAIT,
    BEE_FLOWER_SOFT,
    BEE_FLOWER_SOFT_WAIT,
    BEE_FLOWER_HARD,
    BEE_FLOWER_RECOVER
};

static void bee_flower_think(Actor* a){
    int16_t rate = actor_stage_arg(a, 0);
    actor_rotate_yaw(a, (fx)((int32_t)(rate < 0 ? 50 : rate) * 1144 / 25600));
    bool on = g_mario_link.ride == actor_index(a);
    switch(a->state){
    case BEE_FLOWER_WAIT:
        actor_set_anim(a, ANIM_BEEFLOWER_WAIT);
        if(on){
            a->state = bee_active() ? BEE_FLOWER_SOFT : BEE_FLOWER_HARD;
            a->timer = 0;
            actor_restart_anim(a, bee_active() ? ANIM_BEEFLOWER_ONBEE : ANIM_BEEFLOWER_THROUGHMARIO);

            if(a->state == BEE_FLOWER_SOFT) audio_queue_effect(SFX_BEE_FLOWER_BEE_ON, 2);
            else audio_queue_effect(SFX_BEE_FLOWER_DISAPPEAR, 2);
        }
        break;
    case BEE_FLOWER_SOFT:
        if(actor_anim_frame_done(a, 20)) a->state = BEE_FLOWER_SOFT_WAIT;
        break;
    case BEE_FLOWER_SOFT_WAIT:
        actor_set_anim(a, ANIM_BEEFLOWER_WAIT);
        if(!bee_active() && on){
            a->state = BEE_FLOWER_HARD;
            a->timer = 0;
            actor_restart_anim(a, ANIM_BEEFLOWER_THROUGHMARIO);
            audio_queue_effect(SFX_BEE_FLOWER_DISAPPEAR, 2);
        } else if(!on) a->state = BEE_FLOWER_WAIT;
        break;
    case BEE_FLOWER_HARD:
        if(actor_anim_frame_done(a, 20)){
            actor_show_model(a, false);
            a->state = BEE_FLOWER_RECOVER;
            a->timer = 0;
        }
        break;
    case BEE_FLOWER_RECOVER:
        if(a->timer == 60){
            actor_show_model(a, true);
            actor_restart_anim(a, ANIM_BEEFLOWER_APPEAR);
            audio_queue_effect(SFX_BEE_FLOWER_RECOVER, 2);
        }
        if(a->timer > 60 && actor_anim_frame_done(a, 20)) a->state = BEE_FLOWER_WAIT;
        break;
    }
}

static inline bool bee_flower_solid(const Actor* a){
    return a->state != BEE_FLOWER_HARD && a->state != BEE_FLOWER_RECOVER;
}
