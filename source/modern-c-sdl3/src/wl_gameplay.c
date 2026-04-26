#include "wl_gameplay.h"

#include <stddef.h>
#include <string.h>

int wl_init_player_gameplay_state(wl_player_gameplay_state *state,
                                  int32_t health, int32_t lives,
                                  int32_t score, int32_t next_extra) {
    if (!state || health < 0 || lives < 0 || score < 0 || next_extra <= 0) {
        return -1;
    }
    memset(state, 0, sizeof(*state));
    state->health = health;
    state->lives = lives;
    state->score = score;
    state->next_extra = next_extra;
    state->ammo = 0;
    state->keys = 0;
    state->best_weapon = WL_WEAPON_PISTOL;
    state->weapon = WL_WEAPON_PISTOL;
    state->chosen_weapon = WL_WEAPON_PISTOL;
    state->attack_frame = 0;
    state->treasure_count = 0;
    state->secret_count = 0;
    state->got_gat_gun = 0;
    state->play_state = WL_PLAYER_PLAY_RUNNING;
    return wl_reset_palette_shift_state(&state->palette_shift);
}

int wl_apply_player_damage(wl_player_gameplay_state *state,
                           wl_difficulty difficulty, int32_t points,
                           int god_mode, int victory_flag,
                           wl_player_damage_result *out) {
    if (!state || !out || points < 0 || difficulty > WL_DIFFICULTY_HARD) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->requested_points = points;
    out->health = state->health;
    out->damage_count = state->palette_shift.damage_count;

    if (victory_flag) {
        out->ignored = 1;
        return 0;
    }

    int32_t effective = points;
    if (difficulty == WL_DIFFICULTY_BABY) {
        effective >>= 2;
    }
    out->effective_points = effective;

    if (!god_mode) {
        state->health -= effective;
    }
    if (state->health <= 0) {
        state->health = 0;
        state->play_state = WL_PLAYER_PLAY_DIED;
        out->died = 1;
    }

    if (wl_start_damage_palette_shift(&state->palette_shift, effective) != 0) {
        return -1;
    }
    state->got_gat_gun = 0;

    out->health = state->health;
    out->damage_count = state->palette_shift.damage_count;
    return 0;
}

static uint32_t actor_tile_center(uint16_t tile) {
    return ((uint32_t)tile << 16) + 0x8000u;
}

static uint32_t abs_delta_u32(uint32_t a, uint32_t b) {
    return a >= b ? a - b : b - a;
}

static int actor_can_shoot(const wl_actor_desc *actor) {
    if (!actor || !actor->shootable) {
        return 0;
    }
    switch (actor->kind) {
    case WL_ACTOR_GUARD:
    case WL_ACTOR_OFFICER:
    case WL_ACTOR_SS:
    case WL_ACTOR_MUTANT:
    case WL_ACTOR_BOSS:
        return 1;
    default:
        return 0;
    }
}

static uint16_t actor_player_tile_distance(const wl_actor_desc *actor,
                                           const wl_player_motion_state *player) {
    uint16_t player_tile_x = (uint16_t)(player->x >> 16);
    uint16_t player_tile_y = (uint16_t)(player->y >> 16);
    uint16_t dx = actor->tile_x > player_tile_x ?
                  (uint16_t)(actor->tile_x - player_tile_x) :
                  (uint16_t)(player_tile_x - actor->tile_x);
    uint16_t dy = actor->tile_y > player_tile_y ?
                  (uint16_t)(actor->tile_y - player_tile_y) :
                  (uint16_t)(player_tile_y - actor->tile_y);
    uint16_t dist = dx > dy ? dx : dy;
    if (actor->kind == WL_ACTOR_SS || actor->kind == WL_ACTOR_BOSS) {
        dist = (uint16_t)((dist * 2u) / 3u);
    }
    return dist;
}

static int32_t actor_shot_hit_chance(uint16_t dist, int player_running,
                                     int actor_visible) {
    if (player_running) {
        return actor_visible ? 160 - (int32_t)dist * 16 :
                               160 - (int32_t)dist * 8;
    }
    return actor_visible ? 256 - (int32_t)dist * 16 :
                           256 - (int32_t)dist * 8;
}

static int32_t actor_shot_damage_points(uint16_t dist, uint8_t damage_roll) {
    if (dist < 2u) {
        return (int32_t)(damage_roll >> 2);
    }
    if (dist < 4u) {
        return (int32_t)(damage_roll >> 3);
    }
    return (int32_t)(damage_roll >> 4);
}

int wl_try_actor_bite_player(wl_player_gameplay_state *state,
                             const wl_actor_desc *actor,
                             const wl_player_motion_state *player,
                             wl_difficulty difficulty,
                             uint8_t chance_roll, uint8_t damage_roll,
                             int god_mode, int victory_flag,
                             wl_actor_contact_damage_result *out) {
    const uint32_t bite_axis_range = 0x20000u;
    if (!state || !actor || !player || !out || actor->kind != WL_ACTOR_DOG ||
        actor->tile_x >= WL_MAP_SIDE || actor->tile_y >= WL_MAP_SIDE ||
        player->x >= ((uint32_t)WL_MAP_SIDE << 16) ||
        player->y >= ((uint32_t)WL_MAP_SIDE << 16) ||
        difficulty > WL_DIFFICULTY_HARD) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->chance_roll = chance_roll;
    out->damage_roll = damage_roll;

    uint32_t actor_x = actor_tile_center(actor->tile_x);
    uint32_t actor_y = actor_tile_center(actor->tile_y);
    if (abs_delta_u32(player->x, actor_x) > bite_axis_range ||
        abs_delta_u32(player->y, actor_y) > bite_axis_range) {
        return 0;
    }

    out->in_range = 1;
    if (chance_roll >= 180u) {
        return 0;
    }

    out->chance_hit = 1;
    int32_t points = (int32_t)(damage_roll >> 4);
    if (wl_apply_player_damage(state, difficulty, points, god_mode, victory_flag,
                               &out->damage) != 0) {
        return -1;
    }
    out->damaged = out->damage.ignored ? 0 : 1;
    return 0;
}

static int projectile_kind_valid(wl_projectile_kind kind) {
    return kind >= WL_PROJECTILE_NEEDLE && kind <= WL_PROJECTILE_FIRE;
}

static int projectile_position_is_clear(const wl_game_model *model, uint32_t x,
                                        uint32_t y) {
    const uint32_t projectile_size = 0x2000u;
    if (!model || x < projectile_size || y < projectile_size ||
        x + projectile_size >= ((uint32_t)WL_MAP_SIDE << 16) ||
        y + projectile_size >= ((uint32_t)WL_MAP_SIDE << 16)) {
        return 0;
    }

    uint16_t xl = (uint16_t)((x - projectile_size) >> 16);
    uint16_t yl = (uint16_t)((y - projectile_size) >> 16);
    uint16_t xh = (uint16_t)((x + projectile_size) >> 16);
    uint16_t yh = (uint16_t)((y + projectile_size) >> 16);
    for (uint16_t ty = yl; ty <= yh; ++ty) {
        for (uint16_t tx = xl; tx <= xh; ++tx) {
            uint16_t tile = model->tilemap[(size_t)ty * WL_MAP_SIDE + tx];
            if (tile != 0 && tile != 0x40u) {
                return 0;
            }
        }
    }
    return 1;
}

static int32_t projectile_damage_points(wl_projectile_kind kind,
                                        uint8_t damage_roll) {
    switch (kind) {
    case WL_PROJECTILE_NEEDLE:
        return (int32_t)(damage_roll >> 3) + 20;
    case WL_PROJECTILE_ROCKET:
    case WL_PROJECTILE_HROCKET:
    case WL_PROJECTILE_SPARK:
        return (int32_t)(damage_roll >> 3) + 30;
    case WL_PROJECTILE_FIRE:
        return (int32_t)(damage_roll >> 3);
    }
    return -1;
}

static int32_t actor_start_hitpoints(wl_actor_kind kind,
                                     wl_difficulty difficulty) {
    static const int32_t guards[4] = { 25, 25, 25, 25 };
    static const int32_t officers[4] = { 50, 50, 50, 50 };
    static const int32_t ss[4] = { 100, 100, 100, 100 };
    static const int32_t dogs[4] = { 1, 1, 1, 1 };
    static const int32_t mutants[4] = { 45, 55, 55, 65 };
    static const int32_t bosses[4] = { 850, 950, 1050, 1200 };
    static const int32_t ghosts[4] = { 25, 25, 25, 25 };
    if (difficulty > WL_DIFFICULTY_HARD) {
        return -1;
    }
    switch (kind) {
    case WL_ACTOR_GUARD:
        return guards[difficulty];
    case WL_ACTOR_OFFICER:
        return officers[difficulty];
    case WL_ACTOR_SS:
        return ss[difficulty];
    case WL_ACTOR_DOG:
        return dogs[difficulty];
    case WL_ACTOR_MUTANT:
        return mutants[difficulty];
    case WL_ACTOR_BOSS:
        return bosses[difficulty];
    case WL_ACTOR_GHOST:
        return ghosts[difficulty];
    case WL_ACTOR_DEAD_GUARD:
        return -1;
    }
    return -1;
}

static int32_t actor_kill_points(wl_actor_kind kind) {
    switch (kind) {
    case WL_ACTOR_GUARD:
        return 100;
    case WL_ACTOR_OFFICER:
        return 400;
    case WL_ACTOR_SS:
        return 500;
    case WL_ACTOR_DOG:
        return 200;
    case WL_ACTOR_MUTANT:
        return 700;
    case WL_ACTOR_BOSS:
        return 5000;
    case WL_ACTOR_GHOST:
        return 200;
    case WL_ACTOR_DEAD_GUARD:
        return 0;
    }
    return 0;
}

static int actor_kill_drop(wl_actor_kind kind, wl_weapon_type best_weapon,
                           wl_bonus_item *out) {
    if (!out) {
        return 0;
    }
    switch (kind) {
    case WL_ACTOR_GUARD:
    case WL_ACTOR_OFFICER:
    case WL_ACTOR_MUTANT:
        *out = WL_BONUS_CLIP2;
        return 1;
    case WL_ACTOR_SS:
        *out = best_weapon < WL_WEAPON_MACHINEGUN ?
               WL_BONUS_MACHINEGUN : WL_BONUS_CLIP2;
        return 1;
    case WL_ACTOR_BOSS:
        *out = WL_BONUS_KEY1;
        return 1;
    default:
        return 0;
    }
}

int wl_init_actor_combat_state(const wl_actor_desc *actor,
                               wl_difficulty difficulty,
                               wl_actor_combat_state *out) {
    if (!actor || !out || difficulty > WL_DIFFICULTY_HARD ||
        actor->kind > WL_ACTOR_DEAD_GUARD ||
        actor->tile_x >= WL_MAP_SIDE || actor->tile_y >= WL_MAP_SIDE) {
        return -1;
    }
    int32_t hp = actor_start_hitpoints(actor->kind, difficulty);
    if (hp <= 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->kind = actor->kind;
    out->tile_x = actor->tile_x;
    out->tile_y = actor->tile_y;
    out->hitpoints = hp;
    out->shootable = actor->shootable ? 1u : 0u;
    out->alive = 1;
    return 0;
}

int wl_apply_actor_damage(wl_player_gameplay_state *player,
                          wl_actor_combat_state *actor,
                          int32_t points,
                          wl_actor_damage_result *out) {
    if (!player || !actor || !out || points < 0 || !actor->alive ||
        !actor->shootable || actor->hitpoints <= 0 ||
        actor->kind > WL_ACTOR_DEAD_GUARD) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->requested_points = points;
    int32_t effective = actor->attack_mode ? points : points * 2;
    out->effective_points = effective;
    actor->hitpoints -= effective;

    if (actor->hitpoints <= 0) {
        actor->hitpoints = 0;
        actor->alive = 0;
        actor->shootable = 0;
        out->killed = 1;
        out->score_awarded = actor_kill_points(actor->kind);
        out->dropped_item = actor_kill_drop(actor->kind, player->best_weapon,
                                            &out->drop_item) ? 1u : 0u;
        if (wl_award_player_points(player, out->score_awarded,
                                   &out->extra_lives_awarded,
                                   &out->score_thresholds_crossed) != 0) {
            return -1;
        }
    } else {
        if (!actor->attack_mode) {
            actor->attack_mode = 1;
            out->attack_mode_started = 1;
        }
        switch (actor->kind) {
        case WL_ACTOR_GUARD:
        case WL_ACTOR_OFFICER:
        case WL_ACTOR_SS:
        case WL_ACTOR_MUTANT:
            out->pain_state_variant = (uint8_t)(actor->hitpoints & 1);
            break;
        default:
            out->pain_state_variant = 0;
            break;
        }
    }
    out->hitpoints = actor->hitpoints;
    return 0;
}

static int bonus_item_to_static_type(wl_bonus_item item, uint16_t *out) {
    if (!out) {
        return -1;
    }
    switch (item) {
    case WL_BONUS_KEY1:
        *out = 20;
        return 0;
    case WL_BONUS_KEY2:
        *out = 21;
        return 0;
    case WL_BONUS_CLIP:
        *out = 26;
        return 0;
    case WL_BONUS_MACHINEGUN:
        *out = 27;
        return 0;
    case WL_BONUS_CHAINGUN:
        *out = 28;
        return 0;
    case WL_BONUS_CLIP2:
        *out = 48;
        return 0;
    default:
        return -1;
    }
}

int wl_spawn_actor_drop_static(wl_game_model *model,
                               const wl_actor_combat_state *actor,
                               const wl_actor_damage_result *damage,
                               size_t *out_static_index) {
    if (!model || !actor || !damage || actor->tile_x >= WL_MAP_SIDE ||
        actor->tile_y >= WL_MAP_SIDE) {
        return -1;
    }
    if (out_static_index) {
        *out_static_index = model->static_count;
    }
    if (!damage->killed || !damage->dropped_item) {
        return 0;
    }
    if (model->static_count >= WL_MAX_STATS) {
        return -1;
    }
    uint16_t type = 0;
    if (bonus_item_to_static_type(damage->drop_item, &type) != 0) {
        return -1;
    }

    size_t index = model->static_count++;
    wl_static_desc *stat = &model->statics[index];
    memset(stat, 0, sizeof(*stat));
    stat->x = actor->tile_x;
    stat->y = actor->tile_y;
    stat->source_tile = (uint16_t)(type + 23u);
    stat->type = type;
    stat->blocking = 0;
    stat->bonus = 1;
    stat->treasure = 0;
    stat->active = 1;
    ++model->bonus_static_count;
    if (out_static_index) {
        *out_static_index = index;
    }
    return 0;
}


static int actor_death_sequence(wl_actor_kind kind, const uint16_t **sprites,
                                const int32_t **durations, uint8_t *count) {
    static const uint16_t guard_sprites[] = { 91, 92, 93, 95 };
    static const int32_t guard_tics[] = { 15, 15, 15, 0 };
    static const uint16_t dog_sprites[] = { 131, 132, 133, 134 };
    static const int32_t dog_tics[] = { 15, 15, 15, 15 };
    static const uint16_t ss_sprites[] = { 179, 180, 181, 183 };
    static const int32_t ss_tics[] = { 15, 15, 15, 0 };
    static const uint16_t mutant_sprites[] = { 228, 229, 230, 232, 233 };
    static const int32_t mutant_tics[] = { 7, 7, 7, 7, 0 };
    static const uint16_t officer_sprites[] = { 279, 280, 281, 283, 284 };
    static const int32_t officer_tics[] = { 11, 11, 11, 11, 0 };
    static const uint16_t boss_sprites[] = { 304, 305, 306, 303 };
    static const int32_t boss_tics[] = { 15, 15, 15, 0 };

    if (!sprites || !durations || !count) {
        return -1;
    }
    switch (kind) {
    case WL_ACTOR_GUARD:
        *sprites = guard_sprites;
        *durations = guard_tics;
        *count = (uint8_t)(sizeof(guard_sprites) / sizeof(guard_sprites[0]));
        return 0;
    case WL_ACTOR_DOG:
        *sprites = dog_sprites;
        *durations = dog_tics;
        *count = (uint8_t)(sizeof(dog_sprites) / sizeof(dog_sprites[0]));
        return 0;
    case WL_ACTOR_SS:
        *sprites = ss_sprites;
        *durations = ss_tics;
        *count = (uint8_t)(sizeof(ss_sprites) / sizeof(ss_sprites[0]));
        return 0;
    case WL_ACTOR_MUTANT:
        *sprites = mutant_sprites;
        *durations = mutant_tics;
        *count = (uint8_t)(sizeof(mutant_sprites) / sizeof(mutant_sprites[0]));
        return 0;
    case WL_ACTOR_OFFICER:
        *sprites = officer_sprites;
        *durations = officer_tics;
        *count = (uint8_t)(sizeof(officer_sprites) / sizeof(officer_sprites[0]));
        return 0;
    case WL_ACTOR_BOSS:
        *sprites = boss_sprites;
        *durations = boss_tics;
        *count = (uint8_t)(sizeof(boss_sprites) / sizeof(boss_sprites[0]));
        return 0;
    default:
        return -1;
    }
}

static void fill_actor_death_step_result(const wl_actor_death_state *state,
                                         wl_actor_death_step_result *out) {
    if (!out) {
        return;
    }
    out->finished = state->finished;
    out->death_scream = state->death_scream;
    out->stage = state->stage;
    out->tics_remaining = state->tics_remaining;
    out->sprite_source_index = state->sprite_source_index;
}

int wl_start_actor_death_state(const wl_actor_combat_state *actor,
                               const wl_actor_damage_result *damage,
                               wl_actor_death_state *out) {
    if (!actor || !damage || !out || !damage->killed || actor->alive ||
        actor->shootable) {
        return -1;
    }
    const uint16_t *sprites = NULL;
    const int32_t *durations = NULL;
    uint8_t count = 0;
    if (actor_death_sequence(actor->kind, &sprites, &durations, &count) != 0 ||
        count == 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->kind = actor->kind;
    out->stage_count = count;
    out->finished = durations[0] == 0 ? 1u : 0u;
    out->death_scream = 1;
    out->tics_remaining = durations[0];
    out->sprite_source_index = sprites[0];
    return 0;
}

int wl_step_actor_death_state(wl_actor_death_state *state, int32_t tics,
                              wl_actor_death_step_result *out) {
    if (!state || !out || tics < 0 || state->stage >= state->stage_count) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    const uint16_t *sprites = NULL;
    const int32_t *durations = NULL;
    uint8_t count = 0;
    if (actor_death_sequence(state->kind, &sprites, &durations, &count) != 0 ||
        count != state->stage_count) {
        return -1;
    }

    state->death_scream = 0;
    if (state->finished || durations[state->stage] == 0) {
        state->finished = 1;
        state->sprite_source_index = sprites[state->stage];
        state->tics_remaining = durations[state->stage];
        fill_actor_death_step_result(state, out);
        return 0;
    }

    int32_t remaining_tics = tics;
    while (remaining_tics >= state->tics_remaining && !state->finished) {
        remaining_tics -= state->tics_remaining;
        if (state->stage + 1u >= count) {
            state->finished = 1;
            break;
        }
        ++state->stage;
        out->advanced = 1;
        state->sprite_source_index = sprites[state->stage];
        state->tics_remaining = durations[state->stage];
        if (state->tics_remaining == 0) {
            state->finished = 1;
            break;
        }
    }
    if (!state->finished) {
        state->tics_remaining -= remaining_tics;
    }
    fill_actor_death_step_result(state, out);
    return 0;
}

int wl_build_actor_death_scene_ref(const wl_actor_combat_state *actor,
                                   const wl_actor_death_state *death,
                                   uint16_t vswap_sprite_start,
                                   uint16_t model_index,
                                   wl_scene_sprite_ref *out) {
    if (!actor || !death || !out || actor->alive || actor->shootable ||
        actor->tile_x >= WL_MAP_SIDE || actor->tile_y >= WL_MAP_SIDE ||
        death->stage >= death->stage_count) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->kind = WL_SCENE_SPRITE_ACTOR;
    out->model_index = model_index;
    out->source_index = death->sprite_source_index;
    out->vswap_chunk_index = (uint16_t)(vswap_sprite_start + death->sprite_source_index);
    out->world_x = ((uint32_t)actor->tile_x << 16) + 0x8000u;
    out->world_y = ((uint32_t)actor->tile_y << 16) + 0x8000u;
    return 0;
}

int wl_apply_actor_death_final_frame(wl_game_model *model,
                                     uint16_t model_index,
                                     const wl_actor_death_state *death) {
    if (!model || !death || model_index >= model->actor_count ||
        !death->finished || death->stage >= death->stage_count ||
        death->sprite_source_index == UINT16_MAX) {
        return -1;
    }

    wl_actor_desc *actor = &model->actors[model_index];
    actor->mode = WL_ACTOR_INERT;
    actor->shootable = 0;
    actor->scene_source_index = death->sprite_source_index;
    actor->scene_source_override = 1;
    return 0;
}

int wl_step_live_actor_death_tick(wl_game_model *model,
                                  uint16_t actor_model_index,
                                  wl_actor_death_state *death,
                                  int32_t tics,
                                  uint16_t vswap_sprite_start,
                                  wl_live_actor_death_tick_result *out) {
    if (!model || !death || !out || actor_model_index >= model->actor_count ||
        vswap_sprite_start == 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    if (wl_step_actor_death_state(death, tics, &out->step) != 0) {
        return -1;
    }

    const wl_actor_desc *actor = &model->actors[actor_model_index];
    if (death->sprite_source_index == UINT16_MAX || actor->tile_x >= WL_MAP_SIDE ||
        actor->tile_y >= WL_MAP_SIDE) {
        return -1;
    }
    out->death_ref.kind = WL_SCENE_SPRITE_ACTOR;
    out->death_ref.model_index = actor_model_index;
    out->death_ref.source_index = death->sprite_source_index;
    out->death_ref.vswap_chunk_index = (uint16_t)(vswap_sprite_start +
                                                  death->sprite_source_index);
    out->death_ref.world_x = ((uint32_t)actor->tile_x << 16) + 0x8000u;
    out->death_ref.world_y = ((uint32_t)actor->tile_y << 16) + 0x8000u;
    out->death_ref_built = 1;

    if (death->finished) {
        if (wl_apply_actor_death_final_frame(model, actor_model_index, death) != 0) {
            return -1;
        }
        out->final_frame_applied = 1;
    }
    return 0;
}

int wl_try_actor_shoot_player(wl_player_gameplay_state *state,
                              const wl_actor_desc *actor,
                              const wl_player_motion_state *player,
                              wl_difficulty difficulty,
                              int area_active, int line_of_sight,
                              int player_running, int actor_visible,
                              uint8_t chance_roll, uint8_t damage_roll,
                              int god_mode, int victory_flag,
                              wl_actor_shot_damage_result *out) {
    if (!state || !actor || !player || !out || !actor_can_shoot(actor) ||
        actor->tile_x >= WL_MAP_SIDE || actor->tile_y >= WL_MAP_SIDE ||
        player->x >= ((uint32_t)WL_MAP_SIDE << 16) ||
        player->y >= ((uint32_t)WL_MAP_SIDE << 16) ||
        difficulty > WL_DIFFICULTY_HARD) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->area_active = area_active ? 1u : 0u;
    out->line_of_sight = line_of_sight ? 1u : 0u;
    out->chance_roll = chance_roll;
    out->damage_roll = damage_roll;
    out->distance_tiles = actor_player_tile_distance(actor, player);
    out->hit_chance = actor_shot_hit_chance(out->distance_tiles, player_running,
                                            actor_visible);

    if (!area_active || !line_of_sight || out->hit_chance <= 0 ||
        (int32_t)chance_roll >= out->hit_chance) {
        return 0;
    }

    out->chance_hit = 1;
    int32_t points = actor_shot_damage_points(out->distance_tiles, damage_roll);
    if (wl_apply_player_damage(state, difficulty, points, god_mode, victory_flag,
                               &out->damage) != 0) {
        return -1;
    }
    out->damaged = out->damage.ignored ? 0 : 1;
    return 0;
}

int wl_step_projectile(wl_player_gameplay_state *state,
                       const wl_game_model *model,
                       const wl_player_motion_state *player,
                       wl_projectile_state *projectile,
                       wl_difficulty difficulty,
                       int32_t xmove, int32_t ymove,
                       uint8_t damage_roll,
                       int god_mode, int victory_flag,
                       wl_projectile_step_result *out) {
    if (!state || !model || !player || !projectile || !out ||
        !projectile->active || !projectile_kind_valid(projectile->kind) ||
        projectile->x >= ((uint32_t)WL_MAP_SIDE << 16) ||
        projectile->y >= ((uint32_t)WL_MAP_SIDE << 16) ||
        player->x >= ((uint32_t)WL_MAP_SIDE << 16) ||
        player->y >= ((uint32_t)WL_MAP_SIDE << 16) ||
        difficulty > WL_DIFFICULTY_HARD) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->damage_roll = damage_roll;

    if (xmove > 0x10000) {
        xmove = 0x10000;
    }
    if (ymove > 0x10000) {
        ymove = 0x10000;
    }

    uint32_t old_x = projectile->x;
    uint32_t old_y = projectile->y;
    int64_t moved_x = (int64_t)projectile->x + xmove;
    int64_t moved_y = (int64_t)projectile->y + ymove;
    if (moved_x < 0 || moved_y < 0 ||
        moved_x >= ((int64_t)WL_MAP_SIDE << 16) ||
        moved_y >= ((int64_t)WL_MAP_SIDE << 16)) {
        return -1;
    }
    projectile->x = (uint32_t)moved_x;
    projectile->y = (uint32_t)moved_y;
    out->moved = projectile->x != old_x || projectile->y != old_y;

    if (!projectile_position_is_clear(model, projectile->x, projectile->y)) {
        projectile->active = 0;
        out->hit_wall = 1;
        out->removed = 1;
        out->x = projectile->x;
        out->y = projectile->y;
        out->tile_x = (uint16_t)(projectile->x >> 16);
        out->tile_y = (uint16_t)(projectile->y >> 16);
        projectile->tile_x = out->tile_x;
        projectile->tile_y = out->tile_y;
        return 0;
    }

    const uint32_t projectile_hit_range = 0xc000u;
    if (abs_delta_u32(projectile->x, player->x) < projectile_hit_range &&
        abs_delta_u32(projectile->y, player->y) < projectile_hit_range) {
        int32_t points = projectile_damage_points(projectile->kind, damage_roll);
        if (points < 0 ||
            wl_apply_player_damage(state, difficulty, points, god_mode,
                                   victory_flag, &out->damage) != 0) {
            return -1;
        }
        projectile->active = 0;
        out->hit_player = 1;
        out->removed = 1;
        out->x = projectile->x;
        out->y = projectile->y;
        out->tile_x = (uint16_t)(projectile->x >> 16);
        out->tile_y = (uint16_t)(projectile->y >> 16);
        projectile->tile_x = out->tile_x;
        projectile->tile_y = out->tile_y;
        return 0;
    }

    projectile->tile_x = (uint16_t)(projectile->x >> 16);
    projectile->tile_y = (uint16_t)(projectile->y >> 16);
    out->x = projectile->x;
    out->y = projectile->y;
    out->tile_x = projectile->tile_x;
    out->tile_y = projectile->tile_y;
    return 0;
}

int wl_heal_player(wl_player_gameplay_state *state, int32_t points) {
    if (!state || points < 0) {
        return -1;
    }
    state->health += points;
    if (state->health > 100) {
        state->health = 100;
    }
    state->got_gat_gun = 0;
    return 0;
}

int wl_award_player_points(wl_player_gameplay_state *state, int32_t points,
                           int32_t *out_extra_lives_awarded,
                           int32_t *out_thresholds_crossed) {
    if (!state || points < 0 || state->next_extra <= 0) {
        return -1;
    }

    int32_t extra_lives = 0;
    int32_t thresholds = 0;
    state->score += points;
    while (state->score >= state->next_extra) {
        state->next_extra += WL_EXTRA_POINTS;
        ++thresholds;
        if (state->lives < 9) {
            ++state->lives;
            ++extra_lives;
        }
    }

    if (out_extra_lives_awarded) {
        *out_extra_lives_awarded = extra_lives;
    }
    if (out_thresholds_crossed) {
        *out_thresholds_crossed = thresholds;
    }
    return 0;
}

int wl_give_player_ammo(wl_player_gameplay_state *state, int32_t ammo) {
    if (!state || ammo < 0) {
        return -1;
    }
    if (state->ammo == 0 && state->attack_frame == 0) {
        state->weapon = state->chosen_weapon;
    }
    state->ammo += ammo;
    if (state->ammo > 99) {
        state->ammo = 99;
    }
    return 0;
}

int wl_give_player_weapon(wl_player_gameplay_state *state, wl_weapon_type weapon) {
    if (!state || weapon > WL_WEAPON_CHAINGUN) {
        return -1;
    }
    if (wl_give_player_ammo(state, 6) != 0) {
        return -1;
    }
    if (state->best_weapon < weapon) {
        state->best_weapon = weapon;
        state->weapon = weapon;
        state->chosen_weapon = weapon;
    }
    return 0;
}

int wl_try_player_fire_weapon(wl_player_gameplay_state *state,
                              wl_weapon_type requested_weapon,
                              wl_player_fire_result *out) {
    if (!state || !out || requested_weapon > WL_WEAPON_CHAINGUN ||
        state->best_weapon > WL_WEAPON_CHAINGUN ||
        state->weapon > WL_WEAPON_CHAINGUN ||
        state->chosen_weapon > WL_WEAPON_CHAINGUN ||
        state->ammo < 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->requested_weapon = requested_weapon;
    out->fired_weapon = state->weapon;
    out->ammo_before = state->ammo;
    out->ammo_after = state->ammo;

    if (requested_weapon > state->best_weapon) {
        out->unavailable = 1;
        return 0;
    }

    state->weapon = requested_weapon;
    state->chosen_weapon = requested_weapon;
    out->fired_weapon = requested_weapon;

    if (requested_weapon != WL_WEAPON_KNIFE) {
        if (state->ammo <= 0) {
            state->weapon = WL_WEAPON_KNIFE;
            out->fired_weapon = WL_WEAPON_KNIFE;
            out->no_ammo = 1;
            out->ammo_after = state->ammo;
            return 0;
        }
        --state->ammo;
        out->consumed_ammo = 1;
    }

    out->fired = 1;
    out->ammo_after = state->ammo;
    return 0;
}

int wl_try_player_fire_weapon_attack(wl_player_gameplay_state *state,
                                     wl_weapon_type requested_weapon,
                                     int32_t attack_tics,
                                     wl_player_fire_attack_result *out) {
    if (!state || !out || attack_tics <= 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->frame_before = state->attack_frame;
    out->frame_after = state->attack_frame;

    if (wl_try_player_fire_weapon(state, requested_weapon, &out->fire) != 0) {
        return -1;
    }

    if (out->fire.fired) {
        state->attack_frame = attack_tics;
        out->frame_after = state->attack_frame;
        out->attack_started = 1;
    }
    return 0;
}

int wl_step_player_attack_state(wl_player_gameplay_state *state, int32_t tics,
                                wl_player_attack_step_result *out) {
    if (!state || !out || tics < 0 || state->attack_frame < 0 ||
        state->weapon > WL_WEAPON_CHAINGUN ||
        state->chosen_weapon > WL_WEAPON_CHAINGUN) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->frame_before = state->attack_frame;
    out->frame_after = state->attack_frame;
    out->weapon_before = state->weapon;
    out->weapon_after = state->weapon;

    if (state->attack_frame == 0 || tics == 0) {
        return 0;
    }

    out->advanced = 1;
    state->attack_frame -= tics;
    if (state->attack_frame <= 0) {
        state->attack_frame = 0;
        out->finished = 1;
        if (state->ammo > 0 && state->weapon == WL_WEAPON_KNIFE &&
            state->chosen_weapon != WL_WEAPON_KNIFE) {
            state->weapon = state->chosen_weapon;
            out->restored_chosen_weapon = 1;
        }
    }

    out->frame_after = state->attack_frame;
    out->weapon_after = state->weapon;
    return 0;
}

int wl_give_player_key(wl_player_gameplay_state *state, uint8_t key) {
    if (!state || key >= 32) {
        return -1;
    }
    state->keys |= (uint32_t)1u << key;
    return 0;
}

int wl_apply_player_bonus(wl_player_gameplay_state *state, wl_bonus_item item,
                          uint8_t *out_picked_up) {
    if (!state || item > WL_BONUS_SPEAR) {
        return -1;
    }
    uint8_t picked_up = 1;

    switch (item) {
    case WL_BONUS_FIRSTAID:
        if (state->health == 100) {
            picked_up = 0;
            break;
        }
        if (wl_heal_player(state, 25) != 0) {
            return -1;
        }
        break;
    case WL_BONUS_KEY1:
    case WL_BONUS_KEY2:
    case WL_BONUS_KEY3:
    case WL_BONUS_KEY4:
        if (wl_give_player_key(state, (uint8_t)(item - WL_BONUS_KEY1)) != 0) {
            return -1;
        }
        break;
    case WL_BONUS_CROSS:
        if (wl_award_player_points(state, 100, NULL, NULL) != 0) {
            return -1;
        }
        ++state->treasure_count;
        break;
    case WL_BONUS_CHALICE:
        if (wl_award_player_points(state, 500, NULL, NULL) != 0) {
            return -1;
        }
        ++state->treasure_count;
        break;
    case WL_BONUS_BIBLE:
        if (wl_award_player_points(state, 1000, NULL, NULL) != 0) {
            return -1;
        }
        ++state->treasure_count;
        break;
    case WL_BONUS_CROWN:
        if (wl_award_player_points(state, 5000, NULL, NULL) != 0) {
            return -1;
        }
        ++state->treasure_count;
        break;
    case WL_BONUS_CLIP:
        if (state->ammo == 99) {
            picked_up = 0;
            break;
        }
        if (wl_give_player_ammo(state, 8) != 0) {
            return -1;
        }
        break;
    case WL_BONUS_CLIP2:
        if (state->ammo == 99) {
            picked_up = 0;
            break;
        }
        if (wl_give_player_ammo(state, 4) != 0) {
            return -1;
        }
        break;
    case WL_BONUS_25CLIP:
        if (state->ammo == 99) {
            picked_up = 0;
            break;
        }
        if (wl_give_player_ammo(state, 25) != 0) {
            return -1;
        }
        break;
    case WL_BONUS_MACHINEGUN:
        if (wl_give_player_weapon(state, WL_WEAPON_MACHINEGUN) != 0) {
            return -1;
        }
        break;
    case WL_BONUS_CHAINGUN:
        if (wl_give_player_weapon(state, WL_WEAPON_CHAINGUN) != 0) {
            return -1;
        }
        state->got_gat_gun = 1;
        break;
    case WL_BONUS_FULLHEAL:
        if (wl_heal_player(state, 99) != 0 || wl_give_player_ammo(state, 25) != 0) {
            return -1;
        }
        if (state->lives < 9) {
            ++state->lives;
        }
        ++state->treasure_count;
        break;
    case WL_BONUS_FOOD:
        if (state->health == 100) {
            picked_up = 0;
            break;
        }
        if (wl_heal_player(state, 10) != 0) {
            return -1;
        }
        break;
    case WL_BONUS_ALPO:
        if (state->health == 100) {
            picked_up = 0;
            break;
        }
        if (wl_heal_player(state, 4) != 0) {
            return -1;
        }
        break;
    case WL_BONUS_GIBS:
        if (state->health > 10) {
            picked_up = 0;
            break;
        }
        if (wl_heal_player(state, 1) != 0) {
            return -1;
        }
        break;
    case WL_BONUS_SPEAR:
        state->play_state = WL_PLAYER_PLAY_COMPLETED;
        break;
    case WL_BONUS_DRESSING:
    case WL_BONUS_BLOCK:
        picked_up = 0;
        break;
    }

    if (picked_up && wl_start_player_bonus_flash(state) != 0) {
        return -1;
    }
    if (out_picked_up) {
        *out_picked_up = picked_up;
    }
    return 0;
}

static int static_type_to_bonus_item(uint16_t type, wl_bonus_item *out) {
    if (!out) {
        return -1;
    }
    switch (type) {
    case 6:
        *out = WL_BONUS_ALPO;
        return 0;
    case 20:
        *out = WL_BONUS_KEY1;
        return 0;
    case 21:
        *out = WL_BONUS_KEY2;
        return 0;
    case 24:
        *out = WL_BONUS_FOOD;
        return 0;
    case 25:
        *out = WL_BONUS_FIRSTAID;
        return 0;
    case 26:
        *out = WL_BONUS_CLIP;
        return 0;
    case 27:
        *out = WL_BONUS_MACHINEGUN;
        return 0;
    case 28:
        *out = WL_BONUS_CHAINGUN;
        return 0;
    case 29:
        *out = WL_BONUS_CROSS;
        return 0;
    case 30:
        *out = WL_BONUS_CHALICE;
        return 0;
    case 31:
        *out = WL_BONUS_BIBLE;
        return 0;
    case 32:
        *out = WL_BONUS_CROWN;
        return 0;
    case 33:
        *out = WL_BONUS_FULLHEAL;
        return 0;
    case 34:
    case 38:
        *out = WL_BONUS_GIBS;
        return 0;
    case 48:
        *out = WL_BONUS_CLIP2;
        return 0;
    default:
        return -1;
    }
}

int wl_try_pickup_static_bonus(wl_player_gameplay_state *state,
                               wl_static_desc *stat,
                               uint8_t *out_picked_up) {
    if (!state || !stat) {
        return -1;
    }
    if (!stat->active || !stat->bonus) {
        if (out_picked_up) {
            *out_picked_up = 0;
        }
        return 0;
    }
    wl_bonus_item item;
    if (static_type_to_bonus_item(stat->type, &item) != 0) {
        return -1;
    }
    uint8_t picked_up = 0;
    if (wl_apply_player_bonus(state, item, &picked_up) != 0) {
        return -1;
    }
    if (picked_up) {
        stat->active = 0;
    }
    if (out_picked_up) {
        *out_picked_up = picked_up;
    }
    return 0;
}

static int32_t gameplay_fixed_mul_shift(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
}

static int visible_static_is_in_pickup_range(const wl_static_desc *stat,
                                             uint32_t origin_x, uint32_t origin_y,
                                             int32_t forward_x, int32_t forward_y) {
    const int32_t mindist = 0x5800;
    const int32_t actorsize = 0x2000;
    const int32_t tileglobal = 1 << 16;
    const int32_t halftile = 1 << 15;

    int32_t sprite_x = ((int32_t)stat->x << 16) + halftile;
    int32_t sprite_y = ((int32_t)stat->y << 16) + halftile;
    int32_t gx = sprite_x - (int32_t)origin_x;
    int32_t gy = sprite_y - (int32_t)origin_y;
    int32_t forward_distance = gameplay_fixed_mul_shift(gx, forward_x) +
                               gameplay_fixed_mul_shift(gy, forward_y);
    int32_t lateral = gameplay_fixed_mul_shift(gy, forward_x) -
                      gameplay_fixed_mul_shift(gx, forward_y);
    int32_t nx = forward_distance - actorsize;

    return nx >= mindist && nx < tileglobal && lateral > -halftile && lateral < halftile;
}

int wl_try_pickup_visible_static_bonus(wl_player_gameplay_state *state,
                                       wl_game_model *model,
                                       uint32_t origin_x, uint32_t origin_y,
                                       int32_t forward_x, int32_t forward_y,
                                       uint8_t *out_picked_up,
                                       size_t *out_static_index) {
    if (!state || !model || (forward_x == 0 && forward_y == 0) ||
        origin_x >= ((uint32_t)WL_MAP_SIDE << 16) ||
        origin_y >= ((uint32_t)WL_MAP_SIDE << 16)) {
        return -1;
    }
    if (out_picked_up) {
        *out_picked_up = 0;
    }
    if (out_static_index) {
        *out_static_index = model->static_count;
    }

    for (size_t i = 0; i < model->static_count; ++i) {
        wl_static_desc *stat = &model->statics[i];
        if (!stat->active || !stat->bonus) {
            continue;
        }
        if (!visible_static_is_in_pickup_range(stat, origin_x, origin_y,
                                               forward_x, forward_y)) {
            continue;
        }
        uint8_t picked_up = 0;
        if (wl_try_pickup_static_bonus(state, stat, &picked_up) != 0) {
            return -1;
        }
        if (out_picked_up) {
            *out_picked_up = picked_up;
        }
        if (out_static_index) {
            *out_static_index = i;
        }
        return 0;
    }
    return 0;
}

int wl_init_player_motion_from_spawn(const wl_game_model *model,
                                     wl_player_motion_state *motion) {
    if (!model || !motion || !model->player.present ||
        model->player.x >= WL_MAP_SIDE || model->player.y >= WL_MAP_SIDE) {
        return -1;
    }
    motion->tile_x = model->player.x;
    motion->tile_y = model->player.y;
    motion->x = ((uint32_t)model->player.x << 16) + 0x8000u;
    motion->y = ((uint32_t)model->player.y << 16) + 0x8000u;
    return 0;
}

static int tile_blocks_player_motion(const wl_game_model *model,
                                     uint16_t tile_x, uint16_t tile_y) {
    if (!model || tile_x >= WL_MAP_SIDE || tile_y >= WL_MAP_SIDE) {
        return 1;
    }
    uint16_t tile = model->tilemap[(size_t)tile_y * WL_MAP_SIDE + tile_x];
    if (tile != 0 && tile != 0x40u) {
        return 1;
    }
    for (size_t i = 0; i < model->static_count; ++i) {
        const wl_static_desc *stat = &model->statics[i];
        if (stat->active && stat->blocking && stat->x == tile_x && stat->y == tile_y) {
            return 1;
        }
    }
    return 0;
}

static int player_position_is_clear(const wl_game_model *model, uint32_t x, uint32_t y) {
    const uint32_t player_size = 0x5800u;
    if (!model || x < player_size || y < player_size ||
        x + player_size >= ((uint32_t)WL_MAP_SIDE << 16) ||
        y + player_size >= ((uint32_t)WL_MAP_SIDE << 16)) {
        return 0;
    }
    uint16_t xl = (uint16_t)((x - player_size) >> 16);
    uint16_t yl = (uint16_t)((y - player_size) >> 16);
    uint16_t xh = (uint16_t)((x + player_size) >> 16);
    uint16_t yh = (uint16_t)((y + player_size) >> 16);
    for (uint16_t ty = yl; ty <= yh; ++ty) {
        for (uint16_t tx = xl; tx <= xh; ++tx) {
            if (tile_blocks_player_motion(model, tx, ty)) {
                return 0;
            }
        }
    }
    return 1;
}

static void update_player_motion_tiles(wl_player_motion_state *motion) {
    motion->tile_x = (uint16_t)(motion->x >> 16);
    motion->tile_y = (uint16_t)(motion->y >> 16);
}

static size_t gameplay_map_index(size_t x, size_t y) {
    return y * WL_MAP_SIDE + x;
}

int wl_step_player_motion(wl_player_gameplay_state *state, wl_game_model *model,
                          wl_player_motion_state *motion,
                          int32_t xmove, int32_t ymove,
                          int32_t forward_x, int32_t forward_y,
                          wl_player_step_result *out) {
    if (!state || !model || !motion || !out || (forward_x == 0 && forward_y == 0) ||
        motion->x >= ((uint32_t)WL_MAP_SIDE << 16) ||
        motion->y >= ((uint32_t)WL_MAP_SIDE << 16)) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->static_index = model->static_count;
    uint32_t base_x = motion->x;
    uint32_t base_y = motion->y;
    uint32_t target_x = (uint32_t)((int64_t)base_x + xmove);
    uint32_t target_y = (uint32_t)((int64_t)base_y + ymove);

    if (player_position_is_clear(model, target_x, target_y)) {
        motion->x = target_x;
        motion->y = target_y;
    } else if (player_position_is_clear(model, target_x, base_y)) {
        motion->x = target_x;
        out->blocked = 1;
    } else if (player_position_is_clear(model, base_x, target_y)) {
        motion->y = target_y;
        out->blocked = 1;
    } else {
        out->blocked = 1;
    }

    out->moved = motion->x != base_x || motion->y != base_y;
    update_player_motion_tiles(motion);

    uint8_t picked_up = 0;
    size_t static_index = model->static_count;
    if (wl_try_pickup_visible_static_bonus(state, model, motion->x, motion->y,
                                           forward_x, forward_y,
                                           &picked_up, &static_index) != 0) {
        return -1;
    }
    out->x = motion->x;
    out->y = motion->y;
    out->tile_x = motion->tile_x;
    out->tile_y = motion->tile_y;
    out->picked_up = picked_up;
    out->static_index = static_index;
    return 0;
}

static int use_target_for_facing(uint16_t tile_x, uint16_t tile_y, wl_direction facing,
                                 uint16_t *out_x, uint16_t *out_y,
                                 uint8_t *out_elevator_ok) {
    if (!out_x || !out_y || !out_elevator_ok || tile_x >= WL_MAP_SIDE || tile_y >= WL_MAP_SIDE) {
        return -1;
    }
    *out_x = tile_x;
    *out_y = tile_y;
    *out_elevator_ok = 0;
    switch (facing) {
    case WL_DIR_EAST:
        if (tile_x + 1 >= WL_MAP_SIDE) {
            return -1;
        }
        *out_x = (uint16_t)(tile_x + 1u);
        *out_elevator_ok = 1;
        return 0;
    case WL_DIR_NORTH:
        if (tile_y == 0) {
            return -1;
        }
        *out_y = (uint16_t)(tile_y - 1u);
        return 0;
    case WL_DIR_WEST:
        if (tile_x == 0) {
            return -1;
        }
        *out_x = (uint16_t)(tile_x - 1u);
        *out_elevator_ok = 1;
        return 0;
    case WL_DIR_SOUTH:
        if (tile_y + 1 >= WL_MAP_SIDE) {
            return -1;
        }
        *out_y = (uint16_t)(tile_y + 1u);
        return 0;
    default:
        return -1;
    }
}

int wl_use_player_facing(wl_player_gameplay_state *state, wl_game_model *model,
                         const uint16_t *wall_plane, const uint16_t *info_plane,
                         size_t word_count, const wl_player_motion_state *motion,
                         wl_direction facing, int button_held,
                         wl_player_use_result *out) {
    if (!state || !model || !wall_plane || !info_plane || !motion || !out ||
        word_count != WL_MAP_PLANE_WORDS || motion->tile_x >= WL_MAP_SIDE ||
        motion->tile_y >= WL_MAP_SIDE) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->door_index = model->door_count;
    out->pushwall_index = model->pushwall_count;
    out->dir = facing;
    if (use_target_for_facing(motion->tile_x, motion->tile_y, facing,
                              &out->check_x, &out->check_y,
                              &out->elevator_ok) != 0) {
        return -1;
    }

    size_t target = gameplay_map_index(out->check_x, out->check_y);
    if (info_plane[target] == WL_PUSHABLETILE) {
        out->kind = WL_USE_PUSHWALL;
        for (size_t i = 0; i < model->pushwall_count; ++i) {
            if (model->pushwalls[i].x == out->check_x && model->pushwalls[i].y == out->check_y) {
                out->pushwall_index = i;
                break;
            }
        }
        wl_pushwall_step_result push_result;
        if (wl_start_pushwall(state, model, out->check_x, out->check_y,
                              facing, &push_result) != 0) {
            return -1;
        }
        out->opened = push_result.started;
        out->locked = push_result.blocked;
        return 0;
    }

    uint16_t doornum = model->tilemap[target];
    if (!button_held && doornum == WL_ELEVATORTILE && out->elevator_ok) {
        out->kind = WL_USE_ELEVATOR;
        model->tilemap[target] = (uint16_t)(model->tilemap[target] + 1u);
        state->play_state = WL_PLAYER_PLAY_COMPLETED;
        out->completed = 1;
        out->secret_level = wall_plane[gameplay_map_index(motion->tile_x, motion->tile_y)] ==
                            WL_ALTELEVATORTILE;
        return 0;
    }

    if (!button_held && (doornum & 0x80u)) {
        size_t door = (size_t)(doornum & (uint16_t)~0x80u);
        if (door >= model->door_count) {
            return -1;
        }
        out->kind = WL_USE_DOOR;
        out->door_index = door;
        uint8_t lock = model->doors[door].lock;
        if (lock >= 1u && lock <= 4u && !(state->keys & (1u << (lock - 1u)))) {
            out->locked = 1;
            return 0;
        }
        switch (model->doors[door].action) {
        case WL_DOOR_CLOSED:
        case WL_DOOR_CLOSING:
            model->doors[door].action = WL_DOOR_OPENING;
            out->opened = 1;
            break;
        case WL_DOOR_OPEN:
        case WL_DOOR_OPENING:
            model->doors[door].action = WL_DOOR_CLOSING;
            out->closed = 1;
            break;
        }
        return 0;
    }

    out->kind = WL_USE_NOTHING;
    return 0;
}

static int door_player_blocks_close(const wl_door_desc *door,
                                    const wl_player_motion_state *motion) {
    const uint32_t player_size = 0x5800u;
    if (!door || !motion) {
        return 0;
    }
    if (motion->tile_x == door->x && motion->tile_y == door->y) {
        return 1;
    }
    if (door->vertical) {
        if (motion->tile_y == door->y &&
            (((motion->x + player_size) >> 16) == door->x ||
             (motion->x >= player_size && ((motion->x - player_size) >> 16) == door->x))) {
            return 1;
        }
    } else {
        if (motion->tile_x == door->x &&
            (((motion->y + player_size) >> 16) == door->y ||
             (motion->y >= player_size && ((motion->y - player_size) >> 16) == door->y))) {
            return 1;
        }
    }
    return 0;
}

static int direction_delta(wl_direction dir, int *dx, int *dy) {
    if (!dx || !dy) {
        return -1;
    }
    *dx = 0;
    *dy = 0;
    switch (dir) {
    case WL_DIR_NORTH:
        *dy = -1;
        return 0;
    case WL_DIR_EAST:
        *dx = 1;
        return 0;
    case WL_DIR_SOUTH:
        *dy = 1;
        return 0;
    case WL_DIR_WEST:
        *dx = -1;
        return 0;
    default:
        return -1;
    }
}

static int pushwall_destination(uint16_t x, uint16_t y, wl_direction dir,
                                uint16_t *out_x, uint16_t *out_y) {
    int dx = 0;
    int dy = 0;
    if (!out_x || !out_y || direction_delta(dir, &dx, &dy) != 0) {
        return -1;
    }
    int nx = (int)x + dx;
    int ny = (int)y + dy;
    if (nx < 0 || ny < 0 || nx >= WL_MAP_SIDE || ny >= WL_MAP_SIDE) {
        return -1;
    }
    *out_x = (uint16_t)nx;
    *out_y = (uint16_t)ny;
    return 0;
}

static void fill_pushwall_result(const wl_game_model *model, wl_pushwall_step_result *out) {
    if (!out) {
        return;
    }
    if (model) {
        out->active = model->pushwall_motion.active;
        out->x = model->pushwall_motion.x;
        out->y = model->pushwall_motion.y;
        out->state = model->pushwall_motion.state;
        out->pos = model->pushwall_motion.pos;
    }
}

int wl_step_doors(wl_game_model *model, const wl_player_motion_state *motion,
                  int32_t tics, wl_door_step_result *out) {
    if (!model || !out || tics < 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < model->door_count; ++i) {
        wl_door_desc *door = &model->doors[i];
        size_t idx = gameplay_map_index(door->x, door->y);
        switch (door->action) {
        case WL_DOOR_OPEN:
            door->ticcount += tics;
            if (door->ticcount >= 300) {
                door->ticcount = 0;
                door->action = WL_DOOR_CLOSING;
                model->tilemap[idx] = (uint16_t)(i | 0x80u);
                ++out->restored_collision_count;
                ++out->closing_count;
            } else {
                ++out->open_count;
            }
            break;
        case WL_DOOR_OPENING: {
            uint32_t position = door->position;
            position += (uint32_t)tics << 10;
            if (position >= 0xffffu) {
                door->position = 0xffffu;
                door->ticcount = 0;
                door->action = WL_DOOR_OPEN;
                model->tilemap[idx] = 0;
                ++out->released_collision_count;
                ++out->open_count;
            } else {
                door->position = (uint16_t)position;
                ++out->opening_count;
            }
            break;
        }
        case WL_DOOR_CLOSING:
            if (door_player_blocks_close(door, motion)) {
                door->action = WL_DOOR_OPENING;
                door->ticcount = 0;
                ++out->reopened_blocked_count;
                ++out->opening_count;
                break;
            }
            if (((uint32_t)tics << 10) >= door->position) {
                door->position = 0;
                door->ticcount = 0;
                door->action = WL_DOOR_CLOSED;
                model->tilemap[idx] = (uint16_t)(i | 0x80u);
                ++out->closed_count;
            } else {
                door->position = (uint16_t)(door->position - ((uint32_t)tics << 10));
                ++out->closing_count;
            }
            break;
        case WL_DOOR_CLOSED:
            ++out->closed_count;
            break;
        }
    }
    return 0;
}

int wl_start_pushwall(wl_player_gameplay_state *state, wl_game_model *model,
                      uint16_t x, uint16_t y, wl_direction dir,
                      wl_pushwall_step_result *out) {
    if (!state || !model || x >= WL_MAP_SIDE || y >= WL_MAP_SIDE) {
        return -1;
    }
    if (out) {
        memset(out, 0, sizeof(*out));
    }
    fill_pushwall_result(model, out);
    if (model->pushwall_motion.active) {
        return 0;
    }

    size_t idx = gameplay_map_index(x, y);
    uint16_t oldtile = model->tilemap[idx];
    if (!oldtile) {
        return 0;
    }

    uint16_t dest_x = 0;
    uint16_t dest_y = 0;
    if (pushwall_destination(x, y, dir, &dest_x, &dest_y) != 0) {
        return -1;
    }
    if (tile_blocks_player_motion(model, dest_x, dest_y)) {
        if (out) {
            out->blocked = 1;
        }
        return 0;
    }

    model->tilemap[gameplay_map_index(dest_x, dest_y)] = oldtile;
    model->pushwall_motion.active = 1;
    model->pushwall_motion.state = 1;
    model->pushwall_motion.pos = 0;
    model->pushwall_motion.x = x;
    model->pushwall_motion.y = y;
    model->pushwall_motion.dir = dir;
    model->pushwall_motion.tile = (uint16_t)(oldtile & 63u);
    model->pushwall_motion.marker_index = model->pushwall_count;
    for (size_t i = 0; i < model->pushwall_count; ++i) {
        if (model->pushwalls[i].x == x && model->pushwalls[i].y == y) {
            model->pushwall_motion.marker_index = i;
            break;
        }
    }
    model->tilemap[idx] = (uint16_t)(oldtile | 0xc0u);
    ++state->secret_count;

    fill_pushwall_result(model, out);
    if (out) {
        out->started = 1;
    }
    return 0;
}

int wl_step_pushwall(wl_game_model *model, int32_t tics,
                     wl_pushwall_step_result *out) {
    if (!model || tics < 0) {
        return -1;
    }
    if (out) {
        memset(out, 0, sizeof(*out));
    }
    fill_pushwall_result(model, out);
    if (!model->pushwall_motion.active) {
        return 0;
    }

    uint16_t oldblock = (uint16_t)(model->pushwall_motion.state / 128u);
    model->pushwall_motion.state = (uint16_t)(model->pushwall_motion.state + (uint16_t)tics);
    if (model->pushwall_motion.state / 128u != oldblock) {
        if (out) {
            out->crossed_tile = 1;
        }
        uint16_t oldtile = (uint16_t)(model->tilemap[gameplay_map_index(model->pushwall_motion.x, model->pushwall_motion.y)] & 63u);
        if (!oldtile) {
            oldtile = model->pushwall_motion.tile;
        }
        model->tilemap[gameplay_map_index(model->pushwall_motion.x, model->pushwall_motion.y)] = 0;

        if (model->pushwall_motion.state > 256u) {
            model->pushwall_motion.active = 0;
            model->pushwall_motion.state = 0;
            model->pushwall_motion.pos = 0;
            fill_pushwall_result(model, out);
            if (out) {
                out->crossed_tile = 1;
                out->stopped = 1;
            }
            return 0;
        }

        uint16_t next_x = 0;
        uint16_t next_y = 0;
        if (pushwall_destination(model->pushwall_motion.x, model->pushwall_motion.y,
                                 model->pushwall_motion.dir, &next_x, &next_y) != 0) {
            return -1;
        }
        model->pushwall_motion.x = next_x;
        model->pushwall_motion.y = next_y;

        uint16_t beyond_x = 0;
        uint16_t beyond_y = 0;
        if (pushwall_destination(next_x, next_y, model->pushwall_motion.dir,
                                 &beyond_x, &beyond_y) != 0 ||
            tile_blocks_player_motion(model, beyond_x, beyond_y)) {
            model->pushwall_motion.active = 0;
            model->pushwall_motion.state = 0;
            model->pushwall_motion.pos = 0;
            fill_pushwall_result(model, out);
            if (out) {
                out->crossed_tile = 1;
                out->blocked = 1;
                out->stopped = 1;
            }
            return 0;
        }
        model->tilemap[gameplay_map_index(beyond_x, beyond_y)] = oldtile;
        model->tilemap[gameplay_map_index(next_x, next_y)] = (uint16_t)(oldtile | 0xc0u);
    }

    model->pushwall_motion.pos = (uint16_t)((model->pushwall_motion.state / 2u) & 63u);
    fill_pushwall_result(model, out);
    return 0;
}

int wl_step_live_tick(wl_player_gameplay_state *state, wl_game_model *model,
                      const uint16_t *wall_plane, const uint16_t *info_plane,
                      size_t word_count, wl_player_motion_state *motion,
                      int32_t xmove, int32_t ymove,
                      int32_t forward_x, int32_t forward_y,
                      wl_direction facing, int use_button, int button_held,
                      int32_t tics, wl_live_tick_result *out) {
    if (!state || !model || !wall_plane || !info_plane || !motion || !out || tics < 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->use.door_index = model->door_count;
    out->use.pushwall_index = model->pushwall_count;

    if (wl_step_player_motion(state, model, motion, xmove, ymove,
                              forward_x, forward_y, &out->motion) != 0) {
        return -1;
    }

    if (use_button) {
        if (wl_use_player_facing(state, model, wall_plane, info_plane, word_count,
                                 motion, facing, button_held, &out->use) != 0) {
            return -1;
        }
        out->used = 1;
    }

    if (wl_step_doors(model, motion, tics, &out->doors) != 0) {
        return -1;
    }
    if (wl_step_pushwall(model, tics, &out->pushwall) != 0) {
        return -1;
    }
    if (wl_step_player_attack_state(state, tics, &out->attack) != 0) {
        return -1;
    }
    if (wl_update_palette_shift_state(&state->palette_shift, tics,
                                      &out->palette) != 0) {
        return -1;
    }
    return 0;
}

int wl_step_live_player_fire_tick(wl_player_gameplay_state *state,
                                  wl_game_model *model,
                                  const uint16_t *wall_plane,
                                  const uint16_t *info_plane,
                                  size_t word_count,
                                  wl_player_motion_state *motion,
                                  int32_t xmove, int32_t ymove,
                                  int32_t forward_x, int32_t forward_y,
                                  wl_direction facing, int use_button,
                                  int button_held, int fire_button,
                                  wl_weapon_type requested_weapon,
                                  int32_t tics,
                                  wl_live_player_fire_tick_result *out) {
    if (!state || !model || !wall_plane || !info_plane || !motion || !out ||
        tics < 0 || requested_weapon > WL_WEAPON_CHAINGUN) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    if (wl_step_live_tick(state, model, wall_plane, info_plane, word_count,
                          motion, xmove, ymove, forward_x, forward_y, facing,
                          use_button, button_held, tics, &out->live) != 0) {
        return -1;
    }
    if (fire_button) {
        if (wl_try_player_fire_weapon(state, requested_weapon, &out->fire) != 0) {
            return -1;
        }
        out->fire_attempted = 1;
    }
    return 0;
}

int wl_step_live_actor_ai_tick(wl_player_gameplay_state *state,
                               wl_game_model *model,
                               const uint16_t *wall_plane,
                               const uint16_t *info_plane,
                               size_t word_count,
                               wl_player_motion_state *motion,
                               int32_t xmove, int32_t ymove,
                               int32_t forward_x, int32_t forward_y,
                               wl_direction facing, int use_button,
                               int button_held,
                               uint32_t patrol_speed,
                               int32_t tics,
                               wl_live_actor_ai_tick_result *out) {
    if (!state || !model || !wall_plane || !info_plane || !motion || !out ||
        tics < 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (wl_step_live_tick(state, model, wall_plane, info_plane, word_count,
                          motion, xmove, ymove, forward_x, forward_y, facing,
                          use_button, button_held, tics, &out->live) != 0) {
        return -1;
    }
    if (wl_step_patrol_actors_tics(model, patrol_speed, tics, &out->patrols) != 0) {
        return -1;
    }
    if (wl_step_chase_actors_tics(model, motion->tile_x, motion->tile_y, 1,
                                  patrol_speed, tics, &out->chases) != 0) {
        return -1;
    }
    out->patrols_stepped = out->patrols.tiles_stepped > 0 ? 1u : 0u;
    out->chases_stepped = out->chases.tiles_stepped > 0 ? 1u : 0u;
    return 0;
}

int wl_step_live_projectile_tick(wl_player_gameplay_state *state,
                                 wl_game_model *model,
                                 const uint16_t *wall_plane,
                                 const uint16_t *info_plane,
                                 size_t word_count,
                                 wl_player_motion_state *motion,
                                 int32_t xmove, int32_t ymove,
                                 int32_t forward_x, int32_t forward_y,
                                 wl_direction facing, int use_button,
                                 int button_held,
                                 wl_projectile_state *projectile,
                                 wl_difficulty difficulty,
                                 int32_t projectile_xmove,
                                 int32_t projectile_ymove,
                                 uint8_t projectile_damage_roll,
                                 int god_mode, int victory_flag,
                                 int32_t tics,
                                 wl_live_projectile_tick_result *out) {
    if (!state || !model || !wall_plane || !info_plane || !motion || !out ||
        tics < 0 || difficulty > WL_DIFFICULTY_HARD) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->live.use.door_index = model->door_count;
    out->live.use.pushwall_index = model->pushwall_count;

    if (wl_step_player_motion(state, model, motion, xmove, ymove,
                              forward_x, forward_y, &out->live.motion) != 0) {
        return -1;
    }

    if (use_button) {
        if (wl_use_player_facing(state, model, wall_plane, info_plane, word_count,
                                 motion, facing, button_held,
                                 &out->live.use) != 0) {
            return -1;
        }
        out->live.used = 1;
    }

    if (wl_step_doors(model, motion, tics, &out->live.doors) != 0) {
        return -1;
    }
    if (wl_step_pushwall(model, tics, &out->live.pushwall) != 0) {
        return -1;
    }
    if (projectile && projectile->active) {
        if (wl_step_projectile(state, model, motion, projectile, difficulty,
                               projectile_xmove, projectile_ymove,
                               projectile_damage_roll, god_mode, victory_flag,
                               &out->projectile) != 0) {
            return -1;
        }
        out->projectile_stepped = 1;
    }
    if (wl_update_palette_shift_state(&state->palette_shift, tics,
                                      &out->live.palette) != 0) {
        return -1;
    }
    return 0;
}

int wl_step_live_actor_tick(wl_player_gameplay_state *state,
                            wl_game_model *model,
                            const uint16_t *wall_plane,
                            const uint16_t *info_plane,
                            size_t word_count,
                            wl_player_motion_state *motion,
                            int32_t xmove, int32_t ymove,
                            int32_t forward_x, int32_t forward_y,
                            wl_direction facing, int use_button,
                            int button_held,
                            const wl_actor_desc *actor,
                            wl_difficulty difficulty,
                            int area_active, int line_of_sight,
                            int player_running, int actor_visible,
                            uint8_t chance_roll, uint8_t damage_roll,
                            int god_mode, int victory_flag,
                            int32_t tics,
                            wl_live_actor_tick_result *out) {
    if (!state || !model || !wall_plane || !info_plane || !motion || !out ||
        tics < 0 || difficulty > WL_DIFFICULTY_HARD) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->live.use.door_index = model->door_count;
    out->live.use.pushwall_index = model->pushwall_count;

    if (wl_step_player_motion(state, model, motion, xmove, ymove,
                              forward_x, forward_y, &out->live.motion) != 0) {
        return -1;
    }

    if (use_button) {
        if (wl_use_player_facing(state, model, wall_plane, info_plane, word_count,
                                 motion, facing, button_held,
                                 &out->live.use) != 0) {
            return -1;
        }
        out->live.used = 1;
    }

    if (wl_step_doors(model, motion, tics, &out->live.doors) != 0) {
        return -1;
    }
    if (wl_step_pushwall(model, tics, &out->live.pushwall) != 0) {
        return -1;
    }
    if (actor) {
        if (actor->kind == WL_ACTOR_DOG) {
            if (wl_try_actor_bite_player(state, actor, motion, difficulty,
                                         chance_roll, damage_roll,
                                         god_mode, victory_flag,
                                         &out->bite) != 0) {
                return -1;
            }
            out->attack_kind = WL_LIVE_ACTOR_ATTACK_BITE;
            out->actor_attacked = 1;
        } else if (actor_can_shoot(actor)) {
            if (wl_try_actor_shoot_player(state, actor, motion, difficulty,
                                          area_active, line_of_sight,
                                          player_running, actor_visible,
                                          chance_roll, damage_roll,
                                          god_mode, victory_flag,
                                          &out->shot) != 0) {
                return -1;
            }
            out->attack_kind = WL_LIVE_ACTOR_ATTACK_SHOOT;
            out->actor_attacked = 1;
        }
    }
    if (wl_update_palette_shift_state(&state->palette_shift, tics,
                                      &out->live.palette) != 0) {
        return -1;
    }
    return 0;
}

int wl_step_live_combat_tick(wl_player_gameplay_state *state,
                             wl_game_model *model,
                             const uint16_t *wall_plane,
                             const uint16_t *info_plane,
                             size_t word_count,
                             wl_player_motion_state *motion,
                             int32_t xmove, int32_t ymove,
                             int32_t forward_x, int32_t forward_y,
                             wl_direction facing, int use_button,
                             int button_held,
                             const wl_actor_desc *actor,
                             wl_projectile_state *projectile,
                             wl_difficulty difficulty,
                             int area_active, int line_of_sight,
                             int player_running, int actor_visible,
                             uint8_t actor_chance_roll,
                             uint8_t actor_damage_roll,
                             int32_t projectile_xmove,
                             int32_t projectile_ymove,
                             uint8_t projectile_damage_roll,
                             int god_mode, int victory_flag,
                             int32_t tics,
                             wl_live_combat_tick_result *out) {
    if (!state || !model || !wall_plane || !info_plane || !motion || !out ||
        tics < 0 || difficulty > WL_DIFFICULTY_HARD) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->live.use.door_index = model->door_count;
    out->live.use.pushwall_index = model->pushwall_count;

    if (wl_step_player_motion(state, model, motion, xmove, ymove,
                              forward_x, forward_y, &out->live.motion) != 0) {
        return -1;
    }

    if (use_button) {
        if (wl_use_player_facing(state, model, wall_plane, info_plane, word_count,
                                 motion, facing, button_held,
                                 &out->live.use) != 0) {
            return -1;
        }
        out->live.used = 1;
    }

    if (wl_step_doors(model, motion, tics, &out->live.doors) != 0) {
        return -1;
    }
    if (wl_step_pushwall(model, tics, &out->live.pushwall) != 0) {
        return -1;
    }
    if (actor) {
        if (actor->kind == WL_ACTOR_DOG) {
            if (wl_try_actor_bite_player(state, actor, motion, difficulty,
                                         actor_chance_roll, actor_damage_roll,
                                         god_mode, victory_flag,
                                         &out->bite) != 0) {
                return -1;
            }
            out->actor_attack_kind = WL_LIVE_ACTOR_ATTACK_BITE;
            out->actor_attacked = 1;
        } else if (actor_can_shoot(actor)) {
            if (wl_try_actor_shoot_player(state, actor, motion, difficulty,
                                          area_active, line_of_sight,
                                          player_running, actor_visible,
                                          actor_chance_roll, actor_damage_roll,
                                          god_mode, victory_flag,
                                          &out->shot) != 0) {
                return -1;
            }
            out->actor_attack_kind = WL_LIVE_ACTOR_ATTACK_SHOOT;
            out->actor_attacked = 1;
        }
    }
    if (projectile && projectile->active) {
        if (wl_step_projectile(state, model, motion, projectile, difficulty,
                               projectile_xmove, projectile_ymove,
                               projectile_damage_roll, god_mode, victory_flag,
                               &out->projectile) != 0) {
            return -1;
        }
        out->projectile_stepped = 1;
    }
    if (wl_update_palette_shift_state(&state->palette_shift, tics,
                                      &out->live.palette) != 0) {
        return -1;
    }
    return 0;
}

int wl_step_live_actor_damage_tick(wl_player_gameplay_state *state,
                                   wl_game_model *model,
                                   const uint16_t *wall_plane,
                                   const uint16_t *info_plane,
                                   size_t word_count,
                                   wl_player_motion_state *motion,
                                   int32_t xmove, int32_t ymove,
                                   int32_t forward_x, int32_t forward_y,
                                   wl_direction facing, int use_button,
                                   int button_held,
                                   wl_actor_combat_state *actor,
                                   int32_t damage_points,
                                   int32_t tics,
                                   wl_live_actor_damage_tick_result *out) {
    if (!state || !model || !wall_plane || !info_plane || !motion || !out ||
        tics < 0 || damage_points < 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->drop_static_index = model->static_count;
    out->live.use.door_index = model->door_count;
    out->live.use.pushwall_index = model->pushwall_count;

    if (wl_step_player_motion(state, model, motion, xmove, ymove,
                              forward_x, forward_y, &out->live.motion) != 0) {
        return -1;
    }

    if (use_button) {
        if (wl_use_player_facing(state, model, wall_plane, info_plane, word_count,
                                 motion, facing, button_held,
                                 &out->live.use) != 0) {
            return -1;
        }
        out->live.used = 1;
    }

    if (wl_step_doors(model, motion, tics, &out->live.doors) != 0) {
        return -1;
    }
    if (wl_step_pushwall(model, tics, &out->live.pushwall) != 0) {
        return -1;
    }
    if (actor) {
        if (wl_apply_actor_damage(state, actor, damage_points,
                                  &out->actor_damage) != 0) {
            return -1;
        }
        out->actor_damaged = 1;
        size_t before_drop_count = model->static_count;
        if (wl_spawn_actor_drop_static(model, actor, &out->actor_damage,
                                       &out->drop_static_index) != 0) {
            return -1;
        }
        out->drop_spawned = model->static_count > before_drop_count ? 1u : 0u;
    }
    if (wl_update_palette_shift_state(&state->palette_shift, tics,
                                      &out->live.palette) != 0) {
        return -1;
    }
    return 0;
}

int wl_step_live_full_combat_tick(wl_player_gameplay_state *state,
                                  wl_game_model *model,
                                  const uint16_t *wall_plane,
                                  const uint16_t *info_plane,
                                  size_t word_count,
                                  wl_player_motion_state *motion,
                                  int32_t xmove, int32_t ymove,
                                  int32_t forward_x, int32_t forward_y,
                                  wl_direction facing, int use_button,
                                  int button_held,
                                  const wl_actor_desc *attacker,
                                  wl_projectile_state *projectile,
                                  wl_actor_combat_state *damage_actor,
                                  int32_t damage_actor_points,
                                  uint16_t damage_actor_model_index,
                                  uint16_t vswap_sprite_start,
                                  wl_difficulty difficulty,
                                  int area_active, int line_of_sight,
                                  int player_running, int actor_visible,
                                  uint8_t actor_chance_roll,
                                  uint8_t actor_damage_roll,
                                  int32_t projectile_xmove,
                                  int32_t projectile_ymove,
                                  uint8_t projectile_damage_roll,
                                  int god_mode, int victory_flag,
                                  int32_t tics,
                                  wl_live_full_combat_tick_result *out) {
    if (!state || !model || !wall_plane || !info_plane || !motion || !out ||
        tics < 0 || damage_actor_points < 0 || difficulty > WL_DIFFICULTY_HARD) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->drop_static_index = model->static_count;
    out->live.use.door_index = model->door_count;
    out->live.use.pushwall_index = model->pushwall_count;

    if (wl_step_player_motion(state, model, motion, xmove, ymove,
                              forward_x, forward_y, &out->live.motion) != 0) {
        return -1;
    }

    if (use_button) {
        if (wl_use_player_facing(state, model, wall_plane, info_plane, word_count,
                                 motion, facing, button_held,
                                 &out->live.use) != 0) {
            return -1;
        }
        out->live.used = 1;
    }

    if (wl_step_doors(model, motion, tics, &out->live.doors) != 0) {
        return -1;
    }
    if (wl_step_pushwall(model, tics, &out->live.pushwall) != 0) {
        return -1;
    }
    if (damage_actor) {
        if (wl_apply_actor_damage(state, damage_actor, damage_actor_points,
                                  &out->actor_damage) != 0) {
            return -1;
        }
        out->actor_damaged = 1;
        size_t before_drop_count = model->static_count;
        if (wl_spawn_actor_drop_static(model, damage_actor, &out->actor_damage,
                                       &out->drop_static_index) != 0) {
            return -1;
        }
        out->drop_spawned = model->static_count > before_drop_count ? 1u : 0u;
        if (out->actor_damage.killed) {
            if (wl_start_actor_death_state(damage_actor, &out->actor_damage,
                                           &out->actor_death) != 0) {
                return -1;
            }
            out->death_started = 1;
            if (wl_build_actor_death_scene_ref(damage_actor, &out->actor_death,
                                               vswap_sprite_start,
                                               damage_actor_model_index,
                                               &out->actor_death_ref) != 0) {
                return -1;
            }
            out->death_ref_built = 1;
        }
    }
    if (attacker) {
        if (attacker->kind == WL_ACTOR_DOG) {
            if (wl_try_actor_bite_player(state, attacker, motion, difficulty,
                                         actor_chance_roll, actor_damage_roll,
                                         god_mode, victory_flag,
                                         &out->bite) != 0) {
                return -1;
            }
            out->actor_attack_kind = WL_LIVE_ACTOR_ATTACK_BITE;
            out->actor_attacked = 1;
        } else if (actor_can_shoot(attacker)) {
            if (wl_try_actor_shoot_player(state, attacker, motion, difficulty,
                                          area_active, line_of_sight,
                                          player_running, actor_visible,
                                          actor_chance_roll, actor_damage_roll,
                                          god_mode, victory_flag,
                                          &out->shot) != 0) {
                return -1;
            }
            out->actor_attack_kind = WL_LIVE_ACTOR_ATTACK_SHOOT;
            out->actor_attacked = 1;
        }
    }
    if (projectile && projectile->active) {
        if (wl_step_projectile(state, model, motion, projectile, difficulty,
                               projectile_xmove, projectile_ymove,
                               projectile_damage_roll, god_mode, victory_flag,
                               &out->projectile) != 0) {
            return -1;
        }
        out->projectile_stepped = 1;
    }
    if (wl_update_palette_shift_state(&state->palette_shift, tics,
                                      &out->live.palette) != 0) {
        return -1;
    }
    return 0;
}

int wl_step_live_full_combat_death_tick(wl_player_gameplay_state *state,
                                        wl_game_model *model,
                                        const uint16_t *wall_plane,
                                        const uint16_t *info_plane,
                                        size_t word_count,
                                        wl_player_motion_state *motion,
                                        int32_t xmove, int32_t ymove,
                                        int32_t forward_x, int32_t forward_y,
                                        wl_direction facing, int use_button,
                                        int button_held,
                                        const wl_actor_desc *attacker,
                                        wl_projectile_state *projectile,
                                        wl_actor_combat_state *damage_actor,
                                        int32_t damage_actor_points,
                                        uint16_t damage_actor_model_index,
                                        uint16_t vswap_sprite_start,
                                        wl_difficulty difficulty,
                                        int area_active, int line_of_sight,
                                        int player_running, int actor_visible,
                                        uint8_t actor_chance_roll,
                                        uint8_t actor_damage_roll,
                                        int32_t projectile_xmove,
                                        int32_t projectile_ymove,
                                        uint8_t projectile_damage_roll,
                                        int god_mode, int victory_flag,
                                        int32_t tics,
                                        uint16_t active_death_model_index,
                                        wl_actor_death_state *active_death,
                                        wl_live_full_combat_death_tick_result *out) {
    if (!out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    if (wl_step_live_full_combat_tick(state, model, wall_plane, info_plane,
                                      word_count, motion, xmove, ymove,
                                      forward_x, forward_y, facing, use_button,
                                      button_held, attacker, projectile,
                                      damage_actor, damage_actor_points,
                                      damage_actor_model_index,
                                      vswap_sprite_start, difficulty,
                                      area_active, line_of_sight,
                                      player_running, actor_visible,
                                      actor_chance_roll, actor_damage_roll,
                                      projectile_xmove, projectile_ymove,
                                      projectile_damage_roll, god_mode,
                                      victory_flag, tics, &out->combat) != 0) {
        return -1;
    }
    if (active_death) {
        if (wl_step_live_actor_death_tick(model, active_death_model_index,
                                          active_death, tics,
                                          vswap_sprite_start, &out->death) != 0) {
            return -1;
        }
        out->death_stepped = 1;
    }
    return 0;
}

int wl_start_player_bonus_flash(wl_player_gameplay_state *state) {
    if (!state) {
        return -1;
    }
    return wl_start_bonus_palette_shift(&state->palette_shift);
}
