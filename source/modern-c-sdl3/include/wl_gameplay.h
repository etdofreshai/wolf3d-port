#ifndef WL_GAMEPLAY_H
#define WL_GAMEPLAY_H

#include "wl_assets.h"
#include "wl_game_model.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WL_EXTRA_POINTS 40000

typedef enum wl_player_play_state {
    WL_PLAYER_PLAY_RUNNING = 0,
    WL_PLAYER_PLAY_DIED = 1,
} wl_player_play_state;

typedef struct wl_player_gameplay_state {
    int32_t health;
    int32_t score;
    int32_t lives;
    int32_t next_extra;
    uint8_t got_gat_gun;
    wl_player_play_state play_state;
    wl_palette_shift_state palette_shift;
} wl_player_gameplay_state;

typedef struct wl_player_damage_result {
    int32_t requested_points;
    int32_t effective_points;
    int32_t health;
    uint8_t ignored;
    uint8_t died;
    int32_t damage_count;
} wl_player_damage_result;

int wl_init_player_gameplay_state(wl_player_gameplay_state *state,
                                  int32_t health, int32_t lives,
                                  int32_t score, int32_t next_extra);
int wl_apply_player_damage(wl_player_gameplay_state *state,
                           wl_difficulty difficulty, int32_t points,
                           int god_mode, int victory_flag,
                           wl_player_damage_result *out);
int wl_heal_player(wl_player_gameplay_state *state, int32_t points);
int wl_award_player_points(wl_player_gameplay_state *state, int32_t points,
                           int32_t *out_extra_lives_awarded,
                           int32_t *out_thresholds_crossed);
int wl_start_player_bonus_flash(wl_player_gameplay_state *state);

#ifdef __cplusplus
}
#endif

#endif /* WL_GAMEPLAY_H */
