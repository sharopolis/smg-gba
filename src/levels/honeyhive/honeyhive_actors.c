static void honey_pickup_think(Actor* a){
    Player* p = &g_game.player;
    if(p->action == MA_DEATH || !actor_touches_mario(a, 0)) return;
    a->active = 0;
    switch(a->type){
    case OBJ_QUESTIONCOIN: audio_play_effect(SFX_QUESTION_COIN, 3); break;
    case OBJ_YELLOWCHIP: chip_play_sound(false, ++g_honey.chips[a->args[0] & 3]); break;
    case OBJ_LIFEUP: mario_lifeup_pickup(); break;
    default: audio_play_effect(SFX_COIN, 2); break;
    }
}

static void honey_item_think(Actor* a){
    Player* p = &g_game.player;
    if(p->action == MA_DEATH || !actor_touches_mario(a, 0)) return;
    a->active = 0;
    if(a->type == OBJ_LIFEUP){
        mario_lifeup_pickup();
    } else {
        mario_power_end();
        bee_end();
        g_mario_star_power = 1200;
        audio_play_effect(SFX_RAINBOW_POWER_UP_2, 4);
    }
}

static void honey_kind_think(Actor* a){
    if(!honey_handles_actor(a)) return;
    switch(a->type){
    case OBJ_CRYSTALCAGES: crystal_cage_small_think(a); break;
    case OBJ_STARPIECESPOT: starpiecespot_think(a); break;
    case OBJ_BEEMUSHROOM: bee_mushroom_think(a); break;
    case OBJ_QUESTIONCOIN:
    case OBJ_YELLOWCHIP: honey_pickup_think(a); break;
    case OBJ_LIFEUP: honey_item_think(a); break;
    case OBJ_HONEYQUEEN: honey_queen_think(a); break;
    case OBJ_LUIGI: luigi_think(a); break;
    case OBJ_BEEFLOWER: bee_flower_think(a); break;
    case OBJ_LOTUSLEAF: lotus_leaf_think(a); break;
    case OBJ_FOUNTAINBIG: fountain_think(a); break;
    case OBJ_JUMPSTAND: jumpstand_think(a); break;
    case OBJ_HIPDROPROCK: hipdrop_rock_think(a); break;
    case OBJ_HIPDROPSWITCH:
    case OBJ_PICKETSWITCH: hipdrop_switch_think(a); break;
    case OBJ_FORESTOBSTACLEWOOD: obstacle_wood_think(a); break;
    case OBJ_CREEPER: creeper_think(a); break;
    case OBJ_PLANT: plant_seed_think(a); break;
    case OBJ_TRAPEZE: trapeze_think(a); break;
    case OBJ_POLE: pole_think(a); break;
    case OBJ_EARTHENPIPE: earthenpipe_think(a); break;
    case OBJ_WATERPRESSURE: waterpressure_think(a); break;
    case OBJ_WATERBULLET: waterbullet_think(a); break;
    case OBJ_WATCHTOWERSTEP: watch_tower_think(a); break;
    case OBJ_WATCHTOWERLIFT: watch_tower_lift_think(a); break;
    case OBJ_COINBLOCK:
        coin_block_think(a);
        coin_block_rearm(a);
        break;
    case OBJ_INVINCIBLE: honey_item_think(a); break;
    case OBJ_STINKBUGPARENT: stinkbug_parent_think(a); break;
    case OBJ_BOSSSTINKBUG: boss_stinkbug_think(a); break;
    case OBJ_BOSSBOMB: boss_bomb_think(a); break;
    case OBJ_HANACHAN: hanachan_think(a); break;
    case OBJ_HANACHANBODY: hanachan_body_think(a); break;
    case OBJ_TAKOHEI:
    case OBJ_KARIPON:
    case OBJ_KOTEBUG: small_enemy_think(a); break;
    default: break;
    }
}

static int honey_colmesh_of(const Actor* a){
    switch(a->type){
    case OBJ_BEEFLOWER: return HCOL_BEEFLOWER;
    case OBJ_LOTUSLEAF: return HCOL_LOTUSLEAF;
    case OBJ_JUMPSTAND: return HCOL_JUMPSTAND;
    case OBJ_HIPDROPROCK: return HCOL_HIPDROPROCK;
    case OBJ_HIPDROPSWITCH: return HCOL_HIPDROPSWITCH;
    case OBJ_FORESTAPPEARSTEP: return HCOL_FORESTAPPEARSTEP;
    case OBJ_FORESTOBSTACLEWOOD: return HCOL_FORESTOBSTACLEWOOD;
    case OBJ_FORESTWOODCOVER: return HCOL_FORESTWOODCOVER;
    case OBJ_STRONGBLOCK: return HCOL_STRONGBLOCK;
    case OBJ_WATCHTOWERSTEP: return HCOL_WATCHTOWERSTEP;
    case OBJ_FORESTBARRICADE: return HCOL_FORESTBARRICADE;
    case OBJ_FORESTHOMEGATE: return HCOL_FORESTHOMEGATE;
    case OBJ_PICKETSWITCH: return HCOL_PICKETSWITCH;
    case OBJ_POLE: return HCOL_POLE;
    case OBJ_COINBLOCK: return HCOL_COINBLOCK;
    case OBJ_WATCHTOWERLIFT: return HCOL_WATCHTOWERLIFT;
    case OBJ_STINKBUGPARENT: return HCOL_STINKBUGPARENT;
    case OBJ_BOSSSTINKBUG: return HCOL_BOSSSTINKBUG;
    case OBJ_HONEYQUEEN: return -1;
    default: return -1;
    }
}

static void honey_collision_build(void){
    Game* game = &g_game;
    dyncol_clear();
    if(!honey_stage()) return;
    v3 mario = game->player.figure.position;
    g_dyncol_defer_on = true;
    for(int i = 0; i < game->actor_count; i++){
        Actor* a = &game->actors[i];
        if(!a->active) continue;
        if(a->type == OBJ_HONEYQUEEN){
            v3 d = v3_sub(a->position, mario);
            if(fx_abs(d.x) < FX(3000) && fx_abs(d.y) < FX(3000) && fx_abs(d.z) < FX(3000))
                honey_queen_collision(a, mario);
            continue;
        }
        if(a->type == OBJ_CRYSTALCAGES){
            crystal_cage_small_collision(a, mario);
            continue;
        }
        if(a->type == OBJ_BOSSSTINKBUG){
            boss_stinkbug_collision(a, mario);
            continue;
        }
        int mesh = honey_colmesh_of(a);
        if(mesh < 0) continue;
        if(a->type == OBJ_BEEFLOWER && !bee_flower_solid(a)) continue;
        if(a->type == OBJ_JUMPSTAND && !jumpstand_solid(a)) continue;
        if(a->type == OBJ_HIPDROPROCK && a->state != ACTOR_STATE_WAIT) continue;
        if(a->type == OBJ_BOSSSTINKBUG && !boss_stinkbug_solid(a)) continue;
        if(a->type == OBJ_STINKBUGPARENT && a->state == STINKBUG_PARENT_DEATH) continue;
        v3 at = a->type == OBJ_FORESTOBSTACLEWOOD ? obstacle_wood_origin(a) : a->position;
        v3 d = v3_sub(at, mario);
        if(fx_abs(d.x) > FX(3000) || fx_abs(d.y) > FX(3000) || fx_abs(d.z) > FX(3000)) continue;
        int16_t s[3];
        actor_stage_scale(a, s);
        const HoneyColMesh* collision = &honey_colmeshes[mesh];

        if(a->type == OBJ_STINKBUGPARENT){
            const uint16_t* codes = honey_mandibug_base_codes;
            if(!(a->value & 1)){
                collision = &honey_mandibug_child_mesh;
                codes = honey_mandibug_child_codes;
            }
            dyncol_add_codes(collision, a->position, a->up, a->front, s, i, COL_NOT_GRAB, mario, FX(600),
                             codes);
        } else if(a->type == OBJ_BEEFLOWER){
            _Static_assert(sizeof hcol_beeflower_v / sizeof hcol_beeflower_v[0] == 58,
                           "BeeFlower cache assumes 58 vertices");
            dyncol_add_beeflower(collision, a->position, a->up, a->front, s, i, 0, mario, FX(600), NULL);
        } else dyncol_add(collision, at, a->up, a->front, s, i, 0, mario, FX(600));
    }
    g_dyncol_defer_on = false;
}

enum { KINOPIO_GIVE_STAR = 200, KINOPIO_GAVE_STAR };
static void honey_kinopio_begin(Actor* a){
    const StageObject* o = actor_stage_object(a);
    if(!honey_handles_actor(a) || !o || g_honey.kinopio_done || !(o->args[2] == 8 || o->args[3] > 0)) return;
    g_honey.kinopio_done = 1;
    g_honey.kinopio = (int16_t)actor_index(a);
    a->state = KINOPIO_GIVE_STAR;
    a->timer = 0;
    actor_set_anim(a, ANIM_KINOPIO_TAKEOUTSTAR);
    mario_talk_pause(anim_length_of(a->figure.model, ANIM_KINOPIO_TAKEOUTSTAR) + 290);
}
static void honey_kinopio_think(Actor* a){
    if(a->state == KINOPIO_GIVE_STAR){
        if(a->timer >= anim_length_of(a->figure.model, ANIM_KINOPIO_TAKEOUTSTAR)){
            honey_star_appear_at(g_game.scenario, v3_add(a->position, v3_scale14(a->up, FX(150))));
            a->state = KINOPIO_GAVE_STAR;
            a->timer = 0;
            g_honey.kinopio_done = 2;
        }
    } else if(a->state == KINOPIO_GAVE_STAR){
        if(a->timer >= 290) actor_set_anim(a, ANIM_KINOPIO_WAIT);
    } else talk_actor_think(a);
}

static void honey_actors_pre(void){
    if(!honey_stage()) return;
    if(bee_mushroom_appear_actor()) return;
    Player* p = &g_game.player;
    if(g_mario_link.talk_timer > 0 && --g_mario_link.talk_timer == 0){
        if(p->action == MA_TALK) mario_set_action(MA_WAIT);
    }
    honey_collision_build();
    mario_ride_find();
}

static void honey_actors_post(void){
    if(!honey_stage()) return;
    if(bee_mushroom_appear_actor()) return;
    mario_ride_carry();
    switch_objects_tick();
    for(int i = 0; i < g_game.actor_count; i++){
        Actor* a = &g_game.actors[i];
        if(a->active && a->type == OBJ_POWERSTAR && honey_handles_actor(a) && a->value >= 0)
            power_star_fly_home(a);
    }
}
