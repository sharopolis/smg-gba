enum { GOLD_CHOMP_MOVE = 64, GOLD_CHOMP_BREAK };

static fx rail_closest_coord(int rail, v3 pos){
    fx len = rail_length(rail), best = 0;
    uint64_t d2 = UINT64_MAX;
    for(int i = 0; i < 128; i++){
        fx coord = (int64_t)len * i / 128;
        uint64_t d = v3_length_squared64(v3_sub(rail_position_at(rail, coord), pos));
        if(d < d2){
            best = coord;
            d2 = d;
        }
    }
    fx lo = best - len / 128, hi = best + len / 128;
    for(int i = 0; i < 8; i++){
        fx a = lo + (hi - lo) / 3, b = hi - (hi - lo) / 3;
        uint64_t da = v3_length_squared64(v3_sub(rail_position_at(rail, a), pos));
        uint64_t db = v3_length_squared64(v3_sub(rail_position_at(rail, b), pos));
        if(da < db) hi = b;
        else lo = a;
    }
    best = (lo + hi) / 2;
    if(len > 0){
        best %= len;
        if(best < 0) best += len;
    }
    return best;
}

static bool gold_chomp_init(Actor* a, const StageObject* o){
    if(a->type != OBJ_WANWANROLLINGGOLD) return false;
    a->flags |= ACTOR_HIDDEN;
    a->health = stream_part_state(a->stage_index) != 0;
    a->args[0] = o->args[0] > 0 ? o->args[0] : 10;
    a->args[1] = o->rail;
    a->args[2] = o->args[5] > 0;
    a->args[3] = a->stage_index;
    a->radius = FX(225) * o->scale[0] / 256;
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    actor_set_state(a, GOLD_CHOMP_MOVE, -1);
    return true;
}

static void gold_chomp_creator_think(Actor* a){
    if(actor_has_switch(a, SWITCH_A) && actor_switch_on(a, SWITCH_A)) return;
    if(a->health || a->args[1] < 0) return;
    Actor* gold = actor_spawn_reuse(OBJ_GOLDWANWAN, a->position, a->up, a->front);
    if(!gold) return;
    a->health = 1;
    stream_part_state_set(a->stage_index, 1);
    for(int i = 0; i < 4; i++) gold->args[i] = a->args[i];
    gold->radius = a->radius;
    gold->figure.scale = a->figure.scale;
    fx coord = -1;
    for(unsigned i = 0; i < sizeof gusty_gold_coords / sizeof gusty_gold_coords[0]; i++){
        const GoldSpawnCoord* c = &gusty_gold_coords[i];
        if(c->owner == a->stage_index && c->rail == gold->args[1] && c->position[0] == gold->position.x &&
           c->position[1] == gold->position.y && c->position[2] == gold->position.z){
            coord = c->coord;
            break;
        }
    }
    if(coord < 0) coord = rail_closest_coord(gold->args[1], gold->position);
    gold->value = (int16_t)coord;
    gold->position = rail_position_at(gold->args[1], gold->value);
    gold->velocity = V3(0, 0, 0);
    gold->target = V3(0, 0, 0);
    gold->health = 0;
    gold->front = rail_tangent(gold->args[1], gold->value, gold->front);
    actor_set_state(gold, GOLD_CHOMP_MOVE, -1);
}

static void gold_chomp_break(Actor* a){
    a->figure.model = (const FigureHeader*)figure_wanwanrollinggoldbreak;
    a->velocity = V3(0, 0, 0);
    a->target.x = 0;
    actor_set_state(a, GOLD_CHOMP_BREAK, ANIM_WANWANROLLINGGOLDBREAK_BREAK);
    audio_play_effect(SFX_GOLDEXPLOSION, 5);
    camera_shake(1);
}

static void gold_chomp_move(Actor* a){
    GravitySample g = gravity_sample(g_game.stage.header, a->position);
    if(g.index >= 0) a->up = g.up;
    if(a->flags & ACTOR_ON_GROUND) a->velocity = V3(0, 0, 0);
    a->velocity = v3_sub(a->velocity, v3_scale14(a->up, V16(1.5f)));
    int rail = a->args[1];
    fx length = rail_length(rail);
    fx coord = a->value + a->args[0];
    if(rail_get(rail)->closed && length > 0) coord %= length;
    else if(coord >= length){
        if(a->args[2]) gold_chomp_break(a);
        else a->active = 0;
        return;
    }
    a->value = (int16_t)coord;
    v3 on = rail_position_at(rail, coord);
    v3 horizontal = v3_reject14(v3_sub(on, a->position), a->up);
    v3 velocity = v3_add(V3(horizontal.x * 256, horizontal.y * 256, horizontal.z * 256), a->velocity);
    fx speed = v3_length_exact(velocity);
    if(speed > V16(30)) velocity = v3_scale14(v3_direction14(velocity, a->front), V16(30));
    v3 old = a->position, next = v3_add(old, V3(velocity.x >> 8, velocity.y >> 8, velocity.z >> 8));
    RayHit floor, wall;
    bool ground = collision_ray(v3_add(old, v3_scale14(a->up, FX(60))),
                                v3_sub(next, v3_scale14(a->up, a->radius)), &floor, 0) &&
                  v3_dot14(floor.normal, a->up) > R14(.5f);
    a->flags &= ~ACTOR_ON_GROUND;
    if(ground){
        next = v3_add(floor.point, v3_scale14(a->up, a->radius));
        a->flags |= ACTOR_ON_GROUND;
        if(a->target.z >= 20) audio_play_effect(SFX_GOLDBOUND, 2);
        a->target.z = 0;
    } else if(a->target.z < 20) a->target.z++;
    v3 direction = v3_direction14(v3_sub(next, old), a->front);
    if(collision_ray(old, v3_add(next, v3_scale14(direction, a->radius)), &wall, 0) &&
       v3_dot14(wall.normal, a->up) < R14(.5f) &&
       v3_dot14(wall.normal, rail_tangent(rail, coord, a->front)) < -R14(.5f)){
        a->active = 0;
        return;
    }
    if(v3_length_squared64(v3_sub(next, old)) > 1) a->front = v3_tangent14(direction, a->up, a->front);
    a->position = next;
    a->target.x -= a->args[0] * FXC(1.1f) / fx_max(a->radius, 1);
    if(a->health < 120) a->health++;
    if(a->health >= 120 && actor_mario_within(a, FX(600))){
        audio_play_effect(SFX_GOLDVOICE, 2);
        a->health = 0;
    }
    static const uint16_t rocks[] = {OBJ_ROCK, OBJ_WANWAN};
    Actor* other = actor_overlapping_kind(a, a->position, a->radius, rocks, 2);
    if(other) actor_defeat(other, false);
    int contact = actor_mario_contact(a, 0);
    if(contact == CONTACT_STAR) gold_chomp_break(a);
    else if(contact != CONTACT_NONE) actor_hurt_mario(a, 1);
}

static void gold_chomp_think(Actor* a){
    int owner = a->args[3];
    if(owner >= 0 && switch_on(stage_object_switch(&g_game.stage.objects[owner], SWITCH_A))){
        a->active = 0;
        return;
    }
    if(a->state == GOLD_CHOMP_BREAK){
        if(a->timer == 2) level_star_appear(6, a->position);

        if(a->timer >= 119) a->active = 0;
    } else gold_chomp_move(a);
}

static void gold_chomp_figure(const Actor* a, Figure* f){
    if(a->type == OBJ_GOLDWANWAN && a->state == GOLD_CHOMP_MOVE) f->local_pitch = a->target.x;
}
