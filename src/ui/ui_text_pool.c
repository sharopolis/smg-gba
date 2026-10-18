static void ui_text_copy_tiles(int destination, const uint8_t* bank, unsigned first, int count){
    const uint32_t* header = (const uint32_t*)bank;
    const uint16_t* refs = (const uint16_t*)(bank + 12);
    unsigned data_offset = (12 + header[1] * 2 + 3) & ~3u;
    const uint32_t* tiles = (const uint32_t*)(bank + data_offset);
    volatile uint32_t* dst = (volatile uint32_t*)(OBJ_TILES + destination * 16);
    for(int i = 0; i < count; i++){
        const uint32_t* src = tiles + refs[first + i] * 8;
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        dst[4] = src[4];
        dst[5] = src[5];
        dst[6] = src[6];
        dst[7] = src[7];
        dst += 8;
    }
}
