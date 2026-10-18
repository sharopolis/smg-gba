typedef struct {
    fx x, y;
} v2;

typedef struct {
    fx x, y, z;
} v3;

typedef struct {
    v3 r[3];
} m3;

typedef struct {
    m3 m;
    v3 o;
} xform;

#define CLIP_SIDES  15
#define CLIP_GUARD  16
#define CLIP_NEAR_Z 160
#define N14_ONE     16384

static inline v3 V3(fx x, fx y, fx z);
static inline v2 V2(fx x, fx y);
static inline v3 v3_add(v3 a, v3 b);
static inline v3 v3_sub(v3 a, v3 b);
static inline v3 v3_neg(v3 a);
static inline v3 v3_scale(v3 a, fx s);
static inline v3 v3_mul(v3 a, v3 b);
static inline v3 v3_cross(v3 a, v3 b);
static inline fx v2_length_squared(v2 a);
static inline int32_t mul_shift_exact(int32_t a, int32_t b, int shift);
static inline v3 m3_transform_vector(const m3* m, v3 v);
static inline v3 xform_point(const xform* t, v3 p);
static inline uint32_t view_clip_mask(v3 v);
static v2 fx_sin_cos(fx angle);
static inline fx fx_sin(fx a);
static inline fx fx_cos(fx a);
static m3 m3_identity(void);
static m3 m3_rotate_x(fx a);
static m3 m3_rotate_y(fx a);
static m3 m3_rotate_z(fx a);
static m3 m3_mul(const m3* a, const m3* b);
static xform xform_compose(const xform* self, const xform* then);
static fx fx_sqrt(fx x);
static fx fx_recip(fx x);
static inline int32_t mul14(int32_t a, int32_t b);
static inline v3 v3_scale14(v3 a, int32_t s);
static inline int32_t v3_dot14(v3 a, v3 n);
static inline v3 v3_cross14(v3 a, v3 b);
static inline v3 v3_from_i16(const int16_t* p);
uint32_t isqrt32_arm(uint32_t x);
uint32_t recip30_arm(uint32_t len);
static inline uint32_t isqrt32(uint32_t x);
static uint32_t isqrt64(uint64_t x);
IWRAM_FN static uint64_t v3_length_squared64(v3 a);
static int32_t v3_length_exact(v3 a);
IWRAM_FN static v3 v3_direction14(v3 a, v3 fallback);
static inline v3 v3_reject14(v3 a, v3 n);
static v3 v3_tangent14(v3 want, v3 up, v3 fallback);
static v3 v3_rotate_between14(v3 v, v3 from, v3 to);
static v3 v3_rotate_axis14(v3 v, v3 k, fx angle);
static v3 v3_blend14(v3 a, v3 b, int32_t t);
static inline v3 v3_n14_to_fx(v3 a);
static v3 v3_from_i32(const int32_t p[3]);
