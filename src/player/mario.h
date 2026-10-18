typedef enum {
    MA_WAIT,
    MA_SKID,
    MA_TURN_JUMP,
    MA_JUMP,
    MA_JUMP2,
    MA_JUMP3,
    MA_BACKFLIP,
    MA_LONG_JUMP,
    MA_FALL,
    MA_SPIN,
    MA_SQUAT,
    MA_SQUAT_SLIDE,
    MA_HIPDROP_START,
    MA_HIPDROP,
    MA_HIPDROP_LAND,
    MA_WALL_SLIDE,
    MA_WALL_JUMP,
    MA_HANG,
    MA_CLIMB_UP,
    MA_DAMAGE,
    MA_DAMAGE_FALL,
    MA_LAND,
    MA_STAR_LAUNCH,
    MA_STAR_GET,
    MA_DEATH,
    MA_TALK,
    MA_SWIM,
    MA_SLIDE,
    MA_WALL_PUSH,
    MA_SKATE,
    MA_FIRE_DANCE,
    MA_FIRE_RUN,
    MA_FREEZE,
    MA_LAUNCHER_BIND,
    MA_POLE_BIND,
    MA_COUNT
} MarioAction;

enum { WATER_DRY, WATER_SUBMERGED, WATER_SURFACE };

typedef struct {
    Figure figure;

    v3 velocity;
    v3 up;
    v3 gravity_up;
    v3 front;
    fx forward_speed;

    MarioAction action;
    int action_timer;
    int jump_chain;
    int jump_chain_timer;
    int spin_air_count;
    int invincible_timer;
    int coyote_timer;
    int health;
    bool a_held_since_jump;

    bool on_ground, on_wall, was_on_ground;
    v3 ground_normal;
    uint16_t ground_attr;
    uint8_t ground_code, wall_code;
    v3 wall_normal;
    fx height_above_ground;
    bool has_floor_below;
    v3 floor_point;

    v3 hang_point, hang_normal;

    int anim;
    fx anim_frame;
    fx anim_speed;
    int anim_blend_ticks;
    int anim_blend_total;
    int anim_group;
    int anim2;
    fx anim2_frame;
    int anim2_weight;

    int16_t pose[160][3];
    int16_t pose_from[160][3];
    bool pose_valid;
    bool hidden;
    int water_state;
} Player;

#define G(x)                 ((int32_t)((x) * 256.0f))
#define R14(x)               ((int32_t)((x) * 16384.0f))
#define C_MAX_DROP_SPEED     G(35.0f)
#define C_GRAVITY_RATIO_A    R14(0.5f)
#define C_SPIN_TIME          24
#define C_BODY_RADIUS        FX(60)
#define C_BODY_HEIGHT        FX(70)
#define C_BRAKE_FIRST_TIMER  15
#define C_BRAKE_SECOND_TIMER 33
#define C_START_SPIN_ANIME   R14(1.25f)
#define C_WEIGHT_BLEND_RATIO R14(0.2f)
#define FX(x)                ((fx)(x))

enum {
    JUMP_FIRST,
    JUMP_SECOND,
    JUMP_TRIPLE,
    JUMP_BELLY,
    JUMP_TURN,
    JUMP_LONG,
    JUMP_BACK,
    JUMP_WALL,
    JUMP_SPIN,
    JUMP_FORCED = 12,
    JUMP_DAMAGE = 15
};

typedef struct {
    v3 position16;
    int32_t ratio;
    int slow_start;
    int turn_frames;
    int frames_since_landing;
    int jump_type;
    bool chainable;
    bool hold;
    bool digital;
    bool spin_used;
    bool jumping;
    bool squat;
    bool skidding;
    int skid_timer;
    int walk_index;
    int prev_walk_index;
    int32_t start_spin;
    int32_t run_weight;
    int turn_ready;
    bool turn_armed;
    v3 turn_front;
    v3 last_dir;
    v3 prev_dir;
    int32_t prev_stick;
    bool turning;
    int turn_sign;
    bool hold_still;
    int stand_turn;
    fx turn_yaw;
    int step_ticks;
    bool brake_pose;
    bool run_end;
    int brake_first;
    int brake_second;
    int idle_ticks;
    int idle_stage;
    int32_t wall_ny;
    int air_frames;
    int back_frames;
    int no_control_timer;
    int spin_timer;
    int ground_spin_timer;
    int spin_cooldown;
    int jump_buffer;
    int hipdrop_timer;
    int wall_timer;
    v3 jump_vec;
    v3 move_dir;
    int32_t stick;
    int stick_keys;
    bool stick_changed;
    int void_ticks;
    int land_anim_timer;
    int attack_timer;
    int stomp_count;
    v3 last_safe_position;
    v3 last_safe_up;

    int wall_side_timer;
    int32_t wall_drop;
    bool wall_cancel;
    v3 wall_cancel_normal;
    v3 wall_in_dir;
    bool wall_free;
    int a_age;
    v3 push_after;
    v3 air_vec;
    v3 hipdrop_last;

    int pull_hold;
    int pull_ticks, pull_total;
    v3 pull_target, pull_axis, pull_centre, pull_arm;
    fx pull_angle;
    v3 safe_point, safe_up;
    bool pulling;
    fx pull_radius;
    uint16_t dash, dash_chance;
} MarioPhysics;

typedef struct {
    bool on;
    v3 base, axis;
    fx radius;
} MarioTower;

enum { ANIM_GROUP_SINGLE, ANIM_GROUP_BASIC, ANIM_GROUP_STEP, ANIM_GROUP_SWIM };

static const int32_t jump_gravity[16];
static EWRAM_BSS MarioPhysics g_mario;
static int g_mario_cling;
static bool g_mario_held;
static EWRAM_BSS MarioTower g_mario_tower;
static int g_mario_star_power;

static fx fx_atan2_fast(int32_t y, int32_t x);
static fx angle_between14(v3 a, v3 b);
static inline int32_t vertical_speed(v3 v, v3 up);
static inline v3 horizontal(v3 v, v3 up);
static v3 v3_set_length(v3 v, int32_t length, v3 fallback);
static void mario_start_anim(int group, int anim, int anim2, int interpolation);
static void mario_set_anim(int anim);
static void mario_restart_anim(int anim);
static void mario_set_group(int group, int anim, int anim2, int weight2, int interpolation);
static bool mario_anim_done(void);
static void mario_set_action(MarioAction a);
static void mario_stop_walk(void);
static void mario_take_off(int type, v3 jump_vec, bool digital);
static void mario_jump(void);
static void mario_damage(v3 from, int amount);
static void mario_stomp_bounce(bool a_held);
static void mario_spin_hop(void);
static void mario_nudge(v3 d);
static void mario_hold_begin(void);
static void mario_hold_move(v3 position, v3 front);
static void mario_hold_release(v3 velocity);
static void mario_update_gravity(void);
static void mario_read_stick(const KeyState* keys);
static fx signed_angle14(v3 from, v3 to, v3 up);
static void mario_ground_move(const KeyState* keys);
static void mario_move_and_collide(v3 displacement, bool want_ground);
static void mario_save_safe_point(const RayHit* hit);
static void mario_pull_back_start(void);
static void mario_tick(const KeyState* keys);
static void mario_reset(v3 position, v3 up, v3 front);
