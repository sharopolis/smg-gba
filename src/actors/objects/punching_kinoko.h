enum {
    PUNCHING_KINOKO_WAIT = 64,
    PUNCHING_KINOKO_PUNCHED,
    PUNCHING_KINOKO_BRAKE,
    PUNCHING_KINOKO_SWING,
    PUNCHING_KINOKO_CRUSHED,
    PUNCHING_KINOKO_CRUSHED_END
};

static void punchingkinoko_think(Actor* a);
static void punchingkinoko_figure(const Actor* a, Figure* f);
static void punchingkinoko_init(Actor* a, const StageObject* o);
static bool punchingkinoko_idle(const Actor* a);
