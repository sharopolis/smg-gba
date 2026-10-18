enum { ELECTRON_MOVE = 64, ELECTRON_ATTACK };

#define MARBLE_SHELL   1207
#define MARBLE_STAR_ID 818

#define SFX_MARBLE_HIT_CORE_3 SFX_MARBLE_HIT_CORE

static const int16_t marble_core_scale[4] = {256, 544, 832, 1920};

static void marble_glass_hide(v3 centre){
    const Stage* st = &g_game.stage;
    unsigned slot = TEXCACHE_SLOTS - 1;
    unsigned n = st->header->texture_count;
    if(n > STAGE_MAX_TEXTURES) n = STAGE_MAX_TEXTURES;
    for(unsigned t = 0; t < n; t++)
        if(g_texture_cache_slot[t] == slot + 1) g_texture_cache_slot[t] = 0;
    volatile uint32_t* vram = (volatile uint32_t*)texture_cache_address(slot);
    for(unsigned i = 0; i < TEX_TEXELS / 4; i++) vram[i] = 0;
    g_texture_cache_reference[slot].mip[0] = g_texture_cache_reference[slot].mip[1] = (uint32_t)vram;
    int shift = st->point_shift;
    for(uint32_t i = 0; i < st->header->triangle_count; i++){
        const StageTriangle* tri = &st->triangles[i];
        if(!(tri->flags & TRI_ALPHA) || (tri->texture & SURF_SCENARIO_MASK) != (2u << 10)) continue;
        const int16_t* p = st->points[tri->v[0].point];
        v3 at = V3((fx)p[0] << shift, (fx)p[1] << shift, (fx)p[2] << shift);
        if(v3_length_squared64(v3_sub(at, centre)) > (uint64_t)1700 * 1700) continue;
        unsigned t = stage_triangle_texture(tri);
        if(t < n) g_texture_cache_slot[t] = (uint8_t)(slot + 1);
    }
}
static int marble_star_object(void){
    const Stage* st = &g_game.stage;
    for(uint32_t i = 0; i < st->header->object_count; i++)
        if(st->objects[i].type == OBJ_POWERSTAR && st->objects[i].id == MARBLE_STAR_ID) return (int)i;
    return -1;
}

static v3 marble_support_front(v3 up){
    int32_t ax = fx_abs(up.x), ay = fx_abs(up.y), az = fx_abs(up.z);
    v3 support = (ay >= ax && ay >= az) ? V3(0, 0, N14_ONE) : V3(0, N14_ONE, 0);
    return v3_tangent14(support, up, V3(N14_ONE, 0, 0));
}
static void marble_spawn(Actor* a){
    if(a->args[1] >= 0) return;
    Actor* core = actor_spawn(OBJ_MARBLECORE, a->position, a->up, a->front);
    if(!core) return;
    core->value = (int16_t)actor_index(a);
    core->stage_index = -1;
    actor_set_state(core, MP_WAIT, -1);
    a->args[1] = (int16_t)actor_index(core);
    v3 front = a->front;
    for(int k = 0; k < 3; k++){
        if(k) front = v3_rotate_axis14(front, a->up, FX_TAU / 3);
        v3 pos = v3_add(a->position, v3_scale14(front, 1000));
        Actor* e = actor_spawn(OBJ_MARBLEELECTRON, pos, front, a->front);
        if(!e) continue;
        e->value = (int16_t)actor_index(a);
        e->stage_index = -1;
        e->args[0] = (int16_t)k;
        e->target = marble_support_front(front);
        e->velocity = V3(0, 0, 0);
        actor_set_state(e, ELECTRON_MOVE, -1);
    }
}
static void marble_planet_think(Actor* a){
    int step = a->timer - 1;
    marble_spawn(a);
    Actor* core = actor_active_at(a->args[1], OBJ_MARBLECORE);
    int base = marble_core_scale[3 - fx_clamp(a->health, 0, 3)];
    switch(a->state){
    case MP_WAIT:
        if(core) core->figure.scale = V3(64 * base / 256, 64 * base / 256, 64 * base / 256);
        break;
    case MP_SCALE: {
        int32_t x = fx_min(step, 30) * N14_ONE / 30, r;
        if(x < N14_ONE / 2) r = fx_sin(mul14(x * 2, FX_PI / 2)) << 6;
        else {
            int32_t t = (x - N14_ONE / 2) * 2;
            r = N14_ONE -
                mul14(mul14(N14_ONE - (fx_cos(mul14(t, FX_PI / 2)) << 6), N14_ONE - t), N14_ONE / 2);
        }
        int32_t s = R14(1.3f) - mul14(R14(.3f), r);
        int32_t k = mul14(64 * base / 256, s);
        if(core) core->figure.scale = V3(k, k, k);
        if(step >= 30) actor_set_state(a, MP_WAIT, -1);
        break;
    }
    case MP_BREAK:
        if(core) core->figure.scale = V3(64 * 1920 / 256, 64 * 1920 / 256, 64 * 1920 / 256);
        if(step == 1){
            camera_shake(3);
            missions_reveal_star_at(a->args[0], a->position);
            audio_play_music(MUSIC_MBGM_STAR_EXIST);
            marble_glass_hide(a->position);
            if(actor_has_switch(a, SWITCH_DEAD)) switch_set(actor_switch_key(a, SWITCH_DEAD), true);
            actor_set_state(a, MP_DONE, -1);
        }
        break;
    default: break;
    }
}

static bool marble_hit(Actor* a){
    if(a->state != MP_WAIT || a->health <= 0) return false;
    a->health--;
    camera_shake(a->health ? 1 : 2);
    audio_play_effect(a->health ? SFX_MARBLE_HIT_CORE : SFX_MARBLE_HIT_CORE_3, 4);
    actor_set_state(a, a->health ? MP_SCALE : MP_BREAK, -1);
    return true;
}
static void marble_electron_think(Actor* e){
    Actor* a = actor_active_at(e->value, OBJ_MARBLEPLANET);
    if(!a){
        e->active = 0;
        return;
    }
    const Player* p = &g_game.player;
    v3 c = a->position;
    v3 out = v3_direction14(v3_sub(e->position, c), e->up);
    e->up = out;
    if(e->state == ELECTRON_MOVE){
        e->target = v3_tangent14(e->target, out, marble_support_front(out));
        int32_t speed = mul14(v3_length_exact(e->velocity), R14(.99f));
        if(speed < V16(13)) speed = V16(13);
        e->velocity = v3_scale14(e->target, speed);
        for(int i = 0; i < g_game.actor_count; i++){
            const Actor* o = &g_game.actors[i];
            if(o == e || !o->active || o->type != OBJ_MARBLEELECTRON) continue;
            v3 d = v3_sub(o->position, e->position);
            if(v3_length_squared64(d) >= (uint64_t)440 * 440) continue;

            e->velocity = v3_sub(e->velocity, v3_scale14(v3_direction14(d, e->target), V16(5)));
            e->target = v3_direction14(e->velocity, e->target);
            e->velocity = v3_scale14(e->velocity, R14(1.2f));
            audio_play_effect(SFX_MARBLE_HIT_CORE, 1);
        }
        int32_t len = v3_length_exact(e->velocity);
        if(len > V16(30)) e->velocity = v3_scale14(v3_direction14(e->velocity, e->target), V16(30));
        bool hip = p->action == MA_HIPDROP || (p->action == MA_HIPDROP_LAND && p->action_timer < 2);
        if(hip &&
           v3_length_squared64(v3_sub(mario_centre(), e->position)) < (uint64_t)(500 + 100) * (500 + 100)){
            actor_set_state(e, ELECTRON_ATTACK, -1);
            audio_play_effect(SFX_MARBLE_FLIP, 3);
        }
    } else {
        e->velocity = v3_scale14(v3_direction14(v3_sub(c, e->position), e->target), V16(40));
    }
    e->position = v3_add(
        e->position, V3((e->velocity.x + 128) >> 8, (e->velocity.y + 128) >> 8, (e->velocity.z + 128) >> 8));
    v3 r = v3_sub(e->position, c);
    if(v3_length_squared64(r) > (uint64_t)MARBLE_SHELL * MARBLE_SHELL)
        e->position = v3_add(c, v3_scale14(v3_direction14(r, out), MARBLE_SHELL));
    if(v3_length_squared64(v3_sub(e->position, c)) < (uint64_t)550 * 550 && marble_hit(a)) e->active = 0;
}
static void marble_core_think(Actor* c){
    Actor* a = actor_active_at(c->value, OBJ_MARBLEPLANET);
    if(!a){
        c->active = 0;
        return;
    }
    c->position = a->position;
}

static void marble_electron_figure(const Actor* a, Figure* f){
    f->local_pitch = (fx)((a->timer * FX_TAU / 60) % FX_TAU);
}
