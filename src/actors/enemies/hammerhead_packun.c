static v3 hammerhead_head(const Actor* a){
    const int16_t (*points)[3] = packun_head_wait;
    int count = sizeof packun_head_wait / sizeof packun_head_wait[0], ticks = 6;
#define HEAD_CLIP(clip, step)        \
    case ANIM_PACKUNFLOWER_##clip:   \
        points = packun_head_##step; \
        count = sizeof packun_head_##step / sizeof packun_head_##step[0]
    switch(a->anim){
        HEAD_CLIP(WAIT, wait);
        break;
        HEAD_CLIP(ATTACK, attack);
        ticks = 5;
        break;
        HEAD_CLIP(CHANCE, chance);
        ticks = 4;
        break;
        HEAD_CLIP(RECOVERY, recovery);
        break;
        HEAD_CLIP(SWOONSTART, swoonstart);
        ticks = 5;
        break;
        HEAD_CLIP(SWOON, swoon);
        ticks = 5;
        break;
        HEAD_CLIP(SWOONEND, swoonend);
        break;
        HEAD_CLIP(CROW, crow);
        break;
        HEAD_CLIP(BLOWDOWN, blowdown);
        ticks = 4;
        break;
        HEAD_CLIP(DOWN, down);
        ticks = 4;
        break;
    }
#undef HEAD_CLIP
    int key = a->anim_frame / (ticks << 8);
    if(a->anim == ANIM_PACKUNFLOWER_WAIT || a->anim == ANIM_PACKUNFLOWER_CHANCE ||
       a->anim == ANIM_PACKUNFLOWER_SWOON)
        key %= count;
    else if(key >= count) key = count - 1;
    const int16_t* q = points[key];
    v3 side = v3_cross14(a->up, a->front);
    int scale = a->figure.scale.x;
    return v3_add(a->position,
                  v3_add(v3_add(v3_scale14(side, q[0] * scale / 64), v3_scale14(a->up, q[1] * scale / 64)),
                         v3_scale14(a->front, q[2] * scale / 64)));
}

static int hammerhead_contact(const Actor* a, v3 head, bool chance){
    Player* p = game_player();
    MarioPhysics* m = &g_mario;
    if(p->action == MA_DEATH || g_mario_held || p->action == MA_STAR_LAUNCH) return CONTACT_NONE;
    v3 delta = v3_sub(mario_centre(), head);
    fx above = v3_dot14(delta, a->up);
    fx radius = FX(130) + C_BODY_RADIUS;
    uint64_t distance = v3_length_squared64(delta);
    bool touch = distance <= (uint64_t)radius * radius;
    bool spin = m->attack_timer > 0 && distance <= (uint64_t)(radius + FX(60)) * (radius + FX(60));
    if(touch || spin){
        if(g_mario_star_power > 0) return CONTACT_STAR;
        if(p->action == MA_HIPDROP && above > 0) return CONTACT_HIPDROP;
        if(spin) return CONTACT_SPIN;
        if(m->jumping && vertical_speed(m->jump_vec, p->up) < 0 && above > FX(65)) return CONTACT_TRAMPLE;
    }
    bool landed = !p->was_on_ground && p->on_ground && vertical_speed(m->air_vec, p->up) < 0;
    bool down = (m->jumping && vertical_speed(m->jump_vec, p->up) < 0) || landed;
    if(chance && down && m->attack_timer == 0 && above >= -FX(35) && above <= FX(130) + C_BODY_RADIUS){
        v3 move = V3(m->air_vec.x >> 8, m->air_vec.y >> 8, m->air_vec.z >> 8);
        v3 previous = v3_sub(delta, move);
        fx before = v3_dot14(previous, a->up);
        if(before >= FX(35)){
            v3 at = delta;
            if(above < FX(65) && before > above){
                fx time = fx_clamp((fx)((int64_t)(before - FX(65)) * FX_ONE / (before - above)), 0, FX_ONE);
                at = v3_add(previous, v3_scale(v3_sub(delta, previous), time));
            }
            v3 flat = v3_reject14(at, a->up);
            fx radius = FX(130) + C_BODY_RADIUS + FX(35);
            if(v3_length_squared64(flat) <= (uint64_t)radius * radius){
                if(p->action == MA_HIPDROP || (landed && p->action == MA_HIPDROP_LAND))
                    return CONTACT_HIPDROP;
                return CONTACT_TRAMPLE;
            }
        }
    }
    return touch ? CONTACT_TOUCH : CONTACT_NONE;
}

static void hammerhead_think(Actor* a){
    switch(a->state){
    case HAMMERHEAD_WAIT:
        if(actor_mario_within(a, FX(800))) actor_set_state(a, HAMMERHEAD_SEARCH, ANIM_PACKUNFLOWER_WAIT);
        break;
    case HAMMERHEAD_SEARCH:
        if(actor_turn_toward(a, actor_to_mario(a), DEG(1)) < DEG(30))
            actor_set_state(a, HAMMERHEAD_ATTACK, ANIM_PACKUNFLOWER_ATTACK);
        else if(!actor_mario_within(a, FX(900))) actor_set_state(a, HAMMERHEAD_WAIT, ANIM_PACKUNFLOWER_WAIT);
        break;
    case HAMMERHEAD_ATTACK:
        if(a->timer < 60) actor_turn_toward(a, actor_to_mario(a), DEG(1));
        if(actor_anim_done(a)){
            a->value = 0;
            actor_set_state(a, HAMMERHEAD_REST, ANIM_PACKUNFLOWER_CHANCE);
        }
        break;
    case HAMMERHEAD_REST:
        if(a->timer > 90) actor_set_state(a, HAMMERHEAD_RECOVER, ANIM_PACKUNFLOWER_RECOVERY);
        break;
    case HAMMERHEAD_RECOVER:
    case HAMMERHEAD_CROW:
        if(actor_anim_done(a)) actor_set_state(a, HAMMERHEAD_WAIT, ANIM_PACKUNFLOWER_WAIT);
        break;
    case HAMMERHEAD_DAMAGE:
        if(a->anim == ANIM_PACKUNFLOWER_SWOONSTART && actor_anim_done(a)){
            a->anim = ANIM_PACKUNFLOWER_SWOON;
            a->anim_frame = 0;
        }
        if(a->timer >= 180) actor_set_state(a, HAMMERHEAD_RISE, ANIM_PACKUNFLOWER_SWOONEND);
        break;
    case HAMMERHEAD_SWOON:
        if(a->timer > 180) actor_set_state(a, HAMMERHEAD_RISE, ANIM_PACKUNFLOWER_SWOONEND);
        break;
    case HAMMERHEAD_RISE:
        if(actor_anim_done(a)) actor_set_state(a, HAMMERHEAD_SEARCH, ANIM_PACKUNFLOWER_WAIT);
        break;
    case HAMMERHEAD_KILL:
        if(actor_anim_done(a)){
            collectibles_pop(OBJ_STARBIT, hammerhead_head(a), a->up, 9);
            actor_defeat(a, false);
        }
        return;
    }
    bool chance = (a->state == HAMMERHEAD_REST && a->timer > 0) || a->state == HAMMERHEAD_DAMAGE ||
                  a->state == HAMMERHEAD_SWOON || (a->state == HAMMERHEAD_RECOVER && a->timer <= 50) ||
                  (a->state == HAMMERHEAD_RISE && a->timer <= 30);
    int c = hammerhead_contact(a, hammerhead_head(a), chance);
    if(c == CONTACT_NONE) return;
    if(chance && (c == CONTACT_HIPDROP || c == CONTACT_TRAMPLE)){
        if(c == CONTACT_TRAMPLE) mario_stomp_bounce(g_game.keys.held & KEY_A);
        actor_set_state(a, HAMMERHEAD_KILL, ANIM_PACKUNFLOWER_DOWN);
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
    } else if(c == CONTACT_STAR){
        actor_set_state(a, HAMMERHEAD_KILL, ANIM_PACKUNFLOWER_BLOWDOWN);
    } else if(chance && c == CONTACT_SPIN && a->state != HAMMERHEAD_DAMAGE){
        mario_spin_hop();
        if(a->value >= 3) actor_set_state(a, HAMMERHEAD_RISE, ANIM_PACKUNFLOWER_SWOONEND);
        else {
            ++a->value;
            actor_set_state(a, HAMMERHEAD_DAMAGE, ANIM_PACKUNFLOWER_SWOONSTART);
        }
        audio_play_effect(SFX_ENEMY_HIT, 2);
    } else if(!chance && (c == CONTACT_TOUCH || c == CONTACT_TRAMPLE)){
        actor_hurt_mario(a, 1);
        if(a->state == HAMMERHEAD_ATTACK) actor_set_state(a, HAMMERHEAD_CROW, ANIM_PACKUNFLOWER_CROW);
    }
}

static bool hammerhead_packun_view_bounds(const Figure* f, const CameraView* view){
    return view_sphere_visible(view, f->position, FX(760) * f->scale.x / 64);
}

static const FigureOps hammerhead_packun_figure_ops = {.view_bounds = hammerhead_packun_view_bounds};

static fx hammerhead_draw_extent(const Actor* a){ return FX(760) * a->figure.scale.x / 64; }

static void hammerhead_init(Actor* a, const StageObject* o){
    actor_set_state(a, HAMMERHEAD_WAIT, ANIM_PACKUNFLOWER_WAIT);
}

static void hammerhead_blown_away(Actor* a, Actor* by){
    if(a->state != HAMMERHEAD_KILL) actor_set_state(a, HAMMERHEAD_KILL, ANIM_PACKUNFLOWER_BLOWDOWN);
}
