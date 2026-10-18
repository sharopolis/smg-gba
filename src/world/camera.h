typedef struct {
    v3 position;
    v3 orientation;
    v3 front, up;
    v3 target;
    v3 smooth_up;
    v3 heading;
    fx distance, height;
    int recentre_ticks;
} Camera;

typedef struct {
    m3 view;
    xform to_screen;
    v3 direction;
} CameraView;

typedef struct {
    v3 axis;
    fx angle;
} CamRot;

typedef struct {
    v3 reg;
    v3 reg_up;
    fx dist_min, dist_max;
    fx angle_min, peep_angle, peep_dist;
    int32_t constrain14;
    v3 centre;
    fx radius;
} BossCamera;

static int g_cam_ticks;

static void camera_set_zoom(Camera* camera, int zoom);
static void camera_build_view(const Camera* camera, CameraView* out);
static void camera_update_angles(Camera* camera);
static void camera_snap_behind(Camera* camera, const Player* player);
static int32_t cam_ease(int32_t r, int n);
static CamRot cam_rotation_towards(v3 a, v3 b, int32_t frac, fx max_angle, v3 hint);
static inline v3 cam_rotate(v3 v, CamRot r);
static v3 cam_slerp14(v3 a, v3 b, int32_t t, v3 hint);
__attribute__((noinline)) static void camera_manual_prepare(uint16_t held, uint16_t pressed, bool allowed);
__attribute__((noinline)) static void camera_manual_update(uint16_t held, bool allowed);
static void camera_update(fx dt, const KeyState* keys);
static void camera_follow_update(const KeyState* keys);
