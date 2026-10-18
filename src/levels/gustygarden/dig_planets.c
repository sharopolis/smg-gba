static bool dig_planet_kind(int type){
    switch(type){
    case OBJ_DODORYU:
    case OBJ_DODORYULEADHILL:
    case OBJ_MOGUCCHI:
    case OBJ_GUSTYHILL:
    case OBJ_PICKETSWITCHBIG:
    case OBJ_WORMEATENPLANET:
    case OBJ_CATERPILLAR:
    case OBJ_WORMEATENHILL:
    case OBJ_ARROWSWITCH: return true;
    }
    return false;
}

static bool dig_planet_init(Actor* a, const StageObject* o){
    switch(a->type){
    case OBJ_DODORYU: burrows_init(a, o); return true;
    case OBJ_MOGUCCHI: mogucchi_init(a, o); return true;
    case OBJ_PICKETSWITCHBIG: big_picket_init(a, o); return true;
    case OBJ_WORMEATENPLANET: worm_planet_init(a, o); return true;
    case OBJ_ARROWSWITCH: arrowswitch_init(a, o); return true;
    case OBJ_POLESQUARE:
        pole_square_init(a, o);
        a->flags |= ACTOR_HIDDEN;
        return true;
    }
    return false;
}

static void dig_planet_think(Actor* a){
    switch(a->type){
    case OBJ_DODORYU: burrows_think(a); break;
    case OBJ_DODORYULEADHILL: burrows_lead_think(a); break;
    case OBJ_MOGUCCHI: mogucchi_think(a); break;
    case OBJ_GUSTYHILL: molehill_think(a); break;
    case OBJ_PICKETSWITCHBIG: big_picket_think(a); break;
    case OBJ_WORMEATENPLANET: worm_planet_think(a); break;
    case OBJ_CATERPILLAR: caterpillar_think(a); break;
    case OBJ_WORMEATENHILL: worm_hill_think(a); break;
    case OBJ_ARROWSWITCH: arrowswitch_think(a); break;
    }
}

static bool dig_planet_unclipped(const Actor* a){
    if(a->type == OBJ_DODORYU)
        return a->args[0] != BURROWS_PHASE_WAIT || switch_on(actor_switch_key(a, SWITCH_A));
    return a->type == OBJ_DODORYULEADHILL ||
           (a->type == OBJ_CATERPILLAR && a->state == CATERPILLAR_WRIGGLE) ||
           (a->type == OBJ_WORMEATENPLANET && a->state == WORM_PLANET_EATING);
}

static bool dig_planet_no_yaw(const Actor* a){ return dig_planet_kind(a->type); }
static fx dig_planet_yaw(const Actor* a){ return a->type == OBJ_ARROWSWITCH ? a->target.x : 0; }

static fx dig_planet_extent(const Actor* a){
    return a->type == OBJ_DODORYU ? FX(700) : a->type == OBJ_CATERPILLAR ? FX(500) : 0;
}

static void dig_planet_look(v3 eye, v3 target, v3 up){
    Camera* cam = &g_game.camera;
    cam->position = eye;
    cam->target = target;
    cam->front = v3_direction14(v3_sub(target, eye), cam->front);
    cam->up = v3_tangent14(up, cam->front, cam->up);
    cam->smooth_up = cam->up;
    camera_update_angles(cam);
}

static v3 dig_planet_local(v3 origin, v3 side, v3 up, v3 front, int x, int y, int z){
    return v3_add(origin, v3_add(v3_add(v3_scale14(side, x), v3_scale14(up, y)), v3_scale14(front, z)));
}

static bool dig_planet_camera(void){
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(!a->active) continue;
        if(a->type == OBJ_DODORYU && a->state == BURROWS_DEMO){
            int phase = a->args[0];
            int which = phase == BURROWS_PHASE_OPENING  ? 0
                        : phase == BURROWS_PHASE_APPEAR ? 1
                        : phase == BURROWS_PHASE_ANGRY  ? 2
                        : phase == BURROWS_PHASE_DOWN   ? 3
                                                        : -1;
            if(which < 0) return false;
            const BurrowsCam* c = &burrows_cams[which];
            int f = fx_clamp(a->timer, 0, c->frames), k = f / BURROWS_CAM_STEP, r = f % BURROWS_CAM_STEP;
            int last = (c->frames + BURROWS_CAM_STEP - 1) / BURROWS_CAM_STEP;
            const int16_t* p = c->keys[fx_min(k, last)];
            const int16_t* q = c->keys[fx_min(k + 1, last)];
            int v[6];
            for(int n = 0; n < 6; n++) v[n] = p[n] + (q[n] - p[n]) * r / BURROWS_CAM_STEP;
            v3 origin, side, up, front;
            if(which == 0){
                origin = V3(-18482, 290, 4401);
                side = V3(-N14_ONE, 0, 0);
                up = V3(0, 0, N14_ONE);
                front = V3(0, N14_ONE, 0);
            } else {
                origin = a->position;
                up = a->up;
                front = a->front;
                side = v3_cross14(up, front);
            }
            dig_planet_look(dig_planet_local(origin, side, up, front, v[0], v[1], v[2]),
                            dig_planet_local(origin, side, up, front, v[3], v[4], v[5]), up);
            return true;
        }
        if(a->type == OBJ_WORMEATENPLANET && a->state == WORM_PLANET_EATING){
            Actor* head = actor_find_type(OBJ_CATERPILLAR);
            if(!head) return false;
            v3 eye = v3_add(head->position,
                            v3_add(v3_scale14(head->up, FX(1400)), v3_scale14(head->front, -FX(2200))));
            dig_planet_look(eye, head->position, head->up);
            return true;
        }
    }
    return false;
}
