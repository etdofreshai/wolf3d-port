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

int wl_start_player_bonus_flash(wl_player_gameplay_state *state) {
    if (!state) {
        return -1;
    }
    return wl_start_bonus_palette_shift(&state->palette_shift);
}
