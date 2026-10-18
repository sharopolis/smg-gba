static v3 mandibug_child_center(const Actor* a){
    unsigned anim =
        a->anim >= 0 && a->anim < ANIM_STINKBUGPARENT_COUNT ? (unsigned)a->anim : ANIM_STINKBUGPARENT_WAIT;
    const MandibugChildClip* clip = &mandibug_child_clips[anim];

    const AnimClip* visible = &figure_clips(a->figure.model)[anim];
    unsigned key = ((unsigned)a->anim_frame >> 8) / visible->ticks_per_key;
    if(visible->loop) key %= visible->key_count;
    else if(key >= visible->key_count) key = visible->key_count - 1;
    unsigned frame = key * visible->ticks_per_key;
    if(frame > clip->frames) frame = clip->frames;
    const MandibugChildPoint* point = &clip->points[frame];
    int16_t pos[3] = {point->x, point->y, point->z}, scale[3];
    actor_stage_scale(a, scale);
    return dyncol_point(pos, a->position, v3_cross14(a->up, a->front), a->up, a->front, scale);
}

static v3 stinkbug_parent_clamp_front(v3 front, v3 home_front, v3 up){
    const fx limit = FXC(0.872664626f);
    if(angle_between14(front, home_front) <= limit) return front;
    v3 axis = v3_cross14(home_front, front);
    if((axis.x | axis.y | axis.z) == 0) return front;
    axis = v3_direction14(axis, up);
    return v3_tangent14(v3_rotate_axis14(home_front, axis, limit), up, front);
}

static void stinkbug_parent_set(Actor* a, int state, int anim){
    a->state = (uint8_t)state;
    a->timer = 0;
    actor_restart_anim(a, anim);
}

static v3 nearest_general_pos(v3 near){
    const Stage* st = &g_game.stage;
    v3 best = near;
    uint64_t bd = ~0ull;
    for(uint32_t i = 0; i < st->header->object_count; i++){
        const StageObject* o = &st->objects[i];
        if(o->type != OBJ_GENERALPOS) continue;
        v3 p = V3(o->position[0], o->position[1], o->position[2]);
        uint64_t d = v3_length_squared64(v3_sub(p, near));
        if(d < bd){
            bd = d;
            best = p;
        }
    }
    return best;
}

static void stinkbug_parent_think(Actor* a){
    Player* p = &g_game.player;
    bool angry = a->value & 1;
    fx dash = angry ? FX(25) : FX(20), back = angry ? FX(10) : FX(5);
    int panic_steps = angry ? 20 : 30;
    v3 to = v3_sub(p->figure.position, a->position);
    fx up_d = v3_dot14(to, a->up);
    v3 side = v3_cross14(a->up, a->front);
    bool on_back = g_mario_link.ride == actor_index(a);
    bool done = actor_anim_frame_done(a, 40);
    const StageObject* o = actor_stage_object(a);
    v3 home_front = o ? v3_from_i16(o->front) : a->front;

    bool terr = false;
    if(v3_length_squared64(to) < (uint64_t)FX(1600) * FX(1600) && fx_abs(up_d) <= FX(600) &&
       fx_abs(v3_dot14(to, side)) <= FX(200)){
        v3 apex = v3_sub(a->position, v3_scale14(home_front, FX(261)));
        v3 w = v3_tangent14(v3_sub(p->figure.position, apex), a->up, home_front);
        terr = angle_between14(w, home_front) < FXC(0.8727f);
    }
    switch(a->state){
    case STINKBUG_PARENT_WAIT:
        a->position = a->home;
        actor_set_anim(a, ANIM_STINKBUGPARENT_WAIT);
        if(terr) stinkbug_parent_set(a, STINKBUG_PARENT_DASHSIGN, ANIM_STINKBUGPARENT_RUSHSTART);
        else if(a->timer > 1)
            stinkbug_parent_set(a, STINKBUG_PARENT_SEARCH,
                                angry ? ANIM_STINKBUGPARENT_ANGERSEARCH : ANIM_STINKBUGPARENT_SEARCH);
        break;
    case STINKBUG_PARENT_SEARCH:
        a->position = a->home;

        actor_rotate_yaw(a, (a->flags & 4) ? -FXC(0.017453293f) : FXC(0.017453293f));
        if(angle_between14(a->front, home_front) > FXC(0.872664626f)){
            a->front = stinkbug_parent_clamp_front(a->front, home_front, a->up);
            a->flags ^= 4;
            stinkbug_parent_set(a, STINKBUG_PARENT_WAIT, ANIM_STINKBUGPARENT_WAIT);
        } else if(terr) stinkbug_parent_set(a, STINKBUG_PARENT_DASHSIGN, ANIM_STINKBUGPARENT_RUSHSTART);
        break;
    case STINKBUG_PARENT_DASHSIGN:
        a->position = a->home;
        a->front = v3_turn_toward(a->front, to, a->up, FXC(0.052359878f));
        a->front = stinkbug_parent_clamp_front(a->front, home_front, a->up);
        if(done) stinkbug_parent_set(a, STINKBUG_PARENT_DASHSIGNEND, ANIM_STINKBUGPARENT_RUSHSTART);
        break;
    case STINKBUG_PARENT_DASHSIGNEND:
        if(a->timer >= 10) stinkbug_parent_set(a, STINKBUG_PARENT_DASH, ANIM_STINKBUGPARENT_RUSH);
        break;
    case STINKBUG_PARENT_DASH: {
        v3 goal = v3_add(a->home, v3_scale14(a->front, FX(1200)));
        int32_t left = v3_length_exact(v3_sub(goal, a->position));
        if(v3_length_squared64(v3_sub(a->position, a->home)) >= (uint64_t)FX(1200) * FX(1200) ||
           !actor_stick_to_ground(a)){
            stinkbug_parent_set(a, STINKBUG_PARENT_DASHEND, ANIM_STINKBUGPARENT_RUSHSTOP);
            break;
        }
        a->position = v3_add(a->position, v3_scale14(a->front, fx_min(left, dash)));
        break;
    }
    case STINKBUG_PARENT_DASHEND:
        if(done) stinkbug_parent_set(a, STINKBUG_PARENT_BACK, ANIM_STINKBUGPARENT_BACK);
        break;
    case STINKBUG_PARENT_BACK: {
        v3 th = v3_sub(a->home, a->position);
        int32_t left = v3_length_exact(th);
        if(left <= 2 * back){
            a->position = a->home;
            stinkbug_parent_set(a, STINKBUG_PARENT_WAIT, ANIM_STINKBUGPARENT_WAIT);
            break;
        }
        a->position = v3_add(a->position, v3_scale14(v3_direction14(th, a->front), back));
        actor_stick_to_ground(a);
        break;
    }
    case STINKBUG_PARENT_ATTACK:
    case STINKBUG_PARENT_SPIN:
        if(done) stinkbug_parent_set(a, STINKBUG_PARENT_BACK, ANIM_STINKBUGPARENT_BACK);
        break;
    case STINKBUG_PARENT_PANIC:
        if(!on_back) stinkbug_parent_set(a, STINKBUG_PARENT_RECOVER, ANIM_STINKBUGPARENT_WAIT);
        else if(a->timer >= panic_steps)
            stinkbug_parent_set(a, STINKBUG_PARENT_SHAKESTART, ANIM_STINKBUGPARENT_REPEL);
        break;
    case STINKBUG_PARENT_RECOVER:
        if(a->timer >= 60) stinkbug_parent_set(a, STINKBUG_PARENT_BACK, ANIM_STINKBUGPARENT_BACK);
        break;
    case STINKBUG_PARENT_SHAKESTART:
        if(a->timer >= 40) stinkbug_parent_set(a, STINKBUG_PARENT_SHAKE, ANIM_STINKBUGPARENT_REPEL);
        break;
    case STINKBUG_PARENT_SHAKE:
        if(a->timer <= 25 && on_back && p->action != MA_HIPDROP){
            mario_take_off(JUMP_FIRST, v3_add(v3_scale14(a->front, G(10.0f)), v3_scale14(a->up, G(20.0f))),
                           true);
            mario_set_action(MA_DAMAGE_FALL);
            mario_restart_anim(ANIM_MARIO_DAMAGEAIR);
        }
        if(done) stinkbug_parent_set(a, STINKBUG_PARENT_BACK, ANIM_STINKBUGPARENT_BACK);
        break;
    case STINKBUG_PARENT_CHILDDEATH:
        if(a->timer == 22){
            v3 q = nearest_general_pos(a->home);
            mario_reset(q, a->up, v3_neg(a->front));
            camera_snap_behind(&g_game.camera, &g_game.player);
        }
        if(actor_anim_frame_done(a, 90)){
            a->position = a->home;
            a->front = v3_tangent14(v3_sub(nearest_general_pos(a->home), a->position), a->up, a->front);
            stinkbug_parent_set(a, STINKBUG_PARENT_ANGER, ANIM_STINKBUGPARENT_ANGER);
        }
        break;
    case STINKBUG_PARENT_ANGER:
        if(actor_anim_frame_done(a, 90)){
            collectibles_pop_column(a, OBJ_STARBIT, 8);
            stinkbug_parent_set(a, STINKBUG_PARENT_WAIT, ANIM_STINKBUGPARENT_WAIT);
        }
        break;
    case STINKBUG_PARENT_DEATH:
        if(actor_anim_frame_done(a, 60)){
            level_star_appear(2, v3_add(a->position, v3_scale14(a->up, FX(150))));
            switch_set(actor_switch_key(a, SWITCH_DEAD), true);
            a->active = 0;
        }
        return;
    }
    if(a->state == STINKBUG_PARENT_CHILDDEATH || a->state == STINKBUG_PARENT_ANGER) return;

    if(g_mario_link.hipdrop == actor_index(a)){
        if(!angry){
            v3 child = mandibug_child_center(a);
            v3 dd = v3_reject14(v3_sub(p->figure.position, child), a->up);
            if(v3_length_squared64(dd) <= (uint64_t)FX(150) * FX(150)){
                a->value |= 1;
                stinkbug_parent_set(a, STINKBUG_PARENT_CHILDDEATH, ANIM_STINKBUGPARENT_CHILDDEATH);
                audio_play_effect(SFX_ENEMY_DEFEAT, 4);
                mario_talk_pause(80);
            }
        } else {
            stinkbug_parent_set(a, STINKBUG_PARENT_DEATH, ANIM_STINKBUGPARENT_DEATH);
            audio_play_effect(SFX_ENEMY_DEFEAT, 5);
        }
        return;
    }
    if(on_back && a->state != STINKBUG_PARENT_PANIC && a->state != STINKBUG_PARENT_SHAKESTART &&
       a->state != STINKBUG_PARENT_SHAKE){
        stinkbug_parent_set(a, STINKBUG_PARENT_PANIC, ANIM_STINKBUGPARENT_LOSS);
        return;
    }
    if(p->action == MA_DEATH || on_back) return;
    if(mario_spin_reaches(a) && a->state != STINKBUG_PARENT_DASH && a->state != STINKBUG_PARENT_SPIN){
        stinkbug_parent_set(a, STINKBUG_PARENT_SPIN, ANIM_STINKBUGPARENT_SPINACTION);
        return;
    }
    if(!actor_touches_mario(a, 0)) return;

    if(v3_dot14(to, a->front) >= 0 && fx_abs(up_d) <= FX(25) + FX(150) &&
       a->state != STINKBUG_PARENT_ATTACK && a->state != STINKBUG_PARENT_BACK &&
       a->state != STINKBUG_PARENT_PANIC){
        mario_damage(a->position, 1);
        if(a->state != STINKBUG_PARENT_DASH)
            stinkbug_parent_set(a, STINKBUG_PARENT_ATTACK, ANIM_STINKBUGPARENT_ATTACK);
    }
}
