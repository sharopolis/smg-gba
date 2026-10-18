static void mario_skate_reset(void);
static int32_t mario_ice_inertia(int32_t speed, int32_t start_spin);
static int32_t mario_ice_walk_inertia(int32_t speed, int32_t start_spin);
static bool mario_skate_land(void);
static bool mario_skate_tick(const KeyState* keys);
static void mario_skate_apply_pose(Figure* f);
