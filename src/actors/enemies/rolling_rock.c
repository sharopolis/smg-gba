enum { ROCK_ROLL = 64, ROCK_APPEAR, ROCK_LAUNCH };

static fx rock_rail_nearest(int rail, v3 p){
    fx len = rail_length(rail), best = 0;
    uint64_t bd = UINT64_MAX;
    for(fx d = 0; d <= len; d += 256){
        uint64_t q = v3_length_squared64(v3_sub(rail_position_at(rail, d), p));
        if(q < bd){
            bd = q;
            best = d;
        }
    }
    for(int step = 32; step >= 4; step >>= 3){
        fx c = best;
        for(fx d = c - 8 * step; d <= c + 8 * step; d += step){
            uint64_t q = v3_length_squared64(v3_sub(rail_position_at(rail, d), p));
            if(q < bd){
                bd = q;
                best = d;
            }
        }
    }
    if(rail_closed(rail) && len > 0){
        best %= len;
        if(best < 0) best += len;
    }
    return rail_closed(rail) ? best : fx_clamp(best, 0, len);
}

static void rockcreator_think(Actor* a){
    int interval = actor_arg_or(a, 1, -1);
    if(a->state == ROCKCREATOR_WAIT && a->health && (interval < 0 || a->timer < interval * 60)) return;
    bool wanwan = a->type == OBJ_WANWANROLLING;
    Actor* r = actor_spawn_reuse(wanwan ? OBJ_WANWAN : OBJ_ROCK, a->position, a->up, a->front);
    if(!r && stream_evict_preserving(game_player()->figure.position, a->position))
        r = actor_spawn_reuse(wanwan ? OBJ_WANWAN : OBJ_ROCK, a->position, a->up, a->front);
    if(!r) return;
    a->health = 1;
    actor_set_state(a, ROCKCREATOR_WAIT, -1);

    if(interval < 0) stream_part_state_set(a->stage_index, 1);
    int rail = actor_rail(a);
    r->args[0] = (int16_t)actor_arg_or(a, 0, 10);
    r->args[1] = (int16_t)rail;
    r->args[2] = (int16_t)(wanwan ? actor_object_arg(a, 5) : actor_arg_or(a, 2, -1));
    r->args[3] = (int16_t)actor_arg_or(a, 3, 8);
    r->value = 0;

    const StageObject* o = actor_stage_object(a);
    int sc = o && o->scale[0] ? o->scale[0] : 256;
    r->figure.scale = V3(64 * sc / 256, 64 * sc / 256, 64 * sc / 256);
    r->radius = FX(225) * sc / 256;
    r->home = a->position;
    r->target = V3(0, 0, 0);
    GravitySample g = gravity_sample(g_game.stage.header, a->position);
    if(g.index >= 0){
        r->front = v3_tangent14(v3_rotate_between14(r->front, r->up, g.up), g.up, r->front);
        r->up = g.up;
    }
    if(rail >= 0){
        if(!a->target.z){
            a->target.y = rock_rail_nearest(rail, a->position);
            a->target.z = 1;
        }
        r->target.y = a->target.y;

        fx drop = rail_closed(rail) ? rock_rail_drop(rail, r->target.y, r->radius) : r->radius;
        r->position = v3_sub(rail_position_at(rail, r->target.y), v3_scale14(r->up, drop));
    }
    if(rail >= 0 && !rail_closed(rail)){
        actor_set_state(r, ROCK_APPEAR, -1);

        r->target.x = r->args[3] * 7 * FXC(1.1f) / fx_max(r->radius, 1);
    } else {
        actor_set_state(r, ROCK_ROLL, -1);
    }
}

static void rock_break(Actor* a, bool star_bits){
    if(star_bits) collectibles_pop(OBJ_STARBIT, actor_centre(a), a->up, 9);
    actor_defeat(a, false);
    if(v3_length_squared64(actor_to_mario(a)) < (uint64_t)FX(3000) * FX(3000))
        audio_play_effect(SFX_ROCK_BREAK, 3);
}

static bool rock_spin_hits_ruby(const Actor* a){
    if(g_mario.attack_timer <= 0) return false;
    v2 sc = fx_sin_cos(a->target.x);
    fx off = a->radius * 150 / 225;
    v3 centre = v3_add(a->position, v3_scale14(a->up, a->radius));
    v3 ruby = v3_sub(
        centre, v3_add(v3_scale14(a->front, off * sc.y / FX_ONE), v3_scale14(a->up, off * sc.x / FX_ONE)));
    v3 d = v3_sub(ruby, mario_centre());
    fx r = a->radius * 125 / 225 + C_BODY_RADIUS + FX(60);
    if(fx_abs(d.x) > r || fx_abs(d.y) > r || fx_abs(d.z) > r) return false;
    return v3_length_squared64(d) <= (uint64_t)((int64_t)r * r);
}

static v3 rock_floor_below_rail(const Actor* a, int rail){
    return v3_sub(rail_position_at(rail, a->target.y),
                  v3_scale14(a->up, rock_rail_drop(rail, a->target.y, a->radius)));
}

static void rock_think(Actor* a){
    int32_t speed = a->args[0] > 0 ? a->args[0] : 10;
    int rail = a->args[1];
    fx roll = speed * FXC(1.1f) / fx_max(a->radius, 1);
    uint32_t phase = g_game.tick + (uint32_t)actor_index(a) * 3;

    if(a->state == ROCK_ROLL){
        if(phase & 1) return;
        speed *= 2;
        roll *= 2;
    }
    bool glide = a->state == ROCK_ROLL && a->value == 0 && rail >= 0;

    bool loop = rail >= 0 && rail_closed(rail);
    bool near = !loop && v3_length_squared64(actor_to_mario(a)) < (uint64_t)FX(4000) * FX(4000);
    if(glide ? loop && !(phase & 15) : !(phase & 1)){
        GravitySample g =
            gravity_sample(g_game.stage.header, v3_add(a->position, v3_scale14(a->up, a->radius)));
        if(g.index >= 0){
            a->front = v3_tangent14(v3_rotate_between14(a->front, a->up, g.up), g.up, a->front);
            a->up = g.up;
        }
    }
    if(a->state == ROCK_APPEAR || a->state == ROCK_LAUNCH){
        int appear = a->args[3], t = a->timer, stop = a->type == OBJ_ROCK ? 4 : 25;
        if(a->state == ROCK_LAUNCH || t < appear){
            int32_t step = a->state == ROCK_LAUNCH ? speed : 7;
            a->target.y += step;
            a->target.x -= step * FXC(1.1f) / fx_max(a->radius, 1);
        } else if(t < appear + 15){
            int k = t - appear;
            a->target.x = (15 - k) * fx_sin(k * 447) * 22 / (15 * FX_ONE);
        }
        v3 on = rail_position_at(rail, a->target.y);
        v3 d = v3_sub(on, rail_position_at(rail, a->target.y - 8));
        a->front = v3_tangent14(v3_reject14(d, a->up), a->up, a->front);
        a->position = v3_sub(on, v3_scale14(a->up, a->radius));
        a->velocity = V3(0, 0, 0);
        if(a->state == ROCK_APPEAR && t >= appear + 15 + stop) actor_set_state(a, ROCK_LAUNCH, -1);
        else if(a->state == ROCK_LAUNCH && t > 45){
            a->flags &= ~ACTOR_ON_GROUND;
            actor_set_state(a, ROCK_ROLL, -1);

            if(fx_abs(rock_rail_drop(rail, a->target.y, a->radius) - a->radius) > FX(40)) a->value = 1;
        }
    } else if(rail >= 0){
        fx len = rail_length(rail);
        if(!rail_closed(rail) && a->target.y >= len){
            if(a->args[2] > 0) rock_break(a, false);
            else a->active = 0;
            return;
        }
        a->target.y += speed;
        if(rail_closed(rail) && len > 0 && a->target.y >= len) a->target.y -= len;
        a->target.x -= roll;

        if(a->value == 0 && v3_length_squared64(actor_to_mario(a)) > (uint64_t)FX(3000) * FX(3000)){
            a->flags &= ~ACTOR_HIDDEN;
            if(phase & 3) return;
            a->position = rock_floor_below_rail(a, rail);
            return;
        }
        v3 on = rock_floor_below_rail(a, rail);
        v3 d = v3_reject14(v3_sub(on, a->position), a->up);
        a->front = v3_tangent14(d, a->up, a->front);

        if(a->value == 0){
            a->position = on;
            a->velocity = V3(0, 0, 0);
            if(near && !(phase & 15)){
                v3 top = v3_add(on, v3_scale14(a->up, a->radius));
                RayHit hit;
                if(collision_ray(top, v3_sub(on, v3_scale14(a->up, FX(60))), &hit, 0) &&
                   v3_dot14(hit.normal, a->up) > R14(0.5f)){
                    a->position = hit.point;
                    v3 up = v3_direction14(
                        v3_add(v3_scale14(a->up, 3 * N14_ONE / 4), v3_scale14(hit.normal, N14_ONE / 4)),
                        a->up);
                    a->front = v3_tangent14(v3_rotate_between14(a->front, a->up, up), up, a->front);
                    a->up = up;
                } else {
                    a->value = 1;
                    a->flags &= ~ACTOR_ON_GROUND;
                }
            }
        } else {
            a->position = v3_add(a->position, d);
            a->velocity = v3_scale14(a->up, v3_dot14(a->velocity, a->up));
            a->flags &= ~ACTOR_ON_GROUND;
            actor_move(a, V16(1.5f), N14_ONE, N14_ONE);
            if(!(a->flags & ACTOR_ON_GROUND)) actor_move(a, V16(1.5f), N14_ONE, N14_ONE);
            if((a->flags & ACTOR_ON_GROUND) && fx_abs(v3_dot14(v3_sub(on, a->position), a->up)) < FX(40))
                a->value = 0;
        }

        if(near && (phase & 15) == 8){
            v3 from = v3_add(a->position, v3_scale14(a->up, a->radius));
            RayHit hit;
            if(collision_ray(from, v3_add(from, v3_scale14(a->front, a->radius + FX(40))), &hit, 0) &&
               v3_dot14(hit.normal, a->up) <= R14(0.5f) && v3_dot14(a->front, hit.normal) < -R14(0.5f)){
                if(a->type == OBJ_ROCK) rock_break(a, false);
                else a->active = 0;
                return;
            }
        }
    } else {
        v3 want = v3_scale14(a->front, V16(speed));
        v3 vert = v3_scale14(a->up, v3_dot14(a->velocity, a->up));
        a->velocity = v3_add(want, vert);
        actor_move(a, V16(1.5f), N14_ONE, N14_ONE);
        a->target.x -= roll;
        if(a->timer > 900 || v3_length_exact(v3_sub(a->position, a->home)) > FX(6000) ||
           actor_bounce_wall(a, a->radius, N14_ONE)){
            rock_break(a, false);
            return;
        }
    }
    if(!(phase & 3)){
        static const uint16_t rocks[] = {OBJ_ROCK, OBJ_WANWAN};
        Actor* other = actor_overlapping_kind(a, actor_centre(a), a->radius, rocks, 2);
        if(other && other->state != ROCK_APPEAR && a->state != ROCK_APPEAR){
            rock_break(other, false);
            rock_break(a, false);
            return;
        }
    }

    {
        v3 cd = v3_sub(v3_add(a->position, v3_scale14(a->up, a->radius)), g_game.camera.position);
        fx nc = a->radius * 400 / 225;
        bool close = fx_abs(cd.x) < nc && fx_abs(cd.y) < nc && fx_abs(cd.z) < nc &&
                     v3_length_squared64(cd) < (uint64_t)((int64_t)nc * nc);
        a->flags = (uint8_t)(close ? a->flags | ACTOR_HIDDEN : a->flags & ~ACTOR_HIDDEN);
    }
    int c = actor_mario_contact(a, FX(60));
    if(c == CONTACT_NONE) return;
    if(c == CONTACT_STAR){
        rock_break(a, a->type == OBJ_ROCK);
        return;
    }
    if(a->type == OBJ_ROCK && a->state != ROCK_APPEAR && c == CONTACT_SPIN && rock_spin_hits_ruby(a)){
        rock_break(a, true);
        return;
    }
    if(a->state == ROCK_APPEAR) actor_block_mario(a, a->radius, 2 * a->radius);
    else actor_hurt_mario(a, 1);
}

static void rock_figure(const Actor* a, Figure* f){
    f->local_pitch = a->target.x;
    if(a->radius) f->position = v3_add(f->position, v3_scale14(a->up, a->radius));
}

static void rockcreator_init(Actor* a, const StageObject* o){
    actor_set_state(a, ROCKCREATOR_WAIT, -1);
    a->target.z = 0;

    a->health = actor_arg_or(a, 1, -1) < 0 ? stream_part_state(a->stage_index) & 1 : 0;
}
