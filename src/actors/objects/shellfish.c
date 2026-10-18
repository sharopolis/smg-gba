enum { SH_WAIT = 64, SH_OPEN, SH_OPENWAIT, SH_CLOSESIGNAL, SH_CLOSE, SH_SHOCK, SH_POINTER };

enum { SHELLFISH_ITEM = 4 };
static void shellfish_init(Actor* a, const StageObject* o){
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    a->radius = a->radius * o->scale[0] / 256;
    actor_set_state(a, SH_WAIT, ANIM_SHELLFISH_WAIT);
    a->args[2] = 0;
    a->value = stream_part_state(a->stage_index) != 0;
}
static void shellfish_item_taken(Actor* b){
    if(!(b->flags & SHELLFISH_ITEM) || b->stage_index >= 0 || b->args[3] <= 0) return;
    int parent = b->args[3] - 1;
    if(parent >= (int)g_game.stage.header->object_count || g_game.stage.objects[parent].type != OBJ_SHELLFISH)
        return;
    stream_part_state_set(parent, 1);
    for(int i = 0; i < g_game.actor_count; i++)
        if(g_game.actors[i].type == OBJ_SHELLFISH && g_game.actors[i].stage_index == parent){
            g_game.actors[i].value = 1;
            g_game.actors[i].args[2] = 0;
        }
}
static Actor* shellfish_item(Actor* a){
    if(a->value || a->args[2] <= 0) return NULL;
    int slot = a->args[2] - 1;
    if(slot >= g_game.actor_count) return NULL;
    Actor* b = &g_game.actors[slot];
    return b->active && b->flags & SHELLFISH_ITEM && b->args[3] == a->stage_index + 1 ? b : NULL;
}
static void shellfish_show_item(Actor* a){
    if(a->value) return;
    Actor* b = shellfish_item(a);
    if(!b){
        int kind = a->args[3] == 1 ? OBJ_YELLOWCHIP : a->args[3] == 2 ? OBJ_BLUECHIP : OBJ_COIN;
        b = actor_spawn_reuse(kind, a->position, a->up, a->front);
        if(!b) return;
        b->flags |= SHELLFISH_ITEM;
        b->args[0] = a->args[0];
        b->args[3] = a->stage_index + 1;
        a->args[2] = (int16_t)(b - g_game.actors) + 1;
    }
    int height = a->args[3] ? 100 : 50, front = a->args[3] ? 50 : 30;

    height += fx_clamp((a->timer - 40) / 2, 0, 30);
    b->position = v3_add(a->position, v3_add(v3_scale14(a->up, FX(height)), v3_scale14(a->front, FX(front))));
    b->flags &= ~ACTOR_HIDDEN;
}
static void shellfish_think(Actor* a){
    switch(a->state){
    case SH_WAIT:
        if(a->timer >= 150) actor_set_state(a, SH_OPEN, ANIM_SHELLFISH_OPEN);
        break;
    case SH_OPEN:
        if(a->timer >= 40) shellfish_show_item(a);
        if(a->timer >= 100) actor_set_state(a, SH_OPENWAIT, ANIM_SHELLFISH_OPEN);
        break;
    case SH_OPENWAIT:
        a->anim_frame = FX(1000);
        if(a->timer >= 170) actor_set_state(a, SH_CLOSESIGNAL, ANIM_SHELLFISH_CLOSESIGNAL);
        break;
    case SH_CLOSESIGNAL:
        if(a->timer >= 150) actor_set_state(a, SH_CLOSE, ANIM_SHELLFISH_CLOSE);
        break;
    case SH_CLOSE: {
        Actor* b = shellfish_item(a);
        if(a->timer >= 5 && b) b->flags |= ACTOR_HIDDEN;
        if(a->timer < 15 && actor_touches_mario(a, 0)) actor_hurt_mario(a, 1);
        if(actor_anim_done(a)) actor_set_state(a, SH_WAIT, ANIM_SHELLFISH_WAIT);
        break;
    }
    case SH_SHOCK:
        if(a->timer >= 540) actor_set_state(a, SH_CLOSESIGNAL, ANIM_SHELLFISH_CLOSESIGNAL);
        break;
    }

    actor_block_mario(a, FX(170) * a->figure.scale.x / 64, FX(60) * a->figure.scale.y / 64);
}
