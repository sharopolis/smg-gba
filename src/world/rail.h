static inline const StageRail* rail_get(int rail);
static inline fx rail_length(int rail);
static inline int rail_point_count(int rail);
static v3 rail_point(int rail, int i);
static v3 rail_part_eval(const StageRailPart* part, int32_t t);
static v3 rail_position_at(int rail, fx coord);
__attribute__((noinline)) static bool rail_box_near(int rail, v3 body, fx radius);
