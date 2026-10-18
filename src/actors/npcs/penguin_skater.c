#define PS_OWNS_MARIO 0x100

static const PenguinSkaterSpec* penguin_spec(const Actor* a){
    return a->args[0] >= 0 && a->args[0] < PENGUIN_SKATER_SPEC_COUNT ? &penguin_skater_specs[a->args[0]] : 0;
}
static v3 penguin_vec(const int32_t p[3]){ return V3(p[0], p[1], p[2]); }
static v3 penguin_basis(const int16_t p[3]){ return V3(p[0], p[1], p[2]); }
static v3 penguin_rail_dir(int rail, fx coord){
    return v3_direction14(v3_sub(rail_position_at(rail, coord + 8), rail_position_at(rail, coord - 8)),
                          V3(0, N14_ONE, 0));
}

static bool penguin_part_fast(const StageRailPart* part){
    for(int i = 1; i < 4; i++)
        for(int k = 0; k < 3; k++)
            if(fx_abs(part->p[i][k] - part->p[0][k]) > 131071) return false;
    return true;
}
static v3 penguin_part_eval(const StageRailPart* part, int32_t at){
    v3 p0 = V3(part->p[0][0], part->p[0][1], part->p[0][2]);
    int32_t u = N14_ONE - at,
            b[3] = {mul14(mul14(u, u), at) * 3, mul14(mul14(u, at), at) * 3, mul14(mul14(at, at), at)};
    v3 result = p0;
    for(int i = 1; i < 4; i++){
        v3 delta = V3(part->p[i][0] - p0.x, part->p[i][1] - p0.y, part->p[i][2] - p0.z);
        result = v3_add(
            result, V3((delta.x * b[i - 1]) >> 14, (delta.y * b[i - 1]) >> 14, (delta.z * b[i - 1]) >> 14));
    }
    return result;
}

static EWRAM_BSS struct {
    const StageHeader* stage;
    int rail;
    v3 pos;
    fx coord;
} g_penguin_nearest;
static fx penguin_nearest_coord(int rail, v3 point){
    const StageRail* r = rail_get(rail);
    const StageRailPart* parts = &g_game.stage.rail_parts[r->first_part];
    if(!r->part_count) return 0;
    int best_part[2] = {0, 0}, best_t[2] = {0, 0};
    uint64_t best[2] = {UINT64_MAX, UINT64_MAX};
    for(int i = 0; i < r->part_count; i++){
        int local_t = 0;
        uint64_t local = UINT64_MAX;
        bool fast = penguin_part_fast(&parts[i]);
        for(int k = 0; k <= 8; k++){
            int at = k * (N14_ONE / 8);
            uint64_t d = v3_length_squared64(
                v3_sub(fast ? penguin_part_eval(&parts[i], at) : rail_part_eval(&parts[i], at), point));
            if(d < local){
                local = d;
                local_t = at;
            }
        }
        if(local < best[0]){
            best[1] = best[0];
            best_part[1] = best_part[0];
            best_t[1] = best_t[0];
            best[0] = local;
            best_part[0] = i;
            best_t[0] = local_t;
        } else if(local < best[1]){
            best[1] = local;
            best_part[1] = i;
            best_t[1] = local_t;
        }
    }
    fx result = 0;
    uint64_t nearest = UINT64_MAX;
    for(int j = 0; j < (r->part_count > 1 ? 2 : 1); j++){
        const StageRailPart* p = &parts[best_part[j]];
        bool fast = penguin_part_fast(p);
        int lo = fx_max(0, best_t[j] - N14_ONE / 8), hi = fx_min(N14_ONE, best_t[j] + N14_ONE / 8);
        for(int i = 0; i < 12 && hi - lo > 3; i++){
            int a = lo + (hi - lo) / 3, b = hi - (hi - lo) / 3;
            uint64_t da =
                v3_length_squared64(v3_sub(fast ? penguin_part_eval(p, a) : rail_part_eval(p, a), point));
            uint64_t db =
                v3_length_squared64(v3_sub(fast ? penguin_part_eval(p, b) : rail_part_eval(p, b), point));
            if(da < db) hi = b;
            else lo = a;
        }
        int at = (lo + hi) / 2, k = at >> 10;
        fx coord;
        if(k >= 16) coord = p->start + p->length;
        else {
            uint32_t a = k ? p->lut[k - 1] : 0, b = p->lut[k];
            uint32_t share = a + (((b - a) * (uint32_t)(at & 1023)) >> 10);
            coord = p->start + (fx)((int64_t)p->length * share / 65535);
        }
        uint64_t d = v3_length_squared64(v3_sub(rail_position_at(rail, coord), point));
        if(d < nearest){
            nearest = d;
            result = coord;
        }
    }
    return result;
}
static fx penguin_lead(const Actor* a){
    int rail = a->value;
    v3 pos = g_game.player.figure.position;
    fx player;
    if(g_penguin_nearest.stage == g_game.stage.header && g_penguin_nearest.rail == rail &&
       pos.x == g_penguin_nearest.pos.x && pos.y == g_penguin_nearest.pos.y &&
       pos.z == g_penguin_nearest.pos.z)
        player = g_penguin_nearest.coord;
    else {
        player = penguin_nearest_coord(rail, pos);
        g_penguin_nearest.stage = g_game.stage.header;
        g_penguin_nearest.rail = rail;
        g_penguin_nearest.pos = pos;
        g_penguin_nearest.coord = player;
    }
    fx coord = a->target.x >> 8, length = rail_length(rail), diff = fx_abs(coord - player);
    bool ahead = coord > player;
    if(a->target.z > 0){
        if((ahead && diff > length / 2) || (!ahead && diff < length / 2)) diff = -diff;
    } else if((ahead && diff < length / 2) || (!ahead && diff > length / 2)) diff = -diff;
    return diff;
}
static bool penguin_provoke(fx lead, int range, int rail){
    return fx_abs(rail_length(rail) / 2 - fx_abs(lead)) < range;
}
static void penguin_follow(Actor* a, int blend){
    v3 pos = rail_position_at(a->value, a->target.x >> 8);
    a->up = gravity_sample(g_game.stage.header, pos).up;
    RayHit hit;
    if(collision_ray(v3_add(pos, v3_scale14(a->up, 10)), v3_sub(pos, v3_scale14(a->up, 990)), &hit, 0))
        pos = hit.point;
    v3 front = penguin_rail_dir(a->value, a->target.x >> 8);
    if(a->target.z < 0) front = v3_neg(front);
    a->front =
        v3_tangent14(v3_add(v3_scale14(a->front, N14_ONE - blend), v3_scale14(front, blend)), a->up, front);
    a->position = pos;
}
static int penguin_move(Actor* a, int target, int brake, int blend){
    int32_t speed = a->target.y, goal = V16(target), rate = brake ? V16(.2f) : V16(.15f);
    if(fx_abs(speed - goal) < rate) speed = goal;
    else speed += speed < goal ? rate : -rate;
    int32_t old = a->target.x, coord = old + speed * a->target.z, len = rail_length(a->value) << 8;
    const StageRail* rail = rail_get(a->value);
    if(rail->closed && len){
        coord %= len;
        if(coord < 0) coord += len;
    } else coord = fx_clamp(coord, 0, len);
    int crossed = -1;
    for(int i = 0; i < rail_point_count(a->value); i++){
        int32_t at = rail_point_coord(a->value, i) << 8;
        bool cross =
            a->target.z > 0
                ? ((old < coord) ? at > old && at <= coord : coord < old && (at > old || at <= coord))
                : ((old > coord) ? at < old && at >= coord : coord > old && (at < old || at >= coord));
        if(cross){
            crossed = i;
            break;
        }
    }
    a->target.x = coord;
    a->target.y = speed;
    penguin_follow(a, blend);
    return crossed;
}
static void penguin_reverse(Actor* a){
    a->args[3] = -a->args[3];
    a->target.z = a->args[3];
}
static bool penguin_switch_rail(Actor* a, int point){
    const PenguinSkaterSpec* s = penguin_spec(a);
    if(!s || point < 0) return false;
    v3 cross = rail_point(a->value, point);
    v3 dir = penguin_rail_dir(a->value, a->target.x >> 8);
    if(a->target.z < 0) dir = v3_neg(dir);
    for(int i = 0; i < s->count; i++){
        int rail = s->connectors[i];
        fx len = rail_length(rail);
        if(v3_length_squared64(v3_sub(cross, rail_position_at(rail, 0))) < 100 &&
           v3_dot14(dir, penguin_rail_dir(rail, 0)) > R14(.55f)){
            a->value = rail;
            a->target.x = 0;
            a->target.z = 1;
            return true;
        }
        if(v3_length_squared64(v3_sub(cross, rail_position_at(rail, len))) < 100 &&
           v3_dot14(dir, penguin_rail_dir(rail, len)) < -R14(.55f)){
            a->value = rail;
            a->target.x = len << 8;
            a->target.z = -1;
            return true;
        }
    }
    return false;
}
static void penguin_hold(Actor* a){
    if(g_mario_held || (a->health & PS_OWNS_MARIO)) return;
    a->velocity = g_game.player.figure.position;
    a->health |= PS_OWNS_MARIO;
    mario_hold_begin();
}
static void penguin_release(Actor* a){
    if(!(a->health & PS_OWNS_MARIO)) return;
    a->health &= ~PS_OWNS_MARIO;
    mario_hold_release(V3(0, 0, 0));
    Player* p = &g_game.player;
    if(p->has_floor_below && p->height_above_ground < 100){
        g_mario.jumping = false;
        g_mario.jump_vec = V3(0, 0, 0);
        p->on_ground = true;
        mario_set_action(MA_WAIT);
        mario_set_anim(ANIM_MARIO_WAIT);
    }
}
static void penguin_init(Actor* a, const StageObject* o){
    for(int i = 0; i < 4; i++) a->args[i] = -1;
    a->health = 0;
    for(int i = 0; i < PENGUIN_SKATER_SPEC_COUNT; i++){
        const PenguinSkaterSpec* s = &penguin_skater_specs[i];
        if(s->stage != g_game.stage_index || s->rail != o->rail ||
           v3_length_squared64(v3_sub(a->home, penguin_vec(s->placement))) > 16)
            continue;
        a->args[0] = i;
        a->args[1] = s->rail;
        a->args[2] = s->alternate;
        a->args[3] = 1;
        a->value = s->rail;
        a->target = V3(0, 0, 1);
        actor_set_state(a, PS_WAIT, ANIM_PENGUINSKATER_WAIT);
        penguin_follow(a, N14_ONE);

        if(switch_on(stage_object_switch(o, SWITCH_A))){
            a->position = penguin_vec(s->penguin_pos);
            a->up = penguin_basis(s->penguin_up);
            a->front = penguin_basis(s->penguin_front);
            actor_set_state(a, PS_SITDOWN, ANIM_PENGUINSKATER_SITDOWN);
        }
        return;
    }
    actor_set_state(a, PS_TALK, ANIM_PENGUINSKATER_WAIT);
}
static bool penguin_attackable(const Actor* a){
    return a->state == PS_AWAY || a->state == PS_TURN || a->state == PS_SWITCH;
}
static bool penguin_unclipped(const Actor* a){
    return a->type == OBJ_PENGUINSKATER && a->state >= PS_DEMO && a->state != PS_SITDOWN;
}
static bool penguin_talk_busy(Actor* a){
    return a->state != PS_WAIT && a->state != PS_TALK && a->state != PS_GIVEUP && a->state != PS_SITDOWN &&
           a->state != PS_PROVOKE;
}

static void penguin_give_up(Actor* a){
    switch_set(actor_switch_key(a, SWITCH_A), true);
    actor_set_state(a, PS_SITDOWN, ANIM_PENGUINSKATER_SITDOWN);
    camera_event_cancel();
}
static void penguin_talk_ended(Actor* a){
    if(a->state == PS_WAIT){
        v3 dir = penguin_rail_dir(a->args[1], 0);
        if(v3_dot14(v3_sub(g_game.player.figure.position, a->position), dir) > 0) penguin_reverse(a);
        const PenguinSkaterSpec* s = penguin_spec(a);
        if(s) camera_event_start(s->start_camera, 1, a->position, a->up, a->front);
        actor_set_state(a, PS_DEMO, ANIM_PENGUINSKATER_ICESWIMSTART);
    } else if(a->state == PS_GIVEUP) penguin_give_up(a);
}
static void penguin_think(Actor* a){
    int step = a->timer - 1;
    const PenguinSkaterSpec* spec = penguin_spec(a);
    switch(a->state){
    case PS_REACTION:
        if(actor_anim_done(a)){
            int back = a->health & 255;
            actor_set_state(a, back,
                            back == PS_SITDOWN ? ANIM_PENGUINSKATER_SITDOWN : ANIM_PENGUINSKATER_WAIT);
        }
        break;
    case PS_WAIT:
    case PS_TALK:
    case PS_SITDOWN:
        talk_actor_think(a);
        if(!talk_active() && g_mario.attack_timer > 0 && actor_touches_mario(a, 150)){
            a->health = (a->health & ~255) | a->state;
            actor_set_state(a, PS_REACTION, ANIM_PENGUINSKATER_REACTION);
        }
        break;
    case PS_DEMO:
        if(step == 0){
            penguin_hold(a);
            mario_set_anim(ANIM_MARIO_WAIT);
        }
        if(actor_anim_done(a) && a->anim == ANIM_PENGUINSKATER_ICESWIMSTART){
            a->anim = ANIM_PENGUINSKATER_ICESWIM;
            a->anim_frame = 0;
        }
        if(step == 33) audio_play_effect(SFX_PENGUIN_SKATE_START, 3);
        if(step >= 33) audio_level_effect(SFX_PENGUIN_SKATE);
        penguin_move(a, 18, 0, R14(.05f));
        camera_event_target(a->position, a->up, a->front);
        if(penguin_provoke(penguin_lead(a), 1000, a->value)){
            penguin_release(a);
            camera_event_cancel();
            actor_set_state(a, PS_AWAY, ANIM_PENGUINSKATER_ICESWIM);
        }
        break;
    case PS_AWAY: {
        audio_level_effect(SFX_PENGUIN_SKATE);
        int crossed = penguin_move(a, 14, 0, R14(.05f));
        fx lead = penguin_lead(a);
        if(lead < 0) actor_set_state(a, PS_TURN, ANIM_PENGUINSKATER_ICESWIMTURN);
        else if(fx_abs(lead) <= 3000 && penguin_switch_rail(a, crossed)) actor_set_state(a, PS_SWITCH, -1);
        else if(penguin_provoke(lead, 1000, a->value)) actor_set_state(a, PS_PROVOKE, -1);
        break;
    }
    case PS_TURN:
        if(actor_anim_done(a) && a->anim == ANIM_PENGUINSKATER_ICESWIMTURN){
            a->anim = ANIM_PENGUINSKATER_ICESWIM;
            a->anim_frame = 0;
        }
        penguin_move(a, 0, 1, N14_ONE);
        if(!a->target.y){
            penguin_reverse(a);
            actor_set_state(a, PS_AWAY, ANIM_PENGUINSKATER_ICESWIM);
        }
        break;
    case PS_SWITCH:
        audio_level_effect(SFX_PENGUIN_SKATE);
        penguin_move(a, 10, 0, R14(.05f));
        if((a->target.z > 0 && a->target.x >= rail_length(a->value) * 256) ||
           (a->target.z < 0 && !a->target.x)){
            int old = a->args[1];
            a->args[1] = a->args[2];
            a->args[2] = old;
            a->value = a->args[1];
            a->target.x = penguin_nearest_coord(a->value, a->position) << 8;
            a->target.z = a->args[3];
            actor_set_state(a, PS_AWAY, ANIM_PENGUINSKATER_ICESWIM);
        }
        break;
    case PS_PROVOKE:
        if(a->target.y) penguin_move(a, 0, 1, N14_ONE);
        else {
            if(a->anim != ANIM_PENGUINSKATER_CONGRATULATE1){
                a->anim = ANIM_PENGUINSKATER_CONGRATULATE1;
                a->anim_frame = 0;
            }
            if(v3_length_squared64(v3_sub(a->position, g_game.player.figure.position)) < 10000ll * 10000)
                talk_force_actor(a);
            fx lead = penguin_lead(a);
            if(!penguin_provoke(lead, 2000, a->value)){
                if(lead < 0) penguin_reverse(a);
                actor_set_state(a, PS_AWAY, ANIM_PENGUINSKATER_ICESWIM);
                audio_play_effect(SFX_PENGUIN_SKATE_START, 3);
            }
        }
        break;
    case PS_CAUGHT:
        if(step == 0){
            penguin_hold(a);
            if(spec)
                camera_event_start(spec->end_camera, 1, g_game.player.figure.position, g_game.player.up,
                                   g_game.player.figure.basis_front);
            audio_stop_level_effect(SFX_PENGUIN_SKATE);
            audio_play_effect(SFX_PENGUIN_CAUGHT, 3);
            mario_restart_anim(ANIM_MARIO_TOSSSTART);
        }
        if(step == 20) audio_play_effect(SFX_PENGUIN_CAUGHT_VOICE, 4);
        if(a->health & PS_OWNS_MARIO){
            int32_t t = fx_min(N14_ONE, step * N14_ONE / 5);
            mario_hold_move(v3_add(a->velocity, v3_scale14(v3_sub(a->position, a->velocity), t)), a->front);
            g_game.player.up = a->up;
            camera_event_target(g_game.player.figure.position, a->up, a->front);
        }
        if(actor_anim_done(a)){
            actor_set_state(a, PS_FADEOUT, ANIM_PENGUINSKATER_CAUGHTWAIT);
            mario_restart_anim(ANIM_MARIO_TOSSWAIT);
        }
        break;
    case PS_FADEOUT:
        g_game.fade_target = 16;
        if(g_game.fade >= 16) actor_set_state(a, PS_FADEIN, ANIM_PENGUINSKATER_SITDOWN);
        break;
    case PS_FADEIN:
        if(step == 0 && spec){
            a->position = penguin_vec(spec->penguin_pos);
            a->up = penguin_basis(spec->penguin_up);
            a->front = penguin_basis(spec->penguin_front);
            penguin_release(a);
            camera_event_cancel();
            mario_reset(penguin_vec(spec->mario_pos), penguin_basis(spec->mario_up),
                        penguin_basis(spec->mario_front));
            mario_set_anim(ANIM_MARIO_WATCH);
            camera_snap_behind(&g_game.camera, &g_game.player);
            talk_forward_actor(a);
            g_game.fade_target = 0;
        }
        if(g_game.fade <= 0) actor_set_state(a, PS_GIVEUP, ANIM_PENGUINSKATER_SITDOWN);
        break;
    case PS_GIVEUP:
        if(talk_active() && !talk_actor_normal_talking(a)) break;
        if(!talk_force_actor(a) || !talk_actor_normal_talking(a)) penguin_give_up(a);
        break;
    }
    if(penguin_attackable(a) && !g_mario_held && g_game.player.action != MA_DEATH &&
       (actor_touches_mario(a, 0) || (g_mario.attack_timer > 0 && actor_touches_mario(a, 150))))
        actor_set_state(a, PS_CAUGHT, ANIM_PENGUINSKATER_CAUGHT);
}

static int penguin_talk_pose(const Actor* a, int anim){
    if(a->state < PS_FADEIN || a->state > PS_SITDOWN) return anim;
    return anim ? ANIM_PENGUINSKATER_SITDOWNTALK : ANIM_PENGUINSKATER_SITDOWN;
}

static bool penguin_talk_held(const Actor* a){ return a->state == PS_PROVOKE; }
