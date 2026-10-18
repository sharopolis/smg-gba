typedef struct {
    int16_t ride, hipdrop, bind;
    int talk_timer;
    v3 ride_pos, ride_up, ride_front;
} MarioLink;

static EWRAM_BSS MarioLink g_mario_link;

static void mario_link_reset(void);
static void mario_talk_pause(int ticks);
static bool mario_talk_paused(void);
static void mario_bind_start(Actor* a);
static void mario_bind_pose(v3 pos, v3 up, v3 front);
static void mario_bind_release(v3 v, int type, int anim);
static void mario_ride_find(void);
static void mario_ride_carry(void);
static int mario_ride_actor(void);
static int mario_hipdrop_actor(void);
