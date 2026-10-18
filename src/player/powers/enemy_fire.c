static bool mario_receive_enemy_attack_fire(v3 sender_position){
    Player* p = &g_game.player;

    if(g_mario_held || p->action == MA_TALK || p->action == MA_STAR_LAUNCH || p->action == MA_STAR_GET ||
       p->action == MA_DEATH)
        return false;
    v3 centre = v3_add(p->figure.position, v3_scale14(p->up, C_BODY_HEIGHT));
    v3 normal = v3_direction14(v3_sub(centre, sender_position), p->front);
    (void)mario_fire_hit(normal, false);

    return true;
}
