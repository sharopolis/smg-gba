#ifndef ASSETC_OBJ_READER_H
#define ASSETC_OBJ_READER_H

#include "common.h"

typedef struct {
    int pos;
    int uv;
} ObjVertex;

typedef struct {
    int first, count;
    int mat;
} ObjFace;

typedef struct {
    char *name;
    Vector faces;
} ObjGroup;

typedef struct {
    char *name;
    Vector groups;
} ObjObject;

typedef struct {
    Vector positions;
    Vector uvs;
    Vector verts;
    Vector materials;
    Vector objects;
} Obj;

Obj obj_load(const char *path);

#define OBJ_POS(o, i) (&VEC_AT((o)->positions, float, 0) + 3 * (i))
#define OBJ_UV(o, i)  (&VEC_AT((o)->uvs, float, 0) + 3 * (i))

typedef struct {
    int n;
    char **names;
    char **textures;
} Mtl;

Mtl mtl_load(const char *path);
const char *mtl_texture(const Mtl *mtl, const char *material);

#endif
