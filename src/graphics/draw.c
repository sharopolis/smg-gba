_Static_assert(FIGURE_MAX_POINTS * 6 <= POSE_TAIL_BYTES, "actor poses must fit one VRAM tail");
static EWRAM_BSS FramePoint g_figure_points[FIGURE_MAX_POINTS];
_Static_assert(sizeof(IceStepCollisionCache) <= sizeof g_figure_points,
               "IceStep cache must fit the figure scratch");

static void* geometry_figure_scratch(void){ return g_figure_points; }

static void* geometry_figure_draw_tail(unsigned bytes){
    return (uint8_t*)(g_figure_points + FIGURE_MAX_POINTS) - bytes;
}
_Static_assert(ICESTEP_SLOTS * ICESTEP_PRISMS * sizeof(CollisionTriangle) == 960 * sizeof(FramePoint),
               "IceStep table needs 960 render slots");
_Static_assert(ICESTEP_SLOTS * ICESTEP_PRISMS * sizeof(CollisionTriangle) <= sizeof g_frame_points,
               "IceStep table must fit the render point pool");
static void* geometry_icestep_triangle_scratch(void){ return g_frame_points; }

__attribute__((noinline)) static void geometry_icestep_clear_point_marks(void){
    for(int i = 0; i < 960; i++) g_frame_points[i].mark = 0;
}

_Static_assert(FIGURE_SCREEN_POINT_LIMIT >= FIGURE_PROCEDURAL_MAX_POINTS,
               "screen point pool must cover procedural figures");
_Static_assert(FIGURE_SCREEN_POINT_LIMIT <= FIGURE_MAX_POINTS,
               "screen point pool must fit the transform scratch");
static uint32_t g_figure_screen_positions[FIGURE_SCREEN_POINT_LIMIT];

__attribute__((noinline)) static void figure_render_sources(const Figure* f, bool lod){
    const FigureHeader* model = f->model;
    const FigureHeader* bank = model;
    if(f == &g_game.player.figure && model == (const FigureHeader*)figure_mario && !lod){
        if(mario_power_mode() == 2) bank = (const FigureHeader*)figure_mariofire;
        if(mario_power_mode() == 3) bank = (const FigureHeader*)figure_marioice;
    }

    g_figure_draw_triangles = lod ? model_lod_triangles(bank) : model_triangles(bank);
    g_figure_draw_textures = lod ? model_lod_textures(bank) : model_textures(bank);
}

static EWRAM_BSS FigureOrder g_figure_order;

static void figure_clipped(int face){ engine_figure_clipped(g_figure_points, face); }

__attribute__((noinline)) static const uint16_t* figure_minus_orders(const FigureHeader* model){
#define X(figure, orders) \
    if(model == (const FigureHeader*)figure) return orders;
    FIGURE_MINUS_ORDER_MODELS
#undef X
    return NULL;
}

static EWRAM_BSS uint32_t g_actor_far_lod[(MAX_ACTORS + 31) / 32];
static inline int figure_lod_factor(const Figure* figure, int lod_k){
    uintptr_t offset = (uintptr_t)figure - (uintptr_t)&g_game.actors[0].figure;
    if(offset >= sizeof g_game.actors) return lod_k;
    unsigned i = offset / sizeof(Actor);
    return g_actor_far_lod[i >> 5] >> (i & 31) & 1 ? lod_k >> 1 : lod_k;
}

IWRAM_FN static void draw_figure(uint8_t* framebuffer, const Figure* figure, const CameraView* cv){
    g_engine_fb = framebuffer;
    const FigureHeader* model = figure->model;

    const m3* m = &figure->transform.m;
    v3 d = V3(fx_mul(m->r[0].x, cv->direction.x) + fx_mul(m->r[1].x, cv->direction.y) +
                  fx_mul(m->r[2].x, cv->direction.z),
              fx_mul(m->r[0].y, cv->direction.x) + fx_mul(m->r[1].y, cv->direction.y) +
                  fx_mul(m->r[2].y, cv->direction.z),
              fx_mul(m->r[0].z, cv->direction.x) + fx_mul(m->r[1].z, cv->direction.y) +
                  fx_mul(m->r[2].z, cv->direction.z));

    v3 view_origin = xform_point(&cv->to_screen, figure->position);
    fx height = fx_mul((fx)model->extent, figure->scale.z);
    int lod_k = view_mode()->figure_lod;
    bool lod =
        model->lod_triangle_count && lod_k && view_origin.z > height * figure_lod_factor(figure, lod_k);
    int axis = view_nearest_axis(d);
    const uint16_t* order = lod ? model_lod_draw_order(model, axis) : model_draw_order(model, axis);

    xform t = xform_compose(&figure->transform, &cv->to_screen);

    const int16_t (*pose)[3] = model_resolve_pose(figure->pose, 4);
    TransformJob xj =
        lod ? geometry_transform_job(pose, g_figure_points, g_figure_screen_positions,
                                     model_lod_points(model), (int)model->lod_point_count, &t, 0)
            : geometry_transform_job(pose, g_figure_points, g_figure_screen_positions, NULL,
                                     model->point_count, &t, 0);
    engine_transform_points(&xj);

    figure_render_sources(figure, lod);
    int count = lod ? model->lod_triangle_count : model->triangle_count;
    int step = 2;
    if(axis & 1){
        const uint16_t* minus = figure_minus_orders(model);
        if(minus) order = minus + (lod ? 3 * model->triangle_count : 0) + (axis >> 1) * count;
        else {
            order += count - 1;
            step = -2;
        }
    }
    if(g_figure_order){
        const uint16_t* visible = g_figure_order(figure, axis, lod, &count);
        if(visible){
            order = visible;
            step = 2;
        }
    }
    FigureRasterJob job = {g_figure_points,
                           g_figure_screen_positions,
                           order,
                           count,
                           step,
                           g_figure_draw_triangles,
                           g_figure_draw_textures,
                           g_additive_rows ? RM_ADD : RM_TEX,
                           g_flat_small && !g_additive_rows,
                           figure_clipped};
    engine_draw_figure_triangles(&job);
    g_additive_rows = NULL;
}

__attribute__((noinline)) static void draw_figure_part(uint8_t* framebuffer, const Figure* figure,
                                                       const CameraView* cv){
    xform t = xform_compose(&figure->transform, &cv->to_screen);
    int64_t e = (int64_t)figure->model->extent;
    fx margin[3];
    for(int a = 0; a < 3; a++)
        margin[a] = (fx)((e * (fx_abs(t.m.r[a].x) + fx_abs(t.m.r[a].y) + fx_abs(t.m.r[a].z))) >> 8) + 4;
    v3 c = t.o;
    if(c.x - c.z > margin[0] + margin[2] || -c.x - c.z > margin[0] + margin[2] ||
       c.y - c.z > margin[1] + margin[2] || -c.y - c.z > margin[1] + margin[2])
        return;
    draw_figure(framebuffer, figure, cv);
}
