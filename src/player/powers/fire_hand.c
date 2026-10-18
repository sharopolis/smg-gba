__attribute__((noinline)) static v3 mario_fire_hand_position(void){
    const Player* p = &g_game.player;
    const MarioFireHandClip* clip = &mario_fire_hands[0];
    for(unsigned i = 0; i < sizeof mario_fire_hands / sizeof mario_fire_hands[0]; i++)
        if(mario_fire_hands[i].anim == p->anim){
            clip = &mario_fire_hands[i];
            break;
        }
    int frame = p->anim_frame >> 8;
    if(clip->loop && clip->duration) frame %= clip->duration;
    frame = fx_clamp(frame, 0, clip->duration);
    int key = frame / 3, remain = frame % 3;
    v3 local = v3_from_i16(clip->keys[key]);
    if(key + 1 < clip->count)
        for(int i = 0; i < 3; i++)
            (&local.x)[i] += ((int)clip->keys[key + 1][i] - (&local.x)[i]) * remain / 3;
    return v3_add(p->figure.position,
                  v3_add(v3_add(v3_scale14(v3_cross14(p->up, p->front), local.x), v3_scale14(p->up, local.y)),
                         v3_scale14(p->front, local.z)));
}
