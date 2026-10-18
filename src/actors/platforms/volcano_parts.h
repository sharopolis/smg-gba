enum {
    VOLCANO_PART_WAIT = 180,
    VOLCANO_PART_MOVE,
    VOLCANO_PART_DONE,
    VOLCANO_PART_STAY,
    VOLCANO_PART_MOVE_START,
    VOLCANO_PART_HIPDROP
};
enum { VOLCANO_DEMO_WAIT = 190, VOLCANO_DEMO_RUN, VOLCANO_DEMO_DONE };

static const FigureOps volcano_part_figure_ops;

static bool volcano_part_type(int type);
static const FFM2Model* volcano_part_model(const Actor* a);
static bool volcano_demo_object(const StageObject* o);
static const FFM2DemoSpec* volcano_demo_spec(const Actor* a);
static Actor* volcano_object_actor(unsigned source);
static bool volcano_part_ride_frame(const Actor* a, v3* position, v3* up, v3* front);
static bool volcano_part_collision_one(int i, v3 centre);
static fx volcano_part_extent(const Actor* a);
static bool volcano_part_unclipped(const Actor* a);
static bool volcano_part_stream_required(const StageObject* o);
static bool volcano_launch_star_available(const StageObject* o);
static bool volcano_part_init(Actor* a, const StageObject* o);
static void volcano_part_switch_move_think(Actor* a);
static void volcano_part_rail_init(Actor* a);
static bool volcano_part_anim_tick(Actor* a);
static void volcano_part_animate(Actor* a);
static void volcano_part_prepare_first(const Actor* a, Figure* f);
static void volcano_part_think(Actor* a);
