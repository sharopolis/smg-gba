static DYNCOL_BUILD_ATTR bool DYNCOL_BUILD_NAME(const HoneyColMesh* m, v3 pos, v3 up, v3 front,
                                                const int16_t scale[3], int owner, uint16_t extra_attr,
                                                v3 centre, fx near, const uint16_t* codes
#ifdef DYNCOL_BUILD_PRECOMPUTED
                                                , const int16_t (*normals)[3]
#endif
){
    {
        int smax = 0;
        for(int k = 0; k < 3; k++){
            int z = scale[k] < 0 ? -scale[k] : scale[k];
            if(z > smax) smax = z;
        }
        if(dyncol_mesh_beyond(m, pos, smax, centre, near)){
            icestep_cache_invalidate();
            return true;
        }
    }
    v3 side = v3_cross14(up, front);
    int s = g_collision.shift;
    near += g_dyncol_near_slack;

    v3 delta = v3_sub(centre, pos);
    v3 axes[3] = {side, up, front};
    fx local[3], extent[3];
    for(int k = 0; k < 3; k++){
        int z = scale[k] ? scale[k] : 256;
        fx projected = v3_dot14(delta, axes[k]);
        fx r =
            (fx)(((int64_t)near * (fx_abs(axes[k].x) + fx_abs(axes[k].y) + fx_abs(axes[k].z)) + 16383) >> 14);
        fx expanded = r + 4;

        local[k] = z == 256                        ? projected
                   : fx_abs(projected) < (1 << 22) ? (fx)((projected * 256) / z)
                                                   : (fx)(((int64_t)projected * 256) / z);
        extent[k] = z == 256 && expanded >= 0 ? expanded + 4
                    : expanded >= 0 && expanded < (1 << 22)
                        ? (fx)((expanded * 256 + fx_abs(z) - 1) / fx_abs(z)) + 4
                        : (fx)(((int64_t)expanded * 256 + fx_abs(z) - 1) / fx_abs(z)) + 4;
    }
    icestep_cache_invalidate();
    DynColVertexCache* cache = geometry_figure_scratch();
    bool cache_ready = false;
    uint32_t key[2] = {0x165667B1u, 0x61C88647u};
    {
        const uint32_t words[] = {(uint32_t)(uintptr_t)m,
                                  (uint32_t)(uintptr_t)codes,
#ifdef DYNCOL_BUILD_PRECOMPUTED
                                  (uint32_t)(uintptr_t)normals,
#elif defined(DYNCOL_BUILD_BEE_RECIP)
                                  1u,
#else
                                  0u,
#endif
                                  (uint32_t)pos.x,
                                  (uint32_t)pos.y,
                                  (uint32_t)pos.z,
                                  (uint32_t)up.x,
                                  (uint32_t)up.y,
                                  (uint32_t)up.z,
                                  (uint32_t)front.x,
                                  (uint32_t)front.y,
                                  (uint32_t)front.z,
                                  (uint16_t)scale[0] | (uint32_t)(uint16_t)scale[1] << 16,
                                  (uint16_t)scale[2] | (uint32_t)extra_attr << 16,
                                  (uint32_t)s};
        for(unsigned i = 0; i < sizeof words / sizeof words[0]; i++) dyncol_hash_word(key, words[i]);
    }
#ifdef DYNCOL_BUILD_BEE_RECIP
    DynColReciprocalCache* reciprocal_cache = (DynColReciprocalCache*)(cache + 58);
#define DYNCOL_BUILD_IS_BEE true
#else
#define DYNCOL_BUILD_IS_BEE false
#endif
    int deferred = -1;
    (void)deferred;
    for(int g = 0; g < m->group_count; g++){
        const HoneyColGroup* hg = &m->groups[g];
        bool outside = false;
        for(int k = 0; k < 3; k++)
            if(hg->min[k] > local[k] + extent[k] || hg->max[k] < local[k] - extent[k]) outside = true;
        if(outside) continue;
        uint32_t gk[2] = {key[0], key[1]};
        dyncol_hash_word(gk, (uint32_t)g);
        dyncol_hash_final(gk);

        int next = g_dyncol_group_count;
        bool known = next < DYNCOL_GROUPS && g_dyncol_key[next][0] == gk[0] && g_dyncol_key[next][1] == gk[1];
        v3 lo = pos, hi = pos;
        if(known){
            lo = v3_add(g_dyncol_groups[next].lo, V3(2, 2, 2));
            hi = v3_sub(g_dyncol_groups[next].hi, V3(2, 2, 2));
        } else {
            v3 endpoints[3][2];
            for(int k = 0; k < 3; k++){
                endpoints[k][0] = v3_scale14(axes[k], (hg->min[k] * scale[k]) >> 8);
                endpoints[k][1] = v3_scale14(axes[k], (hg->max[k] * scale[k]) >> 8);
            }
            for(int k = 0; k < 3; k++){
                const fx* a = &endpoints[k][0].x;
                const fx* b = &endpoints[k][1].x;
                for(int axis = 0; axis < 3; axis++){
                    (&lo.x)[axis] += fx_min(a[axis], b[axis]);
                    (&hi.x)[axis] += fx_max(a[axis], b[axis]);
                }
            }
        }
        if(lo.x > centre.x + near || hi.x < centre.x - near || lo.y > centre.y + near ||
           hi.y < centre.y - near || lo.z > centre.z + near || hi.z < centre.z - near)
            continue;
        if(g_dyncol_group_count >= DYNCOL_GROUPS || g_dyncol_tri_count + hg->count > DYNCOL_MAX){
            g_dyncol_full = true;
            return false;
        }
        int slot = g_dyncol_group_count++;
        DynColGroup* d = &g_dyncol_groups[slot];
        d->lo = v3_sub(lo, V3(2, 2, 2));
        d->hi = v3_add(hi, V3(2, 2, 2));
        d->first = (uint16_t)g_dyncol_tri_count;
        d->count = 0;
        d->owner = (int16_t)owner;
        if(known && g_dyncol_key_first[slot] == d->first){
            d->count = g_dyncol_key_count[slot];
            g_dyncol_tri_count += d->count;
            if(g_dyncol_pending[slot]){
                int r = -1;
#ifndef DYNCOL_BUILD_PRECOMPUTED
                if(g_dyncol_defer_on)
                    r = dyncol_defer_inputs(&deferred, m, codes, DYNCOL_BUILD_IS_BEE, pos, side, up, front,
                                            scale, extra_attr);
#endif
                if(r >= 0) g_dyncol_pending[slot] = (uint8_t)(r + 1);
                else dyncol_build_deferred(slot);
            }
            dyncol_mark_part(slot);
            continue;
        }
        if(!cache_ready){
            for(int i = 0; i < 64; i++) cache[i].vertex = 0xFFFFFFFFu;
#ifdef DYNCOL_BUILD_BEE_RECIP
            for(int i = 0; i < 8; i++) reciprocal_cache[i].squared = 0;
#endif
            cache_ready = true;
        }
        {
            DynColTriangleJob job = {m,
                                     hg,
                                     cache,
                                     pos,
                                     side,
                                     up,
                                     front,
                                     scale,
                                     s,
                                     extra_attr,
                                     codes,
#ifdef DYNCOL_BUILD_PRECOMPUTED
                                     normals,
#else
                                     NULL,
#endif
#ifdef DYNCOL_BUILD_BEE_RECIP
                                     reciprocal_cache,
#else
                                     NULL,
#endif
                                     &g_dyncol_tris[d->first]};
            int r = -1;
#ifndef DYNCOL_BUILD_PRECOMPUTED
            if(g_dyncol_defer_on)
                r = dyncol_defer_inputs(&deferred, m, codes, DYNCOL_BUILD_IS_BEE, pos, side, up, front, scale,
                                        extra_attr);
#endif
            if(r >= 0){
                d->count = (uint16_t)dyncol_count_triangles(&job);
                g_dyncol_pending[slot] = (uint8_t)(r + 1);
                g_dyncol_pending_group[slot] = (uint8_t)g;
            } else {
                d->count = (uint16_t)dyncol_build_triangles(&job);
                g_dyncol_pending[slot] = 0;
            }
            g_dyncol_tri_count += d->count;
            dyncol_mark_part(slot);
        }
        g_dyncol_key[slot][0] = gk[0];
        g_dyncol_key[slot][1] = gk[1];
        g_dyncol_key_first[slot] = d->first;
        g_dyncol_key_count[slot] = d->count;
    }
    return true;
}
#undef DYNCOL_BUILD_IS_BEE
