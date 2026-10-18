static bool missions_reveal_star_at(int object, v3 position){
    if(object < 0 || (uint32_t)object >= g_game.stage.header->object_count) return false;
    if(g_game.stage.objects[object].type != OBJ_POWERSTAR) return false;
    Actor* star = 0;
    for(int i = 0; i < g_game.actor_count; i++)
        if(g_game.actors[i].stage_index == object){
            star = &g_game.actors[i];
            break;
        }
    if(!star) star = actors_spawn_object(&g_game.stage, object);
    if(!star) return false;
    star->position = position;
    power_star_reveal(star);
    return true;
}

#define SFX_SYATI_TURN          -1
#define SFX_SYATI_ROLL          -1
#define SFX_SYATI_SWIM          -1
#define SFX_PRIZERING_BLINK     -1
#define SFX_PRIZERING_DISAPPEAR -1
#define SFX_PRIZERING_GET2      SFX_PRIZERING_GET1
#define SFX_PRIZERING_GET3      SFX_PRIZERING_GET1
#define SFX_PRIZERING_GET4      SFX_PRIZERING_GET1
#define SFX_PRIZERING_GET5      SFX_PRIZERING_GET1
static void syati_sound(int cue, int priority){
    if(cue >= 0) audio_play_effect(cue, priority);
}
enum { PR_START = 64, PR_LOOP, PR_TIMEOUT, PR_READY_PASS, PR_PASS, PR_READY_KILL };
#define SY_OWNED 0x100

static v3 syati_vec(const int32_t p[3]){ return V3(p[0], p[1], p[2]); }
static v3 syati_basis(const int16_t p[3]){ return V3(p[0], p[1], p[2]); }
static bool syati_primary(const Actor* a){
    return a->type == OBJ_SYATI && a->stage_index == syati_spec.object &&
           g_game.stage_index == syati_spec.stage;
}
static int syati_passed(const Actor* a){ return a->health & 255; }

static v3 syati_position(fx coord){
    const StageRail* r = rail_get(syati_spec.rail);
    if(!r->part_count) return rail_point(syati_spec.rail, 0);
    coord = fx_clamp(coord, 0, r->length);
    const StageRailPart* parts = &g_game.stage.rail_parts[r->first_part];
    int i = 0;
    while(i + 1 < r->part_count && coord > parts[i].start + parts[i].length) i++;
    const StageRailPart* p = &parts[i];
    fx along = coord - p->start;
    for(int j = 1; j < 4; j++)
        for(int k = 0; k < 3; k++)
            if(fx_abs(p->p[j][k] - p->p[0][k]) > 131071) return rail_position_at(syati_spec.rail, coord);
    if(p->length <= 0 || p->length >= 65536) return rail_position_at(syati_spec.rail, coord);
    uint32_t share = along <= 0           ? 0
                     : along >= p->length ? 65535u
                                          : ((uint32_t)along << 16) / (uint32_t)p->length;
    int k = 0;
    while(k < 15 && p->lut[k] < share) k++;
    uint32_t lo = k ? p->lut[k - 1] : 0, hi = p->lut[k];
    int32_t frac = hi > lo ? (int32_t)(((share - lo) << 10) / (hi - lo)) : 0;
    if(frac > 1024) frac = 1024;
    int32_t t = (k << 10) + frac, u = N14_ONE - t;
    int32_t b[3] = {mul14(mul14(u, u), t) * 3, mul14(mul14(u, t), t) * 3, mul14(mul14(t, t), t)};
    v3 pos = V3(p->p[0][0], p->p[0][1], p->p[0][2]);
    for(int j = 1; j < 4; j++){
        v3 d = V3(p->p[j][0] - p->p[0][0], p->p[j][1] - p->p[0][1], p->p[j][2] - p->p[0][2]);
        pos = v3_add(pos, V3((d.x * b[j - 1]) >> 14, (d.y * b[j - 1]) >> 14, (d.z * b[j - 1]) >> 14));
    }
    return pos;
}
static int syati_current_point(const Actor* a){
    const StageRail* r = rail_get(syati_spec.rail);
    int point = 0;
    fx coord = a->target.x >> 8;
    while(point + 1 < r->point_count && rail_point_coord(syati_spec.rail, point + 1) <= coord) point++;
    return point;
}
static Actor* syati_ring(const Actor* a, int slot){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* b = &g_game.actors[i];
        if(b->type == OBJ_PRIZERING && b->args[0] == a->stage_index && b->args[2] == slot) return b;
    }
    return 0;
}
static Actor* syati_dead_ring(const Actor* a){
    for(int i = 0; i < SYATI_RING_POOL; i++){
        Actor* b = syati_ring(a, i);
        if(b && !b->active) return b;
    }
    return 0;
}
static int syati_living_rings(const Actor* a){
    int n = 0;
    for(int i = 0; i < SYATI_RING_POOL; i++){
        Actor* b = syati_ring(a, i);
        if(b && b->active) n++;
    }
    return n;
}
static void syati_kill_rings(const Actor* a){
    for(int i = 0; i < SYATI_RING_POOL; i++){
        Actor* b = syati_ring(a, i);
        if(b) b->active = 0;
    }
}
static bool syati_reserve_rings(Actor* a){
    for(int i = 0; i < SYATI_RING_POOL; i++){
        if(syati_ring(a, i)) continue;
        Actor* b = actor_spawn_reuse(OBJ_PRIZERING, a->position, a->up, a->front);
        if(!b) return false;
        b->args[0] = a->stage_index;
        b->args[1] = syati_spec.life;
        b->args[2] = i;
        b->flags = ACTOR_WAITING | ACTOR_HIDDEN;
        b->active = 0;
    }
    return true;
}
static void prizering_number(Actor* b, int number){
    static const uint8_t* const models[8] = {figure_prizering1, figure_prizering2, figure_prizering3,
                                             figure_prizering4, figure_prizering5, figure_prizering6,
                                             figure_prizering7, figure_prizering8};
    number = fx_clamp(number, 1, 8);
    b->args[3] = number;
    b->figure.model = (const FigureHeader*)models[number - 1];
}
static void syati_hold(Actor* a){
    if(g_mario_held || (a->health & SY_OWNED)) return;
    a->health |= SY_OWNED;
    mario_hold_begin();
}
static void syati_player_pose(Actor* a){
    if(!(a->health & SY_OWNED)) return;
    v3 up = syati_basis(syati_spec.up), front = syati_basis(syati_spec.front), side = v3_cross14(up, front);
    v3 pos = v3_add(syati_vec(syati_spec.pos),
                    v3_add(v3_scale14(side, 6), v3_add(v3_scale14(up, -176), v3_scale14(front, -590))));
    v3 aim = v3_sub(v3_add(syati_vec(syati_spec.pos), v3_scale14(up, 450)), pos);
    Player* p = &g_game.player;
    GravitySample gravity = gravity_sample(g_game.stage.header, pos);
    p->gravity_up = gravity.up;
    p->up = v3_direction14(aim, up);
    p->front = v3_tangent14(v3_neg(gravity.up), p->up, front);
    g_swim.pitch = 0;
    mario_hold_move(pos, p->front);
    mario_set_anim(ANIM_MARIO_SWIMWAIT);
    p->figure.local_pitch = 0;
    p->velocity = V3(0, 0, 0);
}
static void syati_release(Actor* a){
    if(!(a->health & SY_OWNED)) return;
    a->health &= ~SY_OWNED;
    mario_hold_release(V3(0, 0, 0));
    camera_event_cancel();
}
static void syati_follow(Actor* a){
    if(a->target.y < SYATI_COUNTANDGO_DURATION) a->target.y++;
    int point = syati_current_point(a);
    const StageRail* r = rail_get(syati_spec.rail);
    int speed = g_game.stage.rail_points[r->first_point + point].args[1];
    if(actor_mario_distance(a) >= 5000) speed = 3;
    a->target.x = fx_min(a->target.x + (fx_max(speed, 0) << 8), r->length << 8);
    fx coord = a->target.x >> 8;
    a->position = syati_position(coord);
    a->up = gravity_sample(g_game.stage.header, a->position).up;
    v3 direction = v3_sub(syati_position(coord + 600), a->position);
    v3 want = v3_direction14(direction, a->front);

    a->front = v3_direction14(v3_add(v3_scale14(a->front, R14(.96f)), v3_scale14(want, R14(.04f))), want);
    a->anim_frame -= FX_ONE / 2;
    if(SFX_SYATI_SWIM >= 0) audio_level_effect(SFX_SYATI_SWIM);
}
static bool syati_ready_ring(const Actor* a){
    if(syati_passed(a) == syati_spec.rings || !syati_dead_ring(a)) return false;
    int point = syati_current_point(a);
    const StageRail* r = rail_get(syati_spec.rail);
    if(point == a->value || point + 1 >= r->point_count) return false;
    if(rail_point_coord(syati_spec.rail, point + 1) - (a->target.x >> 8) > 600) return false;
    return g_game.stage.rail_points[r->first_point + point + 1].args[0] == 0;
}
static void syati_emit_ring(Actor* a){
    Actor* b = syati_dead_ring(a);
    if(!b) return;
    fx coord = (a->target.x >> 8) + 600;
    b->position = syati_position(coord);
    b->up = a->up;
    b->front = v3_direction14(v3_sub(syati_position(coord + 8), syati_position(coord - 8)), a->front);
    b->flags = ACTOR_WAITING;
    b->active = 1;
    b->health = 0;
    prizering_number(b, syati_spec.rings - syati_passed(a));
    actor_set_state(b, PR_START, ANIM_PRIZERING8_START);
    a->value = syati_current_point(a);
}
static void syati_ring_control(Actor* a){
    bool passed = false;
    for(int i = 0; i < SYATI_RING_POOL; i++){
        Actor* b = syati_ring(a, i);
        if(!b || !b->active) continue;
        if(b->state == PR_READY_PASS){
            actor_set_state(b, PR_PASS, ANIM_PRIZERING8_END);
            a->health++;
            passed = true;
            Player* p = &g_game.player;
            if(p->health < g_mario_max_health) p->health++;
            mario_swim_recover();
        }
        if(b->state == PR_READY_KILL){
            b->active = 0;
            if(syati_passed(a) == syati_spec.rings){
                syati_kill_rings(a);
                actor_set_state(a, SY_STAR, ANIM_SYATI_STAR);
            }
        }
    }
    if(passed && syati_passed(a) != syati_spec.rings)
        for(int i = 0; i < SYATI_RING_POOL; i++){
            Actor* b = syati_ring(a, i);
            if(b && b->active && b->state < PR_READY_PASS)
                prizering_number(b, syati_spec.rings - syati_passed(a));
        }
}
static void syati_init(Actor* a, const StageObject* o){
    a->health = 0;
    a->value = -1;
    a->target = V3(0, 0, 0);
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    if(syati_primary(a)){
        a->position = syati_position(0);
        actor_set_state(a, SY_WAIT, ANIM_SYATI_WAITDEEPSEA);
    } else {
        a->flags |= ACTOR_HIDDEN;
        actor_set_state(a, SY_HIDE_SHORE, ANIM_SYATI_WAITDEEPSEA);
    }
}
static void syati_think(Actor* a){
    int step = a->timer - 1;
    if(g_game.player.action == MA_DEATH){
        syati_release(a);
        return;
    }
    if(!syati_primary(a)){
        if(a->state == SY_HIDE_SHORE && power_star_revealed(syati_spec.star_object)){
            a->flags &= ~ACTOR_HIDDEN;
            actor_set_state(a, SY_SHORE, ANIM_SYATI_DELIGHTDEEPSEA);
        }
        if(a->state == SY_SHORE) talk_actor_think(a);
        return;
    }
    syati_ring_control(a);
    switch(a->state){
    case SY_WAIT:
        if(step >= 180) a->timer = 1;
        if(actor_switch_on(a, SWITCH_A) && !g_mario_held){
            syati_hold(a);
            actor_set_state(a, SY_FADEOUT_START, -1);
        }
        break;
    case SY_FADEOUT_START:
    case SY_FADEOUT_RETRY:
        if(step == 0) g_game.fade_target = 16;
        else if(g_game.fade == 16)
            actor_set_state(a, a->state == SY_FADEOUT_START ? SY_BLANK_START : SY_BLANK_RETRY, -1);
        break;
    case SY_BLANK_START:
    case SY_BLANK_RETRY:
        syati_player_pose(a);
        if(step == 30)
            actor_set_state(a, a->state == SY_BLANK_START ? SY_FADEIN_START : SY_FADEIN_RETRY,
                            ANIM_SYATI_TALK);
        break;
    case SY_FADEIN_START:
    case SY_FADEIN_RETRY:
        if(step == 0){
            g_game.fade_target = 0;
            camera_event_start(syati_spec.talk_camera, 1, a->position, a->up, a->front);
        }
        syati_player_pose(a);
        camera_event_target(a->position, a->up, a->front);
        if(step > 0 && !g_game.fade)
            actor_set_state(a, a->state == SY_FADEIN_START ? SY_TALK_START : SY_TALK_RETRY,
                            a->state == SY_FADEIN_START ? ANIM_SYATI_TALK : ANIM_SYATI_FAILURE);
        break;
    case SY_TALK_START:
    case SY_TALK_RETRY:
        syati_player_pose(a);
        talk_force_actor(a);
        break;
    case SY_READY:
        if(step == 0){
            if(!syati_reserve_rings(a)){
                a->timer = 0;
                break;
            }
            a->health &= SY_OWNED;
            a->value = -1;
            a->target.x = a->target.y = 0;
            syati_kill_rings(a);
            talk_reset_actor(a);
            camera_event_start(syati_spec.demo_camera, 1, a->position, a->up, a->front);
            syati_sound(SFX_SYATI_TURN, 2);
        }
        syati_player_pose(a);
        camera_event_target(a->position, a->up, a->front);
        if(step >= SYATI_TURNREVERSE_DURATION) actor_set_state(a, SY_COUNTDOWN, ANIM_SYATI_SWIM);
        break;
    case SY_COUNTDOWN:
        if(step % 60 == 0) syati_sound(SFX_SYATI_COUNTDOWN, 4);
        syati_follow(a);
        syati_player_pose(a);
        camera_event_target(a->position, a->up, a->front);
        if(step >= SYATI_COUNTDOWN_DURATION){
            syati_release(a);
            syati_sound(SFX_SYATI_START, 4);
            actor_set_state(a, SY_SWIM, ANIM_SYATI_SWIM);
        }
        break;
    case SY_SWIM:
        syati_follow(a);
        if(syati_ready_ring(a)) actor_set_state(a, SY_EMIT, ANIM_SYATI_SCREW);
        else if((a->target.x >> 8) >= rail_length(syati_spec.rail))
            actor_set_state(a, SY_END, ANIM_SYATI_TURN);
        break;
    case SY_EMIT:
        if(step == 0){
            syati_sound(SFX_SYATI_ROLL, 2);
            syati_emit_ring(a);
        }
        syati_follow(a);
        a->anim_frame += FX_ONE / 2;
        if(syati_ready_ring(a)) actor_set_state(a, SY_EMIT, ANIM_SYATI_SCREW);
        else if(step >= SYATI_SCREW_DURATION) actor_set_state(a, SY_SWIM, ANIM_SYATI_SWIM);
        break;
    case SY_END:
        if(step == 0)
            if(SFX_SYATI_SWIM >= 0) audio_stop_level_effect(SFX_SYATI_SWIM);
        if(step >= SYATI_TURN_DURATION) actor_set_state(a, SY_ALL_RINGS, ANIM_SYATI_WAITDEEPSEA);
        break;
    case SY_ALL_RINGS:
        if(step == 0){
            talk_reset_actor(a);
            talk_forward_actor(a);
        }
        if(!syati_living_rings(a) && syati_passed(a) != syati_spec.rings && !g_mario_held){
            syati_hold(a);
            actor_set_state(a, SY_FADEOUT_RETRY, -1);
        }
        break;
    case SY_STAR:
        if(step == 0){
            if(SFX_SYATI_SWIM >= 0) audio_stop_level_effect(SFX_SYATI_SWIM);
            v3 h = syati_basis(syati_spec.star_head), side = v3_cross14(a->up, a->front);
            v3 pos = v3_add(a->position, v3_add(v3_scale14(side, h.x),
                                                v3_add(v3_scale14(a->up, h.y), v3_scale14(a->front, h.z))));
            missions_reveal_star_at(syati_spec.star_object, pos);
        }
        if(step > 1 && power_star_revealed(syati_spec.star_object)) a->active = 0;
        break;
    case SY_FORCE_KILL:
        syati_release(a);
        missions_kill_mario();
        break;
    }
}
static void prizering_think(Actor* b){
    int step = b->timer - 1;
    switch(b->state){
    case PR_START:
        if(step == 0) syati_sound(SFX_PRIZERING_APPEAR, 2);
        if(step >= PRIZERING_START_DURATION) actor_set_state(b, PR_LOOP, ANIM_PRIZERING8_LOOP);
        break;
    case PR_LOOP:
        if(step == b->args[1]) actor_set_state(b, PR_TIMEOUT, -1);
        break;
    case PR_TIMEOUT: {
        static const int16_t frames[] = {10, 60, 110, 160, 210, 240, 270, 300};
        for(int i = 0; i < 8; i++)
            if(step == frames[i]) syati_sound(SFX_PRIZERING_BLINK, 2);
        if(step >= PRIZERING_TIMEOUT_DURATION){
            syati_sound(SFX_PRIZERING_DISAPPEAR, 2);
            b->active = 0;
        }
        break;
    }
    case PR_PASS:
        if(step == 0){
            static const int sounds[] = {SFX_PRIZERING_GET1, SFX_PRIZERING_GET5, SFX_PRIZERING_GET4,
                                         SFX_PRIZERING_GET3, SFX_PRIZERING_GET2};
            syati_sound(b->args[3] == 1 ? SFX_PRIZERING_LAST : sounds[b->args[3] < 5 ? b->args[3] : 0], 4);
        }
        if(step >= PRIZERING_END_DURATION){
            b->flags |= ACTOR_HIDDEN;
            actor_set_state(b, PR_READY_KILL, -1);
        }
        break;
    }
    if(b->active && b->state < PR_READY_PASS && g_game.player.action != MA_DEATH && !g_mario_held &&
       actor_touches_mario(b, 0))
        actor_set_state(b, PR_READY_PASS, -1);
}

static bool syati_counting(void){
    for(int i = 0; i < g_game.actor_count; i++)
        if(syati_primary(&g_game.actors[i]) &&
           (g_game.actors[i].state == SY_COUNTDOWN ||
            ((g_game.actors[i].state == SY_SWIM || g_game.actors[i].state == SY_EMIT) &&
             g_game.actors[i].target.y < SYATI_COUNTANDGO_DURATION)))
            return true;
    return false;
}
static void syati_draw_hud(void){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(!a->active || !syati_primary(a)) continue;
        if(a->state == SY_COUNTDOWN){
            int art = a->timer <= 60 ? HUD_ART_RACE3 : a->timer <= 120 ? HUD_ART_RACE2 : HUD_ART_RACE1;
            hud_load_text(SLOT_BANNER, art);
            hud_art_centred(art, HUD_RACE_CY, SLOT_BANNER);
        } else if((a->state == SY_SWIM || a->state == SY_EMIT) && a->target.y < SYATI_COUNTANDGO_DURATION){
            hud_load_text(SLOT_BANNER, HUD_ART_RACEGO);
            hud_art_centred(HUD_ART_RACEGO, HUD_RACE_CY, SLOT_BANNER);
        }
    }
}

static void syati_talk_ended(Actor* a){
    if(a->state == SY_TALK_START) actor_set_state(a, SY_READY, ANIM_SYATI_TURNREVERSE);
    else if(a->state == SY_TALK_RETRY) actor_set_state(a, SY_FORCE_KILL, ANIM_SYATI_FAILURE);
}
