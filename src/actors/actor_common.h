#define V16(x) ((int32_t)((x) * 256.0f))
#define DEG(x) FXC((x) * 0.01745329f)

enum {
    ACTOR_SAW_MARIO = 0x08,
    ACTOR_ON_GROUND = 0x20,
    ACTOR_HIDDEN = 0x40,
    ACTOR_WAITING = 0x80,
};

#define OBJECT_ARG(o, n) ((n) < (int)(sizeof((o)->args) / sizeof((o)->args[0])) ? (int)(o)->args[n] : -1)

#define OBJECT_SCALE_PERCENT(o) ((o)->scale[1] * 100 / 256)

enum { CONTACT_NONE, CONTACT_TOUCH, CONTACT_TRAMPLE, CONTACT_HIPDROP, CONTACT_SPIN, CONTACT_STAR };
enum { ACTOR_STATE_WAIT = 64, ACTOR_STATE_TAKEN, ACTOR_STATE_TALK, ACTOR_STATE_DONE };

static int g_mario_max_health;
static EWRAM_BSS uint8_t g_power_star_revealed[(STAGE_MAX_OBJECTS + 7) / 8];

static Actor* actor_spawn(int type, v3 position, v3 up, v3 front);
static void actor_set_state_anim(Actor* a, int state, int anim);
static void actor_anim_hold(Actor* a);
static void actor_star_piece_reaction(Actor* a);
static inline v3 actor_centre(const Actor* a);
static inline v3 mario_centre(void);
static bool actor_touches_mario(const Actor* a, fx extra);
static bool actor_stick_to_ground(Actor* a);
static void actor_defeat(Actor* a, bool drop_coin);
static inline int actor_index(const Actor* a);
static inline bool mario_spin_reaches(const Actor* a);
static inline bool actor_slot_reusable(const Actor* a);
static inline void collectible_dead(Actor* a);
static inline Player* game_player(void);
static inline int actor_arg_or(const Actor* a, int n, int fallback);
static inline const StageObject* actor_stage_object(const Actor* a);
static int actor_object_arg(const Actor* a, int n);
static int actor_rail(const Actor* a);
static inline int actor_switch_key(const Actor* a, int which);
static inline bool actor_switch_on(const Actor* a, int which);
static inline bool actor_has_switch(const Actor* a, int which);
static inline bool rail_closed(int rail);
static fx rock_rail_drop(int rail, fx coord, fx none);
static void actor_set_state(Actor* a, int state, int anim);
static inline bool actor_anim_done(const Actor* a);
static inline v3 actor_to_mario(const Actor* a);
static bool actor_mario_within(const Actor* a, fx r);
static int32_t actor_mario_distance(const Actor* a);
static bool actor_sees_mario(Actor* a, fx range, fx half_h, fx half_v);
static fx actor_turn_toward(Actor* a, v3 dir, fx step);
static void actor_follow_gravity(Actor* a, int32_t rate14);
static void actor_move(Actor* a, int32_t gravity, int32_t ground_friction14, int32_t air_friction14);
static bool actor_floor_ahead(const Actor* a, fx ahead);
static inline void actor_push_forward(Actor* a, int32_t accel);
static int actor_mario_contact(const Actor* a, fx spin_extra);
static void actor_block_mario(const Actor* a, fx radius, fx height);
static Actor* actor_spawn_reuse(int type, v3 position, v3 up, v3 front);
static bool collectible_fall_enabled(int count);
static void collectibles_pop_spread(int type, v3 at, v3 up, int count, int spread, int speed);
static void collectibles_pop(int type, v3 at, v3 up, int count);
static void actor_set_switch_dead(const Actor* a, fx radius);
static void actor_set_switch_a(const Actor* a, fx radius, int type);
static bool power_star_revealed(int stage_index);
static void power_star_reveal(Actor* b);
static void power_stars_reveal_near(const Actor* a, fx radius);
static void actor_hurt_mario(const Actor* a, int amount);
static Actor* actor_overlapping_kind(const Actor* self, v3 centre, fx radius, const uint16_t* kinds, int n);
static bool actor_bounce_wall(Actor* a, fx ahead, int32_t keep14);
static inline bool spin_pressed(void);
static fx rail_nearest_coord(int rail, v3 p, fx step);
static v3 rail_tangent(int rail, fx c, v3 fallback);
static v3 v3_turn_toward(v3 front, v3 want, v3 up, fx step);
static inline v3 gravity_up_at(v3 p, v3 fallback);
static inline void actor_stage_scale(const Actor* a, int16_t s[3]);
static inline int16_t actor_stage_arg(const Actor* a, int k);
static void actor_set_anim(Actor* a, int clip);
static void actor_restart_anim(Actor* a, int clip);
static bool actor_anim_frame_done(const Actor* a, int fallback);
static void actor_rotate_yaw(Actor* a, fx angle);
static void actor_show_model(Actor* a, bool on);
static Actor* actor_active_at(int slot, int type);
