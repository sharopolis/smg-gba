__attribute__((noinline)) static void candle_effect_draw(uint8_t* fb, const CameraView* view, const Actor* a);
static bool candlestand_stream_keep(const Actor* a);
static void candlestand_init(Actor* a, const StageObject* o, bool own_collision);
static bool candlestand_receive_fire(Actor* a, const Actor* by);
__attribute__((noinline)) static void candlestand_think(Actor* a);
static void candlestand_init_with_collision(Actor* a, const StageObject* o);
