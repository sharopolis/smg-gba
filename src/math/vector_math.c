static inline v3 V3(fx x, fx y, fx z){ return (v3){x, y, z}; }
static inline v2 V2(fx x, fx y){ return (v2){x, y}; }
static inline v3 v3_add(v3 a, v3 b){ return V3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline v3 v3_sub(v3 a, v3 b){ return V3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline v3 v3_neg(v3 a){ return V3(-a.x, -a.y, -a.z); }
static inline v3 v3_scale(v3 a, fx s){ return V3(fx_mul(a.x, s), fx_mul(a.y, s), fx_mul(a.z, s)); }
static inline v3 v3_mul(v3 a, v3 b){ return V3(fx_mul(a.x, b.x), fx_mul(a.y, b.y), fx_mul(a.z, b.z)); }
static inline fx v3_dot(v3 a, v3 b){ return fx_mul(a.x, b.x) + fx_mul(a.y, b.y) + fx_mul(a.z, b.z); }
static inline v3 v3_cross(v3 a, v3 b){
    return V3(fx_mul(a.y, b.z) - fx_mul(a.z, b.y), fx_mul(a.z, b.x) - fx_mul(a.x, b.z),
              fx_mul(a.x, b.y) - fx_mul(a.y, b.x));
}

static inline fx v2_dot(v2 a, v2 b){ return fx_mul(a.x, b.x) + fx_mul(a.y, b.y); }
static inline fx v2_length_squared(v2 a){ return v2_dot(a, a); }

static inline int32_t mul_shift_exact(int32_t a, int32_t b, int shift){
    if((uint32_t)(a + 65536) < 131072u && (uint32_t)(b + 32768) < 65536u) return (a * b) >> shift;
    return (int32_t)((int64_t)a * b >> shift);
}

static inline v3 m3_transform_vector(const m3* m, v3 v){
    return V3(v3_dot(m->r[0], v), v3_dot(m->r[1], v), v3_dot(m->r[2], v));
}

static inline v3 xform_point(const xform* t, v3 p){
    const m3* m = &t->m;
    return V3(t->o.x + ((m->r[0].x * p.x + m->r[0].y * p.y + m->r[0].z * p.z) >> 8),
              t->o.y + ((m->r[1].x * p.x + m->r[1].y * p.y + m->r[1].z * p.z) >> 8),
              t->o.z + ((m->r[2].x * p.x + m->r[2].y * p.y + m->r[2].z * p.z) >> 8));
}

static inline uint32_t view_clip_mask(v3 v){
    uint32_t c = 0;
    if(-v.x > v.z) c |= 2;
    if(v.x > v.z) c |= 1;
    if(-v.y > v.z) c |= 4;
    if(v.y > v.z) c |= 8;
    fx g = v.z << 2;
    if(v.z < CLIP_NEAR_Z || v.x > g || -v.x > g || v.y > g || -v.y > g) c |= CLIP_GUARD;
    return c;
}

static v2 fx_sin_cos(fx angle){
    uint32_t i = (uint32_t)(((angle * 41721) >> 10) >> 6) & 1023;
    return V2(lut_sin_cos[i][0], lut_sin_cos[i][1]);
}

static inline fx fx_sin(fx a){ return fx_sin_cos(a).x; }
static inline fx fx_cos(fx a){ return fx_sin_cos(a).y; }

static m3 m3_identity(void){ return (m3){{V3(FX_ONE, 0, 0), V3(0, FX_ONE, 0), V3(0, 0, FX_ONE)}}; }

static m3 m3_rotate_x(fx a){
    v2 sc = fx_sin_cos(a);
    return (m3){{V3(FX_ONE, 0, 0), V3(0, sc.y, -sc.x), V3(0, sc.x, sc.y)}};
}

static m3 m3_rotate_y(fx a){
    v2 sc = fx_sin_cos(a);
    return (m3){{V3(sc.y, 0, sc.x), V3(0, FX_ONE, 0), V3(-sc.x, 0, sc.y)}};
}

static m3 m3_rotate_z(fx a){
    v2 sc = fx_sin_cos(a);
    return (m3){{V3(sc.y, -sc.x, 0), V3(sc.x, sc.y, 0), V3(0, 0, FX_ONE)}};
}

static m3 m3_mul(const m3* a, const m3* b){
    m3 out;
    for(int i = 0; i < 3; i++){
        const v3 r = a->r[i];
        out.r[i] = V3(fx_mul(r.x, b->r[0].x) + fx_mul(r.y, b->r[1].x) + fx_mul(r.z, b->r[2].x),
                      fx_mul(r.x, b->r[0].y) + fx_mul(r.y, b->r[1].y) + fx_mul(r.z, b->r[2].y),
                      fx_mul(r.x, b->r[0].z) + fx_mul(r.y, b->r[1].z) + fx_mul(r.z, b->r[2].z));
    }
    return out;
}

static xform xform_compose(const xform* self, const xform* then){
    return (xform){m3_mul(&then->m, &self->m), v3_add(then->o, m3_transform_vector(&then->m, self->o))};
}

static int32_t table_sqrt(int32_t x, int frac){
    if(x < 0) return 0;
    int scale = 0;
    while(x >= 4096){
        x >>= 4;
        scale += 2;
    }
    return ((int32_t)lut_sqrt[x] << scale << (frac / 2)) >> 8;
}

static fx fx_sqrt(fx x){ return table_sqrt(x, 8); }

static fx fx_recip(fx x){ return x <= -16384 || x >= 16384 ? 0 : lut_recip8[x + 16384]; }

static inline int32_t mul14(int32_t a, int32_t b){ return (a * b) >> 14; }
static inline v3 v3_scale14(v3 a, int32_t s){ return V3(mul14(a.x, s), mul14(a.y, s), mul14(a.z, s)); }
static inline int32_t v3_dot14(v3 a, v3 n){
    return ((a.x * n.x) >> 14) + ((a.y * n.y) >> 14) + ((a.z * n.z) >> 14);
}
static inline v3 v3_cross14(v3 a, v3 b){
    return V3(mul14(a.y, b.z) - mul14(a.z, b.y), mul14(a.z, b.x) - mul14(a.x, b.z),
              mul14(a.x, b.y) - mul14(a.y, b.x));
}
static inline v3 v3_from_i16(const int16_t* p){ return V3(p[0], p[1], p[2]); }

static inline uint32_t isqrt32(uint32_t x){ return isqrt32_arm(x); }

static uint32_t isqrt64(uint64_t x){
    if(x <= 0xFFFFFFFFu) return isqrt32((uint32_t)x);

    int shift = 0;
    while(x > 0xFFFFFFFFu){
        x >>= 2;
        shift++;
    }
    return isqrt32((uint32_t)x) << shift;
}

IWRAM_FN static uint64_t v3_length_squared64(v3 a){
    return (uint64_t)((int64_t)a.x * a.x + (int64_t)a.y * a.y + (int64_t)a.z * a.z);
}

static int32_t v3_length_exact(v3 a){ return (int32_t)isqrt64(v3_length_squared64(a)); }

IWRAM_FN static v3 v3_direction14(v3 a, v3 fallback){
    uint32_t m = (uint32_t)fx_max(fx_max(fx_abs(a.x), fx_abs(a.y)), fx_abs(a.z));
    if(m == 0) return fallback;
    int s = 0;
    if(m >= 32768){
        do {
            m >>= 1;
            s++;
        } while(m >= 32768);
        a = V3(a.x >> s, a.y >> s, a.z >> s);
    } else {
        while(m < 8192){
            m <<= 2;
            s += 2;
        }
        if(m < 16384) s++;
        a = V3(a.x << s, a.y << s, a.z << s);
    }
    uint32_t len = isqrt32((uint32_t)(a.x * a.x + a.y * a.y) + (uint32_t)(a.z * a.z));

    uint32_t r = recip30_arm(len);
    return V3((a.x * (int32_t)r) >> 16, (a.y * (int32_t)r) >> 16, (a.z * (int32_t)r) >> 16);
}

static inline v3 v3_reject14(v3 a, v3 n){ return v3_sub(a, v3_scale14(n, v3_dot14(a, n))); }

static v3 v3_tangent14(v3 want, v3 up, v3 fallback){
    v3 t = v3_reject14(want, up);
    if((t.x | t.y | t.z) == 0 || v3_length_squared64(t) < (uint64_t)64 * 64){
        t = v3_reject14(fallback, up);
        if(v3_length_squared64(t) < (uint64_t)64 * 64)
            t = v3_reject14(fx_abs(up.z) < 12000 ? V3(0, 0, N14_ONE) : V3(N14_ONE, 0, 0), up);
    }
    return v3_direction14(t, fallback);
}

static v3 v3_rotate_between14(v3 v, v3 from, v3 to){
    int32_t c = v3_dot14(from, to);
    if(c > N14_ONE - 2) return v;
    if(c < -N14_ONE + 64) return v;
    v3 k = v3_cross14(from, to);

    v3 kv = v3_cross14(k, v);
    int32_t kd = v3_dot14(k, v);

    int32_t f = kd > -(1 << 17) && kd < (1 << 17) ? (kd << 14) / (N14_ONE + c)
                                                  : (int32_t)(((int64_t)kd << 14) / (N14_ONE + c));
    return V3(mul14(v.x, c) + kv.x + mul14(k.x, f), mul14(v.y, c) + kv.y + mul14(k.y, f),
              mul14(v.z, c) + kv.z + mul14(k.z, f));
}

static v3 v3_rotate_axis14(v3 v, v3 k, fx angle){
    v2 sc = fx_sin_cos(angle);
    int32_t s = sc.x << 6, c = sc.y << 6;
    v3 kv = v3_cross14(k, v);
    int32_t kd = v3_dot14(k, v);
    return V3(mul14(v.x, c) + mul14(kv.x, s) + mul14(mul14(k.x, kd), N14_ONE - c),
              mul14(v.y, c) + mul14(kv.y, s) + mul14(mul14(k.y, kd), N14_ONE - c),
              mul14(v.z, c) + mul14(kv.z, s) + mul14(mul14(k.z, kd), N14_ONE - c));
}

static v3 v3_blend14(v3 a, v3 b, int32_t t){
    v3 m = v3_add(a, v3_scale14(v3_sub(b, a), t));
    return v3_direction14(m, b);
}

static inline v3 v3_n14_to_fx(v3 a){ return V3(a.x >> 6, a.y >> 6, a.z >> 6); }

static v3 v3_from_i32(const int32_t p[3]){ return V3(p[0], p[1], p[2]); }
