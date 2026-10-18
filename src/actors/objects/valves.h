enum { VALVE_DOOR_WAIT = 64, VALVE_DOOR_MOVE, VALVE_DOOR_DONE };

static const FigureOps valve_figure_ops;

static bool valve_type(int type);
static bool valve_demo_object(const StageObject* o);
static bool valve_stream_required(const StageObject* o);
static bool valve_has_model(const Actor* a);
static bool valve_ride_frame(const Actor* a, v3* pos, v3* up, v3* front);
static bool valve_collision_one(int i, v3 centre);
static void valve_demo_animate(Actor* a);
static void valve_prepare_first(const Actor* a, Figure* f);
static void valve_bound_sphere(const Actor* a, v3* centre, fx* radius);
static bool valve_unclipped(const Actor* a);
static bool valve_stream_keep(const Actor* a);
static void valve_set_state(Actor* a, int next);
static bool valve_player_request(const KeyState* keys);
static void valve_door_think(Actor* a);
static bool valve_init(Actor* a, const StageObject* o);
static Actor* valve_object_actor(int source);
static void valve_think(Actor* a);
