typedef struct {
    v3 up;
    fx distance;
    int index;
} GravitySample;

static inline fx gdot(v3 a, const int16_t* n);
static inline v3 gscale(const int16_t* n, fx s);
static inline uint8_t gravity_scenario_bit(void);
static const uint16_t* bound_grid_list(const StageHeader* header, uint32_t grid, v3 p, uint32_t* n);
static GravitySample gravity_sample(const StageHeader* header, v3 p);
static GravitySample gravity_sample_uncached(const StageHeader* header, v3 p);
