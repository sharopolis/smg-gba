enum { PF_WAIT = 64, PF_APPEAR, PF_WAIT2, PF_TAKEN };
static const FigureHeader* powerflower_model(const Actor* a){
    return (const FigureHeader*)actor_kinds[a->type].figure;
}
static void powerflower_wait(Actor* a){
    a->position = a->home;
    a->velocity = V3(0, 0, 0);
    a->state = PF_WAIT;
    a->timer = 0;
    a->value = 0;
    a->health = 0;
    a->anim_frame = FX(1000);
    a->figure.model = powerflower_model(a);
    a->radius = FX(80);
    a->anim = 0;
    if(a->args[3] == -1){
        a->figure.model = (const FigureHeader*)figure_crystalbox;
        a->radius = FX(130);
        a->anim = -1;
    } else if(a->args[3] == 0){
        a->figure.model = (const FigureHeader*)figure_itembubble;
        a->radius = FX(110);
        a->anim = -1;
    }
}
static void powerflower_init(Actor* a, const StageObject* o){
    powerflower_wait(a);
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    if(mario_power_owns(a->stage_index)){
        a->state = PF_TAKEN;
        a->figure.model = NULL;
    }
    actor_scale_from_object(a, o);
}
static bool powerflower_touch(const Actor* a, bool attack){
    int height = a->state == PF_WAIT && a->args[3] == -1  ? 190
                 : a->state == PF_WAIT && a->args[3] == 0 ? 0
                                                          : 60;
    v3 d = v3_sub(v3_add(a->position, v3_scale14(a->up, FX(height))), mario_centre());
    fx radius = a->radius + C_BODY_RADIUS + (attack ? FX(60) : 0);
    return v3_length_squared64(d) <= (uint64_t)((int64_t)radius * radius);
}
static void powerflower_appear(Actor* a, bool crystal){
    a->state = crystal ? PF_APPEAR : PF_WAIT2;
    a->timer = 0;
    a->anim = 0;
    a->anim_frame = 0;
    a->figure.model = powerflower_model(a);
    a->radius = FX(80);
    a->health = 0;
    a->velocity = crystal ? v3_scale14(a->up, G(25)) : V3(0, 0, 0);
}
static void powerflower_think(Actor* a){
    Player* p = &g_game.player;
    if(a->state == PF_TAKEN){
        if(!mario_power_owns(a->stage_index)) powerflower_wait(a);
        return;
    }
    if(a->state == PF_WAIT && a->args[3] == -1){
        bool spin = g_mario.attack_timer > 0 || p->action == MA_HIPDROP || p->action == MA_HIPDROP_LAND;
        if(spin && powerflower_touch(a, true)) powerflower_appear(a, true);
        return;
    }
    if(a->state == PF_APPEAR || (a->state == PF_WAIT2 && a->args[2] != 1 && a->health <= 3)){
        v3 next = v3_add(a->position, V3(a->velocity.x >> 8, a->velocity.y >> 8, a->velocity.z >> 8));
        RayHit hit;
        v3 from = v3_add(a->position, v3_scale14(a->up, FX(60)));
        v3 to = v3_sub(next, v3_scale14(a->up, FX(5)));
        if(v3_dot14(a->velocity, a->up) < 0 && collision_ray(from, to, &hit, 0) &&
           v3_dot14(hit.normal, a->up) > R14(.17f)){
            next = hit.point;
            if(v3_length_exact(a->velocity) < G(2) || a->health >= 3) a->velocity = V3(0, 0, 0);
            else a->velocity = v3_scale14(a->velocity, -R14(.5f));
            a->health++;
        } else a->velocity = v3_add(a->velocity, v3_scale14(a->up, -G(.75f)));
        a->position = next;
        if(a->state == PF_APPEAR && a->timer > 30) a->state = PF_WAIT2;
    }
    if(a->state != PF_APPEAR){
        int mode = -1;
        if(a->stage_index >= 0) mode = g_game.stage.objects[a->stage_index].args[6];
        bool rotate = (mode & 1) != 0;
        if(a->state != PF_WAIT) rotate = (mode & 2) != 0;
        if(rotate) a->value = (int16_t)((a->value + FXC(.034906585f)) % FX_TAU);
    }
    if(p->action == MA_DEATH || p->action == MA_STAR_GET || p->action == MA_STAR_LAUNCH ||
       a->state == PF_APPEAR)
        return;
    if(powerflower_touch(a, false)){
        if(a->state == PF_WAIT && a->args[3] == 0){
            powerflower_appear(a, false);
            return;
        }
        if(g_mario_star_power) return;
        mario_power_start(a->type == OBJ_FIREFLOWER ? 2 : 3, a->stage_index);
        a->state = PF_TAKEN;
        a->timer = 0;
        a->value = 0;
        a->figure.model = NULL;
    }
}
