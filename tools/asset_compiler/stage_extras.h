#ifndef ASSETC_STAGE_EXTRAS_H
#define ASSETC_STAGE_EXTRAS_H

#include "../../src/assets/asset_format.h"
#include "model_builder.h"

typedef struct {
    int kind, flags, faces, scenarios, priority;
    int switch_a, switch_b, switch_appear, switch_sleep;
    float centre[3], axis[3][3], ref_side[3], extent[3];
    float valid_cos;
    float range, distant, base;
    int rail, wire_count;
    float (*wire)[3];
} GravityDef;

typedef struct {
    int type, scenarios, flags;
    float position[3], up[3], front[3], target[3];
    int args[8];
    int rail, camera, id, zone;
    float scale[3];
    int sw[5];
} ObjectDef;

typedef struct {
    float position[3], up[3], front[3];
    int id, scenarios, zone, camera;
} StartDef;

typedef struct {
    float position[3], in[3], out[3];
    int args[8];
} RailPointDef;

typedef struct {
    int closed, zone;
    int args[8];
    int first_point, point_count;
} RailDef;

typedef struct {
    int cls, shape, scenarios, zone, camera, priority, sw;
    int args[8];
    float centre[3], axis[3][3], extent[3];
} AreaDef;

typedef struct {
    int type, vpan, camint, evfrm, num1, num2;
    float angle_a, angle_b, roll, upper, lower, fovy;
    float dist, offset[3], wpoint[3], axis[3], up[3], woffset[3], loffset, loffsetv, uplay, lplay;
    float zone_up[3], extra[3];
    int flags, camendint, reg_type, reg_id;
} CameraDef;

typedef struct {
    char name[32];
    int sw, mode;
} PartDef;

typedef struct {
    StartDef *starts;
    int start_count;
    GravityDef *gravities;
    int gravity_count;
    ObjectDef *objects;
    int object_count;
    RailDef *rails;
    int rail_count;
    RailPointDef *rail_points;
    int rail_point_count;
    AreaDef *areas;
    int area_count;
    CameraDef *cameras;
    int camera_count;
    int galaxy;
    PartDef *parts;
    int part_count;
    const char *part_names[STAGE_MAX_PARTS];
} StageExtras;

void object_types_add(const char *name);
int object_types_count(void);
const char *object_type_name(int i);

StageExtras stage_extras_load(const char *stage_path, const char *galaxy_path);

void stage_extras_up_at(void *extras, const float p[3], float up[3]);

Gravity gravity_to_asset(const GravityDef *g);
StageObject object_to_asset(const ObjectDef *o);
StageStart start_to_asset(const StartDef *s);
StageArea area_to_asset(const AreaDef *a);
StageCamera camera_to_asset(const CameraDef *c);

void rails_to_asset(const StageExtras *e, Vector *rails, Vector *points, Vector *parts);

void collision_build(const Model *m, int shift, Vector *triangles, Vector *nodes);

int rock_drops_build(const StageExtras *e, const Model *m, int shift, const Vector *rails,
                     const Vector *parts, Vector *table);

#endif
