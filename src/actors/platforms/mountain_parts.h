static float float_clamp(float v, float lo, float hi);
static float float_converge(float v, float to, float step);
static int sin14_degrees(float degrees);
static v3 v3_rotate_degrees14(v3 v, v3 axis, float degrees);
static void mountain_part_init(Actor* a, const StageObject* o);
static void mountain_part_think(Actor* a);
static void parts_collision_update(void);
static void parts_collision_pre(void);
static void parts_collision_post(void);
