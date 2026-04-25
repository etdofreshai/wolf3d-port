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

int wl_start_player_bonus_flash(wl_player_gameplay_state *state) {
    if (!state) {
        return -1;
    }
    return wl_start_bonus_palette_shift(&state->palette_shift);
}
