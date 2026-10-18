static void mario_fireball_sound(int cue){
    if(cue >= 0) audio_play_effect(cue, 2);
}
#define FIREBALL_SOUND_SHOOT SFX_PLAYER_FIRE_SHOOT
#define FIREBALL_SOUND_BOUND SFX_MARIO_FIRE_BOUND
#define FIREBALL_SOUND_BREAK SFX_MARIO_FIRE_BREAK
static void mario_fireball_kill(Actor* a){
    a->active = 0;
    mario_fireball_sound(FIREBALL_SOUND_BREAK);
}
static bool mario_fireball_animation_active(void){
    const Player* p = &g_game.player;
    return mario_power_mode() == 2 && (p->anim == ANIM_MARIO_FIRESPIN || p->anim == ANIM_MARIO_THROW) &&
           !mario_anim_done();
}
static bool mario_fireball_spin(bool carry_blocked){
    Player* p = &g_game.player;
    if(mario_power_mode() != 2 || p->action == MA_SWIM || carry_blocked) return false;
    if((p->anim == ANIM_MARIO_FIRESPIN || p->anim == ANIM_MARIO_THROW) && !mario_anim_done()) return true;
    int live = 0;
    for(int i = 0; i < g_game.actor_count; i++)
        live += g_game.actors[i].active && g_game.actors[i].type == OBJ_MARIOFIREBALL;
    if(live >= 3) return true;
    v3 pos = v3_add(mario_fire_hand_position(), v3_scale14(p->front, 30));
    v3 dir = v3_direction14(v3_add(p->front, p->up), p->front);
    Actor* a = actor_spawn_reuse(OBJ_MARIOFIREBALL, pos, p->up, p->front);
    if(!a) return true;
    a->velocity = v3_scale14(dir, V16(30));
    a->value = 0;
    a->home = V3(0, 0, 0);
    a->target = V3(0, 0, 0);
    a->args[0] = -1;
    a->args[1] = 0;
    actor_set_state(a, AS_WAIT, -1);
    if(!g_mario.jumping){
        g_mario.no_control_timer = 45;
        mario_restart_anim(ANIM_MARIO_FIRESPIN);
    } else {
        if(p->spin_air_count >= 3){
            mario_restart_anim(ANIM_MARIO_THROW);
            return true;
        }
        bool first = p->spin_air_count == 0;
        mario_spin_hop();
        p->spin_air_count++;
        int32_t vertical = v3_dot14(g_mario.jump_vec, p->up);
        g_mario.jump_vec =
            v3_add(v3_scale14(v3_reject14(g_mario.jump_vec, p->up), R14(.5f)), v3_scale14(p->up, vertical));
        mario_restart_anim(first ? ANIM_MARIO_FIRESPIN : ANIM_MARIO_THROW);
    }
    return true;
}
static bool mario_fireball_bind(Actor* a, RayHit* hit){
    v3 motion;
    for(int axis = 0; axis < 3; axis++){
        int32_t fraction = (&a->velocity.x)[axis] + (&a->home.x)[axis];
        (&motion.x)[axis] = fraction >> 8;
        (&a->home.x)[axis] = fraction & 255;
    }
    v3 old = a->position, next = v3_add(old, motion);
    CollisionCandidates candidates;
    collision_gather_around(old, next, 52, &candidates);
    bool found = false;
    int depth = -1;
    SphereContact best = {0};
    for(int i = 0; i < candidates.count; i++){
        SphereContact c;
        if(collision_sphere_triangle(candidates.index[i], next, 50, &c) && c.depth > depth){
            found = true;
            depth = c.depth;
            best = c;
        }
    }
    a->position = found ? v3_add(next, v3_scale14(best.normal, depth + 1)) : next;
    if(found){
        hit->point = best.point;
        hit->normal = best.normal;
        hit->tri = best.tri;
        hit->attr = best.attr;
        hit->t = 0;
    }
    return found;
}
__attribute__((noinline)) static void mario_fireball_think(Actor* a){
    int step = a->timer - 1;
    a->up = gravity_sample(g_game.stage.header, a->position).up;
    if(step == 0) mario_fireball_sound(FIREBALL_SOUND_SHOOT);

    int bound = a->args[1];
    if(bound == 1 || bound == 2){
        int speed = v3_dot14(a->velocity, a->target);
        a->velocity = v3_sub(a->velocity, v3_scale14(a->target, mul14(speed, R14(1.9f))));
        mario_fireball_sound(FIREBALL_SOUND_BOUND);
    } else a->velocity = v3_sub(a->velocity, v3_scale14(a->up, V16(2)));
    if(bound){
        if(a->value){
            if(--a->value == 0 && bound == 3){
                mario_fireball_kill(a);
                return;
            }
        } else a->value = 3;
    } else a->value = 0;
    for(int i = 0; i < g_game.actor_count; i++)
        if(candlestand_receive_fire(&g_game.actors[i], a)){
            mario_fireball_kill(a);
            return;
        }
    WaterSample water;
    if(step >= 300 ||
       v3_length_squared64(v3_sub(a->position, g_game.player.figure.position)) > 3000u * 3000u ||
       water_query(a->position, a->up, &water)){
        mario_fireball_kill(a);
        return;
    }
    RayHit hit;
    if(mario_fireball_bind(a, &hit)){
        a->target = hit.normal;
        a->args[0] = (int16_t)hit.tri;
        int height = v3_dot14(hit.normal, a->up);
        a->args[1] = height > R14(.5f) ? 1 : height < -R14(.5f) ? 2 : 3;
    } else {
        a->args[0] = -1;
        a->args[1] = 0;
    }
}
