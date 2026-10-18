static bool packun_petit_swoon_think(Actor* a){
    if(a->state != PACKUN_PETIT_SWOON_START && a->state != PACKUN_PETIT_SWOON &&
       a->state != PACKUN_PETIT_SWOON_TO_THREAT)
        return false;
    const FigureHeader* fh = (const FigureHeader*)figure_packunpetit;
    if(a->state == PACKUN_PETIT_SWOON_START && a->timer >= anim_length_of(fh, ANIM_PACKUNPETIT_SWOONSTART))
        actor_set_state_anim(a, PACKUN_PETIT_SWOON, ANIM_PACKUNPETIT_SWOON);
    else if(a->state == PACKUN_PETIT_SWOON && a->timer >= 250)
        actor_set_state_anim(a, PACKUN_PETIT_SWOON_TO_THREAT, ANIM_PACKUNPETIT_SWOONTOTHREAT);
    else if(a->state == PACKUN_PETIT_SWOON_TO_THREAT &&
            a->timer >= anim_length_of(fh, ANIM_PACKUNPETIT_SWOONTOTHREAT))
        actor_set_state_anim(a, PACKUN_PETIT_THREAT, ANIM_PACKUNPETIT_THREAT);

    const Player* p = &g_game.player;
    bool attack = g_mario.attack_timer > 0 || p->action == MA_HIPDROP ||
                  (g_mario.jumping && vertical_speed(g_mario.jump_vec, p->up) < 0);
    if(attack && actor_touches_mario(a, 0)){
        a->state = PACKUN_PETIT_THREAT;
        return false;
    }
    a->timer++;
    a->anim_frame += FX_ONE;
    return true;
}

static void packun_petit_think(Actor* a){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    v3 to_mario = v3_sub(p->figure.position, a->position);
    uint64_t d2 = v3_length_squared64(to_mario);
    const FigureHeader* fh = (const FigureHeader*)figure_packunpetit;
    if(packun_petit_swoon_think(a)) return;
#define NEAR(r) (d2 < (uint64_t)FX(r) * FX(r))
    if(a->state == PACKUN_PETIT_PRESS || a->state == PACKUN_PETIT_BLOW){
        if(a->timer >= anim_length_of(fh, a->anim)){
            a->active = 0;
        }
        return;
    }
    switch(a->state){
    case PACKUN_PETIT_WAIT:
        if(NEAR(1700)) actor_set_state_anim(a, PACKUN_PETIT_FIND, ANIM_PACKUNPETIT_SUSPECT);
        break;
    case PACKUN_PETIT_FIND:
        if(a->timer >= anim_length_of(fh, ANIM_PACKUNPETIT_SUSPECT))
            actor_set_state_anim(a, PACKUN_PETIT_THREAT, ANIM_PACKUNPETIT_THREAT);
        break;
    case PACKUN_PETIT_THREAT: {
        v3 want = v3_tangent14(to_mario, a->up, a->front);
        fx ang = angle_between14(a->front, want);
        fx step = FXC(0.0349f);
        if(ang <= step) a->front = want;
        else {
            fx sign = v3_dot14(v3_cross14(a->front, want), a->up) >= 0 ? 1 : -1;
            a->front = v3_tangent14(v3_rotate_axis14(a->front, a->up, step * sign), a->up, a->front);
        }
        if(a->timer > 50 && NEAR(1100) && ang < FXC(0.1745f))
            actor_set_state_anim(a, PACKUN_PETIT_ATTACK, ANIM_PACKUNPETIT_ATTACK);
        else if(!NEAR(2000)) actor_set_state_anim(a, PACKUN_PETIT_WAIT, ANIM_PACKUNPETIT_WAIT);
        break;
    }
    case PACKUN_PETIT_ATTACK:
        if(a->timer >= anim_length_of(fh, ANIM_PACKUNPETIT_ATTACK))
            actor_set_state_anim(a, NEAR(1700) ? PACKUN_PETIT_THREAT : PACKUN_PETIT_WAIT,
                                 NEAR(1700) ? ANIM_PACKUNPETIT_THREAT : ANIM_PACKUNPETIT_WAIT);
        break;
    case PACKUN_PETIT_HIT:
        if(a->timer >= anim_length_of(fh, ANIM_PACKUNPETIT_HIT))
            actor_set_state_anim(a, PACKUN_PETIT_THREAT, ANIM_PACKUNPETIT_THREAT);
        break;
    }
#undef NEAR

    bool body = actor_touches_mario(a, 0);
    if(mario_spin_reaches(a)){
        actor_set_state_anim(a, PACKUN_PETIT_BLOW, ANIM_PACKUNPETIT_BLOW);
        for(int k = 0; k < 3; k++){
            Actor* bit = actor_spawn(OBJ_STARBIT, v3_add(actor_centre(a), v3_scale14(a->up, FX(40 * k))),
                                     a->up, a->front);
            if(bit){
                bit->flags |= 2;
                bit->figure.model = (const FigureHeader*)starbit_figures[(actor_index(a) + k) % 6];
            }
        }
        mario_spin_hop();
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
        return;
    }
    bool from_above = m->jumping && vertical_speed(m->jump_vec, p->up) < 0 &&
                      v3_dot14(v3_sub(p->figure.position, a->position), a->up) > FX(150);
    if(body && (p->action == MA_HIPDROP || from_above)){
        if(p->action != MA_HIPDROP) mario_stomp_bounce(g_game.keys.held & KEY_A);
        actor_set_state_anim(a, PACKUN_PETIT_PRESS, ANIM_PACKUNPETIT_PRESS);
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
        return;
    }
    if(body){
        mario_damage(a->position, 1);
        if(a->state == PACKUN_PETIT_ATTACK) actor_set_state_anim(a, PACKUN_PETIT_HIT, ANIM_PACKUNPETIT_HIT);
    }
}

static bool packun_petit_view_bounds(const Figure* f, const CameraView* view){
    return view_sphere_visible(view, f->position, FX(350) * f->scale.x / 64);
}

static const FigureOps packun_petit_figure_ops = {.view_bounds = packun_petit_view_bounds};

static void packun_petit_star_hit(Actor* a){
    if(a->state != PACKUN_PETIT_PRESS && a->state != PACKUN_PETIT_BLOW &&
       a->state != PACKUN_PETIT_SWOON_START && a->state != PACKUN_PETIT_SWOON)
        actor_set_state_anim(a, PACKUN_PETIT_SWOON_START, ANIM_PACKUNPETIT_SWOONSTART);
    audio_play_effect(SFX_ENEMY_HIT, 2);
}

static void packun_petit_spawned(Actor* a, const StageObject* o){
    actor_set_state_anim(a, PACKUN_PETIT_WAIT, ANIM_PACKUNPETIT_WAIT);
}
