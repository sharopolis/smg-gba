static EWRAM_BSS MarioLink g_mario_link;

static void mario_link_reset(void){
    g_mario_link = (MarioLink){0};
    g_mario_link.ride = -1;
    g_mario_link.hipdrop = -1;
    g_mario_link.bind = -1;
}

static void mario_talk_pause(int ticks){
    Player* p = &g_game.player;
    if(p->action == MA_DEATH) return;
    g_mario_link.talk_timer = ticks;
    mario_stop_walk();
    mario_set_action(MA_TALK);
    mario_set_anim(ANIM_MARIO_WAIT);
}

static bool mario_talk_paused(void){ return g_mario_link.talk_timer > 0; }

static void mario_bind_start(Actor* a){
    Player* p = &g_game.player;
    g_mario_link.bind = (int16_t)actor_index(a);
    mario_stop_walk();
    g_mario.jumping = false;
    g_mario.jump_vec = V3(0, 0, 0);
    p->on_ground = false;
    mario_set_action(MA_TALK);
}

static void mario_bind_pose(v3 pos, v3 up, v3 front){
    Player* p = &g_game.player;
    p->figure.position = pos;
    g_mario.position16 = V3(pos.x << 8, pos.y << 8, pos.z << 8);
    p->up = up;
    p->front = v3_tangent14(front, up, p->front);
}

static void mario_bind_release(v3 v, int type, int anim){
    Player* p = &g_game.player;
    g_mario_link.bind = -1;
    p->up = p->gravity_up;
    g_bee.revive = 0;
    g_bee.drift = 0;
    g_bee.flying = false;
    mario_take_off(type, v, true);
    mario_set_action(MA_FALL);
    mario_restart_anim(anim);
}

static bool dyncol_owner_possible(v3 a, v3 b, fx radius){
    if(icestep_stage() && g_icestep_alive) return true;
    v3 lo = V3(fx_min(a.x, b.x) - radius, fx_min(a.y, b.y) - radius, fx_min(a.z, b.z) - radius);
    v3 hi = V3(fx_max(a.x, b.x) + radius, fx_max(a.y, b.y) + radius, fx_max(a.z, b.z) + radius);
    for(int g = 0; g < g_dyncol_group_count; g++){
        const DynColGroup* d = &g_dyncol_groups[g];
        if(!(d->lo.x > hi.x || d->hi.x < lo.x || d->lo.y > hi.y || d->hi.y < lo.y || d->lo.z > hi.z ||
             d->hi.z < lo.z))
            return true;
    }
    return false;
}

static void mario_ride_find(void){
    Game* game = &g_game;
    Player* p = &game->player;
    g_mario_link.ride = -1;
    g_mario_link.hipdrop = -1;
    if(!p->on_ground || g_mario.jumping) return;
    RayHit h;
    v3 feet = p->figure.position;
    v3 from = v3_add(feet, v3_scale14(p->up, FX(40))), to = v3_sub(feet, v3_scale14(p->up, FX(60)));

    if(!dyncol_owner_possible(from, to, FXC(0.05f))) return;
    if(!collision_ray(from, to, &h, 0)) return;
    int o = dyncol_owner(h.tri);
    if(o < 0 || o >= game->actor_count) return;
    const Actor* a = &game->actors[o];
    g_mario_link.ride = (int16_t)o;
    if(p->action == MA_HIPDROP_LAND && p->action_timer == 0 &&
       (a->type != OBJ_BOSSSTINKBUG || dyncol_part(h.tri) == 1))
        g_mario_link.hipdrop = (int16_t)o;
    g_mario_link.ride_pos = a->type == OBJ_FORESTOBSTACLEWOOD ? obstacle_wood_origin(a) : a->position;
    g_mario_link.ride_up = a->up;
    g_mario_link.ride_front = a->front;
    if(a->type == OBJ_BOSSSTINKBUG){
        int16_t scale[3];
        boss_stinkbug_body_frame(a, &g_mario_link.ride_pos, &g_mario_link.ride_up, &g_mario_link.ride_front,
                                 scale);
    } else if(!valve_ride_frame(a, &g_mario_link.ride_pos, &g_mario_link.ride_up, &g_mario_link.ride_front))
        volcano_part_ride_frame(a, &g_mario_link.ride_pos, &g_mario_link.ride_up, &g_mario_link.ride_front);
}

static void mario_ride_carry(void){
    Game* game = &g_game;
    Player* p = &game->player;
    if(g_mario_link.ride < 0) return;
    const Actor* a = &game->actors[g_mario_link.ride];
    if(!a->active) return;
    v3 pos = a->type == OBJ_FORESTOBSTACLEWOOD ? obstacle_wood_origin(a) : a->position, up = a->up,
       front = a->front;
    if(a->type == OBJ_BOSSSTINKBUG){
        int16_t scale[3];
        boss_stinkbug_body_frame(a, &pos, &up, &front, scale);
    } else if(!valve_ride_frame(a, &pos, &up, &front)) volcano_part_ride_frame(a, &pos, &up, &front);
    if(pos.x == g_mario_link.ride_pos.x && pos.y == g_mario_link.ride_pos.y &&
       pos.z == g_mario_link.ride_pos.z && up.x == g_mario_link.ride_up.x && up.y == g_mario_link.ride_up.y &&
       up.z == g_mario_link.ride_up.z && front.x == g_mario_link.ride_front.x &&
       front.y == g_mario_link.ride_front.y && front.z == g_mario_link.ride_front.z)
        return;
    v3 side0 = v3_cross14(g_mario_link.ride_up, g_mario_link.ride_front), side1 = v3_cross14(up, front);
    v3 d = v3_sub(p->figure.position, g_mario_link.ride_pos);
    fx lx = v3_dot14(d, side0), ly = v3_dot14(d, g_mario_link.ride_up),
       lz = v3_dot14(d, g_mario_link.ride_front);
    v3 w = v3_add(pos, v3_add(v3_add(v3_scale14(side1, lx), v3_scale14(up, ly)), v3_scale14(front, lz)));

    fx fx_ = v3_dot14(p->front, side0), fz = v3_dot14(p->front, g_mario_link.ride_front);
    v3 f = v3_add(v3_scale14(side1, fx_), v3_scale14(front, fz));
    p->front = v3_tangent14(f, p->up, p->front);
    p->figure.position = w;
    g_mario.position16 = V3(w.x << 8, w.y << 8, w.z << 8);
}

static int mario_ride_actor(void){ return g_mario_link.ride; }

static int mario_hipdrop_actor(void){ return g_mario_link.hipdrop; }
