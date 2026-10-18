__attribute__((noinline)) static const uint16_t* mandibug_visible_order(const Figure* figure, int axis,
                                                                        bool lod, int* count){
    uintptr_t address = (uintptr_t)figure;
    uintptr_t first = (uintptr_t)&g_game.actors[0].figure;
    if(address < first || address - first >= sizeof g_game.actors || (address - first) % sizeof(Actor))
        return NULL;
    const Actor* actor = &g_game.actors[(address - first) / sizeof(Actor)];
    if(actor->type != OBJ_STINKBUGPARENT || !(actor->value & 1)) return NULL;
    if(actor->anim == ANIM_STINKBUGPARENT_CHILDDEATH){
        unsigned frame = actor->anim_frame > 0 ? (unsigned)actor->anim_frame >> 8 : 0;
        if(frame >= sizeof mandibug_childdeath_visibility) frame = sizeof mandibug_childdeath_visibility - 1;
        if(mandibug_childdeath_visibility[frame]) return NULL;
    }
    if((unsigned)axis >= 6) return NULL;
    *count = lod ? 84 : 142;
    return lod ? mandibug_parent_lod_orders[axis] : mandibug_parent_near_orders[axis];
}

__attribute__((noinline)) static bool mandibug_draw(uint8_t* framebuffer, Figure* figure,
                                                    const CameraView* view){
    g_figure_order = mandibug_visible_order;
    draw_figure(framebuffer, figure, view);
    g_figure_order = NULL;
    return true;
}

static const FigureOps mandibug_figure_ops = {.draw = mandibug_draw};
