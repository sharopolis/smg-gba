enum { SQ_FREE = 64, SQ_INVALID, SQ_START, SQ_WAIT, SQ_TURNL, SQ_TURNR, SQ_TURNEND, SQ_CLIMB, SQ_FALL };
static EWRAM_BSS int16_t g_square_pole;
static void pole_square_reset(void){ g_square_pole = -1; }
static bool pole_square_bound(void){
    if(g_square_pole < 0 || g_square_pole >= g_game.actor_count) return false;
    const Actor* a = &g_game.actors[g_square_pole];
    return a->active && a->type == OBJ_POLESQUARE && g_game.player.action == MA_POLE_BIND;
}
static fx pole_square_height(const Actor* a){
    const StageObject* o = actor_stage_object(a);
    return o ? (100 * (int)o->scale[1] + 128) >> 8 : 1800;
}
static void pole_square_state(Actor* a, int state, int anim){
    actor_set_state(a, state, -1);
    if(anim >= 0) mario_restart_anim(anim);
    g_game.player.anim_speed = state == SQ_CLIMB ? FXC(1.6f) : FX_ONE;
}
static v3 pole_square_face(const Actor* a, v3 face){
    v3 side = v3_cross14(a->up, a->front);
    int x = v3_dot14(face, side), z = v3_dot14(face, a->front);
    return fx_abs(x) > fx_abs(z) ? (x < 0 ? v3_neg(side) : side) : (z < 0 ? v3_neg(a->front) : a->front);
}
static void pole_square_place(Actor* a, v3 at){
    int set = mappart_set(a, kcl_polesquare, KCL_POLESQUARE_COUNT);
    if(set < 0) return;

    MoverFrame f = mover_frame_of(at, a->up, a->front, N14_ONE);
    movers_place(set, &f);
    ColDynSet* d = &g_col_dyn_sets[set];
    for(int i = 0; i < d->count; i++){
        CollisionTriangle* t = &g_col_dyn[d->first + i];
        t->attr = COL_NOT_GRAB | COL_NO_WALL_JUMP;
        t->floor_code = 0;
        t->wall_code = 8;
    }
    movers_enable(set, true);
}
static void pole_square_init(Actor* a, const StageObject* o){
    a->value = -1;
    a->figure.scale = V3(64, 64, 64);
    actor_set_state(a, SQ_FREE, -1);
    const uint8_t* model = figure_polesquare;
    switch(pole_square_height(a)){
    case 1220: model = figure_polesquare1220; break;
    case 1271: model = figure_polesquare1271; break;
    case 1301: model = figure_polesquare1301; break;
    case 1700: model = figure_polesquare1700; break;
    case 600: model = figure_polesquare600; break;
    case 1390: model = figure_polesquare1390; break;
    case 991: model = figure_polesquare991; break;
    }
    a->figure.model = (const FigureHeader*)model;
    a->figure.pose = model_rest_pose(a->figure.model);
}
static void pole_square_pose(Actor* a){
    Player* p = &g_game.player;
    a->position = v3_add(a->home, v3_scale14(a->up, a->velocity.x >> 8));
    p->figure.position = a->position;
    g_mario.position16 = V3(a->position.x << 8, a->position.y << 8, a->position.z << 8);
    p->up = a->up;
    p->front = a->target;
    p->on_ground = false;
}
static void pole_square_leave(Actor* a, bool jump){
    Player* p = &g_game.player;
    v3 out = v3_neg(a->target);
    if((a->state == SQ_TURNL || a->state == SQ_TURNR) && a->timer - 1 > 7)
        out = v3_rotate_axis14(out, a->up, a->state == SQ_TURNL ? DEG(-90) : DEG(90));
    v3 at = jump ? v3_add(p->figure.position, v3_scale14(out, 50)) : v3_add(a->home, v3_scale14(out, 30));
    p->figure.position = at;
    g_mario.position16 = V3(at.x << 8, at.y << 8, at.z << 8);
    p->front = jump ? out : a->target;
    g_square_pole = -1;
    if(jump){
        mario_take_off(JUMP_FORCED, v3_add(v3_scale14(out, G(10)), v3_scale14(a->up, G(25))), true);
        mario_set_action(MA_FALL);
        mario_restart_anim(ANIM_MARIO_WALLJUMP);
        audio_play_effect(SFX_TRAPEZE_JUMP, 2);
    } else {
        g_mario.jumping = false;
        g_mario.jump_vec = V3(0, 0, 0);
        p->on_ground = false;
        mario_set_action(MA_WAIT);
        mario_restart_anim(ANIM_MARIO_WAIT);
    }
    pole_square_state(a, SQ_INVALID, -1);
}
static void pole_square_think(Actor* a){
    Player* p = &g_game.player;
    fx height = pole_square_height(a);
    v3 center = v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT));
    fx h = fx_clamp(v3_dot14(v3_sub(center, a->home), a->up), 0, height + 100);
    if(a->state <= SQ_INVALID) a->position = v3_add(a->home, v3_scale14(a->up, h));
    if(a->state == SQ_INVALID){
        if(p->on_ground || a->timer - 1 > 45 ||
           v3_length_squared64(v3_sub(center, a->position)) > (uint64_t)300 * 300)
            actor_set_state(a, SQ_FREE, -1);
    } else if(a->state == SQ_FREE){
        if(!p->on_ground && !g_mario_held && p->action != MA_DEATH && p->action != MA_TALK &&
           p->action != MA_STAR_LAUNCH && g_square_pole < 0 && g_kamella.held_shell < 0){
            v3 d = v3_sub(a->position, center), flat = v3_reject14(d, a->up);
            int32_t speed = v3_length_exact(v3_reject14(g_mario.jump_vec, a->up));
            int32_t vertical = v3_dot14(g_mario.jump_vec, a->up);
            int reach = 60 + C_BODY_RADIUS;
            if(v3_length_squared64(d) <= (uint64_t)reach * reach && !(speed < G(5) && vertical > G(1))){
                a->target = pole_square_face(a, v3_direction14(flat, a->front));
                a->velocity = V3(fx_clamp(h, 120, height) << 8, 0, 0);
                g_square_pole = (int16_t)actor_index(a);
                mario_stop_walk();
                g_mario.jumping = false;
                g_mario.jump_vec = V3(0, 0, 0);
                mario_set_action(MA_POLE_BIND);
                p->on_ground = false;
                pole_square_state(a, speed > G(6) ? SQ_START : SQ_WAIT,
                                  speed > G(6) ? ANIM_MARIO_SQUAREPOLESTART : ANIM_MARIO_SQUAREPOLEWAIT);
                audio_play_effect(SFX_POLE_GRAB, 2);
                pole_square_pose(a);
            }
        }
    } else {
        if(g_square_pole != actor_index(a) || p->action != MA_POLE_BIND){
            if(g_square_pole == actor_index(a)) g_square_pole = -1;
            actor_set_state(a, SQ_INVALID, -1);
        } else if(g_game.keys.pressed & KEY_A){
            pole_square_leave(a, true);
        } else {
            bool left = g_game.keys.held & KEY_LEFT, right = g_game.keys.held & KEY_RIGHT;
            bool up = g_game.keys.held & KEY_UP, down = g_game.keys.held & KEY_DOWN;
            const StageObject* o = actor_stage_object(a);
            if(o && o->args[1] > 0){
                bool t = left;
                left = right;
                right = t;
                t = up;
                up = down;
                down = t;
            }
            if(a->state == SQ_START){
                if(mario_anim_done()) pole_square_state(a, SQ_WAIT, ANIM_MARIO_SQUAREPOLEWAIT);
            } else if(a->state == SQ_TURNL || a->state == SQ_TURNR){
                if(mario_anim_done()){
                    a->target = v3_rotate_axis14(a->target, a->up, a->state == SQ_TURNL ? DEG(-90) : DEG(90));
                    a->target = pole_square_face(a, a->target);
                    pole_square_state(a, SQ_TURNEND, ANIM_MARIO_SQUAREPOLEWAIT);
                }
            } else if((left || right) && a->state != SQ_TURNEND){
                pole_square_state(a, left ? SQ_TURNL : SQ_TURNR,
                                  left ? ANIM_MARIO_SQUAREPOLETURNL : ANIM_MARIO_SQUAREPOLETURNR);
            } else if(a->state == SQ_TURNEND && (left || right)){
            } else if(up){
                a->velocity.y = 0;
                a->velocity.x = fx_min(a->velocity.x + G(6), height << 8);
                int anim =
                    a->velocity.x == height << 8 ? ANIM_MARIO_SQUAREPOLEPUSHWAIT : ANIM_MARIO_SQUAREPOLECLIMB;
                if(a->state != SQ_CLIMB || p->anim != anim) pole_square_state(a, SQ_CLIMB, anim);
                p->anim_speed = anim == ANIM_MARIO_SQUAREPOLECLIMB ? FXC(1.6f) : FX_ONE;
            } else if(down){
                if(a->state != SQ_FALL) pole_square_state(a, SQ_FALL, ANIM_MARIO_SQUAREPOLEFALL);
                a->velocity.y = fx_max(a->velocity.y - G(.5f), -G(18));
                a->velocity.x += a->velocity.y;
                if(a->velocity.x <= 50 << 8) pole_square_leave(a, false);
            } else {
                a->velocity.y = 0;
                if(a->state != SQ_WAIT) pole_square_state(a, SQ_WAIT, ANIM_MARIO_SQUAREPOLEWAIT);
            }
            if(pole_square_bound()) pole_square_pose(a);
        }
    }
    pole_square_place(a, a->position);
}
