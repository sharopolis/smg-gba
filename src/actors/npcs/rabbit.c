static v3 rabbit_contact_centre(const Actor* a){
    const int16_t (*pose)[3] = rabbit_spine_wait;
    int count = 1;
    bool loop = false;
#define RABBIT_SPINE(name, repeat)                             \
    case ANIM_TRICKRABBIT_##name:                              \
        pose = rabbit_spine_##repeat;                          \
        count = sizeof rabbit_spine_##repeat / sizeof pose[0]; \
        break
    switch(a->anim){
        RABBIT_SPINE(FOLLOWME, followme);
        RABBIT_SPINE(RUN, run);
        RABBIT_SPINE(BRAKE, brake);
        RABBIT_SPINE(JUMPSTART, jumpstart);
        RABBIT_SPINE(JUMPSTARTLOOP, jumpstartloop);
        RABBIT_SPINE(JUMP, jump);
        RABBIT_SPINE(FALL, fall);
        RABBIT_SPINE(FALLDOWNLAND, falldownland);
        RABBIT_SPINE(FALLDOWNLOOP, falldownloop);
        RABBIT_SPINE(FALLDOWNEND, falldownend);
    default: break;
    }
#undef RABBIT_SPINE
    loop = a->anim == ANIM_TRICKRABBIT_FOLLOWME || a->anim == ANIM_TRICKRABBIT_RUN ||
           a->anim == ANIM_TRICKRABBIT_JUMPSTARTLOOP || a->anim == ANIM_TRICKRABBIT_FALLDOWNLOOP;
    int key = (a->anim_frame >> 8) / 4;
    key = loop ? key % count : fx_min(key, count - 1);
    v3 local = V3(-pose[key][0] / 4, pose[key][2] / 4, pose[key][1] / 4);
    v3 side = v3_cross14(a->front, a->up);
    return v3_add(a->position, v3_add(v3_scale14(side, local.x),
                                      v3_add(v3_scale14(a->front, local.y), v3_scale14(a->up, local.z))));
}

static v3 rabbit_node(int n){
    const int32_t* p = rabbit_graph_nodes[n].pos;
    return V3(p[0], p[1], p[2]);
}
static int rabbit_next(const Actor* a){
    if(a->args[1] < 0) return a->args[0];
    const RabbitGraphEdge* e = &rabbit_graph_edges[a->args[1]];
    return e->nodes[0] == a->args[0] ? e->nodes[1] : e->nodes[0];
}
static int rabbit_edge_arg(const Actor* a, int n, int fallback){
    if(a->args[1] < 0) return fallback;
    const RabbitGraphEdge* e = &rabbit_graph_edges[a->args[1]];
    int v = n == 0 ? e->jump : n == 1 ? e->speed : n == 3 ? e->height : n == 7 ? e->level : -1;
    return v < 0 ? fallback : v;
}
static bool rabbit_catchable(const Actor* a){ return a->state >= TR_WAIT && a->state <= TR_GETUP; }
static bool rabbit_can_tumble(const Actor* a){
    return (a->state >= TR_WAIT && a->state <= TR_BRAKE) || a->state == TR_WAIT_START ||
           a->state == TR_GIVE_UP;
}
static bool rabbit_unclipped(const Actor* a){
    return a->type == OBJ_TRICKRABBIT && a->state != TR_WAIT_START;
}
static bool rabbit_select_demo(Actor* a){
    const RabbitGraphNode* n = &rabbit_graph_nodes[a->args[0]];
    bool found = false;
    for(int k = 0; k < n->count; k++){
        int i = rabbit_graph_adjacency[n->first + k];
        const RabbitGraphEdge* e = &rabbit_graph_edges[i];
        int next = e->nodes[0] == a->args[0] ? e->nodes[1] : e->nodes[0];
        if(next != a->args[2] && e->level < -1){
            a->args[1] = i;
            found = true;
        }
    }
    return found;
}
static void rabbit_nearest_node(Actor* a, bool runaway){
    uint64_t best = UINT64_MAX;
    int result = 0;
    for(unsigned i = 0; i < sizeof rabbit_graph_nodes / sizeof rabbit_graph_nodes[0]; i++){
        const RabbitGraphNode* n = &rabbit_graph_nodes[i];
        if(runaway){
            bool valid = false;
            for(int k = 0; k < n->count; k++)
                if(rabbit_graph_edges[rabbit_graph_adjacency[n->first + k]].level != -2) valid = true;
            if(!valid) continue;
        }
        uint64_t d = v3_length_squared64(v3_sub(a->position, rabbit_node(i)));
        if(d < best){
            best = d;
            result = i;
        }
    }
    a->args[0] = result;
    a->args[1] = a->args[2] = -1;
}

static void rabbit_select_run(Actor* a, int32_t threshold){
    const RabbitGraphNode* n = &rabbit_graph_nodes[a->args[0]];
    v3 away = v3_direction14(v3_neg(actor_to_mario(a)), a->front);
    int rank = -1;
    int32_t best = INT32_MIN;
    int selected = -1;
    for(int k = 0; k < n->count; k++){
        int i = rabbit_graph_adjacency[n->first + k];
        const RabbitGraphEdge* e = &rabbit_graph_edges[i];
        if(e->level < -1) continue;
        int next = e->nodes[0] == a->args[0] ? e->nodes[1] : e->nodes[0];
        v3 dir = v3_direction14(v3_sub(rabbit_node(next), rabbit_node(a->args[0])), a->front);
        int32_t dot = v3_dot14(dir, away);
        int level = a->args[3] == 0;
        int r = ((e->level == -1 || e->level == level) ? 4 : 0) | (dot > threshold ? 2 : 0) |
                (next != a->args[2] ? 1 : 0);
        if(r > rank || (r == rank && dot > best)){
            rank = r;
            best = dot;
            selected = i;
        }
    }
    a->args[1] = selected;
}
static void rabbit_advance_node(Actor* a){
    int next = rabbit_next(a);
    a->args[2] = a->args[0];
    a->args[0] = next;
    a->args[1] = -1;
}

static bool rabbit_actor_init(Actor* a, const StageObject* o){
    if(a->type == OBJ_RABBIT){
        a->anim = ANIM_RABBIT_WAIT2;
        return true;
    }
    if(a->type != OBJ_TRICKRABBIT) return false;
    rabbit_nearest_node(a, false);
    a->args[3] = -1;
    unsigned saved = stream_part_state(a->stage_index);
    actor_set_state(a, saved == 2 ? TR_GIVE_UP : saved == 1 ? TR_WAIT : TR_WAIT_START, ANIM_TRICKRABBIT_WAIT);
    if(saved) a->args[3] = 0;
    return true;
}
static void rabbit_physics(Actor* a, int32_t friction){
    actor_follow_gravity(a, N14_ONE);
    actor_move(a, V16(1), friction, friction);
    actor_bounce_wall(a, FX(60), 0);
}
static fx rabbit_run_move(Actor* a){
    v3 next = rabbit_node(rabbit_next(a));
    v3 flat = v3_reject14(v3_sub(next, a->position), a->up);
    fx distance = v3_length_exact(flat);
    if(distance > 0) actor_turn_toward(a, flat, DEG(11.47834f));
    int speed = rabbit_edge_arg(a, 1, 100);
    actor_push_forward(a, (V16(2.4f) * speed) / 100);

    v3 from = rabbit_node(a->args[0]), edge = v3_sub(next, from);
    fx len = v3_length_exact(edge);
    v3 dir = v3_direction14(edge, a->front);
    fx along = fx_min(len, fx_max(0, v3_dot14(v3_sub(a->position, from), dir)));
    v3 correction = v3_reject14(v3_sub(v3_add(from, v3_scale14(dir, along)), a->position), a->up);
    fx separation = v3_length_exact(correction);
    if(separation > 100){
        int weight = fx_min(N14_ONE, (separation - 100) * N14_ONE / 300);
        a->velocity =
            v3_add(a->velocity, v3_scale14(v3_direction14(correction, a->front), mul14(V16(1), weight)));
    }
    if((a->timer & 31) == 3) audio_play_effect(SFX_RABBITJUMP, 1);
    rabbit_physics(a, R14(.85f));
    return distance;
}
static void rabbit_wait(Actor* a){
    actor_turn_toward(a, actor_to_mario(a), DEG(10));
    rabbit_physics(a, R14(.85f));
}
static bool rabbit_is_demo(const Actor* a){ return a->state >= TR_DEMO_RUN && a->state <= TR_DEMO_END; }

static void rabbit_begin_jump(Actor* a, bool comeback){
    a->target = a->position;
    v3 delta = v3_sub(rabbit_node(rabbit_next(a)), a->position);
    fx d = v3_dot14(delta, a->up), height = comeback ? 100 : rabbit_edge_arg(a, 3, 100);
    float pa, pb;
    flight_parabola_params((float)(height + fx_max(0, d)), (float)d, &pa, &pb);
    a->velocity = V3((int32_t)pa, (int32_t)pb - d, 0);
    int duration = comeback ? -1 : rabbit_edge_arg(a, 2, -1);
    if(duration < 0){
        v3 prior = a->target;
        fx length = 0;
        for(int i = 1; i <= 10; i++){
            int t = i * N14_ONE / 10;
            fx lift = (int64_t)t * (a->velocity.y + mul14(a->velocity.x, t)) >> 14;
            v3 p = v3_add(v3_add(a->target, v3_scale14(delta, t)), v3_scale14(a->up, lift));
            length += v3_length_exact(v3_sub(p, prior));
            prior = p;
        }
        duration = length / 22;
    }
    a->health = (int16_t)fx_max(0, duration);
    a->flags &= ~ACTOR_ON_GROUND;
    actor_set_state(a,
                    comeback            ? TR_COMEBACK
                    : rabbit_is_demo(a) ? TR_DEMO_JUMP
                                        : TR_JUMP,
                    ANIM_TRICKRABBIT_JUMP);
    audio_play_effect(SFX_RABBITJUMP, 1);
}
static void rabbit_jump(Actor* a){
    v3 previous = a->position, delta = v3_sub(rabbit_node(rabbit_next(a)), a->target);
    int t = a->health ? fx_min(N14_ONE, a->timer * N14_ONE / a->health) : N14_ONE;
    fx lift = (int64_t)t * (a->velocity.y + mul14(a->velocity.x, t)) >> 14;
    a->position = v3_add(v3_add(a->target, v3_scale14(delta, t)), v3_scale14(a->up, lift));
    actor_turn_toward(a, delta, DEG(25.84193f));
    if(a->timer >= a->health){
        bool demo = a->state == TR_DEMO_JUMP;
        a->velocity = V3((a->position.x - previous.x) * 256, (a->position.y - previous.y) * 256,
                         (a->position.z - previous.z) * 256);
        actor_set_state(a, demo ? TR_DEMO_RUN : TR_RUN, ANIM_TRICKRABBIT_RUN);
    }
}
static void rabbit_caught(Actor* a){
    if(!rabbit_catchable(a) || g_mario_held) return;
    talk_forward_actor(a);
    a->target = g_game.player.figure.position;
    a->velocity = V3(0, 0, 0);
    mario_hold_begin();
    mario_restart_anim(ANIM_MARIO_CATCH);
    actor_set_state(a, TR_CAUGHT, ANIM_TRICKRABBIT_CAUGHT);
    audio_stop_music();
    audio_play_effect(SFX_RABBITCAPTURE, 5);
}
static void rabbit_star_piece(Actor* a){
    if(a->type == OBJ_RABBIT){
        a->anim = ANIM_RABBIT_REACTION;
        a->anim_frame = 0;
        return;
    }
    if(a->state == TR_WAIT_START || a->state == TR_GIVE_UP){
        a->anim = ANIM_TRICKRABBIT_REACTION;
        a->anim_frame = 0;
        audio_play_effect(SFX_RABBITSTARHIT, 2);
        return;
    }
    if(!rabbit_can_tumble(a)) return;
    audio_play_effect(SFX_RABBITSTARHIT, 2);
    actor_set_state(a, a->state == TR_JUMP && a->timer > 1 ? TR_FALL : TR_TUMBLE,
                    a->state == TR_JUMP && a->timer > 1 ? ANIM_TRICKRABBIT_FALL
                                                        : ANIM_TRICKRABBIT_FALLDOWNLOOP);
}
static void rabbit_talk_ended(Actor* a){
    if(a->state == TR_WAIT_START){
        rabbit_nearest_node(a, false);
        mario_hold_begin();
        mario_restart_anim(ANIM_MARIO_WAIT);
        const StageObject* o = actor_stage_object(a);
        if(o) camera_event_start(o->camera, 1, a->position, a->up, a->front);
        actor_set_state(a, rabbit_select_demo(a) ? TR_DEMO_RUN : TR_DEMO_END, ANIM_TRICKRABBIT_RUN);
        stream_part_state_set(a->stage_index, 1);
    } else if(a->state == TR_CAUGHT_EVENT){
        audio_stop_level_effect(SFX_RABBITSTRUGGLE);
        if(g_mario_held) mario_hold_release(V3(0, 0, 0));
        level_star_appear(1, v3_add(a->position, v3_scale14(a->up, FX(200))));
        actor_set_state(a, TR_STAR_DEMO, ANIM_TRICKRABBIT_WAIT);
        stream_part_state_set(a->stage_index, 2);
    }
}
static void rabbit_place_caught_mario(Actor* a){
    Player* p = &g_game.player;
    v3 pos = a->position;
    if(a->state == TR_CAUGHT && a->timer < 5){
        int t = a->timer * N14_ONE / 5;
        pos = v3_add(a->target, v3_scale14(v3_sub(pos, a->target), t));
    }
    p->up = p->gravity_up = a->up;
    p->on_ground = false;
    p->on_wall = false;
    mario_hold_move(pos, a->front);
}
static void rabbit_think(Actor* a){
    talk_actor_think(a);
    if(!talk_active()){
        int hit = actor_mario_contact(a, FX(80));
        int anim = hit == CONTACT_SPIN                                  ? ANIM_RABBIT_SPIN
                   : (hit == CONTACT_TRAMPLE || hit == CONTACT_HIPDROP) ? ANIM_RABBIT_PRESS
                                                                        : -1;
        if(anim >= 0 && a->anim != anim){
            a->anim = anim;
            a->anim_frame = 0;
        } else if(a->anim != ANIM_RABBIT_WAIT2 &&
                  (a->anim_frame >> 8) >= anim_length_of(a->figure.model, a->anim) - 1){
            a->anim = ANIM_RABBIT_WAIT2;
            a->anim_frame = 0;
        }
    }
}

static void trick_rabbit_think(Actor* a){
    if(a->args[3] > 0) a->args[3]--;
    if(rabbit_catchable(a) && g_game.player.action != MA_DEATH && !g_mario_held){
        int contact = actor_mario_contact(a, FX(80));
        v3 caught = v3_sub(mario_centre(), rabbit_contact_centre(a));
        fx r = FX(30) + C_BODY_RADIUS;
        if(contact == CONTACT_SPIN || v3_length_squared64(caught) < (uint64_t)r * r){
            rabbit_caught(a);
            return;
        }
        if(contact == CONTACT_TRAMPLE || contact == CONTACT_HIPDROP || contact == CONTACT_STAR){
            rabbit_star_piece(a);
            audio_play_effect(SFX_RABBITTRAMPLE, 2);
            return;
        }
    }
    switch(a->state){
    case TR_WAIT_START:
    case TR_GIVE_UP:
        rabbit_physics(a, R14(.85f));
        talk_actor_think(a);
        if(!talk_active()){
            int hit = actor_mario_contact(a, FX(80));
            int anim = hit == CONTACT_SPIN                                  ? ANIM_TRICKRABBIT_SPIN
                       : (hit == CONTACT_TRAMPLE || hit == CONTACT_HIPDROP) ? ANIM_TRICKRABBIT_PRESS
                                                                            : -1;
            if(anim >= 0 && a->anim != anim){
                a->anim = anim;
                a->anim_frame = 0;
            } else if(a->anim != ANIM_TRICKRABBIT_WAIT &&
                      (a->anim_frame >> 8) >= anim_length_of(a->figure.model, a->anim) - 1){
                a->anim = ANIM_TRICKRABBIT_WAIT;
                a->anim_frame = 0;
            }
            actor_block_mario(a, FX(70), FX(120));
        }
        break;
    case TR_WAIT:
        rabbit_wait(a);
        if(actor_mario_within(a, FX(1200))){
            rabbit_nearest_node(a, true);
            rabbit_select_run(a, R14(-.3f));
            actor_set_state(a, TR_RUN, ANIM_TRICKRABBIT_RUN);
        }
        break;
    case TR_RUN:
    case TR_DEMO_RUN: {
        bool demo = a->state == TR_DEMO_RUN;
        if(rabbit_run_move(a) < 125){
            rabbit_advance_node(a);
            if(demo && !rabbit_select_demo(a)){
                actor_set_state(a, TR_DEMO_END, ANIM_TRICKRABBIT_FOLLOWME);
                break;
            }
            if(!demo) rabbit_select_run(a, 0);
        }
        if(!demo && !actor_mario_within(a, FX(1500))){
            actor_set_state(a, TR_WAIT, ANIM_TRICKRABBIT_FOLLOWME);
            break;
        }
        if(a->args[1] >= 0 && rabbit_graph_edges[a->args[1]].jump == 1){
            a->health = rabbit_edge_arg(a, 1, 20);
            actor_set_state(a, demo ? TR_DEMO_JUMP_START : TR_JUMP_START, ANIM_TRICKRABBIT_JUMPSTART);
            break;
        }
        if(!demo && a->timer > 60 && actor_mario_within(a, FX(600)) &&
           v3_dot14(v3_direction14(v3_sub(rabbit_node(rabbit_next(a)), a->position), a->front),
                    v3_direction14(actor_to_mario(a), a->front)) > R14(.1f)){
            int old = a->args[0];
            a->args[0] = rabbit_next(a);
            a->args[2] = old;
            actor_set_state(a, TR_BRAKE, ANIM_TRICKRABBIT_BRAKE);
        }
        break;
    }
    case TR_DEMO_JUMP_START:
    case TR_JUMP_START:
    case TR_COMEBACK_START:
        rabbit_physics(a, R14(.88f));
        actor_turn_toward(a, v3_sub(rabbit_node(rabbit_next(a)), a->position), DEG(8.10961f));
        if(a->timer >= 10 && a->anim == ANIM_TRICKRABBIT_JUMPSTART){
            a->anim = ANIM_TRICKRABBIT_JUMPSTARTLOOP;
            a->anim_frame = 0;
        }
        if(a->timer > a->health) rabbit_begin_jump(a, a->state == TR_COMEBACK_START);
        break;
    case TR_JUMP:
    case TR_DEMO_JUMP:
    case TR_COMEBACK: rabbit_jump(a); break;
    case TR_DEMO_END:
        rabbit_wait(a);
        if(a->timer >= 30){
            camera_event_cancel();
            if(g_mario_held) mario_hold_release(V3(0, 0, 0));
            a->args[3] = 1200;
            actor_set_state(a, TR_WAIT, ANIM_TRICKRABBIT_FOLLOWME);
        }
        break;
    case TR_BRAKE:
        a->front = v3_tangent14(v3_sub(rabbit_node(rabbit_next(a)), a->position), a->up, a->front);
        rabbit_physics(a, R14(.93f));
        if(a->timer > 40) actor_set_state(a, TR_RUN, ANIM_TRICKRABBIT_RUN);
        break;
    case TR_FALL:
        rabbit_physics(a, (a->flags & ACTOR_ON_GROUND) ? R14(.85f) : R14(.99f));
        if(a->flags & ACTOR_ON_GROUND){
            a->velocity = V3(0, 0, 0);
            actor_set_state(a, TR_FALL_LAND, ANIM_TRICKRABBIT_FALLDOWNLAND);
        }
        break;
    case TR_FALL_LAND:
        if(a->timer >= 14 && a->anim == ANIM_TRICKRABBIT_FALLDOWNLAND){
            a->anim = ANIM_TRICKRABBIT_FALLDOWNLOOP;
            a->anim_frame = 0;
        }
        if(a->timer > 20) actor_set_state(a, TR_FALL_GETUP, ANIM_TRICKRABBIT_FALLDOWNEND);
        break;
    case TR_FALL_GETUP:
        a->velocity = V3(0, 0, 0);
        if(a->timer > 20){
            a->health = 8;
            actor_set_state(a, TR_COMEBACK_START, ANIM_TRICKRABBIT_JUMPSTART);
        }
        break;
    case TR_TUMBLE:
        if(a->flags & ACTOR_ON_GROUND) a->velocity = V3(0, 0, 0);
        else rabbit_physics(a, R14(.99f));
        if(a->timer > 20 && (a->flags & ACTOR_ON_GROUND))
            actor_set_state(a, TR_GETUP, ANIM_TRICKRABBIT_FALLDOWNEND);
        break;
    case TR_GETUP:
        a->velocity = V3(0, 0, 0);
        if(a->timer > 20) actor_set_state(a, TR_RUN, ANIM_TRICKRABBIT_RUN);
        break;
    case TR_CAUGHT:
        if(a->timer == 5){
            a->velocity = v3_scale14(a->up, V16(20));
            a->flags &= ~ACTOR_ON_GROUND;
        }
        rabbit_physics(a, R14(.99f));
        rabbit_place_caught_mario(a);
        if(a->timer >= 20 && (a->flags & ACTOR_ON_GROUND)){
            a->velocity = V3(0, 0, 0);
            actor_set_state(a, TR_CAUGHT_LAND, ANIM_TRICKRABBIT_CAUGHTLAND);
            mario_restart_anim(ANIM_MARIO_CATCHLAND);
        }
        break;
    case TR_CAUGHT_LAND:
        rabbit_place_caught_mario(a);
        audio_level_effect(SFX_RABBITSTRUGGLE);
        if(a->timer >= 15){
            actor_set_state(a, TR_CAUGHT_EVENT, ANIM_TRICKRABBIT_CAUGHTWAIT);
            mario_restart_anim(ANIM_MARIO_CATCHWAIT);
        }
        break;
    case TR_CAUGHT_EVENT:
        rabbit_place_caught_mario(a);
        audio_level_effect(SFX_RABBITSTRUGGLE);
        talk_force_actor(a);
        if(a->state == TR_CAUGHT_EVENT){
            a->anim = ANIM_TRICKRABBIT_CAUGHTWAIT;
            mario_set_anim(ANIM_MARIO_CATCHWAIT);
        }
        break;
    case TR_STAR_DEMO:
        rabbit_wait(a);

        {
            bool busy = false;
            for(int i = 0; i < g_game.actor_count; i++){
                const Actor* s = &g_game.actors[i];
                if(s->active && s->type == OBJ_POWERSTAR && s->args[0] == 1 && s->value >= 0) busy = true;
            }
            if(!busy && a->timer > 0){
                talk_forward_actor(a);
                actor_set_state(a, TR_GIVE_UP, ANIM_TRICKRABBIT_WAIT);
            }
        }
        break;
    default: break;
    }
    if(rabbit_is_demo(a)) camera_event_target(a->position, a->up, a->front);
}

static int trick_rabbit_talk_pose(const Actor* a, int anim){
    return anim && a->state == TR_GIVE_UP ? ANIM_TRICKRABBIT_GIVEUPTALK : anim;
}
