static inline fx fx_mul(fx a, fx b){ return (a * b) >> 8; }
static inline fx fx_min(fx a, fx b){ return a < b ? a : b; }
static inline fx fx_max(fx a, fx b){ return a > b ? a : b; }
static inline fx fx_clamp(fx a, fx lo, fx hi){ return fx_min(fx_max(a, lo), hi); }
static inline fx fx_abs(fx a){ return a < 0 ? -a : a; }

static inline int32_t divide_euclidean(int32_t a, int32_t b){
    int32_t q = a / b;
    if(a % b < 0) q += b > 0 ? -1 : 1;
    return q;
}
