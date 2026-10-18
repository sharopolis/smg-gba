static EWRAM_BSS MarioFireState g_fire;

static void mario_fire_reset(void){ g_fire = (MarioFireState){0}; }

static void mario_fire_impact(int32_t up_speed){
    MarioPhysics* m = &g_mario;
    Player* p = &g_game.player;
    g_fire.vertical = up_speed;
    if(!g_fire.wall){
        int32_t speed = v3_length_exact(g_fire.horizontal);
        if(speed > G(20)) g_fire.horizontal = v3_set_length(g_fire.horizontal, G(20), p->front);
        g_fire.horizontal = v3_scale14(g_fire.horizontal, R14(.5f));
    }
    mario_take_off(JUMP_DAMAGE, v3_add(g_fire.horizontal, v3_scale14(p->up, up_speed)), true);
    m->spin_timer = m->attack_timer = 0;
    mario_set_action(MA_FIRE_DANCE);
    mario_restart_anim(ANIM_MARIO_FIRERUN);
    p->anim_speed = 6 * FX_ONE;
}

static bool mario_fire_hit(v3 normal, bool wall){
    Player* p = &g_game.player;
    if(mario_freeze_blocks_damage() || g_fire.phase || g_fire.after || p->invincible_timer ||
       g_mario_star_power || g_mario_held || (g_debug_options & DEBUG_NO_DAMAGE) || mario_power_mode() == 3 ||
       p->action == MA_TALK || p->action == MA_STAR_LAUNCH || p->action == MA_STAR_GET ||
       p->action == MA_DEATH)
        return false;
    bee_end();
    mario_power_end();
    g_fire = (MarioFireState){.horizontal = horizontal(g_mario.jump_vec, p->up), .after = 120, .phase = 1};
    int32_t speed = G(40);

    if(p->action == MA_WALL_SLIDE){
        speed = G(10);
        g_fire.wall = 1;
        g_fire.lock = 60;
        g_fire.horizontal = v3_scale14(normal, G(20));
        p->front = v3_tangent14(normal, p->up, p->front);
    }
    {
        int previous_health = p->health;
        p->health = fx_max(0, p->health - 1);
        mario_lifeup_health_changed(previous_health);
    }
    actors_player_damaged();
    mario_fire_impact(speed);
    audio_play_effect(SFX_DAMAGE, 4);
    return true;
}

static void mario_fire_close(void){
    Player* p = &g_game.player;
    g_fire.phase = 0;
    if(p->health <= 0){
        mario_set_action(MA_DEATH);
        mario_restart_anim(ANIM_MARIO_DIEOVER);
        audio_play_effect(SFX_DIE, 5);
    } else {
        mario_set_action(g_mario.jumping ? MA_FALL : MA_WAIT);
        mario_restart_anim(g_mario.jumping ? ANIM_MARIO_FALL : ANIM_MARIO_WAIT);
    }
}

static void mario_fire_run_start(void){
    Player* p = &g_game.player;

    g_fire.phase = 2;
    g_fire.timer = 1;
    g_fire.vertical = p->on_ground ? 0 : G(50);
    mario_set_action(MA_FIRE_RUN);
    mario_restart_anim(ANIM_MARIO_FIRERUNSTART);
}

static void mario_fire_move(v3 displacement, bool ground){
    Player* p = &g_game.player;
    v3 old = p->figure.position;
    mario_move_and_collide(displacement, ground);
    v3 delta = v3_sub(p->figure.position, old);
    p->velocity = v3_sub(V3(delta.x << 8, delta.y << 8, delta.z << 8), displacement);
}

static v3 mario_fire_run_air(void){
    Player* p = &g_game.player;
    v3 displacement = v3_scale14(p->up, g_fire.vertical);
    g_fire.vertical = fx_max(g_fire.vertical - G(5), -G(50));
    if(v3_dot14(p->velocity, p->up) > G(10)){
        g_fire.vertical = 0;
        displacement = v3_add(displacement, v3_scale14(p->front, G(5)));
    }
    return displacement;
}

static bool mario_fire_tick(const KeyState* keys){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    if(!g_fire.phase){
        if(g_fire.after) g_fire.after--;
        return false;
    }

    if(p->health <= 0){
        mario_fire_close();
        return true;
    }
    v3 displacement;
    if(g_fire.phase == 1){
        if(p->on_ground && g_fire.vertical <= 0){
            if(p->ground_attr & COL_BURN){
                {
                    int previous_health = p->health;
                    p->health = fx_max(0, p->health - 1);
                    mario_lifeup_health_changed(previous_health);
                }
                mario_fire_impact(G(40));
                audio_play_effect(SFX_DAMAGE, 4);
            } else if(g_fire.bounce == 1){
                mario_fire_run_start();
            } else {
                g_fire.bounce++;
                mario_fire_impact(G(20));
            }
        }
        if(g_fire.phase == 1){
            int32_t vertical = g_fire.vertical;
            g_fire.vertical -= g_fire.vertical > 0 ? G(1) : G(.75f);
            if(g_fire.vertical < -G(50)) g_fire.vertical = -G(50);
            if(m->stick){
                if(g_fire.lock) g_fire.lock--;
                else {
                    p->front = v3_tangent14(v3_blend14(p->front, m->move_dir, R14(.2f)), p->up, p->front);
                    g_fire.horizontal = v3_add(g_fire.horizontal, v3_scale14(p->front, G(.5f)));
                }
            }
            g_fire.horizontal = horizontal(g_fire.horizontal, p->up);
            if(!g_fire.wall && v3_length_squared64(g_fire.horizontal) > (uint64_t)(G(10) * G(10)))
                g_fire.horizontal = v3_set_length(g_fire.horizontal, G(10), p->front);
            displacement = v3_add(g_fire.horizontal, v3_scale14(p->up, vertical));
            m->jump_vec = v3_scale14(p->up, g_fire.vertical);
            mario_fire_move(displacement, false);
            return true;
        }
    }
    displacement = V3(0, 0, 0);
    if(g_fire.phase == 2){
        if(!p->on_ground){
            displacement = mario_fire_run_air();
        } else if(g_fire.timer) g_fire.timer--;
        else {
            g_fire.phase = 3;
            g_fire.timer = p->health > 0 ? 30 : 15;
            g_fire.vertical = 0;
            mario_restart_anim(ANIM_MARIO_FIRERUN);
            p->anim_speed = 8 * FX_ONE;
        }
    } else {
        if(g_fire.phase == 3 && !p->on_ground){
            g_fire.phase = 4;
            g_fire.timer += 15;
        }
        if(g_fire.phase == 4 && m->stick && p->health > 0){
            mario_fire_close();
            return false;
        }
        if(m->stick) p->front = v3_tangent14(v3_blend14(p->front, m->move_dir, R14(.1f)), p->up, p->front);
        int32_t speed = G(16);
        if(g_fire.phase == 4){
            if(!p->on_ground){
                displacement = mario_fire_run_air();
                speed = 0;
            } else if(g_fire.timer <= 15) speed = speed * g_fire.timer / 15;
        }
        displacement = v3_add(displacement, v3_scale14(p->front, speed));
        if(g_fire.timer) g_fire.timer--;
        if(!g_fire.timer){
            if(g_fire.phase == 3){
                g_fire.phase = 4;
                g_fire.timer = 15;
            } else {
                mario_fire_close();
                return false;
            }
        }
        if((keys->pressed & KEY_A) && p->health > 0){
            m->ratio = N14_ONE;
            mario_fire_close();
            mario_jump();
            return false;
        }
    }
    m->jump_vec = v3_scale14(p->up, g_fire.vertical);
    mario_fire_move(displacement, p->on_ground);
    return true;
}
