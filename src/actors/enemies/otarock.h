enum { OTAROCK_HIDDEN = 64, OTAROCK_SHOW, OTAROCK_WAIT, OTAROCK_THROW, OTAROCK_HIDE, OTAROCK_DOWN };

static bool fireball_attack_enemies(Actor* a);
static void otarock_ball_arrives(Actor* rock, Actor* ball);
static void otarock_think(Actor* a);
static void otarock_init(Actor* a, const StageObject* o);
static void otarock_blown_away(Actor* a, Actor* by);
