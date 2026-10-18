typedef struct {
    void (*tick)(const KeyState* keys);
    void (*camera)(fx dt, const KeyState* keys, uint16_t held, uint16_t pressed);
    void (*pose_player)(Figure* figure);
    void (*draw)(uint8_t* framebuffer);
    void (*draw_hud)(void);
    void (*release)(void);
    bool animates_player;
    bool full_res;
} GameScene;

static const GameScene* game_scene(int scene);
static void game_scenes_release(const GameScene* shown);
