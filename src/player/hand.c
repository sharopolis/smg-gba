static NativeMarioHand mario_hand_sample(void){
    const Player* p = &g_game.player;
    const NativeMarioHandClip* clip = &native_mario_hands[0];
    for(unsigned i = 0; i < sizeof native_mario_hands / sizeof native_mario_hands[0]; i++)
        if(native_mario_hands[i].clip == p->anim){
            clip = &native_mario_hands[i];
            break;
        }
    int frame = p->anim_frame >> 8;
    if(clip->loop && clip->duration) frame %= clip->duration;
    frame = fx_clamp(frame, 0, clip->duration);
    int key = frame / 3, remain = frame % 3;
    NativeMarioHand sample = clip->hand[key];
    if(key + 1 < clip->count){
        int16_t* a = (int16_t*)&sample;
        const int16_t* b = (const int16_t*)&clip->hand[key + 1];
        for(unsigned i = 0; i < sizeof sample / sizeof(int16_t); i++) a[i] += ((int)b[i] - a[i]) * remain / 3;
    }
    return sample;
}
static v3 mario_hand_local(v3 point, bool position){
    const Player* p = &g_game.player;
    v3 out = v3_add(v3_add(v3_scale14(v3_cross14(p->up, p->front), point.x), v3_scale14(p->up, point.y)),
                    v3_scale14(p->front, point.z));
    return position ? v3_add(out, p->figure.position) : out;
}
