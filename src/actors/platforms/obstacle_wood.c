enum { OW_WAIT = ACTOR_STATE_WAIT, OW_MOVE, OW_DONE };
static const int16_t obstacle_wood_lift_offsets[21] = {0,    -174, -185, -196, -200, -201, -203,
                                                       -209, -219, -235, -258, -290, -332, -398,
                                                       -503, -654, -845, -988, -997, -984, -1000};
static const int16_t obstacle_wood_switch_point[3] = {-535, -100, 0};

static v3 obstacle_wood_origin(const Actor* a){
    int key = a->state == OW_WAIT ? 0 : (a->anim_frame >> 8) / 7;
    return v3_add(a->position, v3_scale14(a->up, FX(obstacle_wood_lift_offsets[key < 20 ? key : 20])));
}

static void obstacle_wood_init(Actor* a){
    a->state = OW_WAIT;
    if(stream_part_state(a->stage_index) & 1u){
        a->state = OW_DONE;
        actor_restart_anim(a, ANIM_FORESTOBSTACLEWOOD_MOVE);
        a->anim_frame = 141 << 8;
    }
}

static void obstacle_wood_think(Actor* a){
    if(a->state == OW_WAIT){
        a->anim_frame = 0;
        if(g_mario_link.hipdrop == actor_index(a)){
            int16_t s[3];
            actor_stage_scale(a, s);
            v3 at = dyncol_point(obstacle_wood_switch_point, a->position, v3_cross14(a->up, a->front), a->up,
                                 a->front, s);
            if(v3_length_exact(v3_sub(g_game.player.figure.position, at)) > FX(225)) return;
            a->state = OW_MOVE;
            a->timer = 0;
            actor_restart_anim(a, ANIM_FORESTOBSTACLEWOOD_MOVE);
            stream_part_state_set(a->stage_index, 1);
            camera_shake(1);
            mario_talk_pause(120);
        }
    } else if(a->state == OW_MOVE && actor_anim_frame_done(a, 141)){
        a->state = OW_DONE;
        switch_set(actor_switch_key(a, SWITCH_A), true);
    }
}
