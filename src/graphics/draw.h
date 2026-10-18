#define FIGURE_MAX_POINTS FIGURE_TRANSFORM_MAX_POINTS

typedef const uint16_t* (*FigureOrder)(const Figure* figure, int axis, bool lod, int* count);

static EWRAM_BSS FramePoint g_figure_points[FIGURE_MAX_POINTS];
static uint32_t g_figure_screen_positions[FIGURE_SCREEN_POINT_LIMIT];
static EWRAM_BSS FigureOrder g_figure_order;
static EWRAM_BSS uint32_t g_actor_far_lod[(MAX_ACTORS + 31) / 32];

static void* geometry_figure_scratch(void);
static void* geometry_figure_draw_tail(unsigned bytes);
static void* geometry_icestep_triangle_scratch(void);
__attribute__((noinline)) static void geometry_icestep_clear_point_marks(void);
__attribute__((noinline)) static void figure_render_sources(const Figure* f, bool lod);
static void figure_clipped(int face);
__attribute__((noinline)) static const uint16_t* figure_minus_orders(const FigureHeader* model);
IWRAM_FN static void draw_figure(uint8_t* framebuffer, const Figure* figure, const CameraView* cv);
__attribute__((noinline)) static void draw_figure_part(uint8_t* framebuffer, const Figure* figure,
                                                       const CameraView* cv);
