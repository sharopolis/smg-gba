enum { LG_WAIT = 64, LG_TALK, LG_STAR, LG_DONE, LG_TREE_WAIT, LG_TREE_TOUCH, LG_TREE_FALL, LG_TREE_LAND };

static void luigi_init(Actor* a, const StageObject* o);
static void luigi_tree_hit(Actor* a);
static void luigi_think(Actor* a);
static void luigi_talk_event(Actor* a, int arg);
static bool luigi_present(const StageObject* o);
