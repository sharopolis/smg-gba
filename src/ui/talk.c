enum {
    OVL_BALLOON = 0,
    OVL_BEAK = 168,
    OVL_ABTN = 176,
    OVL_SHORT_BEAK = 96,
    OVL_PROMPT = 104,
    OVL_POINTER = 128,
    OVL_HAND = 136,
};

typedef struct {
    int8_t loaded[8];
    uint8_t tiles[8];
    uint8_t palette;
} Overlay;
static const uint16_t ovl_region_tile[OVL_REGION_COUNT] = {
    OVL_BALLOON, OVL_BEAK, OVL_ABTN, OVL_ABTN + 8, OVL_SHORT_BEAK, OVL_PROMPT, OVL_POINTER, OVL_HAND};

static EWRAM_BSS Overlay g_ovl;

static void ovl_reset(void){
    for(int i = 0; i < OVL_REGION_COUNT; i++) g_ovl.loaded[i] = -1;
    g_ovl.palette = 0;
}

static void ovl_check_hud(void){
    if(g_hud.slot_art[SLOT_BANNER] >= 0){
        for(int i = 0; i < OVL_REGION_COUNT; i++) g_ovl.loaded[i] = -1;
    }
}

static void ovl_palette(int which){
    if(g_ovl.palette == which) return;
    const uint16_t* src = which == 1 ? talk_palette : pointer_palette;
    for(int i = 0; i < 16; i++) OBJ_PALETTE[OVL_PAL * 16 + i] = src[i];
    g_ovl.palette = (uint8_t)which;
}

static inline int ovl_blocks_x(const TalkImage* im){ return (im->cols + 3) >> 2; }
static inline int ovl_blocks_y(const TalkImage* im){ return (im->rows + 1) >> 1; }

static inline int ovl_tile_at(const TalkImage* im, int base, int tx, int ty){
    return base + ((ty >> 1) * ovl_blocks_x(im) + (tx >> 2)) * 8 + (ty & 1) * 4 + (tx & 3);
}

static void ovl_upload(int region, int img){
    ovl_check_hud();
    if(g_ovl.loaded[region] == img) return;
    const TalkImage* im = &talk_images[img];
    int base = OVL_TILE0 + ovl_region_tile[region];
    int bx = ovl_blocks_x(im), by = ovl_blocks_y(im);

    int lo = ovl_region_tile[region], hi = lo + bx * by * 8;
    for(int r = 0; r < OVL_REGION_COUNT; r++)
        if(r != region && g_ovl.loaded[r] >= 0 && ovl_region_tile[r] < hi &&
           ovl_region_tile[r] + g_ovl.tiles[r] > lo)
            g_ovl.loaded[r] = -1;
    g_ovl.tiles[region] = (uint8_t)(bx * by * 8);

    for(int i = 0; i < bx * by * 8 * 16; i++) OBJ_TILES[base * 16 + i] = 0;
    const uint16_t* src = (const uint16_t*)(talk_image_data + im->data);
    for(int ty = 0; ty < im->rows; ty++)
        for(int tx = 0; tx < im->cols; tx++){
            volatile uint16_t* dst = OBJ_TILES + ovl_tile_at(im, base, tx, ty) * 16;
            const uint16_t* s = src + (ty * im->cols + tx) * 16;
            for(int k = 0; k < 16; k++) dst[k] = s[k];
        }
    g_ovl.loaded[region] = (int8_t)img;
    g_hud.slot_art[SLOT_BANNER] = -1;
}

static void ovl_draw(int region, int x, int y){
    int img = g_ovl.loaded[region];
    if(img < 0) return;
    const TalkImage* im = &talk_images[img];
    int base = OVL_TILE0 + ovl_region_tile[region];
    int bx = ovl_blocks_x(im), by = ovl_blocks_y(im);
    for(int j = 0; j < by; j++)
        for(int i = 0; i < bx; i++)
            hud_piece(x + i * 32, y + j * 16, base + (j * bx + i) * 8, OVL_PAL, OVL_OBJ_32x16);
}

static void ovl_glyph(int region, int gx, int gy, int glyph, int colour){
    const TalkImage* im = &talk_images[g_ovl.loaded[region]];
    const TalkGlyph* g = &talk_glyphs[glyph];
    const uint8_t* pix = talk_glyph_data + g->data;
    int base = OVL_TILE0 + ovl_region_tile[region];
    for(int y = 0; y < g->h; y++){
        int py = gy + y;
        if(py < 0 || py >= im->rows * 8) continue;
        for(int x = 0; x < g->w; x++){
            int v = pix[y * g->w + x];
            int px = gx + x;
            if(!v || px < 0 || px >= im->cols * 8) continue;
            int c = g->kind ? v : v == 3 ? colour : talk_text_aa[v];
            volatile uint16_t* w =
                OBJ_TILES + ovl_tile_at(im, base, px >> 3, py >> 3) * 16 + (py & 7) * 2 + ((px & 7) >> 2);
            int sh = (px & 3) * 4;
            *w = (uint16_t)((*w & ~(15 << sh)) | c << sh);
        }
    }
}

static bool talk_project(v3 p, int* sx, int* sy){
    const Camera* c = &g_game.camera;
    v3 d = v3_sub(p, c->position);
    v3 left = v3_cross14(c->up, c->front);
    int32_t z = v3_dot14(d, c->front);
    if(z < FX(20)) return false;
    int32_t x = v3_dot14(d, left), y = v3_dot14(d, c->up);

    *sx = 120 - (int)(((int64_t)x * 193) / z);
    *sy = 80 - (int)(((int64_t)y * 193) / z);
    return true;
}

enum { TALK_NONE, TALK_OPEN, TALK_TYPE, TALK_PAGE_END, TALK_CLOSE };

typedef struct {
    uint8_t state;
    int8_t npc;
    int16_t actor;
    int16_t node, msg, page;
    int16_t shown;
    int32_t progress;
    int16_t wait;
    int16_t timer, a_held, cooldown;
    bool a_released, waited;

    int8_t cand_npc;
    int16_t cand_actor;
    int32_t cand_dist;
    bool cand_event;

    int8_t short_npc;
    int16_t short_actor, short_node, short_msg, short_timer, short_shown;
    int32_t short_progress;
    int8_t delight_pending;
    int16_t delight_actor, delight_timer;

    int16_t cam_t;
    v3 mario_centre;
    v3 cam_eye, cam_watch, cam_from_eye, cam_from_watch;

    uint16_t node_of[TALK_NPC_COUNT];
    uint8_t talked[(TALK_NPC_COUNT + 7) / 8];
    uint8_t read[16];
    uint8_t history[(TALK_MESSAGE_COUNT + 7) / 8];
    int8_t npc_of_actor[MAX_ACTORS];
    uint16_t miss_key[MAX_ACTORS];
    int8_t star_appeared;
} TalkState;

static EWRAM_BSS TalkState g_talk;

#define TALK_SPEED_EVENT   124
#define TALK_SPEED_SHORT   284
#define TALK_OPEN_FRAMES   14
#define TALK_CLOSE_FRAMES  20
#define TALK_CAMERA_FRAMES 30

static inline bool talk_bit(const uint8_t* bits, int i){ return bits[i >> 3] >> (i & 7) & 1; }
static inline void talk_set_bit(uint8_t* bits, int i){ bits[i >> 3] |= (uint8_t)(1 << (i & 7)); }

static void talk_reset(void){
    g_talk = (TalkState){0};
    g_talk.npc = g_talk.cand_npc = g_talk.short_npc = g_talk.delight_pending = -1;
    for(int i = 0; i < TALK_NPC_COUNT; i++) g_talk.node_of[i] = talk_npcs[i].root;
    for(int i = 0; i < MAX_ACTORS; i++) g_talk.npc_of_actor[i] = -1;
    ovl_reset();
}

static bool talk_active(void){ return g_talk.state != TALK_NONE; }
static bool talk_actor_normal_talking(const Actor* a){
    return g_talk.state != TALK_NONE && g_talk.actor == (int)(a - g_game.actors);
}

static inline bool talk_npc_at(const TalkNpc* n, const Actor* a){
    return n->stage == level_current()->galaxy_stage && n->kind == a->type &&
           fx_abs(n->pos[0] - a->home.x) <= 2 && fx_abs(n->pos[1] - a->home.y) <= 2 &&
           fx_abs(n->pos[2] - a->home.z) <= 2;
}

static int talk_npc_of(const Actor* a){
    int ai = (int)(a - g_game.actors);
    int k = g_talk.npc_of_actor[ai];
    uint16_t key = (uint16_t)(a->home.x ^ a->home.y ^ a->type << 12);
    if(k >= 0 && talk_npc_at(&talk_npcs[k], a)) return k;
    if(k == -2 && g_talk.miss_key[ai] == key) return -1;
    for(int i = 0; i < TALK_NPC_COUNT; i++)
        if(talk_npc_at(&talk_npcs[i], a)){
            g_talk.npc_of_actor[ai] = (int8_t)i;
            return i;
        }
    g_talk.npc_of_actor[ai] = -2;
    g_talk.miss_key[ai] = key;
    return -1;
}

static const TalkKind talk_kind_none;

static const TalkKind* talk_kind(const Actor* a){
    const TalkKind* t = actor_kinds[a->type].talk;
    return t ? t : &talk_kind_none;
}

static int talk_pose(const Actor* a, int anim){
    const TalkKind* t = talk_kind(a);
    return t->pose ? t->pose(a, anim) : anim;
}

static int talk_anim_talk(const Actor* a){ return talk_pose(a, talk_kind(a)->anim); }

static int talk_anim_reaction(const Actor* a){
    int anim = actor_kinds[a->type].reaction_anim;
    return anim ? anim : -1;
}

static void talk_set_anim(Actor* a, int anim){
    if(!anim) anim = talk_pose(a, 0);
    if(a->anim != anim){
        a->anim = anim;
        a->anim_frame = 0;
    }
}

static bool talk_power_star_appeared(void){
    if(g_talk.star_appeared < 0){
        g_talk.star_appeared = 0;
        for(int i = 0; i < g_game.actor_count; i++){
            const Actor* a = &g_game.actors[i];
            if(a->type == OBJ_POWERSTAR && a->active && (a->flags & 1)) g_talk.star_appeared = 1;
        }
    }
    return g_talk.star_appeared;
}

static int talk_object_switch(const Actor* a, int which){
    if(!a || a->stage_index < 0) return -1;
    return stage_object_switch(&g_game.stage.objects[a->stage_index], which);
}

static int talk_resolve(int npc, const Actor* a){
    int n = g_talk.node_of[npc];
    const TalkKind* kind = talk_kind(a);
    if(kind->node) n = kind->node(a, n);
    for(int guard = 0; guard < 32 && n != TALK_NODE_END; guard++){
        const TalkNode* t = &talk_nodes[n];
        if(t->type == TALK_NODE_MSG) return n;
        if(t->type == TALK_NODE_BRANCH){
            bool c = false;
            switch(t->a){
            case 3: c = switch_on(talk_object_switch(a, SWITCH_A)); break;
            case 4: c = switch_on(talk_object_switch(a, SWITCH_B)); break;
            case 6: c = bee_active(); break;
            case 8: c = talk_power_star_appeared(); break;
            case 9: c = talk_bit(g_talk.talked, npc); break;
            case 12: c = t->b < 128 && talk_bit(g_talk.read, t->b); break;
            }
            n = c ? t->next : t->alt;
        } else {
            n = t->next;
        }
    }
    return TALK_NODE_END;
}

static void talk_forward(int npc, int msg_node, const Actor* a){
    int n = talk_nodes[msg_node].next;
    if(n == TALK_NODE_END){
        g_talk.node_of[npc] = talk_npcs[npc].root;
        return;
    }
    while(n != TALK_NODE_END && talk_nodes[n].type == TALK_NODE_EVENT){
        const TalkNode* t = &talk_nodes[n];
        if(t->a <= 1 && a && talk_kind(a)->event) talk_kind(a)->event((Actor*)a, t->b);
        else if(t->a == 5) switch_set(talk_object_switch(a, SWITCH_A), true);
        else if(t->a == 6) switch_set(talk_object_switch(a, SWITCH_B), true);
        n = t->next;
    }
    g_talk.node_of[npc] = (uint16_t)(n == TALK_NODE_END ? talk_npcs[npc].root : n);
}

static void talk_reset_actor(Actor* a){
    int npc = talk_npc_of(a);
    if(npc < 0) return;
    g_talk.node_of[npc] = talk_npcs[npc].root;
}
static void talk_forward_actor(Actor* a){
    int npc = talk_npc_of(a);
    if(npc < 0) return;
    int node = talk_resolve(npc, a);
    if(node >= 0 && node != TALK_NODE_END) talk_forward(npc, node, a);
    if(g_talk.short_actor == actor_index(a)){
        g_talk.short_actor = -1;
        g_talk.short_npc = -1;
    }
}

static int talk_message_argument(int msg, const Actor* a){
    const TalkKind* kind = talk_kind(a);
    if(!kind->count) return msg;
    int remaining = kind->count(a);
    for(int i = 0; i < TALK_NUMBER_VARIANT_COUNT; i++){
        const TalkNumberVariants* v = &talk_number_variants[i];
        if(v->base == msg) return v->first + fx_min(remaining, v->max);
    }
    return msg;
}

static int talk_type_of(int msg){
    int t = talk_messages[msg].talk_type;
    if(t == 2 && talk_bit(g_talk.history, msg)) t = 0;
    return t;
}

static int32_t talk_near(const TalkNpc* n, const Actor* a, int scale16){
    const Player* p = &g_game.player;
    v3 d = v3_sub(a->position, g_talk.mario_centre);
    int32_t r = (int32_t)n->distance * scale16 >> 4;
    if(fx_abs(d.x) > 2 * r || fx_abs(d.y) > 2 * r || fx_abs(d.z) > 2 * r) return -1;
    int32_t v = v3_dot14(d, p->up);
    if(fx_abs(v) > r) return -1;
    v3 h = v3_sub(d, v3_scale14(p->up, v));
    uint64_t h2 = v3_length_squared64(h);
    if(h2 >= (uint64_t)r * r) return -1;
    if(n->front_only && v3_dot14(v3_neg(d), a->front) <= 0) return -1;
    return (int32_t)(h2 >> 8);
}

static void talk_turn_to_mario(Actor* a){
    unsigned flags = talk_kind(a)->flags;
    if(flags & NPC_NO_TURN) return;
    bool slow = flags & NPC_SLOW_TURN;
    v3 to = v3_sub(g_game.player.figure.position, a->position);
    if(!slow){
        int32_t fwd = v3_dot14(to, a->front), side = v3_dot14(to, v3_cross14(a->up, a->front));
        if(fwd > 0 && fx_abs(side) * 16 < fwd) return;
    }
    v3 want = v3_tangent14(to, a->up, a->front);
    if(v3_dot14(a->front, want) >= (slow ? R14(.9986295f) : R14(0.99027f))){
        a->front = want;
        return;
    }
    fx sign = v3_dot14(v3_cross14(a->front, want), a->up) >= 0 ? 1 : -1;
    fx step = slow ? FXC(.05235988f) : FXC(0.1396f);
    a->front = v3_tangent14(v3_rotate_axis14(a->front, a->up, step * sign), a->up, a->front);
}

static void talk_actor_think(Actor* a){
    Game* game = &g_game;
    int ai = (int)(a - game->actors);
    const TalkKind* kind = talk_kind(a);
    if(kind->busy && kind->busy(a)) return;
    if(g_talk.state != TALK_NONE && g_talk.actor == ai){
        talk_turn_to_mario(a);
        talk_set_anim(a, g_talk.state == TALK_TYPE ? talk_anim_talk(a) : 0);
        return;
    }

    if(game->tick & 1) return;
    bool talking_here = g_talk.short_npc >= 0 && g_talk.short_actor == ai;
    if(talking_here) talk_set_anim(a, talk_anim_talk(a));
    else if(a->anim && a->anim == talk_anim_talk(a)) talk_set_anim(a, 0);
    else if(a->anim && a->anim == talk_anim_reaction(a) &&
            (a->anim_frame >> 8) >= anim_length_of(kind->flags & NPC_KIND_ANIMS
                                                       ? (const FigureHeader*)actor_kinds[a->type].figure
                                                       : a->figure.model,
                                                   a->anim))
        talk_set_anim(a, 0);

    v3 dm = v3_sub(a->position, g_talk.mario_centre);
    fx reach = kind->flags & NPC_FAR_REACH ? FX(550) : FX(430);
    if(fx_abs(dm.x) > reach || fx_abs(dm.y) > reach || fx_abs(dm.z) > reach) return;
    if(game->player.action == MA_DEATH) return;
    int npc = talk_npc_of(a);
    if(npc < 0) return;
    const TalkNpc* n = &talk_npcs[npc];
    int32_t d2 = talk_near(n, a, talking_here ? 19 : 16);
    if(d2 < 0) return;
    talk_turn_to_mario(a);
    int node = talk_resolve(npc, a);
    if(node == TALK_NODE_END) return;
    int msg = talk_message_argument(talk_nodes[node].b, a);
    int type = talk_type_of(msg);
    if(type == 1){
        if(g_talk.short_npc < 0 || talking_here){
            if(!talking_here || g_talk.short_node != node){
                g_talk.short_npc = (int8_t)npc;
                g_talk.short_actor = (int16_t)ai;
                g_talk.short_node = (int16_t)node;
                g_talk.short_msg = (int16_t)msg;
                g_talk.short_timer = 0;
                g_talk.short_shown = 0;
                g_talk.short_progress = 0;
            }
        }
        return;
    }
    if(type == 0 || type == 2){
        if(g_talk.cand_npc < 0 || d2 < g_talk.cand_dist){
            g_talk.cand_npc = (int8_t)npc;
            g_talk.cand_actor = (int16_t)ai;
            g_talk.cand_dist = d2;
            g_talk.cand_event = type == 2;
        }
        game->hint = HINT_TALK;
    }
}

static void talk_page_start(void){
    const TalkMessage* m = &talk_messages[g_talk.msg];

    g_ovl.loaded[OVL_REGION_BALLOON] = -1;
    ovl_upload(OVL_REGION_BALLOON, m->image);
    g_talk.shown = 0;
    g_talk.progress = 0;
    g_talk.wait = 0;
    g_talk.state = TALK_TYPE;
    g_talk.a_held = 0;
}

static bool talk_mario_can_start(const KeyState* keys){
    const Player* p = &g_game.player;
    if(!p->on_ground || (p->action != MA_WAIT && p->action != MA_LAND)) return false;
    if(keys->held & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) return false;
    return !pointer_active() && g_talk.cooldown == 0 && p->action != MA_STAR_LAUNCH;
}

static void talk_start(int npc, int actor){
    Game* game = &g_game;
    Actor* a = &game->actors[actor];
    int node = talk_resolve(npc, a);
    if(node == TALK_NODE_END) return;
    g_talk.npc = (int8_t)npc;
    g_talk.actor = (int16_t)actor;
    g_talk.node = (int16_t)node;
    g_talk.msg = (int16_t)talk_message_argument(talk_nodes[node].b, a);
    g_talk.page = 0;
    g_talk.state = TALK_OPEN;
    g_talk.timer = 0;
    g_talk.a_released = false;
    g_talk.short_npc = -1;

    Player* p = &game->player;
    mario_stop_walk();
    mario_set_action(MA_TALK);
    mario_set_anim(ANIM_MARIO_WAIT);
    p->front = v3_tangent14(v3_sub(a->position, p->figure.position), p->up, p->front);
    if(!(talk_kind(a)->flags & NPC_OWN_CAMERA)) talk_camera_begin();
    audio_play_effect(SFX_MENU_OPEN, 4);
}

static bool talk_force_actor(Actor* a){
    if(talk_active()) return g_talk.actor == (int)(a - g_game.actors);
    int npc = talk_npc_of(a);
    if(npc < 0) return false;
    int node = talk_resolve(npc, a);
    if(node == TALK_NODE_END) return false;
    int msg = talk_message_argument(talk_nodes[node].b, a);

    if(talk_type_of(msg) == 1){
        int ai = actor_index(a);
        if(g_talk.short_npc != npc || g_talk.short_node != node || g_talk.short_actor != ai){
            g_talk.short_npc = npc;
            g_talk.short_actor = ai;
            g_talk.short_node = node;
            g_talk.short_msg = msg;
            g_talk.short_timer = 0;
            g_talk.short_shown = 0;
            g_talk.short_progress = 0;
        }
        talk_turn_to_mario(a);
        return true;
    }
    talk_start(npc, (int)(a - g_game.actors));
    return talk_active();
}

static void talk_end(void){
    Game* game = &g_game;
    int npc = g_talk.npc;
    Actor* a = &game->actors[g_talk.actor];
    int msg = g_talk.msg;

    if(talk_messages[msg].talk_type == 2) talk_set_bit(g_talk.history, msg);
    if(talk_messages[msg].read_id >= 0) talk_set_bit(g_talk.read, talk_messages[msg].read_id);
    talk_set_bit(g_talk.talked, npc);
    int completed_node = g_talk.node;
    talk_set_anim(a, 0);
    g_talk.state = TALK_NONE;
    g_talk.npc = -1;
    g_talk.cooldown = 30;
    if(game->player.action == MA_TALK) mario_set_action(MA_WAIT);
    const TalkKind* kind = talk_kind(a);
    if(!(kind->flags & NPC_KEEPS_NODE)) talk_forward(npc, completed_node, a);
    if(kind->ended) kind->ended(a);
}

static bool talk_tick(const KeyState* keys){
    Game* game = &g_game;
    g_talk.star_appeared = -1;
    g_talk.mario_centre = v3_add(game->player.figure.position, v3_scale14(game->player.up, C_BODY_HEIGHT));
    if(g_talk.cooldown > 0) g_talk.cooldown--;
    int cand = g_talk.cand_npc, cand_actor = g_talk.cand_actor;
    bool cand_event = g_talk.cand_event;
    if(!(game->tick & 1)) g_talk.cand_npc = -1;
    else if(cand >= 0) game->hint = HINT_TALK;
    if(g_talk.delight_pending >= 0 && !pointer_active()){
        g_talk.short_npc = g_talk.delight_pending;
        g_talk.short_actor = g_talk.delight_actor;
        g_talk.short_node = -1;
        g_talk.short_msg = g_talk.delight_timer;
        g_talk.short_timer = 0;
        g_talk.short_shown = 0;
        g_talk.short_progress = 0;
        g_talk.delight_pending = -1;
    }
    if(g_talk.state == TALK_NONE){
        if(g_talk.short_npc >= 0){
            Actor* sa = &game->actors[g_talk.short_actor];
            const TalkKind* kind = talk_kind(sa);
            bool held = kind->held && kind->held(sa);
            if(!sa->active || (!held && talk_near(&talk_npcs[g_talk.short_npc], sa, 19) < 0) ||
               game->player.action == MA_DEATH){
                if(g_talk.short_node >= 0 && !(kind->flags & NPC_SHORT_KEEPS_NODE)){
                    const TalkMessage* m = &talk_messages[g_talk.short_msg];
                    if(m->read_id >= 0) talk_set_bit(g_talk.read, m->read_id);
                    talk_forward(g_talk.short_npc, g_talk.short_node, sa);
                }
                g_talk.short_npc = -1;
            }
        }
        if(g_talk.short_npc >= 0){
            g_talk.short_timer++;
            const TalkMessage* m = &talk_messages[g_talk.short_msg];
            g_talk.short_progress += TALK_SPEED_SHORT;

            if(m->balloon == 2 && g_talk.short_timer >= 150){
                g_talk.short_timer = 0;
                g_talk.short_shown = 0;
                g_talk.short_progress = 0;
            }
            if(g_talk.short_node < 0 && g_talk.short_timer > 150) g_talk.short_npc = -1;
        }
        if(cand < 0 || game->scene != SCENE_PLAY) return false;

        if(game->player.action == MA_FREEZE) return false;
        if(cand_event ? !(game->player.on_ground && g_talk.cooldown == 0 && !pointer_active())
                      : !((keys->pressed & KEY_A) && talk_mario_can_start(keys)))
            return false;
        if(g_talk.short_npc >= 0 && g_talk.short_node >= 0){
            Actor* sa = &game->actors[g_talk.short_actor];
            if(!(talk_kind(sa)->flags & NPC_SHORT_KEEPS_NODE))
                talk_forward(g_talk.short_npc, g_talk.short_node, sa);
            g_talk.short_npc = -1;
        }
        talk_start(cand, cand_actor);
        return g_talk.state != TALK_NONE;
    }

    g_talk.timer++;
    bool a_down = keys->held & KEY_A;
    if(!a_down) g_talk.a_released = true;
    switch(g_talk.state){
    case TALK_OPEN:
        if(g_talk.timer >= TALK_OPEN_FRAMES + 6){
            g_talk.page = 0;
            talk_page_start();
            audio_play_effect(SFX_MENU_MOVE, 2);
        }
        break;
    case TALK_TYPE: {
        const TalkMessage* m = &talk_messages[g_talk.msg];
        const TalkPage* pg = &talk_pages[m->first_page + g_talk.page];
        g_talk.a_held = a_down && g_talk.a_released ? g_talk.a_held + 1 : 0;
        int steps = g_talk.a_held > 10 ? 3 : 1;
        for(int s = 0; s < steps; s++){
            if(g_talk.wait > 0){
                g_talk.wait--;
                continue;
            }
            g_talk.progress += TALK_SPEED_EVENT;
            while(g_talk.shown < pg->count && g_talk.progress >= 256){
                const TalkChar* c = &talk_chars[pg->first + g_talk.shown];
                if(c->wait && !g_talk.waited){
                    g_talk.wait = c->wait;
                    g_talk.waited = true;
                    g_talk.progress = 0;
                    break;
                }
                g_talk.waited = false;
                g_talk.progress -= 256;
                ovl_glyph(OVL_REGION_BALLOON, c->x, c->y, c->glyph, c->colour);
                g_talk.shown++;
            }
        }
        if(g_talk.shown >= pg->count){
            g_talk.state = TALK_PAGE_END;
            g_talk.timer = 0;
            g_talk.a_released = !a_down;
        }
        break;
    }
    case TALK_PAGE_END: {
        const TalkMessage* m = &talk_messages[g_talk.msg];
        if(g_talk.a_released && (keys->pressed & KEY_A)){
            if(g_talk.page + 1 < m->pages){
                g_talk.page++;
                talk_page_start();
                g_talk.a_released = false;
                audio_play_effect(SFX_MENU_MOVE, 2);
            } else {
                g_talk.state = TALK_CLOSE;
                g_talk.timer = 0;
                audio_play_effect(SFX_MENU_SELECT, 2);
            }
        }
        break;
    }
    case TALK_CLOSE:
        if(g_talk.timer >= TALK_CLOSE_FRAMES) talk_end();
        break;
    }

    if(g_talk.state != TALK_NONE){
        Player* p = &game->player;
        p->front =
            v3_tangent14(v3_sub(game->actors[g_talk.actor].position, p->figure.position), p->up, p->front);
    }
    return g_talk.state != TALK_NONE;
}

static void talk_tico_delight(Actor* tico){
    int npc = talk_npc_of(tico);
    talk_set_anim(tico, actor_kinds[tico->type].reaction_anim);
    if(npc < 0) return;
    g_talk.delight_pending = (int8_t)npc;
    g_talk.delight_actor = (int16_t)(tico - g_game.actors);
    g_talk.delight_timer = (int16_t)(TALK_MSG_TICO_DELIGHT + (int)(g_game.tick % 5));
}

static void talk_camera_pose(v3* eye, v3* watch){
    const Game* game = &g_game;
    const Player* p = &game->player;
    const Actor* a = &game->actors[g_talk.actor];
    const TalkNpc* n = &talk_npcs[g_talk.npc];
    v3 up = p->up;
    v3 mario = p->figure.position, npc = a->position;

    v3 d = v3_sub(npc, mario);
    int32_t dist = v3_length_exact(d);
    int32_t h = v3_dot14(d, up);
    int32_t scale = dist > 0 ? (int32_t)(((int64_t)(h + FX(n->height)) << 8) / dist * 4 / 3) : 256;
    if(scale < 256) scale = 256;
    int32_t cam_dist = (int32_t)(((int64_t)dist * 308 >> 8) * scale >> 8) * 2;
    if(cam_dist < FX(450)) cam_dist = FX(450);
    int32_t cam_h = h / 6 + FX(n->height);

    v3 mid = v3_add(mario, V3(d.x / 2, d.y / 2, d.z / 2));
    v3 front = v3_tangent14(v3_neg(d), up, p->front);
    v3 side = v3_cross14(up, front);
    v3 from = v3_sub(game->camera.position, mario);
    int32_t orient = v3_dot14(v3_cross14(v3_direction14(from, front), front), up);
    fx yaw;
    if(v3_dot14(from, front) >= 0) yaw = orient >= 0 ? -FXC(0.7f) : FXC(0.7f);
    else yaw = orient >= 0 ? -FXC(1.5708f) : FXC(1.5708f);

    v2 sy = fx_sin_cos(yaw), sx = fx_sin_cos(FXC(0.12f));
    v3 dir = v3_add(v3_scale14(side, sy.x << 6), v3_scale14(front, sy.y << 6));
    dir = v3_add(v3_scale14(dir, sx.y << 6), v3_scale14(up, sx.x << 6));
    *watch = v3_add(mid, v3_scale14(up, cam_h));
    *eye = v3_add(*watch, v3_scale14(dir, cam_dist));
}

static void talk_camera_begin(void){
    g_talk.cam_t = 0;
    g_talk.cam_from_eye = g_game.camera.position;
    g_talk.cam_from_watch = g_game.camera.target;
}

static void talk_camera(void){
    if(g_talk.state == TALK_NONE ||
       (g_talk.actor >= 0 && (talk_kind(&g_game.actors[g_talk.actor])->flags & NPC_OWN_CAMERA)))
        return;
    Camera* cam = &g_game.camera;
    v3 eye, watch;
    talk_camera_pose(&eye, &watch);
    if(g_talk.cam_t < TALK_CAMERA_FRAMES) g_talk.cam_t++;

    int32_t r = (g_talk.cam_t << 14) / TALK_CAMERA_FRAMES;
    r = 16384 - mul14(16384 - r, 16384 - r);
    eye = v3_add(g_talk.cam_from_eye, v3_scale14(v3_sub(eye, g_talk.cam_from_eye), r));
    watch = v3_add(g_talk.cam_from_watch, v3_scale14(v3_sub(watch, g_talk.cam_from_watch), r));
    cam->position = eye;
    cam->target = watch;
    cam->front = v3_direction14(v3_sub(watch, eye), cam->front);
    cam->up = v3_tangent14(g_game.player.up, cam->front, cam->up);
    cam->heading = v3_tangent14(cam->front, cam->smooth_up, cam->heading);
    camera_update_angles(cam);

    g_gcam.out_eye = eye;
    g_gcam.out_watch = watch;
    g_gcam.out_up = cam->up;
    g_gcam.out_valid = true;
}

static void talk_draw_conversation(void){
    const TalkMessage* m = &talk_messages[g_talk.msg];
    if(g_talk.state == TALK_OPEN && g_talk.timer < 6) return;
    ovl_palette(1);

    int dy = 0;
    if(g_talk.state == TALK_OPEN){
        int t = g_talk.timer - 6;
        dy = (30 - 30 * t / TALK_OPEN_FRAMES) * 160 / 456;
        if(dy < 0) dy = 0;
        ovl_upload(OVL_REGION_BALLOON, m->image);
    } else if(g_talk.state == TALK_CLOSE){
        dy = -(40 * g_talk.timer / TALK_CLOSE_FRAMES) * 160 / 456;
    }
    int x = TALK_EVENT_X, y = TALK_EVENT_Y - dy;
    if(g_talk.state == TALK_CLOSE && (g_talk.timer & 1) && g_talk.timer > 10) return;

    if(g_talk.state == TALK_PAGE_END){
        int f = (g_talk.timer / 20) & 1;
        ovl_upload(OVL_REGION_ABTN0 + f, TALK_IMG_ABUTTON0 + f);
        ovl_draw(OVL_REGION_ABTN0 + f, x + TALK_EVENT_ABTN_X - talk_abtn_centre[f][0],
                 y + TALK_EVENT_ABTN_Y - talk_abtn_centre[f][1]);
    }

    int sx, sy;
    const Actor* a = &g_game.actors[g_talk.actor];
    if(talk_project(a->position, &sx, &sy) && sx >= 0 && sx < 240 && sy >= 42 && sy < 160){
        int f = sx < 80 ? 0 : sx < 160 ? 1 : 2;
        ovl_upload(OVL_REGION_BEAK, TALK_IMG_EVENT_BEAK0 + f);
        ovl_draw(OVL_REGION_BEAK, x + talk_images[TALK_IMG_EVENT_BALLOON].w / 2 - talk_beak_pivot[f][0],
                 y + TALK_EVENT_BEAK_Y - talk_beak_pivot[f][1]);
    }
    ovl_draw(OVL_REGION_BALLOON, x, y);
}

static void talk_draw_short(void){
    if(g_talk.short_npc < 0 || pointer_active()) return;
    const TalkMessage* m = &talk_messages[g_talk.short_msg];
    if(m->balloon == 2 && g_talk.short_timer >= 120) return;
    const Actor* a = &g_game.actors[g_talk.short_actor];
    const TalkNpc* n = &talk_npcs[g_talk.short_npc];
    int sx, sy;
    v3 head = v3_add(a->position, v3_scale14(a->up, FX(n->height)));
    if(!talk_project(head, &sx, &sy)) return;
    ovl_palette(1);
    if(g_ovl.loaded[OVL_REGION_BALLOON] != m->image){
        ovl_upload(OVL_REGION_BALLOON, m->image);
        g_talk.short_shown = 0;
    }

    const TalkPage* pg = &talk_pages[m->first_page];
    while(g_talk.short_shown < pg->count && g_talk.short_progress >= 256){
        const TalkChar* c = &talk_chars[pg->first + g_talk.short_shown];
        ovl_glyph(OVL_REGION_BALLOON, c->x, c->y, c->glyph, c->colour);
        g_talk.short_shown++;
        g_talk.short_progress -= 256;
    }
    const TalkImage* im = &talk_images[m->image];
    int x = sx - m->anchor_x, y = sy - m->anchor_y - 4;
    if(y < 0) y = 0;
    ovl_upload(OVL_REGION_SHORT_BEAK, TALK_IMG_SHORT_BEAK);
    ovl_draw(OVL_REGION_SHORT_BEAK, x + im->w / 2 - talk_beak_pivot[3][0],
             y + im->h - 3 - talk_beak_pivot[3][1]);
    ovl_draw(OVL_REGION_BALLOON, x, y);
}

static void talk_draw(void){
    ovl_check_hud();
    if(g_talk.state != TALK_NONE){
        talk_draw_conversation();
        return;
    }
    talk_draw_short();

    const Player* p = &g_game.player;
    if(g_game.hint == HINT_TALK && !pointer_active() && p->on_ground && p->action == MA_WAIT &&
       g_talk.cooldown == 0 && !(g_game.keys.held & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT))){
        int sx, sy;
        if(talk_project(v3_add(p->figure.position, v3_scale14(p->up, FX(200))), &sx, &sy)){
            ovl_palette(1);
            ovl_upload(OVL_REGION_PROMPT, TALK_IMG_PROMPT);
            ovl_draw(OVL_REGION_PROMPT, sx - TALK_PROMPT_CX, sy - TALK_PROMPT_CY - 8);
        }
    }
}
