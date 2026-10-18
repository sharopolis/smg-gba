static void blue_chip_counter_draw(void){
    int frame = blue_chip_counter_frame();
    if(frame < 0) return;
    const uint16_t* header = (const uint16_t*)blue_chip_counter;
    int bx = header[0], by = header[1];
    int x = (int16_t)header[2], y = (int16_t)header[3];
    int tiles = bx * by * 8;
    if(tiles > 224) return;
    for(int i = 0; i < 16; i++) OBJ_PALETTE[OVL_PAL * 16 + i] = header[4 + i];
    const uint8_t* data = blue_chip_counter + 40 + frame * tiles * 32;
    hud_copy_tiles(OVL_TILE0, data, tiles);
    for(int row = 0; row < by; row++)
        for(int col = 0; col < bx; col++)
            hud_piece(x + col * 32, y + row * 16, OVL_TILE0 + (row * bx + col) * 8, OVL_PAL, OVL_OBJ_32x16);

    ovl_reset();
    g_hud.slot_art[SLOT_BANNER] = -1;
}
