enum { CAMSRC_DEFAULT, CAMSRC_START, CAMSRC_AREA, CAMSRC_EVENT, CAMSRC_SWIM, CAMSRC_SURFACE, CAMSRC_CAPTURE };

typedef struct {
    v3 pos, up, front, last_move;
} CamTarget;

typedef struct {
    bool active;
    int chunk, source;
    int area_chunk;
    v3 area_exit;
    bool reset;
    v3 eye, watch, up;
    v3 local_ofs;
    fx vpan_h;
    bool vpan_valid;
    fx round_angle, round_target;
    v3 there_up;
    fx there_roll;
    int blend_t, blend_total;
    v3 blend_eye, blend_watch, blend_up;
    v3 out_eye, out_watch, out_up;
    bool out_valid, force_change;
    v3 target_prev;
    fx col_len, col_shown;
    fx col_hit, col_hit_shown;
    bool parity;

    int start_chunk, start_moved;

    int event_first, event_count, event_index, event_frame;
    bool event_active, event_on_mario;
    CamTarget event_target;
    int shake_timer, shake_level;
    bool snap;
} GalaxyCamera;

static EWRAM_BSS GalaxyCamera g_gcam;

static inline v3 gcam_v32(const int32_t* v);
static inline v3 gcam_v16(const int16_t* v);
static void camera_event_start(int first, int count, v3 pos, v3 up, v3 front);
static void camera_event_target(v3 pos, v3 dir, v3 front);
static void camera_event_end_at_landing(void);
static void camera_event_cancel(void);
static bool camera_event_busy(void);
static void camera_event_landed(void);
static void camera_shake(int level);
static void camera_galaxy_snap(void);
static void camera_galaxy_stage_start(int start_chunk);
static bool gcam_area_contains(const StageArea* a, v3 p);
static bool camera_pull_preview_allowed(void);
static void camera_visibility_update(void);
static void camera_manual_resume_source(void);
static bool camera_galaxy_update(fx dt, const KeyState* keys);
