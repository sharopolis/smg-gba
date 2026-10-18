enum { TICOFAT_WAIT = 64, TICOFAT_EAT, TICOFAT_FULL, TICOFAT_TRANSFORM, TICOFAT_FLY };

static void ticofat_receive_piece(Actor* a);
static void ticofat_think(Actor* a);
static void ticofat_pointer_camera(void);
static fx ticofat_pointer_radius(const Actor* a);
static bool ticofat_pointer_blocks_shot(const Actor* a);
static void ticofat_init(Actor* a, const StageObject* o);
static bool ticofat_idle(const Actor* a);
static void ticofat_talk_event(Actor* a, int arg);
static int ticofat_talk_node(const Actor* a, int node);
static int ticofat_talk_count(const Actor* a);
