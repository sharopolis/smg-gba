static fx rail_point_coord(int rail, int i);
static int32_t mappart_scale14(const Actor* a);
static int mappart_set_in(Actor* a, int16_t* slot, const int16_t (*tris)[9], int count);
static int mappart_set(Actor* a, const int16_t (*tris)[9], int count);
static void mappart_place_kcl(Actor* a, const int16_t (*tris)[9], int count);
static void railmove_init(Actor* a, const StageObject* o);
static void railmove_think(Actor* a);
