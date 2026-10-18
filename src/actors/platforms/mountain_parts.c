static const int16_t sin14_table[1024] = {
    0,      101,    201,    302,    402,    503,    603,    704,    804,    904,    1005,   1105,   1205,
    1306,   1406,   1506,   1606,   1706,   1806,   1906,   2006,   2105,   2205,   2305,   2404,   2503,
    2603,   2702,   2801,   2900,   2999,   3098,   3196,   3295,   3393,   3492,   3590,   3688,   3786,
    3883,   3981,   4078,   4176,   4273,   4370,   4467,   4563,   4660,   4756,   4852,   4948,   5044,
    5139,   5235,   5330,   5425,   5520,   5614,   5708,   5803,   5897,   5990,   6084,   6177,   6270,
    6363,   6455,   6547,   6639,   6731,   6823,   6914,   7005,   7096,   7186,   7276,   7366,   7456,
    7545,   7635,   7723,   7812,   7900,   7988,   8076,   8163,   8250,   8337,   8423,   8509,   8595,
    8680,   8765,   8850,   8935,   9019,   9102,   9186,   9269,   9352,   9434,   9516,   9598,   9679,
    9760,   9841,   9921,   10001,  10080,  10159,  10238,  10316,  10394,  10471,  10549,  10625,  10702,
    10778,  10853,  10928,  11003,  11077,  11151,  11224,  11297,  11370,  11442,  11514,  11585,  11656,
    11727,  11797,  11866,  11935,  12004,  12072,  12140,  12207,  12274,  12340,  12406,  12472,  12537,
    12601,  12665,  12729,  12792,  12854,  12916,  12978,  13039,  13100,  13160,  13219,  13279,  13337,
    13395,  13453,  13510,  13567,  13623,  13678,  13733,  13788,  13842,  13896,  13949,  14001,  14053,
    14104,  14155,  14206,  14256,  14305,  14354,  14402,  14449,  14497,  14543,  14589,  14635,  14680,
    14724,  14768,  14811,  14854,  14896,  14937,  14978,  15019,  15059,  15098,  15137,  15175,  15213,
    15250,  15286,  15322,  15357,  15392,  15426,  15460,  15493,  15525,  15557,  15588,  15619,  15649,
    15679,  15707,  15736,  15763,  15791,  15817,  15843,  15868,  15893,  15917,  15941,  15964,  15986,
    16008,  16029,  16049,  16069,  16088,  16107,  16125,  16143,  16160,  16176,  16192,  16207,  16221,
    16235,  16248,  16261,  16273,  16284,  16295,  16305,  16315,  16324,  16332,  16340,  16347,  16353,
    16359,  16364,  16369,  16373,  16376,  16379,  16381,  16383,  16384,  16384,  16384,  16383,  16381,
    16379,  16376,  16373,  16369,  16364,  16359,  16353,  16347,  16340,  16332,  16324,  16315,  16305,
    16295,  16284,  16273,  16261,  16248,  16235,  16221,  16207,  16192,  16176,  16160,  16143,  16125,
    16107,  16088,  16069,  16049,  16029,  16008,  15986,  15964,  15941,  15917,  15893,  15868,  15843,
    15817,  15791,  15763,  15736,  15707,  15679,  15649,  15619,  15588,  15557,  15525,  15493,  15460,
    15426,  15392,  15357,  15322,  15286,  15250,  15213,  15175,  15137,  15098,  15059,  15019,  14978,
    14937,  14896,  14854,  14811,  14768,  14724,  14680,  14635,  14589,  14543,  14497,  14449,  14402,
    14354,  14305,  14256,  14206,  14155,  14104,  14053,  14001,  13949,  13896,  13842,  13788,  13733,
    13678,  13623,  13567,  13510,  13453,  13395,  13337,  13279,  13219,  13160,  13100,  13039,  12978,
    12916,  12854,  12792,  12729,  12665,  12601,  12537,  12472,  12406,  12340,  12274,  12207,  12140,
    12072,  12004,  11935,  11866,  11797,  11727,  11656,  11585,  11514,  11442,  11370,  11297,  11224,
    11151,  11077,  11003,  10928,  10853,  10778,  10702,  10625,  10549,  10471,  10394,  10316,  10238,
    10159,  10080,  10001,  9921,   9841,   9760,   9679,   9598,   9516,   9434,   9352,   9269,   9186,
    9102,   9019,   8935,   8850,   8765,   8680,   8595,   8509,   8423,   8337,   8250,   8163,   8076,
    7988,   7900,   7812,   7723,   7635,   7545,   7456,   7366,   7276,   7186,   7096,   7005,   6914,
    6823,   6731,   6639,   6547,   6455,   6363,   6270,   6177,   6084,   5990,   5897,   5803,   5708,
    5614,   5520,   5425,   5330,   5235,   5139,   5044,   4948,   4852,   4756,   4660,   4563,   4467,
    4370,   4273,   4176,   4078,   3981,   3883,   3786,   3688,   3590,   3492,   3393,   3295,   3196,
    3098,   2999,   2900,   2801,   2702,   2603,   2503,   2404,   2305,   2205,   2105,   2006,   1906,
    1806,   1706,   1606,   1506,   1406,   1306,   1205,   1105,   1005,   904,    804,    704,    603,
    503,    402,    302,    201,    101,    0,      -101,   -201,   -302,   -402,   -503,   -603,   -704,
    -804,   -904,   -1005,  -1105,  -1205,  -1306,  -1406,  -1506,  -1606,  -1706,  -1806,  -1906,  -2006,
    -2105,  -2205,  -2305,  -2404,  -2503,  -2603,  -2702,  -2801,  -2900,  -2999,  -3098,  -3196,  -3295,
    -3393,  -3492,  -3590,  -3688,  -3786,  -3883,  -3981,  -4078,  -4176,  -4273,  -4370,  -4467,  -4563,
    -4660,  -4756,  -4852,  -4948,  -5044,  -5139,  -5235,  -5330,  -5425,  -5520,  -5614,  -5708,  -5803,
    -5897,  -5990,  -6084,  -6177,  -6270,  -6363,  -6455,  -6547,  -6639,  -6731,  -6823,  -6914,  -7005,
    -7096,  -7186,  -7276,  -7366,  -7456,  -7545,  -7635,  -7723,  -7812,  -7900,  -7988,  -8076,  -8163,
    -8250,  -8337,  -8423,  -8509,  -8595,  -8680,  -8765,  -8850,  -8935,  -9019,  -9102,  -9186,  -9269,
    -9352,  -9434,  -9516,  -9598,  -9679,  -9760,  -9841,  -9921,  -10001, -10080, -10159, -10238, -10316,
    -10394, -10471, -10549, -10625, -10702, -10778, -10853, -10928, -11003, -11077, -11151, -11224, -11297,
    -11370, -11442, -11514, -11585, -11656, -11727, -11797, -11866, -11935, -12004, -12072, -12140, -12207,
    -12274, -12340, -12406, -12472, -12537, -12601, -12665, -12729, -12792, -12854, -12916, -12978, -13039,
    -13100, -13160, -13219, -13279, -13337, -13395, -13453, -13510, -13567, -13623, -13678, -13733, -13788,
    -13842, -13896, -13949, -14001, -14053, -14104, -14155, -14206, -14256, -14305, -14354, -14402, -14449,
    -14497, -14543, -14589, -14635, -14680, -14724, -14768, -14811, -14854, -14896, -14937, -14978, -15019,
    -15059, -15098, -15137, -15175, -15213, -15250, -15286, -15322, -15357, -15392, -15426, -15460, -15493,
    -15525, -15557, -15588, -15619, -15649, -15679, -15707, -15736, -15763, -15791, -15817, -15843, -15868,
    -15893, -15917, -15941, -15964, -15986, -16008, -16029, -16049, -16069, -16088, -16107, -16125, -16143,
    -16160, -16176, -16192, -16207, -16221, -16235, -16248, -16261, -16273, -16284, -16295, -16305, -16315,
    -16324, -16332, -16340, -16347, -16353, -16359, -16364, -16369, -16373, -16376, -16379, -16381, -16383,
    -16384, -16384, -16384, -16383, -16381, -16379, -16376, -16373, -16369, -16364, -16359, -16353, -16347,
    -16340, -16332, -16324, -16315, -16305, -16295, -16284, -16273, -16261, -16248, -16235, -16221, -16207,
    -16192, -16176, -16160, -16143, -16125, -16107, -16088, -16069, -16049, -16029, -16008, -15986, -15964,
    -15941, -15917, -15893, -15868, -15843, -15817, -15791, -15763, -15736, -15707, -15679, -15649, -15619,
    -15588, -15557, -15525, -15493, -15460, -15426, -15392, -15357, -15322, -15286, -15250, -15213, -15175,
    -15137, -15098, -15059, -15019, -14978, -14937, -14896, -14854, -14811, -14768, -14724, -14680, -14635,
    -14589, -14543, -14497, -14449, -14402, -14354, -14305, -14256, -14206, -14155, -14104, -14053, -14001,
    -13949, -13896, -13842, -13788, -13733, -13678, -13623, -13567, -13510, -13453, -13395, -13337, -13279,
    -13219, -13160, -13100, -13039, -12978, -12916, -12854, -12792, -12729, -12665, -12601, -12537, -12472,
    -12406, -12340, -12274, -12207, -12140, -12072, -12004, -11935, -11866, -11797, -11727, -11656, -11585,
    -11514, -11442, -11370, -11297, -11224, -11151, -11077, -11003, -10928, -10853, -10778, -10702, -10625,
    -10549, -10471, -10394, -10316, -10238, -10159, -10080, -10001, -9921,  -9841,  -9760,  -9679,  -9598,
    -9516,  -9434,  -9352,  -9269,  -9186,  -9102,  -9019,  -8935,  -8850,  -8765,  -8680,  -8595,  -8509,
    -8423,  -8337,  -8250,  -8163,  -8076,  -7988,  -7900,  -7812,  -7723,  -7635,  -7545,  -7456,  -7366,
    -7276,  -7186,  -7096,  -7005,  -6914,  -6823,  -6731,  -6639,  -6547,  -6455,  -6363,  -6270,  -6177,
    -6084,  -5990,  -5897,  -5803,  -5708,  -5614,  -5520,  -5425,  -5330,  -5235,  -5139,  -5044,  -4948,
    -4852,  -4756,  -4660,  -4563,  -4467,  -4370,  -4273,  -4176,  -4078,  -3981,  -3883,  -3786,  -3688,
    -3590,  -3492,  -3393,  -3295,  -3196,  -3098,  -2999,  -2900,  -2801,  -2702,  -2603,  -2503,  -2404,
    -2305,  -2205,  -2105,  -2006,  -1906,  -1806,  -1706,  -1606,  -1506,  -1406,  -1306,  -1205,  -1105,
    -1005,  -904,   -804,   -704,   -603,   -503,   -402,   -302,   -201,   -101};
enum { MOUNTAIN_PART_STAY = 170, MOUNTAIN_PART_MOVE_START, MOUNTAIN_PART_MOVE, MOUNTAIN_PART_HIPDROP };
static bool mountain_part_type(int type){
    return type >= OBJ_FFMOUNTAINFLOATER && type <= OBJ_FFMOUNTAINSLIPD;
}
static const FFMountainSpec* mountain_part_spec(const Actor* a){
    return a->value >= 0 && (unsigned)a->value < FFMOUNTAIN_SPEC_COUNT ? &ffmountain_specs[a->value] : NULL;
}
static v3 mountain_part_base_up(const Actor* a){ return v3_from_i16(actor_stage_object(a)->up); }
static v3 mountain_part_base_front(const Actor* a){ return v3_from_i16(actor_stage_object(a)->front); }
static float float_clamp(float v, float lo, float hi){ return v < lo ? lo : v > hi ? hi : v; }
static float float_converge(float v, float to, float step){
    return v < to ? float_clamp(v + step, v, to) : float_clamp(v - step, to, v);
}
static int32_t sign_nonzero(int32_t x){ return x < 0 ? -1 : 1; }
static int sin14_degrees(float degrees){
    int32_t phase = float_to_int_saturated(degrees * (1024.0f * 65536.0f / 360.0f));
    unsigned i = ((uint32_t)phase >> 16) & 1023, f = (uint32_t)phase & 65535;
    return sin14_table[i] + ((int64_t)(sin14_table[(i + 1) & 1023] - sin14_table[i]) * f >> 16);
}
static v3 v3_rotate_degrees14(v3 v, v3 axis, float degrees){
    int32_t s = sin14_degrees(degrees), c = sin14_degrees(degrees + 90.0f);
    v3 cross = v3_cross14(axis, v);
    int32_t dot = v3_dot14(axis, v);
    return v3_add(v3_add(v3_scale14(v, c), v3_scale14(cross, s)), v3_scale14(axis, mul14(dot, N14_ONE - c)));
}
static void mountain_part_tilt_pose(Actor* a, float degrees){
    a->up =
        v3_direction14(v3_rotate_degrees14(mountain_part_base_up(a), mountain_part_base_front(a), degrees),
                       mountain_part_base_up(a));
    a->front = v3_tangent14(mountain_part_base_front(a), a->up, mountain_part_base_front(a));
}
static void mountain_part_init(Actor* a, const StageObject* o){
    a->value = -1;
    for(unsigned i = 0; i < FFMOUNTAIN_SPEC_COUNT; i++)
        if(ffmountain_specs[i].source == o->id){
            a->value = (int16_t)i;
            break;
        }
    if(a->value < 0){
        a->active = 0;
        return;
    }
    a->state = MOUNTAIN_PART_STAY;
    a->timer = 0;
    a->health = 0;
    a->target = V3(0, 0, 0);
    a->velocity = V3(0, 0, 0);
    a->anim = -1;
    a->radius = 0;
    for(int k = 0; k < 4; k++) a->args[k] = k == 3 ? N14_ONE : 0;
    a->figure.scale = V3(64 * o->scale[0] / 256, 64 * o->scale[1] / 256, 64 * o->scale[2] / 256);
    const FFMountainSpec* s = mountain_part_spec(a);
    if(!s->shape) a->state = MOUNTAIN_PART_MOVE;
}
static bool mountain_part_ridden(const Actor* a){ return g_mario_link.ride == actor_index(a); }
static void mountain_part_float(Actor* a, const FFMountainSpec* s){
    float velocity = float_from_bits(a->target.x), displacement = float_from_bits(a->target.y);
    if(mountain_part_ridden(a)){
        if((float)s->condition <= displacement) velocity = float_converge(velocity, 0.0f, 0.05f);
        else velocity = velocity + 0.0001f * (float)s->speed;
    } else {
        if(displacement <= 0.0f) velocity = float_converge(velocity, 0.0f, 0.05f);
        else velocity = velocity - 0.0001f * (float)s->angle;
    }
    velocity = float_clamp(velocity * 0.98f, -50.0f, 50.0f);
    displacement = float_clamp(displacement + velocity, 0.0f, (float)s->condition);
    a->target.x = (int32_t)float_bits(velocity);
    a->target.y = (int32_t)float_bits(displacement);
    a->velocity = v3_scale14(mountain_part_base_up(a), -float_to_int_saturated(velocity * 256.0f));
    a->position = v3_sub(a->home, v3_scale14(mountain_part_base_up(a), float_to_int_saturated(displacement)));
}
static void mountain_part_tilt_one_axis(Actor* a, const FFMountainSpec* s){
    bool on = mountain_part_ridden(a), hip = g_mario_link.hipdrop == actor_index(a);
    float velocity = float_from_bits(a->target.x), angle = float_from_bits(a->target.y);
    if(hip){
        a->state = MOUNTAIN_PART_HIPDROP;
        a->timer = 0;
        a->health = 1;
    }
    if(a->state == MOUNTAIN_PART_STAY){
        if(on){
            a->state = MOUNTAIN_PART_MOVE_START;
            a->timer = 0;
        }
        return;
    }
    if(a->state == MOUNTAIN_PART_MOVE_START && (velocity > 0.1f || velocity < -.1f)){
        a->state = MOUNTAIN_PART_MOVE;
        a->timer = 0;
        return;
    }
    GravitySample gs = gravity_sample(g_game.stage.header, a->position);
    v3 up = gs.index < 0 ? mountain_part_base_up(a) : gs.up;
    v3 offset = v3_sub(g_game.player.figure.position, a->position), gravity = v3_neg(up);
    int direction = sign_nonzero(v3_dot14(v3_cross(offset, gravity), a->front));
    int inertia = s->inertia > 0 ? s->inertia : 1000;
    if(a->state == MOUNTAIN_PART_HIPDROP && a->health)
        velocity += (float)direction *
                    (40.0f * (0.001f * (float)fx_abs(v3_dot14(offset, v3_cross14(a->up, a->front)))) /
                     (0.1f * (float)inertia));
    if(on)
        velocity +=
            (float)direction * ((0.001f * (float)fx_abs(v3_dot14(offset, v3_cross14(a->up, a->front)))) /
                                (0.1f * (float)inertia));
    if(a->state != MOUNTAIN_PART_MOVE_START && a->timer > 2){
        int force = v3_dot14(v3_cross14(a->up, mountain_part_base_up(a)), a->front);
        if(fx_abs(force) < 16 && velocity <= 0.1f && velocity >= -.1f) velocity = 0;
        else velocity += (float)sign_nonzero(force) * (0.01f * (float)s->restore) / (0.1f * (float)inertia);
        velocity *= 0.99f;
    }
    velocity = float_clamp(velocity, -0.01f * (float)s->speed, 0.01f * (float)s->speed);
    float next = angle + velocity;
    v3 next_up = v3_rotate_degrees14(mountain_part_base_up(a), mountain_part_base_front(a), next);
    int dot = v3_dot14(next_up, up), limit = sin14_degrees((float)s->angle + 90.0f);
    if(s->angle > 0 && dot <= limit) velocity *= -0.5f;
    else {
        angle = next;
        mountain_part_tilt_pose(a, angle);
    }
    if(a->state == MOUNTAIN_PART_HIPDROP){
        if(!a->health){
            a->state = MOUNTAIN_PART_MOVE;
            a->timer = 0;
        }
        a->health = 0;
    }
    if(velocity == 0.0f && !on){
        a->state = MOUNTAIN_PART_STAY;
        a->timer = 0;
    }
    a->target.x = (int32_t)float_bits(velocity);
    a->target.y = (int32_t)float_bits(angle);
}
static void quat14_normalize(int16_t q[4]){
    uint32_t n = 0;
    for(int k = 0; k < 4; k++) n += (uint32_t)((int32_t)q[k] * q[k]);
    int length = (int)isqrt32(n);
    if(!length){
        q[0] = q[1] = q[2] = 0;
        q[3] = N14_ONE;
        return;
    }
    for(int k = 0; k < 4; k++) q[k] = (int16_t)((int32_t)q[k] * N14_ONE / length);
}
static v3 quat14_rotate(const int16_t q[4], v3 v){
    v3 xyz = V3(q[0], q[1], q[2]), cross = v3_cross14(xyz, v);
    return v3_add(v, v3_scale14(v3_add(v3_scale14(cross, q[3]), v3_cross14(xyz, cross)), 2 * N14_ONE));
}
static void mountain_part_quat_blend(Actor* a, const int16_t goal[4], int t){
    int dot = 0;
    for(int k = 0; k < 4; k++) dot += (int32_t)a->args[k] * goal[k];
    for(int k = 0; k < 4; k++){
        int end = dot < 0 ? -goal[k] : goal[k];
        a->args[k] += (int16_t)(((end - a->args[k]) * t) >> 14);
    }
    quat14_normalize(a->args);
}
static v3 mountain_part_local_to_world(const Actor* a, v3 v){
    return v3_add(
        v3_scale14(v3_cross14(mountain_part_base_up(a), mountain_part_base_front(a)), v.x),
        v3_add(v3_scale14(mountain_part_base_up(a), v.y), v3_scale14(mountain_part_base_front(a), v.z)));
}
static void mountain_part_tilt_two_axes(Actor* a, const FFMountainSpec* s){
    bool on = mountain_part_ridden(a), hip = g_mario_link.hipdrop == actor_index(a);
    if(hip){
        a->state = MOUNTAIN_PART_HIPDROP;
        a->timer = 0;
        a->health = 1;
    }
    if(a->state == MOUNTAIN_PART_STAY){
        if(on){
            a->state = MOUNTAIN_PART_MOVE;
            a->timer = 0;
        }
        return;
    }
    if(a->state == MOUNTAIN_PART_HIPDROP && !a->health){
        a->state = MOUNTAIN_PART_MOVE;
        a->timer = 0;
        return;
    }
    bool hip_nerve = a->state == MOUNTAIN_PART_HIPDROP;
    if(on || hip_nerve){
        v3 delta = v3_sub(g_game.player.figure.position, a->position),
           side = v3_cross14(mountain_part_base_up(a), mountain_part_base_front(a));
        v3 local = V3(v3_dot14(delta, side), v3_dot14(delta, mountain_part_base_up(a)),
                      v3_dot14(delta, mountain_part_base_front(a)));

        v3 original_world_up =
            V3(-mountain_part_base_up(a).x, mountain_part_base_up(a).z, mountain_part_base_up(a).y);
        v3 axis = v3_direction14(v3_cross(original_world_up, local), V3(N14_ONE, 0, 0));
        int sh = sin14_degrees((float)s->angle * .5f), ch = sin14_degrees((float)s->angle * .5f + 90.0f);
        int16_t goal[4] = {(int16_t)mul14(axis.x, sh), (int16_t)mul14(axis.y, sh), (int16_t)mul14(axis.z, sh),
                           (int16_t)ch};
        int inertia = s->inertia > 0 ? s->inertia : 500;
        float rate = float_clamp((.001f * (float)v3_length_exact(local)) /
                                     (hip_nerve ? .1f * (float)inertia : (float)inertia),
                                 0, 1);
        a->target.x = (int32_t)float_bits(rate - float_from_bits(a->target.x));
        mountain_part_quat_blend(a, goal, float_to_int_saturated(rate * 16384.0f));
    }
    if(!hip_nerve){
        const int16_t identity[4] = {0, 0, 0, N14_ONE};
        mountain_part_quat_blend(a, identity, fx_min(s->restore * 16384 / 10000, N14_ONE));
    } else a->health = 0;
    a->up = v3_direction14(mountain_part_local_to_world(a, quat14_rotate(a->args, V3(0, N14_ONE, 0))),
                           mountain_part_base_up(a));
    a->front = v3_tangent14(mountain_part_local_to_world(a, quat14_rotate(a->args, V3(0, 0, N14_ONE))), a->up,
                            mountain_part_base_front(a));
}
static void mountain_part_think(Actor* a){
    const FFMountainSpec* s = mountain_part_spec(a);
    if(!s) return;
    if(!s->shape) mountain_part_float(a, s);
    else if(!s->axis) mountain_part_tilt_one_axis(a, s);
    else mountain_part_tilt_two_axes(a, s);
}

#define PARTS_COL_SLACK           FX(64)
#define PARTS_COL_SIGNATURE_RANGE FX(2600)
static EWRAM_BSS uint32_t g_parts_col_signature, g_parts_col_generation;
static inline uint32_t fnv1a_mix(uint32_t h, uint32_t v){ return (h ^ v) * 16777619u; }
static uint32_t fnv1a_mix_v3(uint32_t h, v3 v){
    return fnv1a_mix(fnv1a_mix(fnv1a_mix(h, (uint32_t)v.x), (uint32_t)v.y), (uint32_t)v.z);
}
static uint32_t parts_collision_signature(v3 centre){
    uint32_t h = fnv1a_mix(2166136261u, (uint32_t)g_game.actor_count);
    for(int i = 0; i < g_game.actor_count; i++){
        const Actor* a = &g_game.actors[i];
        if(!a->active) continue;
        int t = a->type;

        fx range;
        if(mountain_part_type(t) || t == OBJ_WATERLEAKICE) range = PARTS_COL_SIGNATURE_RANGE;
        else if(volcano_part_type(t) && t != OBJ_MARIOFIREBALL) range = FX(9000);
        else if(t == OBJ_CRYSTALCAGEM || t == OBJ_KILLER || valve_type(t)) range = 0;
        else continue;
        if(range){
            v3 d = v3_sub(a->position, centre);
            if(fx_abs(d.x) > range || fx_abs(d.y) > range || fx_abs(d.z) > range) continue;
        }

        h = fnv1a_mix_v3(fnv1a_mix(h, (uint32_t)i | (uint32_t)t << 8), a->position);
        h = fnv1a_mix_v3(h, a->up);
        h = fnv1a_mix_v3(h, a->front);
        if(mountain_part_type(t)) continue;
        h = fnv1a_mix(h, (uint32_t)a->state | (uint32_t)(a->flags & ACTOR_WAITING) << 8);
        if(volcano_part_type(t) || valve_type(t)){
            h = fnv1a_mix(h, (uint32_t)a->anim_frame);
            h = fnv1a_mix(h, (uint32_t)a->anim);
            h = fnv1a_mix(h, (uint32_t)a->args[2] | (uint32_t)(uint16_t)a->value << 16);
        }
    }
    return h;
}

enum {
    PARTS_COL_MOUNTAIN,
    PARTS_COL_WATERLEAK,
    PARTS_COL_CAGE,
    PARTS_COL_VOLCANO,
    PARTS_COL_VALVE,
    PARTS_COL_KINDS
};
#define PARTS_COL_CENTRES 8
static uint32_t g_parts_col_hash[MAX_ACTORS];
static uint8_t g_parts_col_epoch[MAX_ACTORS];
static v3 g_parts_col_centres[PARTS_COL_CENTRES];
static uint8_t g_parts_col_next_centre, g_parts_col_used;
static EWRAM_BSS uint32_t g_parts_col_killers;
static bool g_parts_col_complete;
static int parts_collision_kind(const Actor* a){
    int t = a->type;
    if(mountain_part_type(t)) return mountain_part_spec(a) ? PARTS_COL_MOUNTAIN : -1;
    if(t == OBJ_WATERLEAKICE) return a->state != WL_ICE_DEAD ? PARTS_COL_WATERLEAK : -1;
    if(t == OBJ_CRYSTALCAGEM) return PARTS_COL_CAGE;
    if(volcano_part_type(t)){
        const FFM2Model* m = volcano_part_model(a);
        return m && m->collision ? PARTS_COL_VOLCANO : -1;
    }
    if(valve_type(t)) return valve_has_model(a) ? PARTS_COL_VALVE : -1;
    return -1;
}

static uint32_t parts_collision_actor_hash(int i, const Actor* a, int kind){
    uint32_t h = fnv1a_mix_v3(
        fnv1a_mix(2166136261u, (uint32_t)i | (uint32_t)a->type << 8 | (uint32_t)(uint16_t)a->value << 16),
        a->position);
    h = fnv1a_mix_v3(h, a->up);
    h = fnv1a_mix_v3(h, a->front);
    if(kind != PARTS_COL_MOUNTAIN){
        h = fnv1a_mix(h, (uint32_t)a->state | (uint32_t)(a->flags & ACTOR_WAITING) << 8);
        if(kind == PARTS_COL_VOLCANO || kind == PARTS_COL_VALVE){
            h = fnv1a_mix(h, (uint32_t)a->anim_frame);
            h = fnv1a_mix(h, (uint32_t)a->anim);
            h = fnv1a_mix(h, (uint32_t)(uint16_t)a->args[0] | (uint32_t)(uint16_t)a->args[1] << 16);
            h = fnv1a_mix(h, (uint32_t)(uint16_t)a->args[2] | (uint32_t)(uint16_t)a->args[3] << 16);
        }
    }
    return h | 1;
}
static void parts_collision_build_one(int kind, int i, v3 centre){
    Actor* a = &g_game.actors[i];
    switch(kind){
    case PARTS_COL_MOUNTAIN: {
        const FFMountainSpec* s = mountain_part_spec(a);
        int16_t scale[3];
        actor_stage_scale(a, scale);
        dyncol_add_codes(&ffmountain_mesh[s->shape], a->position, a->up, a->front, scale, i, 0, centre,
                         FX(400), ffmountain_codes[s->shape]);
        break;
    }
    case PARTS_COL_WATERLEAK: waterleak_collision_one(i, centre); break;
    case PARTS_COL_CAGE: crystal_cage_medium_collision(a, centre); break;
    case PARTS_COL_VOLCANO: volcano_part_collision_one(i, centre); break;
    default: valve_collision_one(i, centre); break;
    }
}
static inline bool parts_collision_near_centre(v3 mario, v3 c){
    v3 d = v3_sub(mario, c);
    return fx_abs(d.x) <= PARTS_COL_SLACK && fx_abs(d.y) <= PARTS_COL_SLACK && fx_abs(d.z) <= PARTS_COL_SLACK;
}
static void parts_collision_update(void){
    v3 mario = g_game.player.figure.position;
    uint32_t sig = parts_collision_signature(mario);
    bool ours = g_parts_col_generation == g_dyncol_generation;
    if(ours && sig == g_parts_col_signature){
        bool near = true;
        for(int e = 0; e < PARTS_COL_CENTRES; e++)
            if(g_parts_col_used >> e & 1 && !parts_collision_near_centre(mario, g_parts_col_centres[e]))
                near = false;
        if(near){
            icestep_cache_invalidate();
            return;
        }
    }

    int n = g_game.actor_count;
    uint8_t kind[MAX_ACTORS], order[MAX_ACTORS];
    uint32_t hash[MAX_ACTORS];
    int count[PARTS_COL_KINDS + 1] = {0};
    uint32_t killers = 2166136261u;
    for(int i = 0; i < n; i++){
        const Actor* a = &g_game.actors[i];
        int k = a->active ? parts_collision_kind(a) : -1;
        kind[i] = (uint8_t)k;
        hash[i] = 0;
        if(k < 0){
            g_parts_col_hash[i] = 0;
            if(a->active && a->type == OBJ_KILLER){
                killers = fnv1a_mix_v3(fnv1a_mix_v3(fnv1a_mix(killers, (uint32_t)i), a->position), a->up);
                killers = fnv1a_mix(fnv1a_mix_v3(killers, a->front),
                                    (uint32_t)a->state | (uint32_t)(a->flags & ACTOR_WAITING) << 8);
            }
            continue;
        }
        count[k + 1]++;
        fx range = k == PARTS_COL_MOUNTAIN || k == PARTS_COL_WATERLEAK ? PARTS_COL_SIGNATURE_RANGE
                   : k == PARTS_COL_VOLCANO                            ? FX(9000)
                                                                       : 0;
        if(range){
            v3 d = v3_sub(a->position, mario);
            if(fx_abs(d.x) > range || fx_abs(d.y) > range || fx_abs(d.z) > range) continue;
        }
        hash[i] = parts_collision_actor_hash(i, a, k);
    }
    for(int k = 0; k < PARTS_COL_KINDS; k++) count[k + 1] += count[k];
    int members = count[PARTS_COL_KINDS];
    for(int i = 0; i < n; i++)
        if((int8_t)kind[i] >= 0) order[count[kind[i]]++] = (uint8_t)i;
    bool cages_same = killers == g_parts_col_killers;

    unsigned e = g_parts_col_next_centre;
    g_parts_col_next_centre = (uint8_t)((e + 1) % PARTS_COL_CENTRES);
    g_parts_col_centres[e] = mario;
    ours = ours && g_parts_col_complete;
    int old_n = ours ? g_dyncol_group_count : 0;
    uint8_t old_owner[DYNCOL_GROUPS], old_count[DYNCOL_GROUPS];
    uint16_t old_first[DYNCOL_GROUPS];
    for(int g = 0; g < old_n; g++){
        int owner = g_dyncol_groups[g].owner;
        if(owner < 0 || owner >= n){
            ours = false;
            old_n = 0;
            break;
        }
        old_owner[g] = (uint8_t)owner;
        old_first[g] = g_dyncol_groups[g].first;
        old_count[g] = (uint8_t)g_dyncol_groups[g].count;
    }
    dyncol_clear();
    g_dyncol_near_slack = PARTS_COL_SLACK;
    unsigned used = 0;
    for(int j = 0; j < members; j++){
        int i = order[j], k = kind[i];
        uint32_t h = hash[i];
        if(!h){
            g_parts_col_hash[i] = 0;
            icestep_cache_invalidate();
            continue;
        }
        unsigned c = g_parts_col_epoch[i];
        if(ours && h == g_parts_col_hash[i] && c != e && (k != PARTS_COL_CAGE || cages_same) &&
           parts_collision_near_centre(mario, g_parts_col_centres[c])){
            int s = 0;
            while(s < old_n && old_owner[s] != i) s++;
            if(s == old_n){
                used |= 1u << c;
                icestep_cache_invalidate();
                continue;
            }
            if(s == g_dyncol_group_count && old_first[s] == g_dyncol_tri_count){
                int tris = 0;
                for(; s < old_n && old_owner[s] == i; s++){
                    tris += old_count[s];
                    g_dyncol_group_count++;
                }
                g_dyncol_tri_count += tris;
                used |= 1u << c;
                icestep_cache_invalidate();
                continue;
            }
        }
        g_parts_col_hash[i] = h;
        g_parts_col_epoch[i] = (uint8_t)e;
        used |= 1u << e;
        parts_collision_build_one(k, i, mario);
    }
    for(int i = n; i < MAX_ACTORS; i++) g_parts_col_hash[i] = 0;
    g_dyncol_near_slack = 0;
    g_parts_col_complete = !g_dyncol_full;
    g_parts_col_killers = killers;
    g_parts_col_used = (uint8_t)(used | 1u << e);
    g_parts_col_signature = sig;
    g_parts_col_generation = g_dyncol_generation;
}
static void parts_collision_pre(void){
    parts_collision_update();
    mario_ride_find();
}
static void parts_collision_post(void){
    if(g_mario_link.ride >= 0){
        const Actor* a = &g_game.actors[g_mario_link.ride];
        if(mountain_part_type(a->type) && a->type == OBJ_FFMOUNTAINFLOATER){
            v3 delta = v3_sub(a->position, g_mario_link.ride_pos);
            g_game.player.figure.position = v3_add(g_game.player.figure.position, delta);
            g_mario.position16 = v3_add(g_mario.position16, V3(delta.x << 8, delta.y << 8, delta.z << 8));
        } else mario_ride_carry();
    }

    parts_collision_update();
}
