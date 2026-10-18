volatile KeyState g_keys;
volatile uint32_t g_input_latch_enabled;

static inline uint16_t input_read_keys(void){ return ~REG_KEYINPUT & KEY_MASK; }

static void input_finish_frame(KeyState* k, uint16_t now, fx dt){
    k->pending |= now;
    if(k->held & KEY_L){
        k->l_hold_time = (k->l_was_held ? k->l_hold_time : 0) + dt;
        k->l_was_held = true;
    } else {
        k->l_was_held = false;
    }
    k->pressed = k->pending & ~k->held;
    k->held = k->pending;
    k->pending = 0;
}
