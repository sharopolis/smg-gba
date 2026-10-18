enum { CAGE_MEDIUM_WAIT = 64, CAGE_MEDIUM_BREAK, CAGE_MEDIUM_AFTER };

static void crystal_cage_init(Actor* a, const StageObject* o);
static void crystal_cage_medium_init(Actor* a, const StageObject* o);
static bool crystal_cage_medium_idle(const Actor* a);
static void crystal_cage_medium_think(Actor* a);
static void crystal_cage_medium_collision(Actor* a, v3 mario);
__attribute__((noinline)) static void crystal_cage_medium_collision_all(void);
__attribute__((noinline)) static bool killer_break_cages(Actor* a, v3 next);
static void crystal_cage_basic_think(Actor* a);
static void crystal_cage_large_init(Actor* a, const StageObject* o);
static void crystal_cage_large_think(Actor* a);
static fx crystal_cage_medium_draw_extent(const Actor* a);
