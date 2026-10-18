static uint32_t g_parts_hidden;

static void parts_update(void);
static int parts_filter_leaves(const StageHeader* h, uint16_t* leaves, int n);
static int occluder_filter_leaves(uint16_t* leaves, int n, v3 camera);
