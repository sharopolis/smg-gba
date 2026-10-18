typedef struct {
    int32_t vertical;
    uint16_t timer, after;
    uint8_t phase, frozen, part, part_frame;
} MarioFreezeState;
static EWRAM_BSS MarioFreezeState g_freeze;
static void mario_freeze_reset(void){ g_freeze = (MarioFreezeState){0}; }
static void mario_freeze_hide_part(void){
    if(!g_freeze.frozen) return;
    g_freeze.frozen = 0;
    g_freeze.part = 2;
    g_freeze.part_frame = 0;
}
static void mario_freeze_close(void){
    Player* p = &g_game.player;
    if(g_freeze.frozen) audio_play_effect(SFX_FREEZE_END, 4);
    mario_freeze_hide_part();
    g_freeze.phase = 0;
    g_freeze.after = 120;
    mario_set_action(p->health <= 0 ? MA_DEATH : (p->on_ground ? MA_WAIT : MA_FALL));
    mario_restart_anim(p->health <= 0 ? ANIM_MARIO_DIEOVER
                                      : (p->on_ground ? ANIM_MARIO_WAIT : ANIM_MARIO_FALL));
}

static bool mario_freeze_blocks_damage(void){ return g_freeze.phase || g_freeze.after; }
static bool mario_freeze_hit(void){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    if(g_freeze.phase || g_freeze.after || p->invincible_timer || g_mario_star_power || g_mario_held ||
       (g_debug_options & DEBUG_NO_DAMAGE) || p->action == MA_DEATH || p->action == MA_STAR_LAUNCH ||
       p->action == MA_STAR_GET || p->action == MA_TALK)
        return false;

    if(g_fire.phase) mario_fire_close();
    bee_end();
    mario_power_end();
    mario_swim_reset();
    mario_skate_reset();
    mario_stop_walk();
    m->jump_vec = V3(0, 0, 0);
    m->jumping = false;
    m->squat = false;
    m->pulling = false;
    m->spin_timer = m->attack_timer = 0;
    m->stand_turn = m->turn_yaw = 0;
    g_freeze = (MarioFreezeState){.timer = 180, .phase = 1, .frozen = 1, .part = 1};
    {
        int previous_health = p->health;
        p->health = fx_max(0, p->health - 1);
        mario_lifeup_health_changed(previous_health);
    }
    mario_set_action(MA_FREEZE);
    mario_restart_anim(ANIM_MARIO_FREEZE);
    audio_play_effect(SFX_FREEZE_BEGIN, 4);
    return true;
}

static void mario_freeze_tick_passive(void){
    if(g_freeze.part == 2 && ++g_freeze.part_frame >= 49) g_freeze.part = 0;
    if(g_freeze.after) g_freeze.after--;
}
static bool mario_freeze_tick(const KeyState* keys){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    if(!g_freeze.phase) return false;

    if(!p->health && water_query(p->figure.position, p->gravity_up, 0) &&
       water_query(v3_add(p->figure.position, v3_scale14(p->gravity_up, 110)), p->gravity_up, 0)){
        p->water_state = WATER_SUBMERGED;
        mario_freeze_close();
        return true;
    }
    v3 displacement = V3(0, 0, 0);
    if(g_freeze.timer){
        g_freeze.timer--;
        if(g_freeze.phase == 2){
            if(p->on_ground) displacement = v3_scale14(p->front, -G(1));
        } else if(g_freeze.timer < 120 && p->health && (keys->pressed & KEY_B)){
            mario_freeze_hide_part();
            mario_freeze_close();
            audio_play_effect(SFX_FREEZE_SPIN_ESCAPE, 4);
            mario_set_action(MA_SPIN);
            mario_restart_anim(ANIM_MARIO_SPINGROUND);
            m->ground_spin_timer = C_SPIN_TIME;
            m->attack_timer = 15;
            mario_move_and_collide(v3_scale14(p->front, -G(10)), p->on_ground);
            return true;
        }
    }
    if(!g_freeze.timer){
        if(g_freeze.phase == 2){
            if(p->health){
                int cue = g_freeze.frozen ? SFX_FREEZE_AIR_RECOVER : SFX_FREEZE_RECOVER;
                mario_freeze_hide_part();
                mario_freeze_close();
                audio_play_effect(cue, 4);
                return true;
            }
        } else {
            g_freeze.phase = 2;
            g_freeze.timer = p->on_ground ? 20 : 10;
            if(!p->health){
                mario_freeze_close();
                return true;
            }
            if(p->on_ground){
                mario_restart_anim(ANIM_MARIO_ICEFLICK);
                audio_play_effect(SFX_FREEZE_END, 4);
                mario_freeze_hide_part();
            }
        }
    }
    if(g_freeze.timer < 150){
        if(!p->on_ground || g_freeze.vertical < 0){
            m->jump_vec = v3_scale14(p->up, -g_freeze.vertical);
            displacement = v3_add(displacement, m->jump_vec);
            g_freeze.vertical += G(1.5f);
            if(g_freeze.vertical < 0){
                p->on_ground = false;
                m->jumping = true;
            }
        } else if(g_freeze.vertical > 0){
            if(g_freeze.vertical < G(8)) g_freeze.vertical = 0;
            else {
                g_freeze.vertical = mul14(g_freeze.vertical, -R14(.4f));
                p->on_ground = false;
                m->jumping = true;
                m->jump_vec = v3_scale14(p->up, -g_freeze.vertical);
            }
        }
    }
    mario_move_and_collide(displacement, p->on_ground);
    return true;
}

static void mario_freeze_add_figure(Figure* f){
    if(!g_freeze.part || g_game.player.figure.hidden || g_game.figure_count >= MAX_FIGURES) return;
    Player* p = &g_game.player;
    figure_init(f, g_freeze.part == 1 ? figure_mariofreezeice : figure_mariofreezeicebreak);
    if(g_freeze.part == 2)
        f->pose = figure_clip_pose(f->model, ANIM_MARIOFREEZEICEBREAK_BREAK, g_freeze.part_frame << 8);
    f->position = p->figure.position;
    f->scale = V3(64, 64, 64);
    f->use_basis = true;
    f->basis_up = p->up;
    f->basis_front = p->front;
    figure_update_transform(f);
    g_game.figures[g_game.figure_count++] = f;
}
