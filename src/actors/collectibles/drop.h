#define DROP_NO_FALL 1

__attribute__((target("arm"), always_inline)) static inline v3 collectible_closest_point(v3 a, v3 b, v3 c,
                                                                                         v3 projected);
__attribute__((target("arm"), noinline)) static bool
collectible_sphere_triangle(int tri, v3 centre, fx radius, SphereContact* out);
static bool collectible_is_pop(const Actor* a);
static bool collectible_is_drop(const Actor* a);
static bool collectible_drop_ready(const Actor* a);
__attribute__((noinline)) static void collectible_drop_tick(Actor* a);
static void drop_coin_figure(const Actor* a, Figure* f);
