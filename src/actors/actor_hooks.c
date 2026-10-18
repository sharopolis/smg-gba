static bool actors_hold_player(void){ return mario_launcher_bound() || pole_square_bound(); }

static bool actors_player_carrying(void){ return kamella_holding_shell(); }

static bool actors_player_anim(void){ return kamella_carry_anim(); }

static void actors_player_damaged(void){ kamella_damage_drop(); }

static void actors_camera(void){
    ticofat_pointer_camera();
    warppod_camera();
}

static void actors_player_pose(Figure* figure){ plant_ride_mario_pose(figure); }

static void actors_player_figure(Figure* figure){
    gcapture_add_figure(figure);
    if(!g_mario_held) trapeze_add_rope(figure);
}

static bool actors_banner_active(void){ return syati_counting(); }

static void actors_draw_hud(void){ syati_draw_hud(); }
