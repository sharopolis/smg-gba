enum {
    KAMECK_TURTLE_RUN = 64,
    KAMECK_TURTLE_WAIT,
    JET_TURTLE_RESERVE,
    JET_TURTLE_TAKENSTART,
    JET_TURTLE_HELD,
    JET_TURTLE_THROW,
    JET_TURTLE_RESTART
};
enum {
    KAMECK_WAIT = 64,
    KAMECK_APPEAR,
    KAMECK_ATTACKWAIT,
    KAMECK_ATTACK,
    KAMECK_HIDE,
    KAMECK_MOVE,
    KAMECK_DOWN,
    KAMECK_PRESS,
    KAMECK_DEMO
};
static EWRAM_BSS KamellaState g_kamella;

static void kamella_sound(int cue){
    if(cue >= 0) audio_play_effect(cue, 2);
}
static bool kamella_attack_fire(const Actor* a){
    return mario_fire_hit(v3_direction14(actor_to_mario(a), a->front), false);
}
static bool kamella_holding_shell(void){ return g_kamella.held_shell >= 0; }
static void kamella_reset(void){
    g_kamella = (KamellaState){
        .boss = -1, .barrier = -1, .barrier_set = -1, .charged = -1, .children = {-1, -1}, .held_shell = -1};
}
static const KamellaOwner* kamella_owner(const Actor* a){
    for(unsigned i = 0; i < sizeof kamella_owners / sizeof kamella_owners[0]; i++)
        if(kamella_owners[i].object == a->stage_index && kamella_owners[i].scenario == g_game.scenario)
            return &kamella_owners[i];
    return NULL;
}
static v3 kamella_local(const Actor* a, v3 p){
    return v3_add(a->position,
                  v3_add(v3_add(v3_scale14(v3_cross14(a->up, a->front), p.x), v3_scale14(a->up, p.y)),
                         v3_scale14(a->front, p.z)));
}
static int kamella_clip(int anim){
    switch(anim){
    case ANIM_BOSSKAMECK_WAIT: return 0;
    case ANIM_BOSSKAMECK_HIDE: return 1;
    case ANIM_BOSSKAMECK_MOVE: return 2;
    case ANIM_BOSSKAMECK_APPEAR: return 3;
    case ANIM_BOSSKAMECK_ATTACKWAIT: return 4;
    case ANIM_BOSSKAMECK_ATTACK: return 5;
    case ANIM_BOSSKAMECK_DAMAGE: return 6;
    case ANIM_BOSSKAMECK_DOWN: return 7;
    case ANIM_BOSSKAMECK_GUARD: return 8;
    case ANIM_BOSSKAMECK_SUMMONWAIT: return 9;
    case ANIM_BOSSKAMECK_SUMMON: return 10;
    case ANIM_BOSSKAMECK_START2: return 11;
    case ANIM_BOSSKAMECK_POWERUP2: return 12;
    case ANIM_BOSSKAMECK_DOWN2: return 13;
    default: return 0;
    }
}

static v3 kamella_wand_sample(const KamellaClip* clip, int frame){
    const uint8_t* p = clip->wand + clip->blocks[frame >> 4];
    int16_t xyz[3];
    for(int i = 0; i < 3; i++){
        xyz[i] = (int16_t)(p[0] | p[1] << 8);
        p += 2;
    }
    for(int step = 0; step < (frame & 15); step++){
        if(*p == 128){
            p++;
            for(int i = 0; i < 3; i++){
                xyz[i] = (int16_t)(p[0] | p[1] << 8);
                p += 2;
            }
        } else
            for(int i = 0; i < 3; i++) xyz[i] += (int8_t)*p++;
    }
    return v3_from_i16(xyz);
}
static v3 kamella_wand(const Actor* a){
    const KamellaClip* c = &kamella_clips[kamella_clip(a->anim)];
    int frame = a->anim_frame >> 8;
    if(a->anim == ANIM_BOSSKAMECK_WAIT || a->anim == ANIM_BOSSKAMECK_MOVE ||
       a->anim == ANIM_BOSSKAMECK_ATTACKWAIT)
        frame %= c->duration;
    frame = fx_clamp(frame, 0, c->duration);
    return kamella_local(a, kamella_wand_sample(c, frame));
}
static bool kamella_can_take_damage(const Actor* a){
    return a->state == KAMELLA_WAIT || a->state == KAMELLA_HIDESTART || a->state == KAMELLA_HIDEEND ||
           a->state == KAMELLA_ATTACKWAIT || a->state == KAMELLA_ATTACK || a->state == KAMELLA_SUMMONWAIT ||
           a->state == KAMELLA_SUMMON;
}
static void kamella_clear_beams(const Actor* owner, bool all){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* b = &g_game.actors[i];
        if(!b->active) continue;
        if(b->type == OBJ_BOSSKAMECKBEAM && b->args[1] == actor_index(owner) + 1 &&
           (all || b->state == KAMECK_BEAM_FOLLOW))
            b->active = 0;
        if(all && (b->type == OBJ_KAMECKFIREBALL || b->type == OBJ_KAMECKTURTLE) &&
           b->args[1] == actor_index(owner) + 1)
            b->active = 0;
    }
    if(actor_index(owner) == g_kamella.boss) g_kamella.charged = -1;
}
static bool kamella_damage(Actor* a){
    if(!kamella_can_take_damage(a)) return false;
    kamella_clear_beams(a, true);
    a->velocity = V3(0, 0, 0);
    actor_set_state(a, KAMELLA_DAMAGE, g_kamella.round == 3 ? ANIM_BOSSKAMECK_DOWN : ANIM_BOSSKAMECK_DAMAGE);
    kamella_sound(g_kamella.round == 3 ? SFX_KAMELLA_LAST : SFX_KAMELLA_DAMAGE);
    if(g_kamella.round == 3) audio_stop_music();
    return true;
}
static void kamella_pick_rail_point(Actor* a, bool stoppable){
    int rail = kamella_move_rails[g_kamella.round];
    const StageRail* r = rail_get(rail);
    int choices = 0, last = a->args[2];
    for(int i = 0; i < r->point_count; i++)
        if(i != last && (!stoppable || g_game.stage.rail_points[r->first_point + i].args[0] != 0)) choices++;

    unsigned rand = g_game.tick * 1664525u + (unsigned)(actor_index(a) + a->timer) * 1013904223u;
    int pick = choices ? (int)(rand % (unsigned)choices) : 0;
    for(int i = 0; i < r->point_count; i++)
        if(i != last && (!stoppable || g_game.stage.rail_points[r->first_point + i].args[0] != 0)){
            if(pick-- == 0){
                a->args[2] = (int16_t)i;
                a->target = rail_point(rail, i);
                return;
            }
        }
    a->args[2] = 0;
    a->target = rail_point(rail, 0);
}
static void kamella_start_move(Actor* a){
    g_kamella.hops = 0;
    actor_set_state(a, KAMELLA_HIDESTART, ANIM_BOSSKAMECK_HIDE);
    kamella_sound(SFX_KAMELLA_HIDE_SPIN);
}
static void kamella_fly_to_target(Actor* a, int32_t near_speed, int32_t far_speed, int32_t friction14,
                                  int32_t height_accel){
    v3 to = v3_sub(a->target, a->position), flat = v3_reject14(to, a->up);
    int distance = v3_length_exact(flat);
    int32_t acceleration = near_speed + (far_speed - near_speed) * fx_min(distance, 400) / 400;
    if(distance) a->velocity = v3_add(a->velocity, v3_scale14(v3_direction14(flat, a->front), acceleration));
    int32_t vertical = v3_dot14(to, a->up);
    a->velocity = v3_add(a->velocity, v3_scale14(a->up, height_accel * fx_clamp(vertical, -50, 50) / 50));
    a->velocity = v3_scale14(a->velocity, friction14);
    a->position = v3_add(
        a->position, V3((a->velocity.x + 128) >> 8, (a->velocity.y + 128) >> 8, (a->velocity.z + 128) >> 8));
}
static v3 kameck_wand(const Actor* a){
    unsigned clip = (unsigned)(a->anim - ANIM_KAMECK_WAIT);
    if(clip >= sizeof kameck_clips / sizeof kameck_clips[0]) clip = 0;
    const KamellaClip* c = &kameck_clips[clip];
    int frame = a->anim_frame >> 8;
    if(a->anim == ANIM_KAMECK_WAIT || a->anim == ANIM_KAMECK_ATTACKWAIT || a->anim == ANIM_KAMECK_MOVE)
        frame %= c->duration;
    return kamella_local(a, kamella_wand_sample(c, fx_clamp(frame, 0, c->duration)));
}
static Actor* kameck_beam_charge(Actor* a, int kind){
    int count = 0;
    for(int i = 0; i < g_game.actor_count; i++)
        if(g_game.actors[i].active && g_game.actors[i].type == OBJ_BOSSKAMECKBEAM &&
           g_game.actors[i].args[1] == actor_index(a) + 1)
            count++;
    if(count >= 8) return NULL;
    Actor* b = actor_spawn_reuse(
        OBJ_BOSSKAMECKBEAM, a->type == OBJ_BOSSKAMECK2 ? kamella_wand(a) : kameck_wand(a), a->up, a->front);
    if(!b) return NULL;
    if(a->type == OBJ_KAMECK){
        b->radius = 48;
        b->figure.scale = V3(38, 38, 38);
    }
    b->args[0] = (int16_t)kind;
    b->args[1] = (int16_t)(actor_index(a) + 1);
    actor_set_state(b, KAMECK_BEAM_FOLLOW, -1);
    return b;
}
static void kameck_beam_shoot(Actor* b, bool ground, int speed){
    v3 target = g_game.player.figure.position;
    if(ground) target = v3_add(target, v3_scale14(b->up, 80));
    else target = v3_add(target, v3_scale14(g_game.player.up, C_BODY_HEIGHT));
    b->front = v3_direction14(v3_sub(target, b->position), v3_neg(b->up));
    b->target = b->front;
    b->velocity = v3_scale14(b->front, V16(speed));
    actor_set_state(b, KAMECK_BEAM_SHOOT, -1);
    kamella_sound(SFX_KAMELLA_BEAM_SHOOT);
}
static int kamella_beam_kind(void){
    static const uint8_t p[4][3] = {{2, 1, 0}, {3, 1, 0}, {4, 1, 3}, {4, 1, 3}};
    int kind = p[g_kamella.round][g_kamella.pattern];
    return kind;
}
static void kamella_barrier(Actor* a, bool on){
    Actor* b = actor_active_at(g_kamella.barrier, OBJ_BOSSKAMECKBARRIER);
    if(!b && on){
        const StageObject* o = actor_stage_object(a);
        b = actor_spawn_reuse(OBJ_BOSSKAMECKBARRIER, a->home, v3_from_i16(o->up), v3_from_i16(o->front));
        if(b){
            g_kamella.barrier = (int16_t)actor_index(b);
            b->value = -1;
        }
    }
    if(!b) return;
    if(on){
        mappart_place_kcl(b, kcl_bosskameckbarrier, KCL_BOSSKAMECKBARRIER_COUNT);
        g_kamella.barrier_set = b->value;
        b->flags &= ~ACTOR_HIDDEN;
    } else {
        movers_enable(b->value, false);
        b->active = 0;
        g_kamella.barrier_set = -1;
    }
}
static void kamella_demo_pose(Actor* a, const KamellaDemoPose* poses, int duration){
    int frame = fx_clamp(a->timer - 1, 0, duration), key = frame / KAMELLA_DEMO_STEP,
        r = frame % KAMELLA_DEMO_STEP;
    KamellaDemoPose q = poses[key];
    const int16_t* next = (const int16_t*)&poses[key + 1];
    int16_t* cur = (int16_t*)&q;
    if(frame < duration)
        for(unsigned i = 0; i < sizeof q / sizeof(int16_t); i++)
            cur[i] += ((int)next[i] - cur[i]) * r / KAMELLA_DEMO_STEP;
    const StageObject* o = actor_stage_object(a);
    Actor base = *a;
    base.position = a->home;
    base.up = v3_from_i16(o->up);
    base.front = v3_from_i16(o->front);
    a->position = kamella_local(&base, v3_from_i16(q.boss));
    a->up = v3_direction14(v3_sub(kamella_local(&base, v3_from_i16(q.boss + 3)), base.position), base.up);
    a->front =
        v3_tangent14(v3_sub(kamella_local(&base, v3_from_i16(q.boss + 6)), base.position), a->up, base.front);
    Player* p = &g_game.player;
    p->figure.position = kamella_local(&base, v3_from_i16(q.mario));
    p->up = p->gravity_up =
        v3_direction14(v3_sub(kamella_local(&base, v3_from_i16(q.mario + 3)), base.position), base.up);
    p->front = v3_tangent14(v3_sub(kamella_local(&base, v3_from_i16(q.mario + 6)), base.position), p->up,
                            base.front);
    g_mario.position16 = V3(p->figure.position.x << 8, p->figure.position.y << 8, p->figure.position.z << 8);
    g_mario.jump_vec = V3(0, 0, 0);
    p->velocity = V3(0, 0, 0);
    g_game.camera.position = kamella_local(&base, v3_from_i16(q.eye));
    g_game.camera.target = kamella_local(&base, v3_from_i16(q.watch));
    g_game.camera.front = v3_direction14(v3_sub(g_game.camera.target, g_game.camera.position), base.front);
    g_game.camera.up = v3_direction14(v3_reject14(base.up, g_game.camera.front), base.up);
}

static bool kamella_bind_offset(Actor* a, int radius, int offset, RayHit* hit){
    v3 centre_offset = v3_scale14(a->up, offset);
    v3 old = v3_add(a->position, centre_offset),
       delta = V3((a->velocity.x + 128) >> 8, (a->velocity.y + 128) >> 8, (a->velocity.z + 128) >> 8);
    v3 next = v3_add(old, delta);
    CollisionCandidates cand;
    collision_gather_around(old, next, radius + 2, &cand);
    bool found = false;
    int depth = -1;
    SphereContact best = {0};
    for(int i = 0; i < cand.count; i++){
        int tri = cand.index[i];
        if(g_kamella.barrier_set >= 0){
            const ColDynSet* b = &g_col_dyn_sets[g_kamella.barrier_set];
            if(tri >= COL_DYN_BASE + b->first && tri < COL_DYN_BASE + b->first + b->count) continue;
        }
        SphereContact c;
        if(collision_sphere_triangle(tri, next, radius, &c) && c.depth > depth){
            best = c;
            depth = c.depth;
            found = true;
        }
    }
    a->position = v3_sub(found ? v3_add(next, v3_scale14(best.normal, depth + 1)) : next, centre_offset);
    if(found){
        hit->point = best.point;
        hit->normal = best.normal;
        hit->attr = best.attr;
        hit->tri = best.tri;
        hit->t = 0;
    }
    return found;
}
static bool kamella_bind(Actor* a, int radius, RayHit* hit){ return kamella_bind_offset(a, radius, 0, hit); }
static bool kamella_in_water(const Actor* a){
    WaterSample water;
    return water_query(a->position, a->up, &water);
}
static void kameck_attack_success_sound(const Actor* by){
    int slot = by->args[1] - 1;
    if(slot < 0 || slot >= g_game.actor_count) return;
    Actor* a = &g_game.actors[slot];
    if(a->active && a->type == OBJ_BOSSKAMECK2) kamella_sound(SFX_KAMELLA_ATTACK_SUCCESS);
    else if(a->active && a->type == OBJ_KAMECK) kamella_sound(SFX_KAMELLA_CHILD_ATTACK_SUCCESS);
}
static void jet_turtle_make_pickup(Actor* a, bool spin){
    a->velocity = V3(0, 0, 0);
    a->type = OBJ_JETTURTLE;
    a->radius = 50;
    actor_set_state(a, spin ? JET_TURTLE_RESERVE : KAMECK_TURTLE_WAIT, -1);
    a->args[3] = 0;
}
static bool kameck_beam_to_turtle(Actor* b){
    Actor* t = actor_spawn_reuse(OBJ_KAMECKTURTLE, b->position, b->up, b->front);
    if(!t) return false;
    t->args[1] = b->args[1];
    t->radius = 50;
    t->front = v3_tangent14(b->target, b->up, b->front);
    t->velocity = v3_scale14(b->target, V16(1));
    t->target = t->front;
    actor_set_state(t, KAMECK_TURTLE_RUN, -1);
    b->args[2] = (int16_t)(actor_index(t) + 1);
    b->flags |= ACTOR_HIDDEN;
    actor_set_state(b, KAMECK_BEAM_TURTLE, -1);
    kamella_sound(SFX_KAMELLA_TURTLE_APPEAR);
    return true;
}
static bool kameck_beam_to_fire(Actor* b){
    static const int degrees[3][3] = {{0, 0, 0}, {30, -30, 0}, {0, 120, -120}};
    int count = fx_clamp(b->args[0] - 1, 1, 3), live = 0;
    b->args[2] = b->args[3] = 0;
    b->target.x = 0;
    v3 dir = v3_tangent14(b->front, b->up, V3(N14_ONE, 0, 0));
    for(int i = 0; i < count; i++){
        v3 d = v3_rotate_axis14(dir, v3_neg(b->up), DEG(degrees[count - 1][i]));
        Actor* f = actor_spawn_reuse(OBJ_KAMECKFIREBALL, b->position, b->up, d);
        if(!f) continue;
        f->args[1] = b->args[1];
        f->velocity = v3_sub(v3_scale14(d, V16(15)), v3_scale14(b->up, V16(15)));
        actor_set_state(f, AS_WAIT, -1);
        if(i < 2) b->args[2 + i] = (int16_t)(actor_index(f) + 1);
        else b->target.x = actor_index(f) + 1;
        live++;
    }
    if(!live) return false;
    b->flags |= ACTOR_HIDDEN;
    actor_set_state(b, KAMECK_BEAM_FIRE, -1);
    kamella_sound(SFX_KAMELLA_BEAM_EXPLOSION);
    return true;
}
static void kameck_beam_think(Actor* b){
    int step = b->timer - 1;
    if(b->state == KAMECK_BEAM_FOLLOW){
        int slot = b->args[1] - 1;
        if(slot < 0 || slot >= g_game.actor_count){
            b->active = 0;
            return;
        }
        Actor* owner = &g_game.actors[slot];
        if(!owner->active){
            b->active = 0;
            return;
        }
        b->position = owner->type == OBJ_BOSSKAMECK2 ? kamella_wand(owner) : kameck_wand(owner);
        return;
    }
    if(b->state == KAMECK_BEAM_SHOOT){
        RayHit hit;
        if(kamella_bind(b, b->radius, &hit)){
            b->velocity = V3(0, 0, 0);
            actor_set_state(b, KAMECK_BEAM_EXPLOSION, -1);
            return;
        }
        if(step > 360 || kamella_in_water(b)){
            b->active = 0;
            return;
        }
        v3 centre = v3_add(g_game.player.figure.position, v3_scale14(g_game.player.up, C_BODY_HEIGHT));
        if(b->args[0] == 1 && g_mario.attack_timer > 0 &&
           v3_length_squared64(v3_sub(b->position, centre)) < 500u * 500u){
            if(kameck_beam_to_turtle(b)){
                Actor* t = actor_active_at(b->args[2] - 1, OBJ_KAMECKTURTLE);
                if(t) jet_turtle_make_pickup(t, true);
            } else b->active = 0;
            return;
        }
        if(actor_touches_mario(b, 0)){
            actor_hurt_mario(b, 1);
            kameck_attack_success_sound(b);
            b->active = 0;
            return;
        }
    } else if(b->state == KAMECK_BEAM_EXPLOSION){
        bool changed = b->args[0] == 1 ? kameck_beam_to_turtle(b) : kameck_beam_to_fire(b);
        if(!changed) b->active = 0;
    } else if(b->state == KAMECK_BEAM_TURTLE){
        int slot = b->args[2] - 1;
        if(slot < 0 || slot >= g_game.actor_count || !g_game.actors[slot].active ||
           (g_game.actors[slot].type != OBJ_KAMECKTURTLE && g_game.actors[slot].type != OBJ_JETTURTLE))
            b->active = 0;
    } else if(b->state == KAMECK_BEAM_FIRE){
        bool live = actor_active_at(b->args[2] - 1, OBJ_KAMECKFIREBALL) ||
                    actor_active_at(b->args[3] - 1, OBJ_KAMECKFIREBALL) ||
                    actor_active_at(b->target.x - 1, OBJ_KAMECKFIREBALL);
        if(!live) b->active = 0;
    }
}
static void kameck_fireball_think(Actor* a){
    int step = a->timer - 1;
    a->up = gravity_sample(g_game.stage.header, a->position).up;
    a->velocity = v3_scale14(v3_sub(a->velocity, v3_scale14(a->up, V16(.5f))), R14(.995f));
    RayHit hit;
    if(kamella_bind(a, 58, &hit)){
        int speed = v3_dot14(a->velocity, hit.normal);
        if(speed < 0) a->velocity = v3_sub(a->velocity, v3_scale14(hit.normal, mul14(speed, R14(1.4f))));
        if(v3_dot14(a->velocity, a->up) >= V16(1)) kamella_sound(SFX_KAMELLA_FIRE_BOUND);
    }
    if(step > 180 || kamella_in_water(a)){
        a->active = 0;
        kamella_sound(SFX_KAMELLA_FIRE_DISAPPEAR);
        return;
    }
    if(actor_touches_mario(a, 0) && kamella_attack_fire(a)){
        kameck_attack_success_sound(a);
        a->active = 0;
        kamella_sound(SFX_KAMELLA_FIRE_DISAPPEAR);
    }
}
static v3 jet_turtle_hold_position(void){
    NativeMarioHand h = mario_hand_sample();
    return mario_hand_local(v3_from_i16(h.hand), true);
}
static void kameck_turtle_think(Actor* a){
    int step = a->timer - 1;
    if(a->state == JET_TURTLE_HELD){
        a->position = jet_turtle_hold_position();
        a->up = g_game.player.up;
        a->front = g_game.player.front;
        if(g_game.player.action == MA_DEATH || g_game.player.action == MA_DAMAGE ||
           g_game.player.action == MA_DAMAGE_FALL || g_game.player.action == MA_FIRE_DANCE ||
           g_game.player.action == MA_FIRE_RUN || g_game.player.action == MA_STAR_LAUNCH){
            g_kamella.held_shell = -1;
            a->active = 0;
            return;
        }
        if(step > 0 && g_game.player.action != MA_SWIM && (g_game.keys.pressed & KEY_B)){
            g_kamella.held_shell = -1;
            kamella_sound(g_game.player.action == MA_SWIM ? SFX_SHELL_LAUNCH_WATER : SFX_SHELL_LAUNCH_DRY);
            a->target = g_game.player.front;
            a->home = a->position;
            a->front = v3_neg(a->up);
            a->value = v3_length_exact(g_game.player.velocity);
            a->velocity = v3_scale14(a->target, V16(30) + a->value);
            mario_restart_anim(ANIM_MARIO_THROW);
            g_mario.no_control_timer = g_mario.jumping ? 1 : 15;
            actor_set_state(a, JET_TURTLE_THROW, -1);
        }
        return;
    }
    if(a->state == JET_TURTLE_RESERVE || a->state == JET_TURTLE_TAKENSTART){
        v3 want = jet_turtle_hold_position();
        a->position =
            v3_add(a->position, v3_scale14(v3_sub(want, a->position), a->args[3] ? R14(.5f) : R14(.25f)));
        if(a->state == JET_TURTLE_RESERVE && step >= (a->args[3] ? 2 : 8))
            actor_set_state(a, JET_TURTLE_TAKENSTART, -1);
        else if(a->state == JET_TURTLE_TAKENSTART){
            g_kamella.held_shell = (int16_t)actor_index(a);
            kamella_sound(g_game.player.action == MA_SWIM ? SFX_SHELL_TAKE_WATER : SFX_SHELL_TAKE_DRY);
            mario_restart_anim(g_mario.jumping ? ANIM_MARIO_CARRYAIRSTART : ANIM_MARIO_CARRYSTARTSHORT);
            actor_set_state(a, JET_TURTLE_HELD, -1);
        }
        return;
    }
    a->up = gravity_sample(g_game.stage.header, a->position).up;
    if(a->state == JET_TURTLE_THROW){
        if(step >= 300 || v3_length_squared64(v3_sub(a->home, a->position)) > 10000u * 10000u){
            a->active = 0;
            return;
        }

        v3 gravity = v3_neg(a->up);
        if(gravity.x != a->front.x || gravity.y != a->front.y || gravity.z != a->front.z)
            a->target = v3_direction14(
                v3_add(v3_scale14(a->target, R14(.983f)), v3_scale14(gravity, R14(.017f))), a->target);
        a->front = gravity;
        a->velocity = v3_scale14(a->target, V16(30) + a->value);
        RayHit hit;
        if(kamella_bind_offset(a, 35, 20, &hit) && v3_dot14(hit.normal, a->up) < R14(.5f)){
            a->active = 0;
            return;
        }
        for(int i = 0; i < g_game.actor_count; i++){
            Actor* b = &g_game.actors[i];
            if(!b->active || (b->type != OBJ_BOSSKAMECK2 && b->type != OBJ_KAMECK)) continue;
            int radius = b->type == OBJ_BOSSKAMECK2 ? 300 : 120;
            if(v3_length_squared64(v3_sub(b->position, a->position)) >
               (uint64_t)((radius + 50) * (radius + 50)))
                continue;
            if(b->type == OBJ_BOSSKAMECK2){
                if(kamella_damage(b)){
                    a->active = 0;
                    return;
                }
            } else {
                actor_set_state(b, KAMECK_DOWN, ANIM_KAMECK_DOWN);
                a->active = 0;
                return;
            }
        }
        return;
    }
    if(a->state == KAMECK_TURTLE_RUN){
        if(step > 600){
            a->active = 0;
            kamella_sound(SFX_SHELL_DISAPPEAR);
            return;
        }
        int rate =
            step <= 200 ? N14_ONE
            : step >= 400
                ? 0
                : N14_ONE - (int)(-(fx_cos((step - 200) * FX_PI / 200) - FX_ONE) * N14_ONE / (2 * FX_ONE));
        a->velocity = v3_add(a->velocity, v3_scale14(a->target, mul14(V16(.5f), rate)));
        if(!(a->flags & ACTOR_ON_GROUND)) a->velocity = v3_sub(a->velocity, v3_scale14(a->up, V16(.7f)));
        a->velocity = v3_scale14(a->velocity, R14(.98f));
        a->front = v3_rotate_axis14(a->front, v3_neg(a->up), mul14(DEG(-25), rate));
        RayHit hit;
        a->flags &= ~ACTOR_ON_GROUND;
        if(kamella_bind_offset(a, 40, 10, &hit)){
            int speed = v3_dot14(a->velocity, hit.normal);
            if(v3_dot14(hit.normal, a->up) > R14(.5f)){
                a->flags |= ACTOR_ON_GROUND;
                if(speed < 0) a->velocity = v3_sub(a->velocity, v3_scale14(hit.normal, speed));
            } else if(speed < 0){
                a->velocity = v3_add(v3_sub(a->velocity, v3_scale14(hit.normal, mul14(speed, R14(1.5f)))),
                                     v3_scale14(a->up, V16(15)));
                a->target = v3_tangent14(a->velocity, a->up, a->target);
                kamella_sound(SFX_SHELL_REBOUND);
            }
        }
        if(g_mario.attack_timer > 0 && actor_mario_within(a, 300)) jet_turtle_make_pickup(a, true);
        else if(actor_mario_contact(a, 0) == CONTACT_TRAMPLE) jet_turtle_make_pickup(a, false);
        else if(actor_touches_mario(a, 0)){
            if(step < 300){
                actor_hurt_mario(a, 1);
                kameck_attack_success_sound(a);
            } else jet_turtle_make_pickup(a, false);
        }
    } else if(a->state == KAMECK_TURTLE_WAIT && g_kamella.held_shell < 0 && g_mario.attack_timer > 0 &&
              actor_mario_within(a, 350))
        actor_set_state(a, JET_TURTLE_RESERVE, -1);
}
static void kameck_think(Actor* a){
    int step = a->timer - 1;
    if(a->state == KAMECK_DEMO){
        actor_turn_toward(a, actor_to_mario(a), DEG(6));
        return;
    }
    switch(a->state){
    case KAMECK_WAIT:
        if(actor_mario_within(a, 2000)){
            Actor* b = kameck_beam_charge(a, a->stage_index < 0 ? 2
                                             : a->args[0] >= 3  ? 4
                                             : a->args[0] == 2  ? 3
                                                                : 2);
            if(b){
                a->value = (int16_t)actor_index(b) + 1;
                actor_set_state(a, KAMECK_ATTACKWAIT, ANIM_KAMECK_ATTACKWAIT);
            }
        }
        break;
    case KAMECK_APPEAR:
        a->flags &= ~ACTOR_HIDDEN;
        actor_turn_toward(a, actor_to_mario(a), DEG(6));
        if(step >= 79) actor_set_state(a, KAMECK_WAIT, ANIM_KAMECK_WAIT);
        break;
    case KAMECK_ATTACKWAIT:
        actor_turn_toward(a, actor_to_mario(a), DEG(6));
        if(step > 60) actor_set_state(a, KAMECK_ATTACK, ANIM_KAMECK_ATTACK);
        break;
    case KAMECK_ATTACK:
        if(step == 0) kamella_sound(SFX_KAMELLA_CHILD_SWING);
        if(step == 9){
            Actor* b = actor_active_at(a->value - 1, OBJ_BOSSKAMECKBEAM);
            if(b) kameck_beam_shoot(b, false, 12);
            a->value = 0;
        }
        if(step > 30){
            actor_set_state(a, KAMECK_HIDE, ANIM_KAMECK_HIDE);
            kamella_sound(SFX_KAMELLA_CHILD_HIDE);
        }
        break;
    case KAMECK_HIDE:
        if(step >= 79){
            actor_set_state(a, KAMECK_MOVE, ANIM_KAMECK_MOVE);
            a->flags |= ACTOR_HIDDEN;
            kamella_sound(SFX_KAMELLA_CHILD_SMOKE);
        }
        break;
    case KAMECK_MOVE: {
        int rail = a->stage_index >= 0 ? actor_rail(a) : a->args[2];
        if(rail >= 0){
            const StageRail* r = rail_get(rail);
            int index = (int)a->target.x, dir = (int)a->target.y;
            if(!dir) dir = 1;
            int next = index + dir;
            if(next < 0 || next >= r->point_count){
                dir = -dir;
                next = index + dir;
                a->target.y = dir;
            }
            fx from =
                index < r->part_count ? g_game.stage.rail_parts[r->first_part + index].start : r->length;
            fx to = next < r->part_count ? g_game.stage.rail_parts[r->first_part + next].start : r->length;
            int duration = fx_max(fx_abs(to - from) / 20, 1);
            int share = fx_clamp(step * N14_ONE / duration, 0, N14_ONE);
            share = mul14(mul14(share, share), 3 * N14_ONE - 2 * share);
            a->position = rail_position_at(rail, from + mul14(to - from, share));
            if(step > duration){
                a->target.x = next;
                actor_set_state(a, KAMECK_APPEAR, ANIM_KAMECK_APPEAR);
                kamella_sound(SFX_KAMELLA_CHILD_APPEAR);
            }
        } else if(step > 240) actor_set_state(a, KAMECK_APPEAR, ANIM_KAMECK_APPEAR);
        break;
    }
    case KAMECK_DOWN:
        actor_move(a, V16(1), R14(.99f), R14(.99f));
        if(step >= 49){
            kamella_sound(SFX_KAMELLA_CHILD_EXPLODE);
            collectibles_pop(OBJ_COIN, a->position, a->up, 1);
            a->active = 0;
        }
        break;
    case KAMECK_PRESS:
        if(step >= 24){
            kamella_sound(SFX_KAMELLA_CHILD_EXPLODE);
            collectibles_pop(OBJ_COIN, a->position, a->up, 1);
            a->active = 0;
        }
        break;
    }
    if(a->state == KAMECK_WAIT || a->state == KAMECK_APPEAR || a->state == KAMECK_ATTACKWAIT ||
       a->state == KAMECK_ATTACK || a->state == KAMECK_HIDE){
        int contact = actor_mario_contact(a, 0);
        if(contact == CONTACT_TRAMPLE || contact == CONTACT_HIPDROP){
            kamella_clear_beams(a, true);
            a->velocity = V3(0, 0, 0);
            actor_set_state(a, KAMECK_PRESS, ANIM_KAMECK_PRESS);
            kamella_sound(SFX_KAMELLA_CHILD_DEAD_STOMP);
            mario_stomp_bounce(1);
        } else if(contact == CONTACT_SPIN || contact == CONTACT_STAR){
            kamella_clear_beams(a, true);
            a->velocity = v3_add(v3_scale14(v3_tangent14(actor_to_mario(a), a->up, a->front), V16(-25)),
                                 v3_scale14(a->up, V16(25)));
            actor_set_state(a, KAMECK_DOWN, ANIM_KAMECK_DOWN);
            kamella_sound(SFX_KAMELLA_CHILD_DEAD_BLOW);
        } else if(contact == CONTACT_TOUCH) actor_hurt_mario(a, 1);
    }
}
static void kamella_init(Actor* a, const StageObject* o){
    a->flags |= ACTOR_HIDDEN;
    a->args[2] = -1;
    a->health = 4;
    actor_set_state(a, KAMELLA_DORMANT, ANIM_BOSSKAMECK_WAIT);
}

static void kameck_init(Actor* a, const StageObject* o){
    int rail = actor_rail(a);
    if(rail >= 0){
        a->position = rail_point(rail, 0);
        a->target = V3(0, 1, 0);
    }
    a->value = 0;
    actor_set_state(a, KAMECK_WAIT, ANIM_KAMECK_WAIT);
}

static void jet_turtle_init(Actor* a, const StageObject* o){
    a->args[1] = 0;
    actor_set_state(a, KAMECK_TURTLE_WAIT, -1);
}

static void kamella_damage_drop(void){
    Actor* a = actor_active_at(g_kamella.held_shell, OBJ_JETTURTLE);
    if(a && a->state == JET_TURTLE_HELD){
        g_kamella.held_shell = -1;
        a->active = 0;
    }
}
static bool kamella_carry_anim(void){
    Actor* a = actor_active_at(g_kamella.held_shell, OBJ_JETTURTLE);
    if(!a || a->state != JET_TURTLE_HELD) return false;
    Player* p = &g_game.player;
    if((p->anim == ANIM_MARIO_CARRYSTARTSHORT || p->anim == ANIM_MARIO_CARRYAIRSTART) && !mario_anim_done())
        return true;
    mario_set_anim(ANIM_MARIO_CARRYWAIT);
    return true;
}
static int kamella_barrier_material_frame(void){
    const Actor* a = actor_active_at(g_kamella.barrier, OBJ_BOSSKAMECKBARRIER);
    return a ? fx_max((int)a->timer - 1, 0) : 0;
}

__attribute__((noinline)) static bool kamella_barrier_draw(uint8_t* framebuffer, Figure* figure,
                                                           const CameraView* view){
    int frame = fx_min(kamella_barrier_material_frame(), KAMELLA_BARRIER_BRK_DURATION);
    const uint16_t* source = kamella_barrier_maps[kamella_barrier_frames[frame]];
    uint16_t* rows = (uint16_t*)geometry_point_scratch();
    uint8_t* table = (uint8_t*)rows + 512;

    memset(rows, 0, 512);
    const uint8_t* classes = kamella_barrier_classes;
    _Static_assert(512 + sizeof kamella_barrier_classes * 256 <= 8192,
                   "additive tables must fit the point list scratch");
    for(unsigned i = 0; i < sizeof kamella_barrier_classes; i++){
        rows[classes[i]] = (uint16_t)i;
        dma3_copy32(table + i * 256, kamella_barrier_additive[source[classes[i]]], 64);
    }
    g_additive_rows = rows;
    g_additive_table = table;
    draw_figure(framebuffer, figure, view);
    return true;
}

static const FigureOps kamella_barrier_figure_ops = {.draw = kamella_barrier_draw};
static bool kamella_camera_transparent_triangle(int triangle){
    int set = g_kamella.barrier_set;
    if(!actor_active_at(g_kamella.barrier, OBJ_BOSSKAMECKBARRIER) || set < 0 || set >= g_col_dyn_sets_used)
        return false;
    const ColDynSet* d = &g_col_dyn_sets[set];
    return d->on && triangle >= COL_DYN_BASE + d->first && triangle < COL_DYN_BASE + d->first + d->count;
}
static void kamella_reveal_owned_star(Actor* a){
    const KamellaOwner* owner = kamella_owner(a);
    if(!owner) return;
    int star = owner->star;
    g_power_star_revealed[star >> 3] |= (uint8_t)(1u << (star & 7));
    for(int i = 0; i < g_game.actor_count; i++)
        if(g_game.actors[i].stage_index == star) power_star_reveal(&g_game.actors[i]);
}
static void kamella_children_set(Actor* a, int action){
    if(g_game.scenario != 1) return;
    for(int i = 0; i < 2; i++){
        Actor* b = actor_active_at(g_kamella.children[i], OBJ_KAMECK);
        if(!b && action >= 0){
            const KamellaChild* c = &kamella_children[i];
            b = actor_spawn_reuse(OBJ_KAMECK, V3(c->pos[0], c->pos[1], c->pos[2]), v3_from_i16(c->up),
                                  v3_from_i16(c->front));
            if(b){
                g_kamella.children[i] = (int16_t)actor_index(b);
                b->args[0] = 2;
                b->args[2] = c->rail;
                b->target = V3(0, 1, 0);
            }
        }
        if(!b) continue;
        if(action < 0) b->active = 0;
        else if(action == 0) actor_set_state(b, KAMECK_DEMO, ANIM_KAMECK_APPEAR);
        else if(action == 1) actor_set_state(b, KAMECK_WAIT, ANIM_KAMECK_WAIT);
        else if(action == 2) actor_set_state(b, KAMECK_APPEAR, ANIM_KAMECK_APPEAR);
    }
}
static void kamella_think(Actor* a){
    int step = a->timer - 1;
    const KamellaOwner* owner = kamella_owner(a);
    if(!owner) return;
    switch(a->state){
    case KAMELLA_DORMANT:
        if(switch_on(owner->sw_a)){
            g_kamella.boss = (int16_t)actor_index(a);
            g_kamella.round = g_kamella.pattern = g_kamella.casts = 0;
            kamella_demo_begin(a, KAMELLA_WAITAPPEAR, ANIM_BOSSKAMECK_WAIT);
            audio_stop_music();
        }
        break;
    case KAMELLA_WAITAPPEAR:
        if(step > 120){
            a->flags &= ~ACTOR_HIDDEN;
            kamella_demo_begin(a, KAMELLA_START, ANIM_BOSSKAMECK_START2);
        }
        break;
    case KAMELLA_START:
        kamella_demo_pose(a, kamella_demo_start2, KAMELLA_DEMOBOSSKAMECKSTART2_DURATION);
        if(step == 60) kamella_sound(SFX_KAMELLA_APPEAR_VOICE);
        if(step == 155) kamella_sound(SFX_KAMELLA_APPEAR_END);
        if(step >= KAMELLA_DEMOBOSSKAMECKSTART2_DURATION){
            kamella_demo_end();
            kamella_barrier(a, true);
            kamella_start_move(a);
        }
        break;
    case KAMELLA_HIDESTART:
        a->velocity = V3(0, 0, 0);
        actor_turn_toward(a, actor_to_mario(a), DEG(6));
        if(step >= KAMELLA_HIDE_DURATION){
            a->flags |= ACTOR_HIDDEN;
            kamella_pick_rail_point(a, false);
            actor_set_state(a, KAMELLA_HIDEMOVE, ANIM_BOSSKAMECK_MOVE);
            kamella_sound(SFX_KAMELLA_HIDE_SMOKE);
        }
        break;
    case KAMELLA_HIDEMOVE:
        actor_turn_toward(a, actor_to_mario(a), DEG(6));
        kamella_fly_to_target(a, V16(g_kamella.round == 3 ? 2 : 1.5f), V16(g_kamella.round == 3 ? 20 : 15),
                              R14(.9f), V16(.5f));
        if(v3_length_squared64(v3_sub(a->target, a->position)) < 100u * 100u){
            if(g_kamella.hops >= (g_kamella.round == 3 ? 4 : 3)){
                a->flags &= ~ACTOR_HIDDEN;
                actor_set_state(a, KAMELLA_HIDEEND, ANIM_BOSSKAMECK_APPEAR);
                kamella_sound(SFX_KAMELLA_APPEAR);
            } else {
                g_kamella.hops++;
                kamella_pick_rail_point(a, g_kamella.hops == (g_kamella.round == 3 ? 4 : 3));
            }
        }
        break;
    case KAMELLA_HIDEEND:
        a->velocity = V3(0, 0, 0);
        actor_turn_toward(a, actor_to_mario(a), DEG(6));
        if(step >= KAMELLA_APPEAR_DURATION){
            int live = 0;
            for(int i = 0; i < 2; i++) live += actor_active_at(g_kamella.children[i], OBJ_KAMECK) != NULL;
            if(g_kamella.round >= 2 && g_kamella.casts > 3 && !live){
                g_kamella.casts = 0;
                actor_set_state(a, KAMELLA_SUMMONWAIT, ANIM_BOSSKAMECK_SUMMONWAIT);
            } else {
                Actor* b = kameck_beam_charge(a, kamella_beam_kind());
                if(b){
                    g_kamella.charged = (int16_t)actor_index(b);
                    g_kamella.pattern = (g_kamella.pattern + 1) % (g_kamella.round < 2 ? 2 : 3);
                    g_kamella.casts = live ? 0 : g_kamella.casts + 1;
                    actor_set_state(a, KAMELLA_ATTACKWAIT, ANIM_BOSSKAMECK_ATTACKWAIT);
                } else kamella_start_move(a);
            }
        }
        break;
    case KAMELLA_ATTACKWAIT:
        actor_turn_toward(a, actor_to_mario(a), DEG(6));
        kamella_fly_to_target(a, V16(.09f), V16(.9f), R14(.9f), V16(.2f));
        if(step > 60) actor_set_state(a, KAMELLA_ATTACK, ANIM_BOSSKAMECK_ATTACK);
        break;
    case KAMELLA_ATTACK:
        if(step == 12) kamella_sound(SFX_KAMELLA_SWING);
        if(step == 23){
            Actor* b = actor_active_at(g_kamella.charged, OBJ_BOSSKAMECKBEAM);
            if(b) kameck_beam_shoot(b, true, 20);
            g_kamella.charged = -1;
            kamella_sound(SFX_KAMELLA_STAFF_EFFECT);
        }
        a->velocity = v3_scale14(a->velocity, R14(.96f));
        if(step > 30) actor_set_state(a, KAMELLA_WAIT, ANIM_BOSSKAMECK_WAIT);
        break;
    case KAMELLA_WAIT:
        a->velocity = v3_scale14(a->velocity, R14(.96f));
        if(step > 60) kamella_start_move(a);
        break;
    case KAMELLA_GUARD:
        a->velocity = v3_scale14(a->velocity, R14(.96f));
        if(step >= KAMELLA_GUARD_DURATION) kamella_start_move(a);
        break;
    case KAMELLA_DAMAGE:
        if(step >= (g_kamella.round == 3 ? KAMELLA_DOWN_DURATION : KAMELLA_DAMAGE_DURATION)){
            if(g_kamella.round == 3){
                kamella_barrier(a, false);
                kamella_children_set(a, -1);
                kamella_demo_begin(a, KAMELLA_DOWN, ANIM_BOSSKAMECK_DOWN2);
                audio_stop_music();
            } else {
                g_kamella.round++;
                a->health = 4 - g_kamella.round;
                g_kamella.pattern = 0;
                a->args[2] = -1;
                if(g_kamella.round == 1){
                    collectibles_pop(OBJ_STARBIT, a->position, a->up, 8);
                    kamella_sound(SFX_KAMELLA_STAR_PIECE_BURST);
                    kamella_start_move(a);
                } else if(g_kamella.round == 2)
                    kamella_demo_begin(a, KAMELLA_POWERUP, ANIM_BOSSKAMECK_POWERUP2);
                else {
                    collectibles_pop(OBJ_STARBIT, a->position, a->up, 16);
                    kamella_sound(SFX_KAMELLA_STAR_PIECE_BURST);
                    kamella_start_move(a);
                }
            }
        }
        break;
    case KAMELLA_POWERUP:
        kamella_demo_pose(a, kamella_demo_powerup2, KAMELLA_DEMOBOSSKAMECKPOWERUP2_DURATION);
        if(step == 200) kamella_children_set(a, 0);
        if(step >= KAMELLA_DEMOBOSSKAMECKPOWERUP2_DURATION){
            kamella_children_set(a, 1);
            kamella_demo_end();
            kamella_start_move(a);
        }
        break;
    case KAMELLA_SUMMONWAIT:
        if(step >= KAMELLA_SUMMONWAIT_DURATION){
            kamella_children_set(a, 2);
            actor_set_state(a, KAMELLA_SUMMON, ANIM_BOSSKAMECK_SUMMON);
        }
        break;
    case KAMELLA_SUMMON:
        if(step >= KAMELLA_SUMMON_DURATION) actor_set_state(a, KAMELLA_WAIT, ANIM_BOSSKAMECK_WAIT);
        break;
    case KAMELLA_DOWN:
        kamella_demo_pose(a, kamella_demo_down2, KAMELLA_DEMOBOSSKAMECKDOWN2_DURATION);
        if(step == 18) kamella_sound(SFX_KAMELLA_DEAD_VOICE);
        if(step == 150) kamella_sound(SFX_KAMELLA_DEAD_MOTION);
        if(step == 285) audio_play_music(MUSIC_MBGM_STAR_EXIST);
        if(step == 345) kamella_sound(SFX_KAMELLA_STAFF_BREAK);
        if(step >= KAMELLA_DEMOBOSSKAMECKDOWN2_DURATION){
            kamella_reveal_owned_star(a);
            kamella_demo_end();
            a->active = 0;
        }
        break;
    }
    if(!g_kamella.demo && kamella_can_take_damage(a)){
        int contact = actor_mario_contact(a, 0);
        if(contact == CONTACT_SPIN){
            kamella_clear_beams(a, false);
            actor_set_state(a, KAMELLA_GUARD, ANIM_BOSSKAMECK_GUARD);
            actor_turn_toward(a, actor_to_mario(a), DEG(18));
        } else if(contact == CONTACT_TOUCH) actor_hurt_mario(a, 1);
    }
}

static void kamella_dispatch(Actor* a){
    switch(a->type){
    case OBJ_BOSSKAMECK2: kamella_think(a); break;
    case OBJ_KAMECK: kameck_think(a); break;
    case OBJ_BOSSKAMECKBEAM: kameck_beam_think(a); break;
    case OBJ_KAMECKFIREBALL: kameck_fireball_think(a); break;
    case OBJ_KAMECKTURTLE:
    case OBJ_JETTURTLE: kameck_turtle_think(a); break;
    case OBJ_BOSSKAMECKBARRIER:
        if(a->timer > KAMELLA_BARRIER_BRK_DURATION + 1) a->timer = KAMELLA_BARRIER_BRK_DURATION + 1;
        break;
    }
}

__attribute__((noinline, optimize("Os"))) static bool kamella_camera_points(
    v3* points, int* count, v3* up, v3* arena_front){
    if(g_kamella.demo) return false;
    const Actor* boss = actor_active_at(g_kamella.boss, OBJ_BOSSKAMECK2);
    if(!boss || !kamella_owner(boss) || boss->state < KAMELLA_START) return false;
    const Player* p = &g_game.player;
    *up = p->up;
    *arena_front = v3_from_i16(actor_stage_object(boss)->front);
    int n = 0;
    points[n++] = v3_add(p->figure.position, v3_scale14(p->up, 40));
    points[n++] = v3_add(p->figure.position, v3_scale14(p->up, 160));
    if(!(boss->flags & ACTOR_HIDDEN)){
        points[n++] = v3_sub(boss->position, v3_scale14(boss->up, 300));
        points[n++] = v3_add(boss->position, v3_scale14(boss->up, 300));
    }
    int nearest[2] = {-1, -1};
    uint64_t distances[2] = {UINT64_MAX, UINT64_MAX};
    for(int i = 0; i < g_game.actor_count; i++){
        const Actor* a = &g_game.actors[i];
        if(!a->active || (a->flags & ACTOR_HIDDEN)) continue;
        bool spell = a->type == OBJ_BOSSKAMECKBEAM && a->state == KAMECK_BEAM_SHOOT;
        bool fire = a->type == OBJ_KAMECKFIREBALL;
        bool shell =
            a->type == OBJ_KAMECKTURTLE || (a->type == OBJ_JETTURTLE && a->state != JET_TURTLE_THROW);

        v3 toward = v3_sub(p->figure.position, a->position);
        if((spell || fire) && (int64_t)a->velocity.x * toward.x + (int64_t)a->velocity.y * toward.y +
                                      (int64_t)a->velocity.z * toward.z <=
                                  0)
            continue;
        if(!spell && !fire && !shell) continue;
        uint64_t d = v3_length_squared64(v3_sub(a->position, p->figure.position));
        if(d > 2000u * 2000u) continue;
        if(d < distances[0]){
            nearest[1] = nearest[0];
            distances[1] = distances[0];
            nearest[0] = i;
            distances[0] = d;
        } else if(d < distances[1]){
            nearest[1] = i;
            distances[1] = d;
        }
    }
    for(int i = 0; i < 2; i++)
        if(nearest[i] >= 0) points[n++] = g_game.actors[nearest[i]].position;
    *count = n;
    return n > 2;
}

static void kameck_beam_figure(const Actor* a, Figure* f){
    bool ready = a->state == KAMECK_BEAM_FOLLOW;
    f->model =
        (const FigureHeader*)(a->args[0] ? (ready ? figure_kamellabeamturtleready : figure_kamellabeamturtle)
                                         : (ready ? figure_kamellabeamfireready : figure_kamellabeamfire));
}
