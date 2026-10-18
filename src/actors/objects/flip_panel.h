enum { FP_BLUE = 1, FP_COMPLETE = 2, FP_WAIT = 64, FP_OBSERVE_COMPLETE };

static bool flippanel_available(const StageObject* panel);
static void flippanel_init(Actor* a, const StageObject* o);
static void flippanel_think(Actor* a);
static void flippanelobserver_think(Actor* a);
static void flippanelobserver_init(Actor* a, const StageObject* o);
