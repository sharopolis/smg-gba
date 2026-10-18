static void crystal_cage_medium_init(Actor* a, const StageObject* o){
    a->figure.model = (const FigureHeader*)figure_eggcrystalmintact;
    a->figure.pose = model_rest_pose(a->figure.model);
    actor_set_state(a, CAGE_MEDIUM_WAIT, -1);
    a->anim = -1;
    GravitySample gravity = gravity_sample(g_game.stage.header, a->home);
    a->velocity = gravity.index >= 0 ? gravity.up : a->up;
    a->target = a->home;
    RayHit hit;
    fx reach = FX(300) * a->figure.scale.x / 64;
    if(collision_ray(v3_add(a->home, v3_scale14(a->up, reach)), v3_sub(a->home, v3_scale14(a->up, reach)),
                     &hit, 0))
        a->target = hit.point;
    if(stream_part_state(a->stage_index) || actor_switch_on(a, SWITCH_DEAD)){
        a->state = CAGE_MEDIUM_AFTER;
        a->position = a->target;
        a->figure.model = (const FigureHeader*)figure_eggcrystalmwreck;
        a->figure.pose = model_rest_pose(a->figure.model);
    }
    crystal_cage_init(a, o);
}

static bool crystal_cage_medium_break(Actor* a){
    if(!a->active || a->type != OBJ_CRYSTALCAGEM || a->state != CAGE_MEDIUM_WAIT ||
       (a->flags & ACTOR_WAITING))
        return false;

    int owner = actor_index(a);
    for(int g = 0; g < g_dyncol_group_count; g++)
        if((g_dyncol_groups[g].owner & 255) == owner) g_dyncol_groups[g].count = 0;
    actor_set_state(a, CAGE_MEDIUM_BREAK, ANIM_HONEYCRYSTALBREAK_BREAK);
    a->timer = -1;
    return true;
}

static bool crystal_cage_medium_player_attack(const Actor* a){
    if(!level_has(LEVEL_CAGES_BREAK_ON_ATTACK)) return false;
    const Player* p = &g_game.player;
    if(p->action == MA_DEATH || p->action == MA_STAR_LAUNCH || g_mario_held) return false;
    bool hipdrop = p->action == MA_HIPDROP;
    if(g_mario.attack_timer <= 0 && g_mario_star_power <= 0 && !hipdrop) return false;
    fx h = fx_clamp(v3_dot14(v3_sub(p->figure.position, a->position), a->up), -FX(218), FX(540));
    v3 d = v3_sub(mario_centre(), v3_add(a->position, v3_scale14(a->up, h)));
    fx r = FX(265) * a->figure.scale.x / 64 + (g_mario.attack_timer > 0 ? FX(100) : C_BODY_RADIUS + FX(20));
    return fx_abs(d.x) <= r && fx_abs(d.y) <= r && fx_abs(d.z) <= r &&
           v3_length_squared64(d) < (uint64_t)r * r;
}

static void crystal_cage_medium_think(Actor* a){
    if(a->state == CAGE_MEDIUM_WAIT){
        if(actor_switch_on(a, SWITCH_A) || crystal_cage_medium_player_attack(a)) crystal_cage_medium_break(a);
        return;
    }
    if(a->state != CAGE_MEDIUM_BREAK) return;
    if(a->timer == 0){
        a->position = a->target;
        a->figure.model = (const FigureHeader*)figure_honeycrystalbreak;
        a->figure.pose = figure_clip_pose(a->figure.model, ANIM_HONEYCRYSTALBREAK_BREAK, 0);
        a->anim_frame = 0;
        stream_part_state_set(a->stage_index, 1);
        if(a->args[3] == 0) collectibles_pop(OBJ_COIN, v3_add(a->home, v3_scale14(a->up, FX(200))), a->up, 1);

        audio_queue_effect(SFX_CRYSTAL_CAGE_S_BREAK, 3);
    }

    if(a->timer == (a->args[3] == 3 ? 10 : 0)) actor_set_switch_dead(a, 0);
    if(a->timer >= 49){
        a->figure.model = (const FigureHeader*)figure_eggcrystalmwreck;
        a->figure.pose = model_rest_pose(a->figure.model);
        actor_set_state(a, CAGE_MEDIUM_AFTER, -1);
        a->anim = -1;
        a->anim_frame = 0;
        const StageObject* o = actor_stage_object(a);
        if(o && o->args[0] > 0){
            a->flags |= ACTOR_HIDDEN;
            a->active = 0;
        }
    }
}

_Static_assert(sizeof(egg_crystalm_triangles) / sizeof(egg_crystalm_triangles[0]) ==
                   sizeof(egg_crystalm_wait_normals) / sizeof(egg_crystalm_wait_normals[0]),
               "each cage triangle needs a normal");
#define DYNCOL_BUILD_NAME dyncol_add_codes_cage_medium
#define DYNCOL_BUILD_ATTR __attribute__((noinline))
#define DYNCOL_BUILD_PRECOMPUTED
#include "world/dyncol_build.inc"
#undef DYNCOL_BUILD_PRECOMPUTED
#undef DYNCOL_BUILD_ATTR
#undef DYNCOL_BUILD_NAME

static void crystal_cage_medium_collision(Actor* a, v3 mario){
    if(a->state != CAGE_MEDIUM_WAIT || (a->flags & ACTOR_WAITING)) return;
    const StageObject* o = actor_stage_object(a);
    int16_t scale[3];
    for(int k = 0; k < 3; k++) scale[k] = o && o->scale[k] ? o->scale[k] : 256;
    v3 anchor = mario;
    uint64_t distance = v3_length_squared64(v3_sub(anchor, a->position));

    fx near = FX(600);
    for(int b = 0; b < g_game.actor_count; b++){
        const Actor* bill = &g_game.actors[b];
        if(!bill->active || bill->type != OBJ_KILLER || bill->state == AS_DEFEATED) continue;
        uint64_t d = v3_length_squared64(v3_sub(bill->position, a->position));
        if(d < distance){
            distance = d;
            anchor = bill->position;
            near = FX(2000);
        }
    }

    if(a->up.x == 0 && a->up.y == 0 && a->up.z == N14_ONE && a->front.x == 0 && a->front.y == N14_ONE &&
       a->front.z == 0 && scale[0] == 256 && scale[1] == 256 && scale[2] == 256)
        dyncol_add_codes_cage_medium(&egg_crystalm_mesh, a->position, a->up, a->front, scale, actor_index(a),
                                     0, anchor, near, egg_crystalm_codes, egg_crystalm_wait_normals);
    else
        dyncol_add_codes(&egg_crystalm_mesh, a->position, a->up, a->front, scale, actor_index(a), 0, anchor,
                         near, egg_crystalm_codes);
}

__attribute__((noinline)) static void crystal_cage_medium_collision_all(void){
    v3 mario = g_game.player.figure.position;

    for(Actor *a = g_game.actors, *end = a + g_game.actor_count; a < end; a++)
        if(a->type == OBJ_CRYSTALCAGEM && a->active) crystal_cage_medium_collision(a, mario);
}

__attribute__((noinline)) static bool killer_break_cages(Actor* a, v3 next){
    fx radius = FX(70) * a->figure.scale.x / 64;
    CollisionCandidates candidates;
    collision_gather_around(a->position, next, radius, &candidates);
    bool bound = false;
    for(int i = 0; i < candidates.count; i++){
        SphereContact contact;
        if(!collision_sphere_triangle(candidates.index[i], next, radius, &contact)) continue;
        bound = true;
        int owner = dyncol_owner(contact.tri);
        if(owner >= 0 && owner < g_game.actor_count) crystal_cage_medium_break(&g_game.actors[owner]);
    }
    return bound;
}

static void crystal_cage_basic_think(Actor* a){
    fx r = FX(130) * a->figure.scale.x / 64;
    int c = actor_mario_contact(a, FX(60));
    if(c == CONTACT_SPIN || c == CONTACT_STAR || c == CONTACT_HIPDROP){
        if(c == CONTACT_SPIN) mario_spin_hop();
        if(a->args[3] == 0) collectibles_pop(OBJ_COIN, actor_centre(a), a->up, 1);
        actor_set_switch_dead(a, FX(300));
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
        actor_defeat(a, false);
        return;
    }
    actor_block_mario(a, r, r * 2);
}

enum { CAGE_LARGE_WAIT = 64, CAGE_LARGE_BREAK };

static void crystal_cage_large_init(Actor* a, const StageObject* o){
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    a->radius = a->radius * o->scale[0] / 256;
    a->health = 2;
    a->value = 0;
    actor_set_state(a, CAGE_LARGE_WAIT, -1);
}

static void power_star_reveal_by_id(int id){
    if(id < 1) id = g_game.scenario_loaded;
    for(uint32_t i = 0; i < g_game.stage.header->object_count; i++){
        const StageObject* o = &g_game.stage.objects[i];
        if(o->type != OBJ_POWERSTAR || o->args[0] != id || !(o->scenarios & (1 << (g_game.scenario - 1))))
            continue;
        g_power_star_revealed[i >> 3] |= (uint8_t)(1 << (i & 7));
        for(int j = 0; j < g_game.actor_count; j++)
            if(g_game.actors[j].stage_index == (int)i) power_star_reveal(&g_game.actors[j]);
    }
}
static void crystal_cage_large_think(Actor* a){
    if(a->state == CAGE_LARGE_BREAK){
        if(actor_anim_done(a)) a->active = 0;
        return;
    }
    if(a->value > 0) a->value--;
    int contact = actor_mario_contact(a, FX(60));
    if(!a->value && (contact == CONTACT_SPIN || contact == CONTACT_STAR || contact == CONTACT_HIPDROP)){
        if(contact == CONTACT_SPIN){
            mario_spin_hop();
            g_mario.attack_timer = 0;
        }
        if(--a->health > 0){
            a->value = 20;
            a->figure.model = (const FigureHeader*)figure_crystalcageldamage;
            audio_play_effect(SFX_ENEMY_HIT, 3);
        } else {
            a->figure.model = (const FigureHeader*)figure_crystalcagelbreak;
            actor_set_state(a, CAGE_LARGE_BREAK, ANIM_CRYSTALCAGELBREAK_BREAK);
            power_star_reveal_by_id(a->args[0]);
            audio_play_effect(SFX_ENEMY_DEFEAT, 3);
        }
        return;
    }
    actor_block_mario(a, FX(130) * a->figure.scale.x / 64, FX(500) * a->figure.scale.y / 64);
}

static fx crystal_cage_medium_draw_extent(const Actor* a){ return FX(600) * a->figure.scale.x / 64; }

static void crystal_cage_init(Actor* a, const StageObject* o){
    if(OBJECT_ARG(o, 7) != -1) a->args[3] = (int16_t)OBJECT_ARG(o, 7);
}

static bool crystal_cage_medium_idle(const Actor* a){ return a->state == CAGE_MEDIUM_WAIT; }
