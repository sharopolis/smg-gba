enum { SHIP_BRIDGE_WAIT = 64, SHIP_BRIDGE_MOVE };

static void phantom_ship_bridge_place(Actor* a){
    mappart_place_kcl(a, kcl_phantomshipbridge, KCL_PHANTOMSHIPBRIDGE_COUNT);
    int16_t slot = (int16_t)a->velocity.x;
    int set = mappart_set_in(a, &slot, kcl_phantomshipbridge_move, KCL_PHANTOMSHIPBRIDGE_MOVE_COUNT);
    a->velocity.x = slot;
    if(set < 0) return;
    const int16_t (*tab)[2] = a->target.y ? joint_phantomshipbridge_moveb : joint_phantomshipbridge_movea;
    int frames = a->target.y ? JOINT_PHANTOMSHIPBRIDGE_MOVEB_FRAMES : JOINT_PHANTOMSHIPBRIDGE_MOVEA_FRAMES;
    int fr = fx_clamp(a->target.x, 0, frames - 1);
    MoverFrame f = mover_frame_of(a->position, a->up, a->front, mappart_scale14(a));
    v3 pivot = mover_to_world(&f, V3(FX(JOINT_PHANTOMSHIPBRIDGE_PIVOT_X), FX(JOINT_PHANTOMSHIPBRIDGE_PIVOT_Y),
                                     FX(JOINT_PHANTOMSHIPBRIDGE_PIVOT_Z)));
    int32_t c = tab[fr][0], s = tab[fr][1];

    v3 x = v3_add(v3_scale14(f.x, c), v3_scale14(f.y, s));
    v3 y = v3_sub(v3_scale14(f.y, c), v3_scale14(f.x, s));
    MoverFrame m = {pivot, x, y, f.z, f.scale};
    movers_place(set, &m);
}

static void phantom_ship_bridge_init(Actor* a, const StageObject* o){
    a->value = -1;
    a->velocity = V3(-1, 0, 0);
    a->target = V3(0, 0, 0);
    a->anim = ANIM_PHANTOMSHIPBRIDGE_MOVEA;
    a->anim_frame = 0;
    actor_set_state(a, SHIP_BRIDGE_WAIT, -1);
    if(!actor_has_switch(a, SWITCH_A) || actor_switch_on(a, SWITCH_A)){
        a->target.x = JOINT_PHANTOMSHIPBRIDGE_MOVEA_FRAMES - 1;
        a->anim_frame = (int32_t)anim_length_of(a->figure.model, a->anim) << 8;
        a->health = 2;
    }
    if(actor_has_switch(a, SWITCH_B) && actor_switch_on(a, SWITCH_B)){
        a->target.y = 1;
        a->target.x = JOINT_PHANTOMSHIPBRIDGE_MOVEB_FRAMES - 1;
        a->anim = ANIM_PHANTOMSHIPBRIDGE_MOVEB;
        a->anim_frame = (int32_t)anim_length_of(a->figure.model, a->anim) << 8;
    }
    phantom_ship_bridge_place(a);
}

static void phantom_ship_bridge_think(Actor* a){
    if(a->state == SHIP_BRIDGE_WAIT){
        if(a->health != 2 && actor_has_switch(a, SWITCH_A) && actor_switch_on(a, SWITCH_A)){
            a->health = 2;
            a->target = V3(0, 0, 0);
            if(g_game.player.action != MA_DEATH) audio_queue_effect(SFX_BRIDGE_RIDDLE, 3);
            actor_set_state(a, SHIP_BRIDGE_MOVE, ANIM_PHANTOMSHIPBRIDGE_MOVEA);
        } else if(a->target.y == 0 && actor_has_switch(a, SWITCH_B) && actor_switch_on(a, SWITCH_B)){
            a->target = V3(0, 1, 0);
            audio_queue_effect(SFX_BRIDGE_FALL, 4);
            actor_set_state(a, SHIP_BRIDGE_MOVE, ANIM_PHANTOMSHIPBRIDGE_MOVEB);
        } else {
            a->anim_frame = a->target.x > 0 ? (int32_t)anim_length_of(a->figure.model, a->anim) << 8 : 0;
        }
    } else {
        int frames =
            a->target.y ? JOINT_PHANTOMSHIPBRIDGE_MOVEB_FRAMES : JOINT_PHANTOMSHIPBRIDGE_MOVEA_FRAMES;
        a->target.x = a->timer;
        if(a->timer == 55){
            audio_queue_effect(SFX_BRIDGE_LAND, 4);
            camera_shake(1);
        }
        if(a->timer >= frames - 1){
            a->target.x = frames - 1;
            actor_set_state(a, SHIP_BRIDGE_WAIT, -1);
        }
    }
    phantom_ship_bridge_place(a);
}
