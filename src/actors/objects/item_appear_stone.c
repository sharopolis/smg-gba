static void itemstone_think(Actor* a){
    int key = actor_switch_key(a, SWITCH_B);
    int set = mappart_set(a, kcl_itemappearstone, KCL_ITEMAPPEARSTONE_COUNT);
    if(key >= 0 && switch_on(key)){
        if(set >= 0) movers_enable(set, false);
        a->active = 0;
        return;
    }
    if(a->timer == 1 && set >= 0){
        MoverFrame f = mover_frame_of(a->home, a->up, a->front, mappart_scale14(a));
        movers_place(set, &f);
    }
    a->front = v3_rotate_axis14(a->front, a->up, FX_TAU / 600);
}
