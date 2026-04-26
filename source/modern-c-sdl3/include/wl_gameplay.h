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

typedef struct wl_player_fire_result {
    wl_weapon_type requested_weapon;
    wl_weapon_type fired_weapon;
    int32_t ammo_before;
    int32_t ammo_after;
    uint8_t fired;
    uint8_t consumed_ammo;
    uint8_t no_ammo;
    uint8_t unavailable;
} wl_player_fire_result;

typedef struct wl_player_attack_step_result {
    int32_t frame_before;
    int32_t frame_after;
    wl_weapon_type weapon_before;
    wl_weapon_type weapon_after;
    uint8_t advanced;
    uint8_t finished;
    uint8_t restored_chosen_weapon;
} wl_player_attack_step_result;

typedef struct wl_player_fire_attack_result {
    wl_player_fire_result fire;
    int32_t attack_frame_before;
    int32_t attack_frame_after;
    uint8_t attack_started;
} wl_player_fire_attack_result;

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

typedef struct wl_actor_shot_damage_result {
    uint8_t area_active;
    uint8_t line_of_sight;
    uint8_t chance_hit;
    uint8_t damaged;
    uint8_t chance_roll;
    uint8_t damage_roll;
    uint16_t distance_tiles;
    int32_t hit_chance;
    wl_player_damage_result damage;
} wl_actor_shot_damage_result;

typedef struct wl_actor_combat_state {
    wl_actor_kind kind;
    uint16_t tile_x;
    uint16_t tile_y;
    int32_t hitpoints;
    uint8_t attack_mode;
    uint8_t shootable;
    uint8_t alive;
} wl_actor_combat_state;

typedef struct wl_actor_damage_result {
    int32_t requested_points;
    int32_t effective_points;
    int32_t hitpoints;
    int32_t score_awarded;
    wl_bonus_item drop_item;
    uint8_t dropped_item;
    uint8_t killed;
    uint8_t attack_mode_started;
    uint8_t pain_state_variant;
    int32_t extra_lives_awarded;
    int32_t score_thresholds_crossed;
} wl_actor_damage_result;

typedef struct wl_actor_death_state {
    wl_actor_kind kind;
    uint8_t stage;
    uint8_t stage_count;
    uint8_t finished;
    uint8_t death_scream;
    int32_t tics_remaining;
    uint16_t sprite_source_index;
} wl_actor_death_state;

typedef struct wl_actor_death_step_result {
    uint8_t advanced;
    uint8_t finished;
    uint8_t death_scream;
    uint8_t stage;
    int32_t tics_remaining;
    uint16_t sprite_source_index;
} wl_actor_death_step_result;

typedef enum wl_projectile_kind {
    WL_PROJECTILE_NEEDLE = 0,
    WL_PROJECTILE_ROCKET = 1,
    WL_PROJECTILE_HROCKET = 2,
    WL_PROJECTILE_SPARK = 3,
    WL_PROJECTILE_FIRE = 4,
} wl_projectile_kind;

typedef struct wl_projectile_state {
    uint32_t x;
    uint32_t y;
    uint16_t tile_x;
    uint16_t tile_y;
    wl_projectile_kind kind;
    uint8_t active;
} wl_projectile_state;

typedef struct wl_projectile_step_result {
    uint8_t moved;
    uint8_t hit_wall;
    uint8_t hit_player;
    uint8_t removed;
    uint8_t damage_roll;
    uint32_t x;
    uint32_t y;
    uint16_t tile_x;
    uint16_t tile_y;
    wl_player_damage_result damage;
} wl_projectile_step_result;

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
    wl_player_attack_step_result attack;
    wl_palette_shift_result palette;
    uint8_t used;
} wl_live_tick_result;

typedef struct wl_live_player_fire_tick_result {
    wl_live_tick_result live;
    wl_player_fire_result fire;
    uint8_t fire_attempted;
} wl_live_player_fire_tick_result;

typedef struct wl_live_projectile_tick_result {
    wl_live_tick_result live;
    wl_projectile_step_result projectile;
    uint8_t projectile_stepped;
} wl_live_projectile_tick_result;

typedef struct wl_live_actor_ai_tick_result {
    wl_live_tick_result live;
    wl_actor_patrols_tic_result patrols;
    wl_actor_chases_tic_result chases;
    uint8_t patrols_stepped;
    uint8_t chases_stepped;
} wl_live_actor_ai_tick_result;

typedef enum wl_live_actor_attack_kind {
    WL_LIVE_ACTOR_ATTACK_NONE = 0,
    WL_LIVE_ACTOR_ATTACK_BITE = 1,
    WL_LIVE_ACTOR_ATTACK_SHOOT = 2,
} wl_live_actor_attack_kind;

typedef struct wl_live_actor_tick_result {
    wl_live_tick_result live;
    wl_actor_contact_damage_result bite;
    wl_actor_shot_damage_result shot;
    wl_live_actor_attack_kind attack_kind;
    uint8_t actor_attacked;
} wl_live_actor_tick_result;

typedef struct wl_live_combat_tick_result {
    wl_live_tick_result live;
    wl_projectile_step_result projectile;
    wl_actor_contact_damage_result bite;
    wl_actor_shot_damage_result shot;
    wl_live_actor_attack_kind actor_attack_kind;
    uint8_t actor_attacked;
    uint8_t projectile_stepped;
} wl_live_combat_tick_result;

typedef struct wl_live_actor_damage_tick_result {
    wl_live_tick_result live;
    wl_actor_damage_result actor_damage;
    size_t drop_static_index;
    uint8_t actor_damaged;
    uint8_t drop_spawned;
} wl_live_actor_damage_tick_result;

typedef struct wl_live_full_combat_tick_result {
    wl_live_tick_result live;
    wl_projectile_step_result projectile;
    wl_actor_contact_damage_result bite;
    wl_actor_shot_damage_result shot;
    wl_actor_damage_result actor_damage;
    wl_actor_death_state actor_death;
    wl_scene_sprite_ref actor_death_ref;
    size_t drop_static_index;
    wl_live_actor_attack_kind actor_attack_kind;
    uint8_t actor_attacked;
    uint8_t projectile_stepped;
    uint8_t actor_damaged;
    uint8_t drop_spawned;
    uint8_t death_started;
    uint8_t death_ref_built;
} wl_live_full_combat_tick_result;

typedef struct wl_live_actor_death_tick_result {
    wl_actor_death_step_result step;
    wl_scene_sprite_ref death_ref;
    uint8_t death_ref_built;
    uint8_t final_frame_applied;
} wl_live_actor_death_tick_result;

typedef struct wl_live_full_combat_death_tick_result {
    wl_live_full_combat_tick_result combat;
    wl_live_actor_death_tick_result death;
    uint8_t death_stepped;
} wl_live_full_combat_death_tick_result;

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
int wl_try_actor_shoot_player(wl_player_gameplay_state *state,
                              const wl_actor_desc *actor,
                              const wl_player_motion_state *player,
                              wl_difficulty difficulty,
                              int area_active, int line_of_sight,
                              int player_running, int actor_visible,
                              uint8_t chance_roll, uint8_t damage_roll,
                              int god_mode, int victory_flag,
                              wl_actor_shot_damage_result *out);
int wl_init_actor_combat_state(const wl_actor_desc *actor,
                               wl_difficulty difficulty,
                               wl_actor_combat_state *out);
int wl_apply_actor_damage(wl_player_gameplay_state *player,
                          wl_actor_combat_state *actor,
                          int32_t points,
                          wl_actor_damage_result *out);
int wl_spawn_actor_drop_static(wl_game_model *model,
                               const wl_actor_combat_state *actor,
                               const wl_actor_damage_result *damage,
                               size_t *out_static_index);
int wl_start_actor_death_state(const wl_actor_combat_state *actor,
                               const wl_actor_damage_result *damage,
                               wl_actor_death_state *out);
int wl_step_actor_death_state(wl_actor_death_state *state, int32_t tics,
                              wl_actor_death_step_result *out);
int wl_build_actor_death_scene_ref(const wl_actor_combat_state *actor,
                                   const wl_actor_death_state *death,
                                   uint16_t vswap_sprite_start,
                                   uint16_t model_index,
                                   wl_scene_sprite_ref *out);
int wl_apply_actor_death_final_frame(wl_game_model *model,
                                     uint16_t model_index,
                                     const wl_actor_death_state *death);
int wl_step_live_actor_death_tick(wl_game_model *model,
                                  uint16_t actor_model_index,
                                  wl_actor_death_state *death,
                                  int32_t tics,
                                  uint16_t vswap_sprite_start,
                                  wl_live_actor_death_tick_result *out);
int wl_step_projectile(wl_player_gameplay_state *state,
                       const wl_game_model *model,
                       const wl_player_motion_state *player,
                       wl_projectile_state *projectile,
                       wl_difficulty difficulty,
                       int32_t xmove, int32_t ymove,
                       uint8_t damage_roll,
                       int god_mode, int victory_flag,
                       wl_projectile_step_result *out);
int wl_heal_player(wl_player_gameplay_state *state, int32_t points);
int wl_award_player_points(wl_player_gameplay_state *state, int32_t points,
                           int32_t *out_extra_lives_awarded,
                           int32_t *out_thresholds_crossed);
int wl_give_player_ammo(wl_player_gameplay_state *state, int32_t ammo);
int wl_give_player_weapon(wl_player_gameplay_state *state, wl_weapon_type weapon);
int wl_try_player_fire_weapon(wl_player_gameplay_state *state,
                              wl_weapon_type requested_weapon,
                              wl_player_fire_result *out);
int wl_start_player_fire_attack(wl_player_gameplay_state *state,
                                wl_weapon_type requested_weapon,
                                wl_player_fire_attack_result *out);
int wl_step_player_attack_state(wl_player_gameplay_state *state, int32_t tics,
                                wl_player_attack_step_result *out);
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
                                  wl_live_player_fire_tick_result *out);
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
                               wl_live_actor_ai_tick_result *out);
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
                                 wl_live_projectile_tick_result *out);
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
                            wl_live_actor_tick_result *out);
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
                             wl_live_combat_tick_result *out);
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
                                   wl_live_actor_damage_tick_result *out);
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
                                  wl_live_full_combat_tick_result *out);
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
                                        wl_live_full_combat_death_tick_result *out);
int wl_start_player_bonus_flash(wl_player_gameplay_state *state);

#ifdef __cplusplus
}
#endif

#endif /* WL_GAMEPLAY_H */
