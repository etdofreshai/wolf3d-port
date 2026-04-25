#ifndef WL_GAMEPLAY_H
#define WL_GAMEPLAY_H

#include "wl_assets.h"
#include "wl_game_model.h"

#include <stddef.h>
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
    int32_t secret_count;
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

typedef struct wl_actor_contact_damage_result {
    uint8_t in_range;
    uint8_t chance_hit;
    uint8_t damaged;
    uint8_t chance_roll;
    uint8_t damage_roll;
    wl_player_damage_result damage;
} wl_actor_contact_damage_result;

typedef struct wl_player_motion_state {
    uint32_t x;
    uint32_t y;
    uint16_t tile_x;
    uint16_t tile_y;
} wl_player_motion_state;

typedef struct wl_player_step_result {
    uint32_t x;
    uint32_t y;
    uint16_t tile_x;
    uint16_t tile_y;
    uint8_t moved;
    uint8_t blocked;
    uint8_t picked_up;
    size_t static_index;
} wl_player_step_result;

typedef enum wl_player_use_kind {
    WL_USE_NOTHING = 0,
    WL_USE_PUSHWALL = 1,
    WL_USE_ELEVATOR = 2,
    WL_USE_DOOR = 3,
} wl_player_use_kind;

typedef struct wl_player_use_result {
    wl_player_use_kind kind;
    uint16_t check_x;
    uint16_t check_y;
    wl_direction dir;
    uint8_t elevator_ok;
    uint8_t locked;
    uint8_t opened;
    uint8_t closed;
    uint8_t completed;
    uint8_t secret_level;
    size_t door_index;
    size_t pushwall_index;
} wl_player_use_result;

typedef struct wl_door_step_result {
    size_t opening_count;
    size_t open_count;
    size_t closing_count;
    size_t closed_count;
    size_t released_collision_count;
    size_t restored_collision_count;
    size_t reopened_blocked_count;
} wl_door_step_result;

typedef struct wl_pushwall_step_result {
    uint8_t active;
    uint8_t started;
    uint8_t crossed_tile;
    uint8_t stopped;
    uint8_t blocked;
    uint16_t x;
    uint16_t y;
    uint16_t state;
    uint16_t pos;
} wl_pushwall_step_result;

typedef struct wl_live_tick_result {
    wl_player_step_result motion;
    wl_player_use_result use;
    wl_door_step_result doors;
    wl_pushwall_step_result pushwall;
    wl_palette_shift_result palette;
    uint8_t used;
} wl_live_tick_result;

int wl_init_player_gameplay_state(wl_player_gameplay_state *state,
                                  int32_t health, int32_t lives,
                                  int32_t score, int32_t next_extra);
int wl_apply_player_damage(wl_player_gameplay_state *state,
                           wl_difficulty difficulty, int32_t points,
                           int god_mode, int victory_flag,
                           wl_player_damage_result *out);
int wl_try_actor_bite_player(wl_player_gameplay_state *state,
                             const wl_actor_desc *actor,
                             const wl_player_motion_state *player,
                             wl_difficulty difficulty,
                             uint8_t chance_roll, uint8_t damage_roll,
                             int god_mode, int victory_flag,
                             wl_actor_contact_damage_result *out);
int wl_heal_player(wl_player_gameplay_state *state, int32_t points);
int wl_award_player_points(wl_player_gameplay_state *state, int32_t points,
                           int32_t *out_extra_lives_awarded,
                           int32_t *out_thresholds_crossed);
int wl_give_player_ammo(wl_player_gameplay_state *state, int32_t ammo);
int wl_give_player_weapon(wl_player_gameplay_state *state, wl_weapon_type weapon);
int wl_give_player_key(wl_player_gameplay_state *state, uint8_t key);
int wl_apply_player_bonus(wl_player_gameplay_state *state, wl_bonus_item item,
                          uint8_t *out_picked_up);
int wl_try_pickup_static_bonus(wl_player_gameplay_state *state,
                               wl_static_desc *stat,
                               uint8_t *out_picked_up);
int wl_try_pickup_visible_static_bonus(wl_player_gameplay_state *state,
                                       wl_game_model *model,
                                       uint32_t origin_x, uint32_t origin_y,
                                       int32_t forward_x, int32_t forward_y,
                                       uint8_t *out_picked_up,
                                       size_t *out_static_index);
int wl_init_player_motion_from_spawn(const wl_game_model *model,
                                     wl_player_motion_state *motion);
int wl_step_player_motion(wl_player_gameplay_state *state, wl_game_model *model,
                          wl_player_motion_state *motion,
                          int32_t xmove, int32_t ymove,
                          int32_t forward_x, int32_t forward_y,
                          wl_player_step_result *out);
int wl_use_player_facing(wl_player_gameplay_state *state, wl_game_model *model,
                         const uint16_t *wall_plane, const uint16_t *info_plane,
                         size_t word_count, const wl_player_motion_state *motion,
                         wl_direction facing, int button_held,
                         wl_player_use_result *out);
int wl_step_doors(wl_game_model *model, const wl_player_motion_state *motion,
                  int32_t tics, wl_door_step_result *out);
int wl_start_pushwall(wl_player_gameplay_state *state, wl_game_model *model,
                      uint16_t x, uint16_t y, wl_direction dir,
                      wl_pushwall_step_result *out);
int wl_step_pushwall(wl_game_model *model, int32_t tics,
                     wl_pushwall_step_result *out);
int wl_step_live_tick(wl_player_gameplay_state *state, wl_game_model *model,
                      const uint16_t *wall_plane, const uint16_t *info_plane,
                      size_t word_count, wl_player_motion_state *motion,
                      int32_t xmove, int32_t ymove,
                      int32_t forward_x, int32_t forward_y,
                      wl_direction facing, int use_button, int button_held,
                      int32_t tics, wl_live_tick_result *out);
int wl_start_player_bonus_flash(wl_player_gameplay_state *state);

#ifdef __cplusplus
}
#endif

#endif /* WL_GAMEPLAY_H */
