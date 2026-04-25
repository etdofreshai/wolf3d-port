#include "wl_gameplay.h"

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

int wl_start_player_bonus_flash(wl_player_gameplay_state *state) {
    if (!state) {
        return -1;
    }
    return wl_start_bonus_palette_shift(&state->palette_shift);
}
