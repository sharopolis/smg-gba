typedef int32_t fx;

#define FX_ONE 256
#define FXC(f) ((fx)((f) * 256.0f))
#define TAU_F  6.28318530717958647692f
#define FX_TAU FXC(TAU_F)
#define FX_PI  FXC(TAU_F * 0.5f)

static inline fx fx_mul(fx a, fx b);
static inline fx fx_min(fx a, fx b);
static inline fx fx_max(fx a, fx b);
static inline fx fx_clamp(fx a, fx lo, fx hi);
static inline fx fx_abs(fx a);
static inline int32_t divide_euclidean(int32_t a, int32_t b);
