enum { ARS_WAIT = 64, ARS_ROTATE };
static void arrow_switch_multi_init(Actor* a, const StageObject* o){
    a->value = 0;
    a->args[0] = 0;
    actor_set_state(a, ARS_WAIT, -1);
}
static void arrow_switch_multi_think(Actor* a){
    if(a->state == ARS_WAIT){
        v3 sensor = V3(deep_arrow_sensor[0], deep_arrow_sensor[1], deep_arrow_sensor[2]);
        int reach = 100 + C_BODY_RADIUS + 80;
        if(g_mario.attack_timer > 0 &&
           v3_length_squared64(v3_sub(mario_centre(), sensor)) < (uint64_t)reach * reach){
            a->args[1] = a->args[0];
            a->args[0] = (int16_t)((a->args[0] + 1) & 3);
            audio_play_effect(SFX_ARROW_SWITCH_ON, 3);
            actor_set_state(a, ARS_ROTATE, -1);
        }
        return;
    }
    fx want = a->args[0] * (FX_PI / 2);
    a->value += DEG(6);
    if(a->value >= want + (a->args[0] == 0 ? 2 * FX_PI : 0)){
        a->value = want;
        switch_set(deep_arrow_targets[a->args[1]], false);
        switch_set(deep_arrow_targets[a->args[0]], true);
        actor_set_state(a, ARS_WAIT, -1);
    }
}
