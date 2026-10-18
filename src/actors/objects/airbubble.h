enum {
    AIRBUBBLE_WAIT = 64,
    AIRBUBBLE_MOVE,
    AIRBUBBLE_BREAK,
    AIRBUBBLE_KILL_WAIT,
    AIRBUBBLE_GENERATOR_WAIT = 64,
    AIRBUBBLE_GENERATOR_GENERATE
};

static void airbubble_init(Actor* a, const StageObject* o);
static void airbubble_think(Actor* a);
static void airbubblegenerator_think(Actor* a);
