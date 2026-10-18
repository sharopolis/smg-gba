#define SWITCHES_ON_MAX 96

static EWRAM_BSS int16_t g_switches_on[SWITCHES_ON_MAX];
static EWRAM_BSS int g_switch_count;
static EWRAM_BSS uint32_t g_switch_generation;

static void switches_reset(void){
    g_switch_count = 0;
    g_switch_generation++;
}
static uint32_t switch_generation(void){ return g_switch_generation; }

static bool switch_on(int key){
    if(key < 0) return false;
    for(int i = 0; i < g_switch_count; i++)
        if(g_switches_on[i] == key) return true;
    return false;
}

static void switch_set(int key, bool on){
    if(key < 0) return;
    for(int i = 0; i < g_switch_count; i++){
        if(g_switches_on[i] != key) continue;
        if(!on){
            g_switches_on[i] = g_switches_on[--g_switch_count];
            g_switch_generation++;
        }
        return;
    }
    if(on && g_switch_count < SWITCHES_ON_MAX){
        g_switches_on[g_switch_count++] = (int16_t)key;
        g_switch_generation++;
    }
}

static inline int stage_object_switch(const StageObject* o, int which){ return o->sw[which]; }
