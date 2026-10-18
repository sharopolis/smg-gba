enum { HANACHAN_WALK = ACTOR_STATE_WAIT, HANACHAN_ANGRY, HANACHAN_OVERTURN, HANACHAN_DEAD, HANACHAN_BLOW };

__attribute__((noinline)) static bool hanachan_star_contact(const Actor* part){
    Player* p = &g_game.player;
    if(g_mario_star_power <= 0 || g_mario.attack_timer > 0 || g_mario_held || p->action == MA_DEATH ||
       p->action == MA_STAR_LAUNCH || p->action == MA_STAR_GET || p->action == MA_TALK ||
       p->action == MA_HIPDROP || p->action == MA_HIPDROP_START || p->action == MA_HIPDROP_LAND)
        return false;
    fx height = part->type == OBJ_HANACHANBODY && (part->value & 1) ? FX(85) : FX(100);
    v3 sensor = v3_add(part->position, v3_scale14(part->up, height));
    v3 d = v3_sub(sensor, mario_centre());
    fx radius = FX(85) + C_BODY_RADIUS;
    if(fx_abs(d.x) > radius || fx_abs(d.y) > radius || fx_abs(d.z) > radius ||
       v3_length_squared64(d) > (uint64_t)radius * radius)
        return false;

    if(g_mario.jumping && vertical_speed(g_mario.jump_vec, p->up) < 0 &&
       v3_dot14(v3_sub(p->figure.position, part->position), part->up) > FX(80))
        return true;
    bool ordinary = p->action != MA_HANG && p->action != MA_CLIMB_UP && p->action != MA_SWIM &&
                    p->action != MA_SLIDE && p->action != MA_WALL_SLIDE && p->action != MA_WALL_JUMP &&
                    p->action != MA_WALL_PUSH && p->action != MA_FIRE_DANCE && p->action != MA_FIRE_RUN;
    bool damaging = p->action == MA_DAMAGE || p->action == MA_DAMAGE_FALL;
    if(ordinary && !p->on_wall && !damaging && v3_dot14(v3_reject14(d, p->gravity_up), p->front) <= 0)
        return false;
    return true;
}

__attribute__((noinline)) static void hanachan_start_blow(Actor* head){
    if(head->state == HANACHAN_BLOW || head->state == HANACHAN_DEAD) return;
    int owner = actor_index(head);
    v3 host = head->position;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* b = &g_game.actors[i];
        if(b->active && b->type == OBJ_HANACHANBODY && b->target.x == owner &&
           b->target.y == head->stage_index && b->value == 2){
            host = b->position;
            break;
        }
    }
    v3 sender = mario_centre();
    v3 dir = v3_direction14(v3_reject14(v3_sub(host, sender), head->up), V3(0, 0, 0));
    v3 side = v3_cross14(dir, v3_neg(head->up));
    fx angle = FXC(0.2f), step = (FX_PI - 2 * FXC(0.2f)) / 4;
    if(v3_dot14(v3_sub(head->position, sender), side) < 0){
        step = -step;
        angle = FX_PI - angle;
    }
    for(int k = 0; k <= 4; k++){
        Actor* part = k == 0 ? head : NULL;
        if(k)
            for(int i = 0; i < g_game.actor_count; i++){
                Actor* b = &g_game.actors[i];
                if(b->active && b->type == OBJ_HANACHANBODY && b->target.x == owner &&
                   b->target.y == head->stage_index && b->value == k){
                    part = b;
                    break;
                }
            }
        if(part){
            v3 blow = v3_add(v3_scale14(side, fx_cos(angle) << 6), v3_scale14(dir, fx_sin(angle) << 6));
            blow = v3_direction14(blow, V3(0, 0, 0));
            part->velocity = v3_add(v3_scale14(blow, V16(30)), v3_scale14(part->up, V16(20)));
            part->state = HANACHAN_BLOW;
            part->timer = -1;
            part->args[3] = (int16_t)g_game.tick;
            if(k == 0) part->target.z = 0;
            actor_restart_anim(part, k == 0 ? ANIM_HANACHANHEAD_ROTATE : ANIM_HANACHANBODY_ROTATE);
        }
        angle += step;
    }
}

__attribute__((noinline)) static void hanachan_blow_part(Actor* part){
    if((uint16_t)part->args[3] == (uint16_t)g_game.tick){
        part->timer = -1;
        return;
    }
    v3 up = gravity_sample(g_game.stage.header, part->position).up;
    part->velocity = v3_sub(part->velocity, v3_scale14(up, V16(1)));
    v3 old = part->position;
    part->position = v3_add(old, V3(part->velocity.x >> 8, part->velocity.y >> 8, part->velocity.z >> 8));
    RayHit hit;
    bool bound = vertical_speed(part->velocity, up) <= 0 &&
                 collision_ray(v3_add(old, v3_scale14(up, FX(100))),
                               v3_sub(part->position, v3_scale14(up, FX(40))), &hit, 0) &&
                 v3_dot14(hit.normal, up) > R14(0.5f);
    if(bound){
        part->position = hit.point;
        part->velocity = V3(0, 0, 0);
    }
    if((part->timer > 15 && bound) || part->timer > 30){
        if(part->type == OBJ_HANACHAN || !(part->value & 1))
            collectibles_pop_spread(OBJ_STARBIT, part->position, up, 2, 10, 40);
        if(part->type == OBJ_HANACHAN){
            part->target.z = 1;
            actor_show_model(part, false);
        } else part->active = 0;
    }
}

static void hanachan_think(Actor* a){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    int rail = actor_rail(a);

    if(!(a->flags & 0x04)){
        a->flags |= 0x04;
        if(rail >= 0) a->velocity.x = rail_nearest_coord(rail, a->position, FX(50)) << 8;
        for(int k = 1; k <= 4; k++){
            Actor* b = actor_spawn(OBJ_HANACHANBODY, v3_sub(a->position, v3_scale14(a->front, FX(87 * k))),
                                   a->up, a->front);
            if(b){
                b->value = (int16_t)k;
                b->target.x = actor_index(a);
                b->target.y = a->stage_index;
            }
        }
    }
    switch(a->state){
    case HANACHAN_WALK:
        if(rail >= 0){
            a->velocity.x += G(4.0f);
            if(a->velocity.x >= rail_length(rail) << 8) a->velocity.x = 0;
            v3 goal = rail_position_at(rail, a->velocity.x >> 8);
            a->front = v3_turn_toward(a->front, v3_sub(goal, a->position), a->up, FXC(0.08f));
            a->position = v3_add(a->position, v3_scale14(a->front, FX(4)));
            actor_stick_to_ground(a);
        }
        break;
    case HANACHAN_ANGRY:
        a->front = v3_turn_toward(a->front, v3_sub(p->figure.position, a->position), a->up, FXC(0.08f));
        a->position = v3_add(a->position, v3_scale14(a->front, FX(13)));
        actor_stick_to_ground(a);
        if(a->timer > 300) a->state = HANACHAN_WALK;
        break;
    case HANACHAN_OVERTURN:
        if(a->timer > 180){
            a->state = HANACHAN_ANGRY;
            a->timer = 0;
        }
        break;
    case HANACHAN_DEAD:
        if(a->timer > 30){
            collectibles_pop_column(a, OBJ_STARBIT, 6);
            a->active = 0;
        }
        return;
    case HANACHAN_BLOW:
        if(!a->target.z) hanachan_blow_part(a);
        if(a->target.z){
            for(int i = 0; i < g_game.actor_count; i++){
                const Actor* b = &g_game.actors[i];
                if(b->active && b->type == OBJ_HANACHANBODY && b->target.x == actor_index(a) &&
                   b->target.y == a->stage_index)
                    return;
            }
            a->active = 0;
        }
        return;
    }
    if(p->action == MA_DEATH) return;
    if(g_mario_star_power > 0 && hanachan_star_contact(a)){
        hanachan_start_blow(a);
        return;
    }

    if(p->action == MA_HIPDROP_LAND && p->action_timer == 0 && a->state != HANACHAN_OVERTURN &&
       v3_length_squared64(v3_sub(p->figure.position, a->position)) < (uint64_t)FX(1000) * FX(1000)){
        a->state = HANACHAN_OVERTURN;
        a->timer = 0;
    }
    if(!actor_touches_mario(a, 0)){
        if(mario_spin_reaches(a) && a->state != HANACHAN_OVERTURN){
            mario_spin_hop();
            a->state = HANACHAN_OVERTURN;
            a->timer = 0;
            audio_play_effect(SFX_ENEMY_HIT, 2);
        }
        return;
    }
    bool from_above = m->jumping && vertical_speed(m->jump_vec, p->up) < 0 &&
                      v3_dot14(v3_sub(p->figure.position, a->position), a->up) > FX(80);
    if(p->action == MA_HIPDROP || (from_above && a->state == HANACHAN_OVERTURN)){
        if(p->action != MA_HIPDROP) mario_stomp_bounce(g_game.keys.held & KEY_A);
        a->state = HANACHAN_DEAD;
        a->timer = 0;
        audio_play_effect(SFX_ENEMY_DEFEAT, 3);
    } else if(from_above){
        mario_stomp_bounce(g_game.keys.held & KEY_A);
        if(a->state == HANACHAN_WALK){
            a->state = HANACHAN_ANGRY;
            a->timer = 0;
        }
    } else if(m->attack_timer > 0 && a->state != HANACHAN_OVERTURN){
        mario_spin_hop();
        a->state = HANACHAN_OVERTURN;
        a->timer = 0;
        audio_play_effect(SFX_ENEMY_HIT, 2);
    } else if(p->on_ground && a->state != HANACHAN_OVERTURN){
        mario_damage(a->position, 1);
        if(a->state == HANACHAN_ANGRY){
            a->state = HANACHAN_WALK;
            a->timer = 0;
        }
    }
}

static void hanachan_body_think(Actor* b){
    Game* game = &g_game;
    int h = b->target.x;
    if(h < 0 || h >= game->actor_count || game->actors[h].type != OBJ_HANACHAN || !game->actors[h].active ||
       game->actors[h].stage_index != b->target.y){
        b->active = 0;
        return;
    }
    if(b->state == HANACHAN_BLOW){
        hanachan_blow_part(b);
        return;
    }
    if(g_mario_star_power > 0 && hanachan_star_contact(b)){
        hanachan_start_blow(&game->actors[h]);
        if(b->state == HANACHAN_BLOW) return;
    }

    const Actor* prev = &game->actors[h];
    for(int i = 0; i < game->actor_count; i++){
        const Actor* c = &game->actors[i];
        if(c->type == OBJ_HANACHANBODY && c->active && c->target.x == h && c->value == b->value - 1){
            prev = c;
            break;
        }
    }
    v3 d = v3_sub(prev->position, b->position);
    int32_t l = v3_length_exact(d);
    if(l > FX(87)) b->position = v3_add(b->position, v3_scale14(v3_direction14(d, b->front), l - FX(87)));
    b->up = prev->up;
    b->front = v3_tangent14(d, b->up, b->front);
    actor_stick_to_ground(b);
    Player* p = &game->player;
    if(p->action != MA_DEATH && actor_touches_mario(b, 0) && game->actors[h].state != HANACHAN_OVERTURN &&
       p->on_ground)
        mario_damage(b->position, 1);
}
