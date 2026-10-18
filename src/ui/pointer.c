
#define PTR_MARGIN 5

typedef struct {
    bool active;
    int32_t x, y;
    int16_t held_ticks;
    int8_t hand;
    uint16_t raw_held, raw_pressed;
    bool a_pressed, a_held, b_pressed;
    int16_t shoot_step;
    int16_t no_piece_timer;
    uint8_t last_hit;
    bool pull_mode;
    int16_t pull_target;
    uint16_t pull_pause;
} StarPointer;

static EWRAM_BSS StarPointer g_ptr;

static void pointer_reset(void){
    g_ptr = (StarPointer){0};
    g_ptr.hand = -1;
    g_ptr.pull_target = -1;
    g_ptr.x = 120 << 4;
    g_ptr.y = 80 << 4;
}

static bool pointer_active(void){ return g_ptr.active; }
static void pointer_screen(int* x, int* y){
    *x = g_ptr.x >> 4;
    *y = g_ptr.y >> 4;
}
static bool pointer_pressed(void){ return g_ptr.active && g_ptr.a_pressed; }
static bool pointer_held(void){ return g_ptr.active && g_ptr.a_held; }
static bool pointer_shoot_held(void){ return g_ptr.active && (g_ptr.raw_held & KEY_B); }
static bool pointer_shoot_pressed(void){ return g_ptr.active && g_ptr.b_pressed; }
static void pointer_set_hand(int hand){ g_ptr.hand = (int8_t)(hand < -1 || hand > 2 ? -1 : hand); }

static void pointer_world_ray(v3* origin, v3* dir){
    const Camera* c = &g_game.camera;
    v3 left = v3_cross14(c->up, c->front);
    int32_t sx = g_ptr.x - (120 << 4), sy = g_ptr.y - (80 << 4);

    v3 d = v3_add(c->front, v3_add(v3_scale14(left, -(sx * 16384 / 193) >> 4),
                                   v3_scale14(c->up, -(sy * 16384 / 193) >> 4)));
    *origin = c->position;
    *dir = v3_direction14(d, c->front);
}

static bool pointer_is_pointing_at(v3 p, fx radius3d, v3 left){
    const Camera* c = &g_game.camera;
    v3 d = v3_sub(p, c->position);
    int32_t z = v3_dot14(d, c->front);
    if(z < FX(50) || z > FX(20000)) return false;
    int32_t x = v3_dot14(d, left), y = v3_dot14(d, c->up);
    int32_t r = radius3d * 193 / z + PTR_MARGIN;

    int32_t sx = (120 << 4) - x * (193 << 4) / z - g_ptr.x;
    int32_t sy = (80 << 4) - y * (193 << 4) / z - g_ptr.y;
    if(sx > (r << 4) || sx < -(r << 4) || sy > (r << 4) || sy < -(r << 4)) return false;
    return sx * sx + sy * sy < (r * r) << 8;
}

static bool pointer_is_pointing(const Actor* a, fx radius3d){
    if(!g_ptr.active) return false;
    const Camera* c = &g_game.camera;
    return pointer_is_pointing_at(actor_centre(a), radius3d, v3_cross14(c->up, c->front));
}

static bool pointer_fireball_reflect(Actor* a, v3* direction){
    int32_t dx = g_ptr.x - a->target.x, dy = g_ptr.y - a->target.y;
    bool previous = a->target.z != 0;
    a->target = V3(g_ptr.x, g_ptr.y, g_ptr.active && !g_ptr.pull_mode);
    if(!previous || !g_ptr.active || g_ptr.pull_mode || !g_ptr.a_held || !pointer_is_pointing(a, FX(150)))
        return false;
    int32_t sx = dx * 608 / 240, sy = dy * 456 / 160;
    if((int64_t)sx * sx + (int64_t)sy * sy <= (30 * 16) * (30 * 16)) return false;
    const Camera* c = &g_game.camera;
    v3 left = v3_cross14(c->up, c->front);
    *direction = v3_direction14(v3_add(v3_scale14(left, -dx), v3_scale14(c->up, -dy)), a->up);
    return true;
}

static fx pointer_radius(const Actor* a){
    const ActorKind* k = &actor_kinds[a->type];
    return k->pointer_radius ? k->pointer_radius(a) : a->radius;
}

static bool pointer_preferred(const Actor* a){
    const ActorKind* k = &actor_kinds[a->type];
    return k->pointer_preferred && k->pointer_preferred(a);
}

static bool pointer_swipe_target(void){
    for(int i = 0; i < g_game.actor_count; i++){
        const Actor* a = &g_game.actors[i];
        const ActorKind* k = &actor_kinds[a->type];
        if(a->active && k->pointer_swipe && k->pointer_swipe(a) && pointer_is_pointing(a, pointer_radius(a)))
            return true;
    }
    return false;
}

static bool pointer_targetable(const Actor* a){
    const ActorKind* k = &actor_kinds[a->type];
    if(k->pointer_target && !k->pointer_target(a)) return false;
    return a->active && a->state != AS_DEFEATED && a->figure.model && !(k->flags & KIND_NO_POINTER);
}

static Actor* pointer_target_actor(void){
    if(!g_ptr.active) return NULL;
    Actor* best = NULL;
    int32_t best_z = 0x7FFFFFFF;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(!pointer_targetable(a) || !pointer_is_pointing(a, pointer_radius(a))) continue;
        int32_t z = v3_dot14(v3_sub(a->position, g_game.camera.position), g_game.camera.front);

        bool preferred = pointer_preferred(a);
        bool best_preferred = best && pointer_preferred(best);
        if((preferred && !best_preferred) || (preferred == best_preferred && z < best_z)){
            best_z = z;
            best = a;
        }
    }
    return best;
}

static bool pointer_pull_usable(const Actor* a){
    const ActorKind* k = &actor_kinds[a->type];
    if(!k->pull_target || !k->pull_target(a)) return level_pull_target(a);
    fx range = FX(actor_arg_or(a, 1, 2500));
    return v3_length_squared64(v3_sub(actor_centre(a), g_game.player.figure.position)) <
           (uint64_t)range * range;
}

static Actor* pointer_pull_selected(void){
    if(!g_ptr.active || !g_ptr.pull_mode) return NULL;

    if(g_ptr.pull_target < 0 || g_ptr.pull_target >= g_game.actor_count ||
       !pointer_pull_usable(&g_game.actors[g_ptr.pull_target]))
        g_ptr.pull_target = (int16_t)pointer_pull_nearest();
    if(g_ptr.pull_target < 0) return NULL;
    Actor* a = &g_game.actors[g_ptr.pull_target];
    return pointer_pull_usable(a) ? a : NULL;
}

static bool pointer_pull_camera_targets(v3* selected, v3* held){
    Actor* a = pointer_pull_selected();
    if(!a) return false;
    *selected = actor_centre(a);
    *held = g_game.player.figure.position;
    const Actor* h = g_mario_held ? gcapture_held_actor() : NULL;
    if(h) *held = actor_centre(h);
    return true;
}

static int pointer_pull_nearest(void){
    int best = -1;
    uint64_t distance = UINT64_MAX;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(!pointer_pull_usable(a)) continue;
        uint64_t d = v3_length_squared64(v3_sub(actor_centre(a), g_game.player.figure.position));
        if(d < distance){
            distance = d;
            best = i;
        }
    }
    return best;
}

static void pointer_pull_cycle(int dx, int dy){
    Actor* current = pointer_pull_selected();
    if(!current || (!dx && !dy)) return;
    const Camera* c = &g_game.camera;
    v3 left = v3_cross14(c->up, c->front);
    int best = -1, fallback = -1;
    int64_t score = INT64_MIN;
    uint64_t near = UINT64_MAX;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(a == current || !pointer_pull_usable(a)) continue;
        v3 delta = v3_sub(actor_centre(a), actor_centre(current));
        uint64_t d = v3_length_squared64(delta);
        if(d < near){
            near = d;
            fallback = i;
        }
        fx x = -v3_dot14(delta, left), y = -v3_dot14(delta, c->up);
        fx along = dx * x + dy * y;
        if(along <= 0) continue;

        fx side = fx_abs(dx * y - dy * x);
        int64_t s = (int64_t)along * 16384 / (along + side + 1) - v3_length_exact(delta);
        if(s > score){
            score = s;
            best = i;
        }
    }
    if(best < 0) best = fallback;
    if(best >= 0) g_ptr.pull_target = (int16_t)best;
}

static void pointer_pull_project(void){
    Actor* a = pointer_pull_selected();
    if(!a) return;
    const Camera* c = &g_game.camera;
    v3 delta = v3_sub(actor_centre(a), c->position);
    fx depth = v3_dot14(delta, c->front);
    if(depth < FX(50)) depth = FX(50);
    int x = 120 - v3_dot14(delta, v3_cross14(c->up, c->front)) * 193 / depth;
    int y = 80 - v3_dot14(delta, c->up) * 193 / depth;
    g_ptr.x = fx_clamp(x, PTR_MARGIN, 239 - PTR_MARGIN) << 4;
    g_ptr.y = fx_clamp(y, PTR_MARGIN, 159 - PTR_MARGIN) << 4;
    g_ptr.hand = g_ptr.a_held ? 1 : 0;
}

static __attribute__((noinline)) bool pointer_launch_takeover_ready(void){
    const Player* p = &g_game.player;
    v3 body = v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT));
    fx radius = FX(300) + C_BODY_RADIUS;
    for(int i = 0; i < g_game.actor_count; i++){
        const Actor* a = &g_game.actors[i];
        if(!a->active || (a->flags & (ACTOR_HIDDEN | ACTOR_WAITING)) || a->state != STAR_WAIT ||
           a->timer <= FLIGHT_BIND_TIME || !flight_takeover_accepts(a))
            continue;
        if(v3_length_squared64(v3_sub(body, a->position)) <= (uint64_t)((int64_t)radius * radius))
            return true;
    }
    return false;
}

static void pointer_filter_keys(KeyState* keys){
    if(cutscene_blocks(BLOCK_INPUT)){
        *keys = (KeyState){0};
        return;
    }
    if(g_game.scene == SCENE_PLAY && cutscene_blocks(BLOCK_INPUT_EXCEPT_START)){
        keys->held &= KEY_START;
        keys->pressed &= KEY_START;
        return;
    }
    Game* game = &g_game;
    const Player* p = &game->player;
    bool rail_flight = p->action == MA_STAR_LAUNCH && g_flight.phase == FLIGHT_SHOOT;
    bool can = game->scene == SCENE_PLAY && !cutscene_blocks(BLOCK_POINTER_INPUT) && p->action != MA_DEATH &&
               (p->action != MA_STAR_LAUNCH || rail_flight) && p->action != MA_STAR_GET &&
               (p->action != MA_TALK || (g_gcapture_held >= 0 && g_mario_held));
    bool manual = (keys->held & KEY_L) && (keys->held & KEY_R);
    if(g_ptr.pull_pause) g_ptr.pull_pause--;
    bool capture = g_mario_held && g_gcapture_held >= 0;
    bool pull = can && !rail_flight && !manual && !g_ptr.pull_pause &&
                (capture || ((g_debug_options & DEBUG_PULL_ASSIST) &&
                             (camera_pull_preview_allowed() || level_pull_preview())));
    if(pull){
        if(g_ptr.pull_target < 0 || g_ptr.pull_target >= game->actor_count ||
           !pointer_pull_usable(&game->actors[g_ptr.pull_target]))
            g_ptr.pull_target = (int16_t)pointer_pull_nearest();
        pull = g_ptr.pull_target >= 0;
    }

    if((keys->pressed & KEY_B) && g_mario_held && g_gcapture_held >= 0 &&
       g_gcapture_held < game->actor_count){
        g_ptr.pull_pause = 90;
        pull = false;
        gcapture_let_go(&game->actors[g_gcapture_held], game->actors[g_gcapture_held].velocity);
    } else if(pull && (keys->pressed & KEY_B)){
        g_ptr.pull_pause = 90;
        pull = false;
    }
    g_ptr.pull_mode = pull;
    if(!pull) audio_stop_level_effect(SFX_CAPTURE_POINT);
    bool want = can && (mario_launcher_aiming() ||
                        (rail_flight && (g_debug_options & DEBUG_FLIGHT_POINTER)) || manual || pull);
    if(want && !g_ptr.active){
        g_ptr.active = true;
        g_ptr.x = 120 << 4;
        g_ptr.y = 80 << 4;
        g_ptr.shoot_step = 0;
    } else if(!want && g_ptr.active){
        g_ptr.active = false;
        g_ptr.hand = -1;
    }
    g_ptr.raw_held = keys->held;
    g_ptr.raw_pressed |= keys->pressed;
    bool launch_shake = rail_flight && !manual && (keys->pressed & KEY_B) && pointer_launch_takeover_ready();
    if(launch_shake){
        g_ptr.raw_held &= ~KEY_B;
        g_ptr.raw_pressed &= ~KEY_B;
    }

    if((keys->held & KEY_L) && !(keys->held & KEY_R)){
        g_ptr.raw_held &= ~(KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT);
        g_ptr.raw_pressed &= ~(KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT);
    }
    if(g_ptr.active){
        uint16_t mine = KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_B | KEY_L | KEY_R |
                        (rail_flight && g_flight.star < 0 ? 0 : KEY_A);
        if(pull){
            mine = KEY_A;
            if(g_mario_held || !p->on_ground || (keys->held & KEY_A))
                mine |= KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT;
        }
        if(launch_shake) mine &= ~KEY_B;
        keys->held &= (uint16_t)~mine;
        keys->pressed &= (uint16_t)~mine;
    }
}

static void pointer_collect_bit(void){
    Game* game = &g_game;
    game->star_bits++;
    if(game->star_bits % 50 == 0) game->lives++;
    audio_play_effect(SFX_STARBIT, 1);
}

static Actor* pointer_launch(v3 from, v3 goal, const Actor* target, bool gift){
    Game* game = &g_game;
    if(game->star_bits <= 0) return NULL;

    if(game->actor_count == MAX_ACTORS && !actors_stream_free_slot())
        stream_evict(game->player.figure.position);
    Actor* s = actor_spawn(OBJ_STARBIT, from, game->player.up, game->camera.front);
    if(!s) return NULL;
    s->figure.model = (const FigureHeader*)starbit_figures[game->tick % 6];
    s->flags = PTR_SHOT | (gift ? PTR_GIFT : 0);
    s->home = from;
    s->target = goal;
    s->value = target ? (int16_t)(target - game->actors) : -1;
    s->timer = 0;
    game->star_bits--;
    return s;
}

static bool pointer_gift_starbit(Actor* target){
    const Player* p = &g_game.player;
    return pointer_launch(v3_add(p->figure.position, v3_scale14(p->up, FX(100))), actor_centre(target),
                          target, true) != NULL;
}

static void pointer_shoot(void){
    Game* game = &g_game;
    const Camera* c = &game->camera;
    v3 origin, dir;
    pointer_world_ray(&origin, &dir);
    Actor* target = pointer_target_actor();

    for(int i = 0; i < game->actor_count; i++){
        const Actor* r = &game->actors[i];
        const ActorKind* k = &actor_kinds[r->type];
        if(r->active && k->pointer_blocks_shot && k->pointer_blocks_shot(r) &&
           pointer_is_pointing(r, pointer_radius(r)))
            return;
    }
    v3 goal;
    RayHit hit;
    v3 far = v3_add(
        origin, v3_scale14(dir, FX(3000) + v3_length_exact(v3_sub(game->player.figure.position, origin))));
    if(target) goal = actor_centre(target);
    else if(collision_ray(origin, far, &hit, COL_NO_CAMERA)) goal = hit.point;
    else goal = far;
    v3 left = v3_cross14(c->up, c->front);
    v3 from = v3_add(c->position, v3_add(v3_scale14(left, -FX(300)), v3_scale14(c->front, FX(30))));
    if(pointer_launch(from, goal, target, false)) audio_play_effect(SFX_STARBIT, 2);
}

static void pointer_star_piece_hit(Actor* a, bool gift){
    const ActorKind* kind = &actor_kinds[a->type];
    if(gift){
        if(kind->star_piece_gift) kind->star_piece_gift(a);
        return;
    }
    g_ptr.last_hit = (uint8_t)(a->type + 1);
    if(kind->star_piece) kind->star_piece(a);
}

static inline bool pointer_actor_hook(Actor* a){
    if(!(a->flags & (PTR_SHOT | PTR_HOMING))) return false;
    return pointer_actor_think(a);
}

static bool pointer_actor_think(Actor* a){
    Game* game = &g_game;
    if(a->type == OBJ_STARBIT && (a->flags & PTR_SHOT)){
        int t = a->timer;
        Actor* target = a->value >= 0 && a->value < game->actor_count ? &game->actors[a->value] : NULL;
        if(target && !target->active) target = NULL;
        if(t <= 30){
            v3 goal = target ? actor_centre(target) : a->target;
            v3 prev = a->position;
            a->position = v3_add(a->home, v3_scale14(v3_sub(goal, a->home), (t << 14) / 30));
            a->velocity = v3_sub(a->position, prev);
            if(target &&
               (t >= 30 || v3_length_squared64(v3_sub(a->position, goal)) < (uint64_t)FX(60) * FX(60))){
                pointer_star_piece_hit(target, a->flags & PTR_GIFT);
                collectible_dead(a);
                return true;
            }

            RayHit hit;
            if(!(a->flags & PTR_GIFT) &&
               (t > 28 || v3_length_squared64(v3_sub(a->position, game->camera.position)) >
                              (uint64_t)FX(1500) * FX(1500)) &&
               collision_ray(prev, a->position, &hit, COL_NO_CAMERA)){
                collectible_dead(a);
                return true;
            }
        } else {
            a->velocity = v3_sub(v3_scale14(a->velocity, R14(0.97f)), v3_scale14(game->player.up, FX(1)));
            a->position = v3_add(a->position, a->velocity);
            if(t > 90) collectible_dead(a);
        }
        return true;
    }
    if(a->type == OBJ_STARBIT && (a->flags & PTR_HOMING)){
        const Player* p = &game->player;
        v3 to = v3_sub(v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT)), a->position);
        int32_t len = v3_length_exact(to);
        if(len < FX(160)){
            collectible_dead(a);
            pointer_collect_bit();
            return true;
        }
        int32_t chase = fx_min((a->timer + 1) * R14(0.03f), N14_ONE);
        fx speed = FX(1) + mul14(FX(85), chase);
        v3 dir = v3_direction14(to, p->up);
        v3 motion = v3_scale14(dir, speed);

        v3 player_vel = p->action == MA_STAR_LAUNCH
                            ? g_flight.vel
                            : V3(g_mario.jump_vec.x >> 8, g_mario.jump_vec.y >> 8, g_mario.jump_vec.z >> 8);
        if(player_vel.x | player_vel.y | player_vel.z){
            int32_t dot = v3_dot14(dir, v3_direction14(player_vel, dir));
            if(dot > 0) motion = v3_add(motion, v3_scale14(player_vel, mul14(chase, dot)));
        }
        a->position = v3_add(a->position, motion);
        return true;
    }
    return false;
}

static void pointer_tick(void){
    if(cutscene_blocks(BLOCK_POINTER_UPDATE)) return;
    Game* game = &g_game;
    uint16_t held = g_ptr.raw_held, pressed = g_ptr.raw_pressed;
    g_ptr.raw_pressed = 0;
    g_ptr.a_pressed = g_ptr.b_pressed = false;
    g_ptr.a_held = false;

    const Player* p = &game->player;
    if((g_debug_options & DEBUG_MAGNET) && p->on_ground && !g_mario_held && !talk_active() &&
       p->action != MA_STAR_LAUNCH && p->action != MA_DEATH){
        v3 centre = v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT));
        for(int i = 0; i < game->actor_count; i++){
            Actor* a = &game->actors[i];
            if(!a->active || a->type != OBJ_STARBIT || !collectible_drop_ready(a) ||
               (a->flags & (PTR_SHOT | PTR_HOMING | ACTOR_HIDDEN)))
                continue;
            if(v3_length_squared64(v3_sub(a->position, centre)) <= (uint64_t)FX(300) * FX(300)){
                a->flags |= PTR_HOMING;
                a->timer = 0;
            }
        }
    }
    if(!g_ptr.active) return;

    int dx = ((held & KEY_RIGHT) != 0) - ((held & KEY_LEFT) != 0);
    int dy = ((held & KEY_DOWN) != 0) - ((held & KEY_UP) != 0);
    if(g_ptr.pull_mode){
        int sx = ((pressed & KEY_RIGHT) != 0) - ((pressed & KEY_LEFT) != 0);
        int sy = ((pressed & KEY_DOWN) != 0) - ((pressed & KEY_UP) != 0);
        pointer_pull_cycle(sx, sy);
        g_ptr.held_ticks = 0;
    } else if(dx || dy){
        g_ptr.held_ticks++;
        int sp = 24 + (g_ptr.held_ticks < 20 ? g_ptr.held_ticks * 2 : 40);

        if((held & KEY_A) && pointer_swipe_target()) sp = 12 << 4;
        g_ptr.x += dx * sp;
        g_ptr.y += dy * sp;
        if(g_ptr.x < 0) g_ptr.x = 0;
        if(g_ptr.x > (239 << 4)) g_ptr.x = 239 << 4;
        if(g_ptr.y < 0) g_ptr.y = 0;
        if(g_ptr.y > (159 << 4)) g_ptr.y = 159 << 4;
    } else {
        g_ptr.held_ticks = 0;
    }
    g_ptr.a_pressed = pressed & KEY_A;
    g_ptr.a_held = held & KEY_A;
    g_ptr.b_pressed = pressed & KEY_B;
    if(mario_launcher_aiming()) return;
    if(g_ptr.pull_mode){
        pointer_pull_project();
        return;
    }

    if(pressed & KEY_B){
        g_ptr.shoot_step = 1;
        if(game->star_bits > 0) pointer_shoot();
        else g_ptr.no_piece_timer = 20;
    } else if((held & KEY_B) && g_ptr.shoot_step > 0){
        if(++g_ptr.shoot_step >= 14){
            g_ptr.shoot_step = 1;
            if(game->star_bits > 0) pointer_shoot();
        }
    } else {
        g_ptr.shoot_step = 0;
    }
    if(g_ptr.no_piece_timer > 0) g_ptr.no_piece_timer--;

    if(game->tick & 1) return;
    v3 mario = game->player.figure.position;
    v3 left = v3_cross14(game->camera.up, game->camera.front);
    for(int i = 0; i < game->actor_count; i++){
        Actor* a = &game->actors[i];
        if(a->type != OBJ_STARBIT || !a->active || !collectible_drop_ready(a) ||
           (a->flags & (PTR_SHOT | PTR_HOMING)))
            continue;
        v3 d = v3_sub(a->position, mario);
        if(fx_abs(d.x) > FX(15000) || fx_abs(d.y) > FX(15000) || fx_abs(d.z) > FX(15000)) continue;
        if(pointer_is_pointing_at(a->position, FX(28), left)){
            a->flags |= PTR_HOMING;
            a->timer = 0;
        }
    }
}

static void pointer_draw(void){
    if(cutscene_blocks(BLOCK_INPUT)) return;
    if(!g_ptr.active) return;
    if(g_ptr.pull_mode) pointer_pull_project();
    ovl_palette(2);
    int x = g_ptr.x >> 4, y = g_ptr.y >> 4;
    if(g_ptr.hand >= 0){
        ovl_upload(OVL_REGION_HAND, TALK_IMG_HAND0 + g_ptr.hand);
        ovl_draw(OVL_REGION_HAND, x + 3 - 16, y + 7 - 16);
        return;
    }
    ovl_upload(OVL_REGION_POINTER, TALK_IMG_POINTER);

    if(g_ptr.no_piece_timer & 2) return;
    ovl_draw(OVL_REGION_POINTER, x - 8, y - 8);
}
