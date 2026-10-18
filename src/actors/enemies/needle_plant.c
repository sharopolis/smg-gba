static void needleplant_kill(Actor* a){
    if(!a->active || a->state == AS_DEFEATED) return;
    int item = actor_arg_or(a, 0, -1);

    if(item == -1) collectibles_pop(OBJ_COIN, a->position, a->up, 1);
    else if(item == 0) collectibles_pop(OBJ_STARBIT, a->position, a->up, 3);
    actor_set_switch_dead(a, 0);
    actor_defeat(a, false);
    a->active = 0;
}

static void needleplant_think(Actor* a){
    if(a->state == NEEDLEPLANT_SHAKE && actor_anim_done(a))
        actor_set_state(a, NEEDLEPLANT_WAIT, ANIM_NEEDLEPLANT_WAIT);
    int c = actor_mario_contact(a, 0);
    if(c == CONTACT_STAR){
        needleplant_kill(a);
    } else if(c != CONTACT_NONE && actor_touches_mario(a, a->radius * 70 / 180 - a->radius)){
        actor_hurt_mario(a, 1);
        if(a->state != NEEDLEPLANT_SHAKE) actor_set_state(a, NEEDLEPLANT_SHAKE, ANIM_NEEDLEPLANT_SHAKE);
    }
}

static void needleplant_init(Actor* a, const StageObject* o){
    a->radius = FX(180) * (o->scale[0] ? o->scale[0] : 256) / 256;
    actor_set_state(a, NEEDLEPLANT_WAIT, ANIM_NEEDLEPLANT_WAIT);
}

static void needleplant_blown_away(Actor* a, Actor* by){ needleplant_kill(a); }
