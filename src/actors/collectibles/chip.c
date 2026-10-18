static void chip_reset(void){ g_blue_chip_object = 0; }

static void chip_play_sound(bool blue, int count){
    static const uint8_t yellow[5] = {SFX_YELLOWCHIP1, SFX_YELLOWCHIP2, SFX_YELLOWCHIP3, SFX_YELLOWCHIP4,
                                      SFX_YELLOWCHIP5};
    static const uint8_t bluec[5] = {SFX_BLUECHIP1, SFX_BLUECHIP2, SFX_BLUECHIP3, SFX_BLUECHIP4,
                                     SFX_BLUECHIP5};
    if(count < 1) count = 1;
    if(count > 5) count = 5;
    audio_play_effect((blue ? bluec : yellow)[count - 1], count == 5 ? 5 : 3);
}

static int chip_stage_group_count(const Actor* a){
    const StageObject* own = actor_stage_object(a);
    int zone = own ? own->zone : a->args[3] > 0 ? g_game.stage.objects[a->args[3] - 1].zone : -1;
    int count = 0;
    for(uint32_t i = 0; i < g_game.stage.header->object_count; i++){
        const StageObject* o = &g_game.stage.objects[i];
        bool shell = o->type == OBJ_SHELLFISH && o->args[3] == (a->type == OBJ_YELLOWCHIP ? 1 : 2);
        if((o->type != a->type && !shell) || o->zone != zone || o->args[0] != a->args[0] ||
           !(o->scenarios & (1 << (g_game.scenario - 1))))
            continue;
        if(shell ? stream_part_state(i) != 0 : stream_object_collected(i) || i == (uint32_t)a->stage_index)
            count++;
    }
    return count;
}

static void chip_think(Actor* a){
    if(a->flags & ACTOR_HIDDEN) return;
    if(a->timer < 40 || actor_mario_contact(a, FX(20)) == CONTACT_NONE) return;
    shellfish_item_taken(a);
    a->active = 0;
    int group_type = a->type == OBJ_BLUECHIP ? OBJ_BLUECHIPGROUP : OBJ_YELLOWCHIPGROUP;
    int count = 1;
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* g = &g_game.actors[i];
        if(g->type != group_type || g->args[0] != a->args[0]) continue;
        count = ++g->value;
        if(count == 5){
            if(blue_chip_completion_group(g)) blue_chip_completion_request(g);
            else
                actor_set_switch_a(g, FX(20000),
                                   a->type == OBJ_BLUECHIP ? OBJ_GCAPTURETARGET : OBJ_LAUNCHSTAR);
        }
        break;
    }
    if(level_has(LEVEL_CHIP_GROUPS_FROM_STAGE)) count = chip_stage_group_count(a);
    chip_play_sound(a->type == OBJ_BLUECHIP, count);
}

static void chip_group_init(Actor* a, const StageObject* o){ a->value = 0; }
