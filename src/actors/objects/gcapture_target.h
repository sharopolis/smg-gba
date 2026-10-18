enum { GC_WAIT = 64, GC_APPEAR };

static int16_t g_gcapture_held;

static bool gcapture_available(const Actor* a);
static bool gcapture_hidden(const Actor* a);
static bool gcapture_visible(const Actor* a);
static const Actor* gcapture_held_actor(void);
static void gcapture_reset(void);
static void capture_bubble_init(Figure* bubble, v3 centre, v3 cam_front, v3 cam_up, int grow);
static void gcapture_add_figure(Figure* bubble);
static void gcapture_let_go(Actor* t, v3 velocity);
static void gcapture_cancel_for_launch(void);
static void gcapturetarget_think(Actor* a);
static void gcapturetarget_init(Actor* a, const StageObject* o);
static bool gcapturetarget_idle(const Actor* a);
