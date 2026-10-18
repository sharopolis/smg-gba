typedef struct {
    uint8_t stage, kind, scenarios, cold;
    int32_t origin[3];
    int16_t axes[3][3];
    int32_t size[3];
} WaterVolume;

typedef struct {
    uint16_t first, count;
} WaterStageSpan;

typedef struct {
    v3 surface, normal;
    fx depth;
    bool cold;
} WaterSample;

static bool water_query_raw(v3 position, v3 gravity_up, WaterSample* sample, const WaterVolume** volume);
static bool water_query(v3 position, v3 gravity_up, WaterSample* sample);
