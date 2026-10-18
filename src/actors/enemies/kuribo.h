enum {
    KURIBO_WAIT = 64,
    KURIBO_WALK,
    KURIBO_SLEEP,
    KURIBO_FIND,
    KURIBO_JUMPSTART,
    KURIBO_JUMP,
    KURIBO_LAND,
    KURIBO_CHASE,
    KURIBO_CHASE_END,
    KURIBO_HIT,
    KURIBO_STAGGER,
    KURIBO_STAGGER_END,
    KURIBO_FLAT,
    KURIBO_HIPDROP,
    KURIBO_BLOWDOWN
};

typedef struct {
    int16_t wait, walk, run, turn, jumpstart, jump, land, hit, flat, hipdrop, blow, stagger_start, stagger,
        stagger_end;
    int32_t wander, chase;
    bool mini;
} KuriboKind;

static const KuriboKind kuribo_kind;
static const KuriboKind kuribomini_kind;

static void kuribo_blow_down(Actor* a, const KuriboKind* k, int32_t h, int32_t v, bool bits);
static const KuriboKind* kuribo_kind_of(const Actor* a);
static void kuribo_init(Actor* a, const StageObject* o);
static void kuribo_blown_away(Actor* a, Actor* by);
static void kuribo_think(Actor* a);
