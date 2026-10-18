enum { TICO_META = 200 };

static void tico_spawned(Actor* a, const StageObject* o){
    if(o->args[0] > 0 && o->args[0] < 6) a->figure.model = (const FigureHeader*)tico_figures[o->args[0]];
}

static void tico_talk_ended(Actor* a){
    if(a->args[1] != 3 || a->state == TICO_META) return;
    a->state = TICO_META;
    a->timer = 0;
    a->anim = ANIM_TICO_METAMORPHOSIS;
    a->anim_frame = 0;
}

static bool tico_talk_busy(Actor* a){
    if(a->state != TICO_META) return false;
    if(a->timer >= anim_length_of(a->figure.model, a->anim)){
        audio_play_effect(SFX_LAUNCH, 3);
        a->active = 0;
    }
    return true;
}
