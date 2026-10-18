static uint32_t float_bits(float f){
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

static float float_from_bits(uint32_t u){
    float f;
    memcpy(&f, &u, 4);
    return f;
}

static float fx_to_float(fx x){ return (float)x * (1.0f / 256.0f); }

static int32_t float_to_int_saturated(float f){
    if(f != f) return 0;
    if(f >= 2147483648.0f) return INT32_MAX;
    if(f <= -2147483648.0f) return INT32_MIN;
    return (int32_t)f;
}

static fx float_to_fx(float f){ return float_to_int_saturated(f * 256.0f); }

static float float_sqrt_approx(float x){
    if(!(x >= 0.0f)) return float_from_bits(0x7FC00000u);
    return float_from_bits((float_bits(x) + 0x3F800000u) >> 1);
}

static const float atan_hi[] = {4.6364760399e-01f, 7.8539812565e-01f, 9.8279368877e-01f, 1.5707962513e+00f};
static const float atan_lo[] = {5.0121582440e-09f, 3.7748947079e-08f, 3.4473217170e-08f, 7.5497894159e-08f};
static const float atan_t[] = {3.3333328366e-01f, -1.9999158382e-01f, 1.4253635705e-01f, -1.0648017377e-01f,
                               6.1687607318e-02f};

static float float_atan(float x){
    uint32_t ix = float_bits(x);
    int sign = ix >> 31;
    ix &= 0x7FFFFFFF;
    int id;
    if(ix >= 0x4C800000){
        if(ix > 0x7F800000) return x;
        float z = atan_hi[3] + 0x1p-120f;
        return sign ? -z : z;
    }
    if(ix < 0x3EE00000){
        if(ix < 0x39800000) return x;
        id = -1;
    } else {
        x = float_from_bits(ix);
        if(ix < 0x3F980000){
            if(ix < 0x3F300000){
                id = 0;
                x = (2.0f * x - 1.0f) / (2.0f + x);
            } else {
                id = 1;
                x = (x - 1.0f) / (x + 1.0f);
            }
        } else if(ix < 0x401C0000){
            id = 2;
            x = (x - 1.5f) / (1.0f + 1.5f * x);
        } else {
            id = 3;
            x = -1.0f / x;
        }
    }
    float z = x * x, w = z * z;
    float s1 = z * (atan_t[0] + w * (atan_t[2] + w * atan_t[4]));
    float s2 = w * (atan_t[1] + w * atan_t[3]);
    if(id < 0) return x - x * (s1 + s2);
    z = atan_hi[id] - ((x * (s1 + s2) - atan_lo[id]) - x);
    return sign ? -z : z;
}

static float float_atan2(float y, float x){
    static const float pi = 3.1415927410e+00f, pi_lo = -8.7422776573e-08f;
    if(x != x || y != y) return x + y;
    uint32_t ix = float_bits(x), iy = float_bits(y);
    if(ix == 0x3F800000) return float_atan(y);
    uint32_t m = ((iy >> 31) & 1) | ((ix >> 30) & 2);
    ix &= 0x7FFFFFFF;
    iy &= 0x7FFFFFFF;
    if(iy == 0){
        switch(m){
        case 0:
        case 1: return y;
        case 2: return pi;
        default: return -pi;
        }
    }
    if(ix == 0) return m & 1 ? -pi / 2 : pi / 2;
    if(ix == 0x7F800000){
        if(iy == 0x7F800000){
            switch(m){
            case 0: return pi / 4;
            case 1: return -pi / 4;
            case 2: return 3 * pi / 4;
            default: return -3 * pi / 4;
            }
        }
        switch(m){
        case 0: return 0.0f;
        case 1: return -0.0f;
        case 2: return pi;
        default: return -pi;
        }
    }
    if(ix + (26 << 23) < iy || iy == 0x7F800000) return m & 1 ? -pi / 2 : pi / 2;
    float z;
    if((m & 2) && iy + (26 << 23) < ix) z = 0.0f;
    else z = float_atan(float_from_bits(float_bits(y / x) & 0x7FFFFFFF));
    switch(m){
    case 0: return z;
    case 1: return -z;
    case 2: return pi - (z - pi_lo);
    default: return (z - pi_lo) - pi;
    }
}

static fx fx_atan2(fx y, fx x){ return float_to_fx(float_atan2(fx_to_float(y), fx_to_float(x))); }

static const int32_t atan_q24[257] = {
    0,        65536,    131069,   196599,   262123,   327638,   393144,   458638,   524117,   589581,
    655027,   720453,   785857,   851237,   916591,   981917,   1047214,  1112479,  1177710,  1242905,
    1308063,  1373181,  1438258,  1503292,  1568280,  1633221,  1698113,  1762954,  1827743,  1892476,
    1957153,  2021772,  2086331,  2150827,  2215260,  2279627,  2343926,  2408156,  2472315,  2536402,
    2600414,  2664350,  2728207,  2791985,  2855682,  2919296,  2982825,  3046268,  3109623,  3172888,
    3236062,  3299143,  3362130,  3425021,  3487814,  3550509,  3613103,  3675595,  3737983,  3800267,
    3862444,  3924513,  3986473,  4048323,  4110060,  4171684,  4233193,  4294587,  4355862,  4417020,
    4478057,  4538973,  4599767,  4660437,  4720982,  4781401,  4841693,  4901856,  4961890,  5021793,
    5081565,  5141204,  5200709,  5260079,  5319313,  5378410,  5437369,  5496190,  5554870,  5613410,
    5671808,  5730064,  5788176,  5846144,  5903966,  5961643,  6019173,  6076555,  6133789,  6190874,
    6247809,  6304594,  6361227,  6417708,  6474037,  6530212,  6586234,  6642101,  6697813,  6753369,
    6808770,  6864013,  6919099,  6974027,  7028797,  7083409,  7137860,  7192153,  7246285,  7300256,
    7354067,  7407716,  7461204,  7514530,  7567693,  7620694,  7673531,  7726205,  7778716,  7831063,
    7883246,  7935264,  7987118,  8038807,  8090331,  8141690,  8192884,  8243913,  8294775,  8345472,
    8396004,  8446369,  8496568,  8546602,  8596469,  8646170,  8695704,  8745073,  8794275,  8843311,
    8892180,  8940884,  8989421,  9037792,  9085997,  9134035,  9181908,  9229615,  9277156,  9324532,
    9371741,  9418786,  9465665,  9512378,  9558927,  9605311,  9651530,  9697585,  9743475,  9789201,
    9834763,  9880161,  9925396,  9970468,  10015376, 10060122, 10104705, 10149126, 10193384, 10237481,
    10281417, 10325191, 10368805, 10412257, 10455550, 10498682, 10541655, 10584468, 10627123, 10669619,
    10711956, 10754135, 10796157, 10838022, 10879729, 10921280, 10962675, 11003914, 11044997, 11085926,
    11126700, 11167320, 11207785, 11248098, 11288257, 11328264, 11368118, 11407821, 11447372, 11486773,
    11526023, 11565122, 11604073, 11642874, 11681526, 11720030, 11758386, 11796594, 11834656, 11872571,
    11910341, 11947964, 11985443, 12022777, 12059966, 12097012, 12133915, 12170675, 12207292, 12243768,
    12280103, 12316297, 12352350, 12388263, 12424037, 12459673, 12495169, 12530528, 12565749, 12600834,
    12635782, 12670594, 12705270, 12739812, 12774219, 12808492, 12842631, 12876638, 12910512, 12944254,
    12977865, 13011344, 13044693, 13077912, 13111002, 13143963, 13176795,
};

static fx fx_atan2_small(fx y, fx x){
    if(y == 0 || x == 0 || y < -256 || y > 256 || x < -256 || x > 256) return fx_atan2(y, x);
    uint32_t ay = (uint32_t)(y < 0 ? -y : y), ax = (uint32_t)(x < 0 ? -x : x);
    bool steep = ay > ax;
    uint32_t r = steep ? (ax << 16) / ay : (ay << 16) / ax;
    uint32_t i = r >> 8, f = r & 255;
    int32_t t = atan_q24[i] + (i < 256 ? (int32_t)(((atan_q24[i + 1] - atan_q24[i]) * (int32_t)f) >> 8) : 0);
    if(steep) t = 26353589 - t;
    if(x < 0) t = 52707179 - t;
    uint32_t rem = (uint32_t)t & 0xFFFF;
    if(rem < 512 || rem > 65536 - 512) return fx_atan2(y, x);
    fx v = t >> 16;
    return y < 0 ? -v : v;
}
