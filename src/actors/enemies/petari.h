enum {
    PETARI_LURK = 64,
    PETARI_WAIT,
    PETARI_APPROACH,
    PETARI_ESCAPE,
    PETARI_SWOONSTART,
    PETARI_SWOON,
    PETARI_SWOONEND,
    PETARI_LAND,
    PETARI_BURST
};

static void petari_think(Actor* a);
static void petari_init(Actor* a, const StageObject* o);
static void petari_blown_away(Actor* a, Actor* by);
