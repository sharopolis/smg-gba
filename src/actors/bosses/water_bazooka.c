enum {
    MOGUCCHI_SHOOTER_WAIT = 64,
    MOGUCCHI_SHOOTER_SHOT,
    MOGUCCHI_SHOOTER_LAUGH,
    MOGUCCHI_SHOOTER_SHOCK,
    MOGUCCHI_SHOOTER_PANIC,
    MOGUCCHI_SHOOTER_DOWN,
    MOGUCCHI_SHOOTER_GONE
};

#define WATER_BAZOOKA_CANNON_Y      370
#define WATER_BAZOOKA_COCKPIT_Y     412
#define WATER_BAZOOKA_TOP_Y         492
#define WATER_BAZOOKA_DAMAGE_FRAMES 90

static v3 water_bazooka_point(const Actor* a, int y){ return v3_add(a->home, v3_scale14(a->up, y)); }

static bool water_bazooka_in_demo(const Actor* a){
    return a->state >= WATER_BAZOOKA_CRACK && a->state <= WATER_BAZOOKA_BREAKEND;
}
static bool water_bazooka_mario_on_top(const Actor* a){
    const Player* p = &g_game.player;
    if(!p->on_ground) return false;
    v3 d = v3_sub(p->figure.position, a->home);
    int32_t h = v3_dot14(d, a->up);
    return h > 300 && v3_length_squared64(v3_reject14(d, a->up)) < (uint64_t)340 * 340;
}

static bool water_bazooka_aim(Actor* a){
    v3 gun = water_bazooka_point(a, WATER_BAZOOKA_CANNON_Y);
    v3 aim =
        v3_direction14(v3_sub(v3_add(g_game.player.figure.position, v3_scale14(a->up, 100)), gun), a->target);
    v3 side = a->target;
    int32_t c = v3_dot14(side, aim);
    if(c < R14(.99978f)){
        v3 axis = v3_direction14(v3_cross14(side, aim), a->up);
        side = v3_rotate_axis14(side, axis, DEG(1.2f));
    } else side = aim;

    int32_t s = v3_dot14(side, a->up);
    if(s > 0) s = 0;
    if(s < -R14(.2588f)) s = -R14(.2588f);
    v3 flat = v3_direction14(v3_reject14(side, a->up), a->front);
    side = v3_direction14(
        v3_add(v3_scale14(flat, isqrt64((uint64_t)((int64_t)N14_ONE * N14_ONE - (int64_t)s * s))),
               v3_scale14(a->up, s)),
        flat);
    a->target = side;
    return v3_dot14(side, aim) >= R14(.99939f);
}
static void water_bazooka_shoot(Actor* a){
    v3 gun = v3_add(water_bazooka_point(a, WATER_BAZOOKA_CANNON_Y), v3_scale14(a->target, 550));
    Actor* b = actor_spawn(OBJ_WATERBULLET, gun, a->up, a->target);
    if(!b) return;
    b->velocity = v3_scale14(a->target, V16(40));
    b->value = 1;
    b->state = 64;
    b->timer = 0;
    b->target = a->up;
    audio_play_effect(SFX_WATERBAZ_SHOT, 3);
}
static void water_bazooka_throw_off_mario(Actor* a){
    if(!water_bazooka_mario_on_top(a)) return;

    const Stage* st = &g_game.stage;
    Player* p = &g_game.player;
    v3 best = V3(0, 0, 0);
    uint64_t bd = UINT64_MAX;
    for(uint32_t i = 0; i < st->header->object_count; i++){
        const StageObject* o = &st->objects[i];
        if(o->type != OBJ_GENERALPOS || o->id < 778 || o->id > 780) continue;
        v3 at = V3(o->position[0], o->position[1], o->position[2]);
        uint64_t d = v3_length_squared64(v3_sub(at, p->figure.position));
        if(d < bd){
            bd = d;
            best = at;
        }
    }
    if(bd == UINT64_MAX)
        best = v3_add(
            a->home,
            v3_scale14(v3_direction14(v3_reject14(v3_sub(p->figure.position, a->home), a->up), a->front),
                       800));

    v3 flat = v3_reject14(v3_sub(best, p->figure.position), a->up);
    int32_t dist = v3_length_exact(flat);
    v3 v = v3_add(v3_scale14(v3_direction14(flat, a->front), V16(20)), v3_scale14(a->up, V16(10) + dist * 4));
    mario_take_off(JUMP_FORCED, v, true);
    mario_set_action(MA_FALL);
    mario_restart_anim(ANIM_MARIO_FALL);
}
static void water_bazooka_damage(Actor* a){
    if(a->health == 3){
        actor_set_state(a, WATER_BAZOOKA_CRACK, -1);
        audio_play_effect(SFX_ENEMY_HIT, 3);
    } else if(a->health == 2){
        actor_set_state(a, WATER_BAZOOKA_ANGER, -1);
        audio_play_effect(SFX_ENEMY_HIT, 3);
    } else actor_set_state(a, WATER_BAZOOKA_BREAKWAIT, -1);
    a->health--;
    camera_shake(1);
    water_bazooka_throw_off_mario(a);
}

static bool water_bazooka_was_hit(Actor* a){
    if(water_bazooka_in_demo(a)) return false;
    v3 cannon = water_bazooka_point(a, WATER_BAZOOKA_CANNON_Y),
       capsule = water_bazooka_point(a, WATER_BAZOOKA_TOP_Y + 70);
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* c = &g_game.actors[i];
        if(!c->active || c->type != OBJ_COCONUT || c->state != COCONUT_MOVE || !(c->flags & COCONUT_SPUN))
            continue;
        if(v3_length_squared64(v3_sub(c->position, cannon)) < (uint64_t)(330 + 55) * (330 + 55) ||
           v3_length_squared64(v3_sub(c->position, capsule)) < (uint64_t)(160 + 55) * (160 + 55)){
            actor_set_state(c, COCONUT_GONE, -1);
            return true;
        }
    }
    const Player* p = &g_game.player;
    if(p->action == MA_HIPDROP_LAND && p->action_timer == 0){
        v3 d = v3_sub(p->figure.position, a->home);
        if(v3_dot14(d, a->up) > WATER_BAZOOKA_TOP_Y + 60 &&
           v3_length_squared64(v3_reject14(d, a->up)) < (uint64_t)170 * 170)
            return true;
    }
    return false;
}
static int water_bazooka_child(Actor* a, int slot, int type){
    Actor* c = actor_active_at(a->args[slot], type);
    if(c && c->value == actor_index(a)) return actor_index(c);
    c = actor_spawn(type, a->position, a->up, a->front);
    if(!c) return -1;
    c->value = (int16_t)actor_index(a);
    c->home = a->home;
    c->target = a->front;
    c->stage_index = -1;
    actor_set_state(c, MOGUCCHI_SHOOTER_WAIT, type == OBJ_MOGUCCHISHOOTER ? ANIM_MOGUCCHISHOOTER_WAIT : -1);
    a->args[slot] = (int16_t)actor_index(c);
    return actor_index(c);
}
static void water_bazooka_collision(Actor* a, bool on, bool capsule){
    int set = mappart_set(a, kcl_waterbazooka, capsule ? KCL_WATERBAZOOKA_COUNT : KCL_WATERBAZOOKA_CAPSULE);
    if(set < 0) return;
    if(on){
        MoverFrame f = mover_frame_of(a->home, a->up, a->front, N14_ONE);
        movers_place(set, &f);
    }
    movers_enable(set, on);
}
static void water_bazooka_think(Actor* a){
    Player* p = &g_game.player;
    int step = a->timer - 1;
    water_bazooka_child(a, 1, OBJ_MOGUCCHISHOOTER);
    water_bazooka_child(a, 2, OBJ_WBCAPSULE);
    bool pinch = a->health <= 1;
    if(!water_bazooka_in_demo(a) && a->state != WATER_BAZOOKA_PANIC && a->state != WATER_BAZOOKA_STORM &&
       water_bazooka_mario_on_top(a)){
        actor_set_state(a, WATER_BAZOOKA_PANIC, ANIM_WATERBAZOOKA_WAIT);
        a->args[0] = 0;
        step = -1;
    }
    if(water_bazooka_was_hit(a)){
        water_bazooka_damage(a);
        return;
    }
    const Actor* mole = actor_active_at(a->args[1], OBJ_MOGUCCHISHOOTER);
    bool laughing = mole && mole->state == MOGUCCHI_SHOOTER_LAUGH;
    switch(a->state){
    case WATER_BAZOOKA_WAIT:
        if(laughing){
            actor_set_state(a, WATER_BAZOOKA_LAUGH, -1);
            break;
        }
        if(step > 0 && p->action != MA_DEATH && !p->figure.hidden)
            actor_set_state(a, WATER_BAZOOKA_AIM, ANIM_WATERBAZOOKA_SHOTSTART);
        break;
    case WATER_BAZOOKA_AIM:
        if(water_bazooka_aim(a) || step >= 60)
            actor_set_state(a, WATER_BAZOOKA_AIMEND, ANIM_WATERBAZOOKA_WAIT);
        break;
    case WATER_BAZOOKA_AIMEND:
        if(step >= 11){
            if(pinch) actor_set_state(a, WATER_BAZOOKA_SHOTNM, ANIM_WATERBAZOOKA_SHOT);
            else {
                actor_set_state(a, WATER_BAZOOKA_SHOT, ANIM_WATERBAZOOKA_SHOT);
                a->args[0]++;
            }
        }
        break;
    case WATER_BAZOOKA_SHOT:
        if(step == 30) water_bazooka_shoot(a);
        if(step >= 77){
            if(a->args[0] >= 3){
                a->args[0] = 0;
                actor_set_state(a, WATER_BAZOOKA_TIRE, ANIM_WATERBAZOOKA_TIRE);
            } else if(pinch) actor_set_state(a, WATER_BAZOOKA_SHOTNM, ANIM_WATERBAZOOKA_SHOT);
            else actor_set_state(a, WATER_BAZOOKA_LAUGH, -1);
        }
        break;
    case WATER_BAZOOKA_SHOTNM:
        if(step == 0){
            a->args[0]++;
            water_bazooka_shoot(a);
        }
        water_bazooka_aim(a);
        if(laughing){
            actor_set_state(a, WATER_BAZOOKA_LAUGH, -1);
            break;
        }
        if(step >= 30){
            if(a->args[0] >= 5){
                a->args[0] = 0;
                actor_set_state(a, WATER_BAZOOKA_TIRE, ANIM_WATERBAZOOKA_TIRE);
            } else actor_set_state(a, WATER_BAZOOKA_SHOTNM, ANIM_WATERBAZOOKA_SHOT);
        }
        break;
    case WATER_BAZOOKA_TIRE:
        if(laughing){
            actor_set_state(a, WATER_BAZOOKA_LAUGH, -1);
            break;
        }
        if(step >= 120) actor_set_state(a, WATER_BAZOOKA_WAIT, ANIM_WATERBAZOOKA_WAIT);
        break;
    case WATER_BAZOOKA_LAUGH:
        if(laughing) a->args[0] = 0;
        else actor_set_state(a, WATER_BAZOOKA_WAIT, ANIM_WATERBAZOOKA_WAIT);
        break;
    case WATER_BAZOOKA_CRACK:
    case WATER_BAZOOKA_ANGER:
        if(step >= WATER_BAZOOKA_DAMAGE_FRAMES){
            collectibles_pop(OBJ_STARBIT, v3_add(a->home, v3_scale14(a->up, 600)), a->up,
                             a->state == WATER_BAZOOKA_CRACK ? 8 : 16);
            actor_set_state(a, WATER_BAZOOKA_WAIT, ANIM_WATERBAZOOKA_WAIT);
        }
        break;
    case WATER_BAZOOKA_BREAKWAIT:
        if(step >= 5){
            actor_set_state(a, WATER_BAZOOKA_BREAKSIGN, ANIM_WATERBAZOOKA_DOWN);
            water_bazooka_throw_off_mario(a);
        }
        break;
    case WATER_BAZOOKA_BREAKSIGN:
        if(step >= 230){
            actor_set_state(a, WATER_BAZOOKA_EXPLODE, -1);
            a->flags |= ACTOR_HIDDEN;
            audio_play_effect(SFX_WATERBAZ_EXPLODE, 4);
            camera_shake(2);
            Actor* cap = actor_active_at(a->args[2], OBJ_WBCAPSULE);
            if(cap) cap->flags |= ACTOR_HIDDEN;
        }
        break;
    case WATER_BAZOOKA_EXPLODE:
        if(step >= 60) actor_set_state(a, WATER_BAZOOKA_BREAKEND, -1);
        break;
    case WATER_BAZOOKA_BREAKEND:
        if(step >= 60){
            Actor* c = actor_active_at(a->args[1], OBJ_MOGUCCHISHOOTER);
            if(c) c->active = 0;
            c = actor_active_at(a->args[2], OBJ_WBCAPSULE);
            if(c) c->active = 0;
            water_bazooka_collision(a, false, false);
            if(actor_has_switch(a, SWITCH_DEAD)) switch_set(actor_switch_key(a, SWITCH_DEAD), true);
            a->active = 0;
            return;
        }
        break;
    case WATER_BAZOOKA_PANIC:
        water_bazooka_aim(a);
        if(!water_bazooka_mario_on_top(a) && p->on_ground){
            actor_set_state(a, WATER_BAZOOKA_WAIT, ANIM_WATERBAZOOKA_WAIT);
            break;
        }
        if(step >= 300){
            actor_set_state(a, WATER_BAZOOKA_STORM, -1);
            a->velocity.x = 0;
        }
        break;
    case WATER_BAZOOKA_STORM: {
        int32_t t = fx_min(step, 150) * N14_ONE / 150;
        fx deg = DEG(25) - mul14(DEG(25), fx_sin(mul14(t, FX_PI / 2)) << 6);
        a->target = v3_rotate_axis14(a->target, a->up, deg);
        if(water_bazooka_mario_on_top(a) && p->action != MA_DEATH) mario_damage(a->home, 1);
        if(step >= 150) actor_set_state(a, WATER_BAZOOKA_WAIT, ANIM_WATERBAZOOKA_WAIT);
        break;
    }
    }

    if(a->state < WATER_BAZOOKA_EXPLODE) water_bazooka_collision(a, true, !water_bazooka_in_demo(a));
    else water_bazooka_collision(a, false, false);
    a->front = v3_direction14(v3_reject14(a->target, a->up), a->front);
}
static void mogucchi_shooter_think(Actor* m){
    Actor* a = actor_active_at(m->value, OBJ_WATERBAZOOKA);
    if(!a){
        m->active = 0;
        return;
    }
    int step = m->timer - 1;
    m->position = water_bazooka_point(a, WATER_BAZOOKA_COCKPIT_Y);
    m->up = a->up;
    bool bound = g_mario_link.bind >= 0;
    bool panic = a->state == WATER_BAZOOKA_PANIC || a->state == WATER_BAZOOKA_STORM;

    if(m->state != MOGUCCHI_SHOOTER_DOWN){
        actor_turn_toward(m, actor_to_mario(m), DEG(1));
        v3 f = a->front;
        if(v3_dot14(m->front, f) < R14(.866f)){
            v3 side = v3_cross14(a->up, f);
            int s = v3_dot14(m->front, side) >= 0 ? 1 : -1;
            m->front = v3_add(v3_scale14(f, R14(.866f)), v3_scale14(side, s * R14(.5f)));
        }
    }
    if(a->state >= WATER_BAZOOKA_BREAKSIGN){
        if(m->state != MOGUCCHI_SHOOTER_DOWN && m->state != MOGUCCHI_SHOOTER_PANIC &&
           a->state == WATER_BAZOOKA_BREAKSIGN)
            actor_set_state(m, MOGUCCHI_SHOOTER_PANIC, ANIM_MOGUCCHISHOOTER_PANIC);
        if(a->state == WATER_BAZOOKA_EXPLODE && m->state != MOGUCCHI_SHOOTER_DOWN)
            actor_set_state(m, MOGUCCHI_SHOOTER_DOWN, ANIM_MOGUCCHISHOOTER_DOWN);
        if(m->state == MOGUCCHI_SHOOTER_DOWN && step >= 48) m->flags |= ACTOR_HIDDEN;
        return;
    }
    switch(m->state){
    case MOGUCCHI_SHOOTER_WAIT:
        if(bound) actor_set_state(m, MOGUCCHI_SHOOTER_LAUGH, ANIM_MOGUCCHISHOOTER_LAUGH);
        else if(panic) actor_set_state(m, MOGUCCHI_SHOOTER_SHOCK, ANIM_MOGUCCHISHOOTER_SHOCK);
        else if(a->state == WATER_BAZOOKA_SHOT && a->timer == 1)
            actor_set_state(m, MOGUCCHI_SHOOTER_SHOT, ANIM_MOGUCCHISHOOTER_SHOT);
        else if(a->state == WATER_BAZOOKA_CRACK && a->timer <= 1)
            actor_set_state(m, MOGUCCHI_SHOOTER_SHOCK, ANIM_MOGUCCHISHOOTER_SHOCK);
        break;
    case MOGUCCHI_SHOOTER_SHOT:
        if(bound) actor_set_state(m, MOGUCCHI_SHOOTER_LAUGH, ANIM_MOGUCCHISHOOTER_LAUGH);
        else if(step >= 65) actor_set_state(m, MOGUCCHI_SHOOTER_WAIT, ANIM_MOGUCCHISHOOTER_WAIT);
        break;
    case MOGUCCHI_SHOOTER_LAUGH:
        if(step >= 120) actor_set_state(m, MOGUCCHI_SHOOTER_WAIT, ANIM_MOGUCCHISHOOTER_WAIT);
        break;
    case MOGUCCHI_SHOOTER_SHOCK:
        if(step >= 19) actor_set_state(m, MOGUCCHI_SHOOTER_PANIC, ANIM_MOGUCCHISHOOTER_PANIC);
        break;
    case MOGUCCHI_SHOOTER_PANIC:
        if(!panic && !water_bazooka_in_demo(a) && g_game.player.on_ground)
            actor_set_state(m, MOGUCCHI_SHOOTER_WAIT, ANIM_MOGUCCHISHOOTER_WAIT);
        break;
    default: break;
    }
}
static void water_bazooka_capsule_think(Actor* c){
    Actor* a = actor_active_at(c->value, OBJ_WATERBAZOOKA);
    if(!a){
        c->active = 0;
        return;
    }
    c->position = water_bazooka_point(a, WATER_BAZOOKA_TOP_Y);
    c->up = a->up;
    c->front = a->front;
}
