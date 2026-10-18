__attribute__((noinline)) static void boss_stinkbug_body_frame(const Actor* a, v3* pos, v3* up, v3* front,
                                                               int16_t scale[3]);
__attribute__((noinline)) static void boss_stinkbug_collision(Actor* a, v3 mario);
static void boss_stinkbug_think(Actor* a);
static inline bool boss_stinkbug_solid(const Actor* a);
static void boss_bomb_think(Actor* b);
