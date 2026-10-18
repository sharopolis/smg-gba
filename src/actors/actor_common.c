static Actor* actor_spawn(int type, v3 position, v3 up, v3 front){
    Game* game = &g_game;
    Actor* a = actors_stream_free_slot();
    if(!a){
        if(game->actor_count >= MAX_ACTORS) return NULL;
        a = &game->actors[game->actor_count++];
    }
    *a = (Actor){0};
    a->type = (uint16_t)type;
    a->active = 1;
    a->stage_index = -1;
    a->position = position;
    a->home = position;
    a->up = up;
    a->front = v3_tangent14(front, up, V3(0, N14_ONE, 0));
    const ActorKind* k = &actor_kinds[type];
    a->radius = k->radius;
    if(k->figure){
        figure_init(&a->figure, k->figure);
        a->figure.use_basis = true;
        a->figure.scale = V3(k->scale, k->scale, k->scale);
    }
    a->health = 1;
    return a;
}

static void actor_set_state_anim(Actor* a, int state, int anim){
    a->state = (uint8_t)state;
    a->timer = 0;
    a->anim = anim;
    a->anim_frame = 0;
}

static void actor_anim_hold(Actor* a){}

static void actor_star_piece_reaction(Actor* a){
    a->anim = actor_kinds[a->type].reaction_anim;
    a->anim_frame = 0;
}

static inline v3 actor_centre(const Actor* a){
    if(a->type == OBJ_TRICKRABBIT) return rabbit_contact_centre(a);
    fx height = a->type == OBJ_FIREBALL && a->state == FIREBALL_ARC ? FX(50) : actor_kinds[a->type].height;

    if(a->type == OBJ_NEEDLEPLANT) height = height * a->figure.scale.y / 64;
    if(a->type == OBJ_ROCK || a->type == OBJ_WANWAN) height = a->radius;
    return v3_add(a->position, v3_scale14(a->up, height));
}

static inline v3 mario_centre(void){
    Player* p = &g_game.player;
    return v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT));
}

static bool actor_touches_mario(const Actor* a, fx extra){
    v3 d = v3_sub(actor_centre(a), mario_centre());
    fx r = a->radius + C_BODY_RADIUS + extra;
    if(fx_abs(d.x) > r || fx_abs(d.y) > r || fx_abs(d.z) > r) return false;
    return v3_length_squared64(d) <= (uint64_t)((int64_t)r * r);
}

static bool actor_stick_to_ground(Actor* a){
    RayHit hit;
    v3 from = v3_add(a->position, v3_scale14(a->up, FX(60)));
    v3 to = v3_sub(a->position, v3_scale14(a->up, FX(120)));
    if(collision_ray(from, to, &hit, 0) && v3_dot14(hit.normal, a->up) > R14(0.5f)){
        a->position = hit.point;
        return true;
    }
    return false;
}

static void actor_defeat(Actor* a, bool drop_coin){
    a->state = AS_DEFEATED;
    a->timer = 0;
    audio_play_effect(SFX_ENEMY_DEFEAT, 3);
    if(drop_coin){
        Actor* c = actor_spawn(OBJ_COIN, v3_add(a->position, v3_scale14(a->up, FX(40))), a->up, a->front);
        if(c) c->flags |= 2;
    }
}

static inline int actor_index(const Actor* a){ return (int)(a - g_game.actors); }

static inline bool mario_spin_reaches(const Actor* a){
    return g_mario.attack_timer > 0 && g_game.player.action != MA_DEATH && actor_touches_mario(a, FX(60));
}

static inline bool actor_slot_reusable(const Actor* a){
    return !a->active && a->stage_index < 0 && (a->type == OBJ_STARBIT || !(a->flags & ACTOR_WAITING));
}

static inline void collectible_dead(Actor* a){
    if(a->type == OBJ_STARBIT) a->flags &= ~PTR_HOMING;
    a->active = 0;
}

#define OBJECT_RAIL(o) ((o)->rail)

static int g_mario_max_health = 3;

static inline Player* game_player(void){ return &g_game.player; }

static inline int actor_arg_or(const Actor* a, int n, int fallback){
    return n < 4 && a->args[n] != -1 ? a->args[n] : fallback;
}

static inline const StageObject* actor_stage_object(const Actor* a){
    return a->stage_index >= 0 ? &g_game.stage.objects[a->stage_index] : NULL;
}

static int actor_object_arg(const Actor* a, int n){
    if(n < 4) return a->args[n];
    const StageObject* o = actor_stage_object(a);
    return o ? OBJECT_ARG(o, n) : -1;
}

static int actor_rail(const Actor* a){
    const StageObject* o = actor_stage_object(a);
    return o ? OBJECT_RAIL(o) : -1;
}

static inline int actor_switch_key(const Actor* a, int which){
    const StageObject* o = actor_stage_object(a);
    return o ? stage_object_switch(o, which) : -1;
}
static inline bool actor_switch_on(const Actor* a, int which){
    return switch_on(actor_switch_key(a, which));
}
static inline bool actor_has_switch(const Actor* a, int which){ return actor_switch_key(a, which) >= 0; }
static inline bool rail_closed(int rail){ return rail_get(rail)->closed; }

static const uint8_t* const rock_drop_tables[STAGE_COUNT] = STAGE_ROCK_DROPS;
static fx rock_rail_drop(int rail, fx coord, fx none){
    const uint8_t* t =
        (unsigned)g_game.stage_index < STAGE_COUNT ? rock_drop_tables[g_game.stage_index] : NULL;
    if(!t) return none;
    const uint16_t* e = (const uint16_t*)t;
    unsigned at = 0;
    for(int i = e[0]; i > 0; i--){
        e += 2;
        if(e[-1] == rail){
            at = e[0];
            break;
        }
    }
    if(!at) return none;
    const StageRail* r = rail_get(rail);
    if(r->closed && r->length > 0){
        coord %= r->length;
        if(coord < 0) coord += r->length;
    } else coord = fx_clamp(coord, 0, r->length);
    _Static_assert(ROCK_DROP_STEP == 1 << 7, "rock drop lookup assumes a step of 128");
    const uint8_t* s = t + at + (coord >> 7);
    if(s[0] == ROCK_DROP_NONE || s[1] == ROCK_DROP_NONE) return none;
    return (s[0] * 128 + (s[1] - s[0]) * (coord & 127)) * ROCK_DROP_UNIT >> 7;
}

static void actor_set_state(Actor* a, int state, int anim){
    a->state = (uint8_t)state;
    a->timer = 0;
    if(anim >= 0){
        a->anim = anim;
        a->anim_frame = 0;
    }
}

static inline const FigureHeader* actor_model(const Actor* a){ return a->figure.model; }
static inline bool actor_anim_done(const Actor* a){
    return a->timer >= anim_length_of(actor_model(a), a->anim);
}

static inline v3 actor_to_mario(const Actor* a){
    return v3_sub(game_player()->figure.position, a->position);
}

static bool actor_mario_within(const Actor* a, fx r){
    v3 d = actor_to_mario(a);
    if(fx_abs(d.x) > r || fx_abs(d.y) > r || fx_abs(d.z) > r) return false;
    return v3_length_squared64(d) < (uint64_t)((int64_t)r * r);
}

static int32_t actor_mario_distance(const Actor* a){ return v3_length_exact(actor_to_mario(a)); }

static bool actor_sees_mario(Actor* a, fx range, fx half_h, fx half_v){
    if(!actor_mario_within(a, range)) return false;
    if((g_game.tick + (uint32_t)actor_index(a)) & 3) return (a->flags & ACTOR_SAW_MARIO) != 0;
    v3 d = actor_to_mario(a);
    v3 flat = v3_reject14(d, a->up);
    a->flags &= ~ACTOR_SAW_MARIO;
    if(v3_length_squared64(flat) > 64 && angle_between14(a->front, v3_direction14(flat, a->front)) > half_h)
        return false;
    int32_t len = v3_length_exact(d);
    int32_t vert = v3_dot14(d, a->up);
    bool seen = len == 0 || fx_abs(vert) * FX_ONE / len <= fx_sin(half_v) + 4;
    if(seen) a->flags |= ACTOR_SAW_MARIO;
    return seen;
}

static fx actor_turn_toward(Actor* a, v3 dir, fx step){
    v3 want = v3_tangent14(dir, a->up, a->front);
    int32_t c = v3_dot14(a->front, want);
    if(c >= (fx_cos(step) << 6)){
        a->front = want;
    } else {
        fx sign = v3_dot14(v3_cross14(a->front, want), a->up) >= 0 ? 1 : -1;
        a->front = v3_rotate_axis14(a->front, a->up, step * sign);
        if(((g_game.tick + (uint32_t)actor_index(a)) & 7) == 0)
            a->front = v3_tangent14(a->front, a->up, want);
    }
    uint32_t chord2 = (uint32_t)fx_max(0, 2 * (N14_ONE - c));
    return (fx)(isqrt32(chord2 << 14) >> 6);
}

static void actor_follow_gravity(Actor* a, int32_t rate14){
    int32_t sp = fx_abs(a->velocity.x) + fx_abs(a->velocity.y) + fx_abs(a->velocity.z);
    uint32_t phase = g_game.tick + (uint32_t)actor_index(a);

    if((a->flags & ACTOR_ON_GROUND) ? (phase & 7) && sp < V16(24.0f) : (phase & 1)) return;
    GravitySample g = gravity_sample(g_game.stage.header, v3_add(a->position, v3_scale14(a->up, FX(50))));
    if(g.index < 0) return;
    v3 old = a->up;
    a->up = rate14 >= N14_ONE
                ? g.up
                : v3_direction14(v3_add(v3_scale14(a->up, N14_ONE - rate14), v3_scale14(g.up, rate14)), g.up);
    a->front = v3_tangent14(v3_rotate_between14(a->front, old, a->up), a->up, a->front);
}

static void actor_move(Actor* a, int32_t gravity, int32_t ground_friction14, int32_t air_friction14){
    v3 v = a->velocity;

    if((a->flags & ACTOR_ON_GROUND) && fx_abs(v.x) + fx_abs(v.y) + fx_abs(v.z) < V16(0.05f)){
        a->velocity = V3(0, 0, 0);
        return;
    }
    if(a->flags & ACTOR_ON_GROUND){
        v = v3_scale14(v3_reject14(v, a->up), ground_friction14);
    } else {
        v = v3_scale14(v3_sub(v, v3_scale14(a->up, gravity)), air_friction14);
    }
    a->position = v3_add(a->position, V3((v.x + 128) >> 8, (v.y + 128) >> 8, (v.z + 128) >> 8));
    int32_t vs = v3_dot14(v, a->up);

    int32_t sp = fx_abs(v.x) + fx_abs(v.y) + fx_abs(v.z);
    uint32_t phase = g_game.tick + (uint32_t)actor_index(a);
    if((a->flags & ACTOR_ON_GROUND) &&
       ((sp < V16(8.0f) && (phase & 3)) || (sp < V16(16.0f) && (phase & 1)))){
        a->velocity = v;
        return;
    }
    RayHit hit;
    fx below = FX(40) + (vs < 0 ? (-vs >> 8) : 0);
    a->flags &= ~ACTOR_ON_GROUND;
    if(vs <= V16(2.0f) &&
       collision_ray(v3_add(a->position, v3_scale14(a->up, FX(80))),
                     v3_sub(a->position, v3_scale14(a->up, below)), &hit, 0) &&
       v3_dot14(hit.normal, a->up) > R14(0.5f)){
        a->position = hit.point;
        a->flags |= ACTOR_ON_GROUND;
        if(vs < 0) v = v3_reject14(v, a->up);
    }
    a->velocity = v;
}

static bool actor_floor_ahead(const Actor* a, fx ahead){
    RayHit hit;
    v3 p = v3_add(a->position, v3_scale14(a->front, ahead));
    return collision_ray(v3_add(p, v3_scale14(a->up, FX(150))), v3_sub(p, v3_scale14(a->up, FX(150))), &hit,
                         0);
}

static inline void actor_push_forward(Actor* a, int32_t accel){
    a->velocity = v3_add(a->velocity, v3_scale14(a->front, accel));
}

static int actor_mario_contact(const Actor* a, fx spin_extra){
    Player* p = game_player();
    MarioPhysics* m = &g_mario;
    if(p->action == MA_DEATH || g_mario_held) return CONTACT_NONE;
    if(p->action == MA_STAR_LAUNCH) return CONTACT_NONE;
    bool spin = m->attack_timer > 0 && actor_touches_mario(a, spin_extra);
    if(!spin && !actor_touches_mario(a, 0)) return CONTACT_NONE;
    if(g_mario_star_power > 0) return CONTACT_STAR;
    fx above = v3_dot14(v3_sub(mario_centre(), actor_centre(a)), a->up);
    if(p->action == MA_HIPDROP && above > 0) return CONTACT_HIPDROP;
    if(spin) return CONTACT_SPIN;
    if(m->jumping && vertical_speed(m->jump_vec, p->up) < 0 && above > a->radius / 2) return CONTACT_TRAMPLE;
    return CONTACT_TOUCH;
}

static void actor_block_mario(const Actor* a, fx radius, fx height){
    Player* p = game_player();
    v3 d = v3_sub(p->figure.position, a->position);
    fx h = v3_dot14(d, a->up);
    if(h < -FX(60) || h > height) return;
    v3 flat = v3_reject14(d, a->up);
    fx r = radius + C_BODY_RADIUS;
    if(fx_abs(flat.x) > r || fx_abs(flat.y) > r || fx_abs(flat.z) > r) return;
    int32_t len = v3_length_exact(flat);
    if(len >= r) return;
    if(h > height - FX(40)) return;
    v3 out = v3_direction14(flat, v3_neg(game_player()->front));
    mario_nudge(v3_scale14(out, r - len));
}

static Actor* actor_spawn_reuse(int type, v3 position, v3 up, v3 front){
    Game* game = &g_game;
    for(int i = 0; i < game->actor_count; i++){
        Actor* f = &game->actors[i];
        if(actor_slot_reusable(f)){
            int keep = game->actor_count;
            game->actor_count = i;
            Actor* a = actor_spawn(type, position, up, front);
            game->actor_count = keep;
            return a;
        }
    }
    return actor_spawn(type, position, up, front);
}

static bool collectible_fall_enabled(int count){
    return (g_debug_options & DEBUG_FALLING_DROPS) &&
           (count <= 4 || (g_debug_options & DEBUG_LARGE_FALLING_DROPS));
}

static void collectibles_pop_spread(int type, v3 at, v3 up, int count, int spread, int speed){
    bool fall = collectible_fall_enabled(count);
    for(int k = 0; k < count; k++){
        v3 side =
            v3_rotate_axis14(v3_tangent14(V3(N14_ONE, 0, 0), up, V3(0, N14_ONE, 0)), up, FXC(1.2566f) * k);
        Actor* it = actor_spawn_reuse(type, fall ? at : v3_add(at, v3_scale14(side, count > 1 ? FX(40) : 0)),
                                      up, side);

        if(!it && stream_evict_preserving(game_player()->figure.position, at))
            it = actor_spawn_reuse(type, fall ? at : v3_add(at, v3_scale14(side, count > 1 ? FX(40) : 0)), up,
                                   side);
        if(!it) return;
        it->flags |= 2 | (fall ? 16 : 1);
        it->value = (int16_t)count;
        it->health = (int16_t)V16(spread);
        it->args[0] = fall ? (int16_t)V16(speed) : 0;
        if(type == OBJ_STARBIT)
            it->figure.model = (const FigureHeader*)starbit_figures[(g_game.tick + k) % 6];
    }
}
static void collectibles_pop(int type, v3 at, v3 up, int count){
    collectibles_pop_spread(type, at, up, count,
                            type == OBJ_STARBIT ? 10
                            : count > 1         ? 4
                                                : 0,
                            type == OBJ_STARBIT ? 40 : 25);
}

static void actor_set_switch_dead(const Actor* a, fx radius){
    switch_set(actor_switch_key(a, SWITCH_DEAD), true);
}

static void actor_set_switch_a(const Actor* a, fx radius, int type){
    switch_set(actor_switch_key(a, SWITCH_A), true);
}

static EWRAM_BSS uint8_t g_power_star_revealed[(STAGE_MAX_OBJECTS + 7) / 8];
static bool power_star_revealed(int stage_index){
    return stage_index >= 0 && (g_power_star_revealed[stage_index >> 3] >> (stage_index & 7) & 1);
}
static void power_star_reveal(Actor* b){
    b->active = 1;
    if(b->stage_index >= 0)
        g_power_star_revealed[b->stage_index >> 3] |= (uint8_t)(1 << (b->stage_index & 7));
}

static void power_stars_reveal_near(const Actor* a, fx radius){
    Game* game = &g_game;
    for(int i = 0; i < game->actor_count; i++){
        Actor* b = &game->actors[i];
        if(b->type != OBJ_POWERSTAR || b->active) continue;
        if(v3_length_exact(v3_sub(b->position, a->position)) < radius) power_star_reveal(b);
    }
}

static void actor_hurt_mario(const Actor* a, int amount){ mario_damage(a->position, amount); }

static Actor* actor_overlapping_kind(const Actor* self, v3 centre, fx radius, const uint16_t* kinds, int n){
    Game* game = &g_game;
    for(int i = 0; i < game->actor_count; i++){
        Actor* b = &game->actors[i];
        if(!b->active || b == self || b->state == AS_DEFEATED) continue;
        bool kind = false;
        for(int k = 0; k < n; k++) kind |= b->type == kinds[k];
        if(!kind) continue;
        v3 d = v3_sub(actor_centre(b), centre);
        fx r = radius + b->radius;
        if(fx_abs(d.x) > r || fx_abs(d.y) > r || fx_abs(d.z) > r) continue;
        if(v3_length_squared64(d) < (uint64_t)((int64_t)r * r)) return b;
    }
    return NULL;
}

static bool actor_bounce_wall(Actor* a, fx ahead, int32_t keep14){
    v3 v = v3_reject14(a->velocity, a->up);
    if((v.x | v.y | v.z) == 0) return false;
    v3 dir = v3_direction14(v, a->front);
    v3 from = v3_add(a->position, v3_scale14(a->up, FX(60)));
    RayHit hit;
    if(!collision_ray(from, v3_add(from, v3_scale14(dir, ahead)), &hit, 0) ||
       v3_dot14(hit.normal, a->up) > R14(0.5f))
        return false;
    int32_t into = v3_dot14(a->velocity, hit.normal);
    if(into < 0) a->velocity = v3_scale14(v3_sub(a->velocity, v3_scale14(hit.normal, 2 * into)), keep14);
    return true;
}

static inline bool spin_pressed(void){ return (g_game.keys.pressed & KEY_B) != 0; }

static fx rail_nearest_coord(int rail, v3 p, fx step){
    fx len = rail_length(rail), best = 0;
    uint64_t best_d = ~0ull;
    for(fx c = 0; c <= len; c += step){
        uint64_t d = v3_length_squared64(v3_sub(rail_position_at(rail, c), p));
        if(d < best_d){
            best_d = d;
            best = c;
        }
    }
    return best;
}

static v3 rail_tangent(int rail, fx c, v3 fallback){
    fx len = rail_length(rail);
    fx a = fx_max(c - FX(20), 0), b = fx_min(c + FX(20), len);
    return v3_direction14(v3_sub(rail_position_at(rail, b), rail_position_at(rail, a)), fallback);
}

static v3 v3_turn_toward(v3 front, v3 want, v3 up, fx step){
    want = v3_tangent14(want, up, front);
    fx ang = angle_between14(front, want);
    if(ang <= step) return want;
    fx sign = v3_dot14(v3_cross14(front, want), up) >= 0 ? 1 : -1;
    return v3_tangent14(v3_rotate_axis14(front, up, step * sign), up, front);
}

static inline v3 gravity_up_at(v3 p, v3 fallback){
    GravitySample g = gravity_sample(g_game.stage.header, p);
    return g.index >= 0 ? g.up : fallback;
}

static inline void actor_stage_scale(const Actor* a, int16_t s[3]){
    const StageObject* o = actor_stage_object(a);
    for(int k = 0; k < 3; k++) s[k] = o && o->scale[k] ? o->scale[k] : 256;
}

static inline int16_t actor_stage_arg(const Actor* a, int k){
    const StageObject* o = actor_stage_object(a);
    return o ? o->args[k] : -1;
}

static void actor_set_anim(Actor* a, int clip){
    if(a->anim == clip) return;
    a->anim = clip;
    a->anim_frame = 0;
}

static void actor_restart_anim(Actor* a, int clip){
    a->anim = clip;
    a->anim_frame = 0;
}

static bool actor_anim_frame_done(const Actor* a, int fallback){
    const FigureHeader* f = (const FigureHeader*)actor_kinds[a->type].figure;
    if(f && f->clip_count && a->anim < (int)f->clip_count)
        return (a->anim_frame >> 8) >= anim_length_of(f, a->anim);
    return (a->anim_frame >> 8) >= fallback;
}

static void actor_rotate_yaw(Actor* a, fx angle){
    a->front = v3_tangent14(v3_rotate_axis14(a->front, a->up, angle), a->up, a->front);
}

static void actor_show_model(Actor* a, bool on){
    a->figure.model = on ? (const FigureHeader*)actor_kinds[a->type].figure : NULL;
}

static Actor* actor_active_at(int slot, int type){
    if(slot < 0 || slot >= g_game.actor_count) return NULL;
    Actor* a = &g_game.actors[slot];
    return a->active && a->type == type ? a : NULL;
}
