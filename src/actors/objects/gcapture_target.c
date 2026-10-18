static bool gcapture_available(const Actor* a){
    if(!a->active || a->type != OBJ_GCAPTURETARGET) return false;
    if(blue_chip_completion_cast(a)) return blue_chip_completion_cast_shown() && a->state != GC_APPEAR;
    return !(a->flags & ACTOR_HIDDEN);
}

static bool gcapture_hidden(const Actor* a){ return !gcapture_visible(a); }

static bool gcapture_visible(const Actor* a){
    return a->active && a->type == OBJ_GCAPTURETARGET && !(a->flags & ACTOR_HIDDEN) &&
           (!blue_chip_completion_cast(a) || blue_chip_completion_cast_shown());
}

static int16_t g_gcapture_held = -1;

static const Actor* gcapture_held_actor(void){
    if(g_gcapture_held < 0 || g_gcapture_held >= g_game.actor_count) return NULL;
    const Actor* h = &g_game.actors[g_gcapture_held];
    return h->active && h->type == OBJ_GCAPTURETARGET ? h : NULL;
}

static void gcapture_reset(void){
    g_gcapture_held = -1;
    audio_stop_level_effect(SFX_CAPTURE_POINT);
    audio_stop_level_effect(SFX_CAPTURE_PULL);
    audio_stop_level_effect(SFX_CAPTURE_HOLD);
}

static void capture_bubble_init(Figure* bubble, v3 centre, v3 cam_front, v3 cam_up, int grow){
    figure_init(bubble, figure_capturebubble);
    bubble->position = v3_sub(centre, v3_scale14(cam_front, FX(80)));
    grow = fx_min(grow, 20);
    bubble->scale = V3(64 + grow * 26 / 20, 64 + grow * 26 / 20, 64);
    bubble->use_basis = true;
    bubble->basis_front = v3_neg(cam_front);
    bubble->basis_up = cam_up;
    figure_update_transform(bubble);
}

static void gcapture_add_figure(Figure* bubble){
    if(!g_mario_held || g_gcapture_held < 0 || g_gcapture_held >= g_game.actor_count ||
       g_game.figure_count >= MAX_FIGURES)
        return;
    Actor* a = &g_game.actors[g_gcapture_held];
    if(!gcapture_available(a) || !a->value) return;
    const Player* p = &g_game.player;
    const Camera* c = &g_game.camera;
    v3 centre = v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT));
    capture_bubble_init(bubble, centre, c->front, c->up, a->value == 1 ? a->timer : 20);
    g_game.figures[g_game.figure_count++] = bubble;
}

static Actor* gcapture_best(const Actor* except){
    Actor* selected = pointer_pull_selected();
    if(selected) return selected == except ? NULL : selected;
    Actor* best = NULL;
    int32_t best_d = 0x7FFFFFFF;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* t = &g_game.actors[i];
        if(!gcapture_available(t) || t == except || !pointer_is_pointing(t, FX(100))) continue;
        int32_t d = actor_mario_distance(t);
        if(d < FX(actor_arg_or(t, 1, 2500)) && d < best_d){
            best_d = d;
            best = t;
        }
    }
    return best;
}

static void gcapture_grab(Actor* t, v3 velocity){
    bool recapture = g_gcapture_held >= 0 && g_mario_held;
    if(!recapture) mario_hold_begin();
    g_gcapture_held = (int16_t)actor_index(t);
    t->value = 1;
    t->health = 0;
    t->velocity = velocity;
    actor_set_state(t, GC_WAIT, ANIM_GCAPTURETARGET_REACT);
    if(recapture) t->timer = 15;
    mario_set_action(MA_TALK);
    game_player()->on_ground = false;
    mario_restart_anim(ANIM_MARIO_SPACESTRUGGLE);
    audio_stop_level_effect(SFX_CAPTURE_POINT);
    audio_stop_level_effect(SFX_CAPTURE_HOLD);
    audio_stop_level_effect(SFX_CAPTURE_PULL);
    audio_play_effect(SFX_CAPTURE_ON, 2);
}

static void gcapture_let_go(Actor* t, v3 velocity){
    t->value = 0;
    g_gcapture_held = -1;
    actor_set_state(t, GC_WAIT, ANIM_GCAPTURETARGET_WAIT);
    audio_stop_level_effect(SFX_CAPTURE_HOLD);
    audio_stop_level_effect(SFX_CAPTURE_PULL);
    audio_play_effect(SFX_CAPTURE_RELEASE, 2);
    mario_hold_release(velocity);
}

static void gcapture_cancel_for_launch(void){
    if(!g_mario_held || g_gcapture_held < 0 || g_gcapture_held >= g_game.actor_count) return;
    Actor* old = &g_game.actors[g_gcapture_held];
    old->value = 0;
    actor_set_state(old, GC_WAIT, ANIM_GCAPTURETARGET_WAIT);
    gcapture_reset();
    g_mario_held = false;
    audio_play_effect(SFX_CAPTURE_RELEASE, 2);
}

static void gcapturetarget_think(Actor* a){
    if(gcapture_appear_tick(a)) return;
    if(blue_chip_completion_cast(a)){
        if(!gcapture_available(a)){
            a->flags |= ACTOR_HIDDEN;
            return;
        }
        a->flags &= ~ACTOR_HIDDEN;
    } else if(!gcapture_available(a)) return;
    Player* p = game_player();
    Actor* pointed = gcapture_best(NULL);
    if(pointed == a){
        pointer_set_hand(pointer_held() ? 1 : 0);
        if(!g_mario_held) audio_level_effect(SFX_CAPTURE_POINT);
    }
    if(g_gcapture_held < 0 || !g_mario_held){
        g_gcapture_held = -1;
        if(pointer_pressed() && p->action != MA_DEATH && pointed == a) gcapture_grab(a, V3(0, 0, 0));
        return;
    }
    if(g_gcapture_held != actor_index(a)) return;
    v3 pos = p->figure.position;
    v3 to = v3_sub(actor_centre(a), pos);
    int32_t d = v3_length_exact(to);
    v3 dir = v3_direction14(to, p->front);
    v3 v = a->velocity;
    if(a->value == 1 && a->timer >= 30){
        a->value = 2;
        mario_restart_anim(ANIM_MARIO_SPACESTRUGGLE);
    }
    if(a->value == 1) v = v3_scale14(v, R14(0.99f));
    if(a->value == 2){
        audio_level_effect(SFX_CAPTURE_PULL);
        v = v3_add(v, v3_scale14(dir, mul14(V16(0.7f), a->health)));
        int damping = d < FX(500) ? R14(0.75f) + R14(0.24f) * d / FX(500) : R14(0.99f);
        v = v3_scale14(v, damping);
        a->health = (int16_t)fx_min(a->health + R14(0.97f), N14_ONE);
        if(!pointer_held() && a->timer > 30){
            a->value = 3;
            a->timer = 0;
            audio_stop_level_effect(SFX_CAPTURE_PULL);
            mario_restart_anim(ANIM_MARIO_SPACEWAIT);
        }
    } else if(a->value == 3){
        audio_level_effect(SFX_CAPTURE_HOLD);
        a->health = (int16_t)fx_max(a->health - R14(0.12f), 0);
        v = v3_scale14(v, R14(0.99f));

        GravitySample gravity = gravity_sample(g_game.stage.header, pos);
        v3 up = gravity.index >= 0 ? gravity.up : p->gravity_up;
        RayHit floor;
        if(collision_ray(v3_add(pos, v3_scale14(up, FX(70))), v3_sub(pos, v3_scale14(up, FX(60))), &floor,
                         0) &&
           v3_dot14(floor.normal, up) >= R14(0.7f)){
            gcapture_let_go(a, v3_reject14(v, floor.normal));
            return;
        }
    }
    if(pointer_pressed() && a->value >= 2){
        Actor* next = gcapture_best(a->value == 3 ? NULL : a);
        if(next){
            a->value = 0;
            actor_set_state(a, GC_WAIT, ANIM_GCAPTURETARGET_WAIT);
            gcapture_grab(next, v);
            return;
        }
    }
    v3 step = V3(v.x >> 8, v.y >> 8, v.z >> 8);
    RayHit hit;
    if(collision_ray(pos, v3_add(pos, v3_add(step, v3_scale14(dir, FX(60)))), &hit, 0)){
        gcapture_let_go(a, v3_scale14(hit.normal, V16(25.0f)));
        return;
    }
    a->velocity = v;
    if(d > FX(120))
        mario_hold_move(v3_add(pos, step), (v.x | v.y | v.z) ? v3_direction14(v, p->front) : p->front);
    if(a->value == 3 && a->timer > 180) gcapture_let_go(a, v);
    if(p->action == MA_DEATH) gcapture_let_go(a, V3(0, 0, 0));
}

static void gcapturetarget_init(Actor* a, const StageObject* o){
    actor_set_state(a, GC_WAIT, ANIM_GCAPTURETARGET_WAIT);
    if(!gcapture_available(a)) a->flags |= ACTOR_HIDDEN;
}

static bool gcapturetarget_idle(const Actor* a){ return g_gcapture_held < 0; }
