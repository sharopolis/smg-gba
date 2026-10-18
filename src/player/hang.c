static uint16_t g_hang_cooldown;

static void mario_hang_reset(void){ g_hang_cooldown = 0; }

static void mario_hang_check(void){
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    if(g_hang_cooldown || !m->jumping || p->on_ground || !p->on_wall || bee_active() ||
       p->action == MA_SWIM || p->action == MA_DAMAGE_FALL || p->action == MA_HIPDROP ||
       p->action == MA_HIPDROP_START || vertical_speed(m->jump_vec, p->up) > 0)
        return;
    v3 front = v3_neg(p->wall_normal);
    if(v3_dot14(p->front, front) < R14(.8f)) return;
    v3 above = v3_add(p->figure.position, v3_add(v3_scale14(p->up, FX(150)), v3_scale14(front, FX(80))));
    RayHit floor, block;
    if(!collision_ray(above, v3_sub(above, v3_scale14(p->up, FX(150))), &floor, 0) ||
       floor.tri >= DYNCOL_BASE || floor.attr & (COL_DEATH | COL_WATER | COL_NOT_GRAB) ||
       v3_dot14(floor.normal, p->up) < R14(.707f))
        return;
    fx height = v3_dot14(v3_sub(floor.point, p->figure.position), p->up);
    if(height < 0 || height > FX(150) || (p->has_floor_below && p->height_above_ground + height < FX(225)))
        return;
    v3 edge = v3_sub(floor.point, v3_scale14(front, FX(60)));
    v3 head = v3_add(edge, v3_scale14(p->up, FX(30)));
    if(collision_ray(head, v3_add(head, v3_scale14(front, FX(60))), &block, 0)) return;
    p->figure.position = edge;
    m->position16 = V3(edge.x << 8, edge.y << 8, edge.z << 8);
    m->jump_vec = V3(0, 0, 0);
    m->jumping = false;
    m->squat = false;
    mario_stop_walk();
    p->front = front;
    p->on_ground = false;
    mario_set_action(MA_HANG);
    mario_restart_anim(ANIM_MARIO_HANGSTARTUNDER);
}

static bool mario_hang_tick(const KeyState* keys){
    if(g_hang_cooldown) g_hang_cooldown--;
    Player* p = &g_game.player;
    MarioPhysics* m = &g_mario;
    if(p->action != MA_HANG && p->action != MA_CLIMB_UP) return false;
    if(p->action == MA_CLIMB_UP){
        if(!p->on_ground) mario_move_and_collide(v3_scale14(p->front, G(6)), true);
        if(mario_anim_done()){
            g_hang_cooldown = 120;
            mario_set_action(p->on_ground ? MA_WAIT : MA_FALL);
            if(!p->on_ground) m->jumping = true;
            mario_set_anim(p->on_ground ? ANIM_MARIO_WAIT : ANIM_MARIO_FALL);
        }
        return true;
    }
    if(mario_anim_done()) mario_set_anim(ANIM_MARIO_HANGWAIT);
    int32_t direction = m->stick ? v3_dot14(m->move_dir, p->front) : 0;
    if(direction < -R14(.5f) && p->action_timer >= 20){
        v3 pos = v3_sub(p->figure.position, v3_scale14(p->front, FX(100)));
        p->figure.position = pos;
        m->position16 = V3(pos.x << 8, pos.y << 8, pos.z << 8);
        mario_take_off(JUMP_FIRST, V3(0, 0, 0), false);
        m->hold = false;
        g_hang_cooldown = 120;
        mario_set_action(MA_FALL);
        mario_set_anim(ANIM_MARIO_FALL);
    } else if((keys->pressed & KEY_A) || (direction > R14(.5f) && p->action_timer > 18)){
        RayHit obstruction;
        v3 head = v3_add(p->figure.position, v3_scale14(p->up, FX(30)));
        if(!collision_ray(head, v3_add(head, v3_scale14(p->front, FX(60))), &obstruction, 0) &&
           !collision_ray(p->figure.position, v3_add(p->figure.position, v3_scale14(p->up, FX(160))),
                          &obstruction, 0)){
            mario_set_action(MA_CLIMB_UP);
            mario_restart_anim(ANIM_MARIO_HANGUP);
        }
    }
    return true;
}
