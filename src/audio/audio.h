typedef struct {
    const int8_t* music;
    uint32_t music_length, music_loop, music_position;
    uint16_t effect_pending;
    uint16_t current_effect;
    uint32_t effect_length, effect_position;
    int8_t music_pending;
    uint8_t reserved29;
    uint8_t music_playing, effect_playing;
    int8_t current_music;
    uint8_t reserved33;
    uint8_t effect_priority;
    uint16_t music_dma_position;
    uint8_t music_buffer, music_dirty;
    uint8_t effect_buffer, effect_dirty;
    uint16_t effect_dma_position;
} AudioState;

volatile AudioState g_audio EWRAM_BSS;

static void audio_clear_effect_queue(void);
static void audio_init(void);
static void audio_play_music(int track);
static void audio_stop_music(void);
static void audio_play_sub_music(int track, int lead_in);
static void audio_stop_sub_music(void);
static void audio_play_stage_music(int track);
static void audio_play_effect(int effect, int priority);
static void audio_play_effect_soft(int effect, int priority);
static void audio_queue_effect(int effect, int priority);
static void audio_stop_effects(void);
static void audio_level_effect(int effect);
static void audio_stop_level_effect(int effect);
void audio_vblank(void);
