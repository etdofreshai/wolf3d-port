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
    WL_PLAYER_PLAY_COMPLETED = 2,
} wl_player_play_state;

typedef enum wl_weapon_type {
    WL_WEAPON_KNIFE = 0,
    WL_WEAPON_PISTOL = 1,
    WL_WEAPON_MACHINEGUN = 2,
    WL_WEAPON_CHAINGUN = 3,
} wl_weapon_type;

typedef enum wl_bonus_item {
    WL_BONUS_DRESSING = 0,
    WL_BONUS_BLOCK = 1,
    WL_BONUS_GIBS = 2,
    WL_BONUS_ALPO = 3,
    WL_BONUS_FIRSTAID = 4,
    WL_BONUS_KEY1 = 5,
    WL_BONUS_KEY2 = 6,
    WL_BONUS_KEY3 = 7,
    WL_BONUS_KEY4 = 8,
    WL_BONUS_CROSS = 9,
    WL_BONUS_CHALICE = 10,
    WL_BONUS_BIBLE = 11,
    WL_BONUS_CROWN = 12,
    WL_BONUS_CLIP = 13,
    WL_BONUS_CLIP2 = 14,
    WL_BONUS_MACHINEGUN = 15,
    WL_BONUS_CHAINGUN = 16,
    WL_BONUS_FOOD = 17,
    WL_BONUS_FULLHEAL = 18,
    WL_BONUS_25CLIP = 19,
    WL_BONUS_SPEAR = 20,
} wl_bonus_item;

typedef struct wl_player_gameplay_state {
    int32_t health;
    int32_t score;
    int32_t lives;
    int32_t next_extra;
    int32_t ammo;
    uint32_t keys;
    wl_weapon_type best_weapon;
    wl_weapon_type weapon;
    wl_weapon_type chosen_weapon;
    int32_t attack_frame;
    int32_t treasure_count;
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
int wl_give_player_ammo(wl_player_gameplay_state *state, int32_t ammo);
int wl_give_player_weapon(wl_player_gameplay_state *state, wl_weapon_type weapon);
int wl_give_player_key(wl_player_gameplay_state *state, uint8_t key);
int wl_apply_player_bonus(wl_player_gameplay_state *state, wl_bonus_item item,
                          uint8_t *out_picked_up);
int wl_start_player_bonus_flash(wl_player_gameplay_state *state);

#ifdef __cplusplus
}
#endif

#endif /* WL_GAMEPLAY_H */
