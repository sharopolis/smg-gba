typedef struct Figure {
    const FigureHeader* model;
    const void* pose;
    v3 position, orientation;
    v3 extra_rotation;
    v3 scale;
    v3 up;
    fx rotation_pivot_height;
    bool transform_dirty;
    bool has_shadow;
    bool use_basis;
    bool hidden;
    v3 shadow;
    v3 basis_front, basis_up;
    fx local_pitch, local_roll, local_yaw;
    xform transform;
    struct Figure* next;
} Figure;

#define POSE_TAIL_BYTES 3328

extern const uint32_t __pose_code_start[], __pose_code_end[];

static bool g_pose_code_ready;

static const int16_t (*model_rest_pose(const FigureHeader* f))[3];
static const void* model_keyframe(const FigureHeader* f, int k);
static void model_invalidate_pose_cache(void);
static inline const int16_t (*model_resolve_pose(const void* pose, unsigned slot))[3];
static const FigureTriangle* model_triangles(const FigureHeader* f);
static const uint16_t* model_draw_order(const FigureHeader* f, int k);
static const TextureReference* model_textures(const FigureHeader* f);
static const FigureTriangle* model_lod_triangles(const FigureHeader* f);
static const uint16_t* model_lod_draw_order(const FigureHeader* f, int k);
static const uint16_t* model_lod_points(const FigureHeader* f);
static const TextureReference* model_lod_textures(const FigureHeader* f);
static void figure_init(Figure* figure, const uint8_t* model);
static m3 m3_from_basis14(v3 front, v3 up);
static void figure_update_transform(Figure* figure);
