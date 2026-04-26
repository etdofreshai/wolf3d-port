#ifndef WL_GAME_MODEL_H
#define WL_GAME_MODEL_H

#include "wl_map_semantics.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WL_MAX_DOORS 64
#define WL_MAX_STATS 400
#define WL_MAX_ACTORS 150
#define WL_MAX_PATH_MARKERS WL_MAP_PLANE_WORDS
#define WL_MAX_PUSHWALLS WL_MAP_PLANE_WORDS
#define WL_NUM_AREAS 37

typedef enum wl_difficulty {
    WL_DIFFICULTY_BABY = 0,
    WL_DIFFICULTY_EASY = 1,
    WL_DIFFICULTY_MEDIUM = 2,
    WL_DIFFICULTY_HARD = 3,
} wl_difficulty;

typedef enum wl_actor_kind {
    WL_ACTOR_GUARD,
    WL_ACTOR_OFFICER,
    WL_ACTOR_SS,
    WL_ACTOR_DOG,
    WL_ACTOR_MUTANT,
    WL_ACTOR_BOSS,
    WL_ACTOR_GHOST,
    WL_ACTOR_DEAD_GUARD,
} wl_actor_kind;

typedef enum wl_actor_mode {
    WL_ACTOR_STAND,
    WL_ACTOR_PATROL,
    WL_ACTOR_CHASE,
    WL_ACTOR_INERT,
    WL_ACTOR_BOSS_MODE,
    WL_ACTOR_GHOST_MODE,
} wl_actor_mode;

typedef enum wl_door_action {
    WL_DOOR_OPEN = 0,
    WL_DOOR_CLOSED = 1,
    WL_DOOR_OPENING = 2,
    WL_DOOR_CLOSING = 3,
} wl_door_action;

typedef struct wl_player_spawn {
    int present;
    uint16_t x;
    uint16_t y;
    wl_direction dir;
} wl_player_spawn;

typedef struct wl_door_desc {
    uint16_t x;
    uint16_t y;
    uint16_t source_tile;
    int vertical;
    uint8_t lock;
    uint8_t area1;
    uint8_t area2;
    wl_door_action action;
    uint16_t position;
    int32_t ticcount;
} wl_door_desc;

typedef struct wl_static_desc {
    uint16_t x;
    uint16_t y;
    uint16_t source_tile;
    uint16_t type;
    int blocking;
    int bonus;
    int treasure;
    uint8_t active;
} wl_static_desc;

typedef struct wl_actor_desc {
    uint16_t spawn_x;
    uint16_t spawn_y;
    uint16_t tile_x;
    uint16_t tile_y;
    uint32_t fine_x;
    uint32_t fine_y;
    uint32_t patrol_remainder;
    uint16_t source_tile;
    wl_actor_kind kind;
    wl_actor_mode mode;
    wl_direction dir;
    uint16_t scene_source_index;
    uint8_t scene_source_override;
    int ambush;
    int shootable;
    int counts_for_kill_total;
} wl_actor_desc;

typedef struct wl_marker_desc {
    uint16_t x;
    uint16_t y;
    uint16_t source_tile;
    wl_direction dir;
} wl_marker_desc;

typedef enum wl_scene_sprite_kind {
    WL_SCENE_SPRITE_STATIC = 0,
    WL_SCENE_SPRITE_ACTOR = 1,
} wl_scene_sprite_kind;

typedef struct wl_scene_sprite_ref {
    wl_scene_sprite_kind kind;
    uint16_t model_index;
    uint16_t source_index;
    uint16_t vswap_chunk_index;
    uint32_t world_x;
    uint32_t world_y;
} wl_scene_sprite_ref;

typedef struct wl_actor_patrol_step_result {
    uint8_t stepped;
    uint8_t blocked;
    wl_direction dir;
    uint16_t tile_x;
    uint16_t tile_y;
} wl_actor_patrol_step_result;

typedef struct wl_actor_patrol_tic_result {
    uint16_t tiles_stepped;
    uint8_t blocked;
    wl_direction dir;
    uint16_t tile_x;
    uint16_t tile_y;
    uint32_t requested_move;
    uint32_t leftover_move;
    uint32_t fine_x;
    uint32_t fine_y;
} wl_actor_patrol_tic_result;

typedef struct wl_actor_patrols_tic_result {
    uint16_t actors_considered;
    uint16_t actors_stepped;
    uint16_t actors_blocked;
    uint16_t tiles_stepped;
} wl_actor_patrols_tic_result;

typedef struct wl_actor_chase_dir_result {
    uint8_t selected;
    uint8_t blocked;
    wl_direction dir;
    uint16_t next_x;
    uint16_t next_y;
} wl_actor_chase_dir_result;

typedef struct wl_actor_chase_step_result {
    uint8_t stepped;
    uint8_t blocked;
    wl_direction dir;
    uint16_t tile_x;
    uint16_t tile_y;
} wl_actor_chase_step_result;

typedef struct wl_actor_chase_tic_result {
    uint16_t tiles_stepped;
    uint8_t blocked;
    wl_direction dir;
    uint16_t tile_x;
    uint16_t tile_y;
    uint32_t requested_move;
    uint32_t leftover_move;
    uint32_t fine_x;
    uint32_t fine_y;
} wl_actor_chase_tic_result;

typedef struct wl_actor_chases_tic_result {
    uint16_t actors_considered;
    uint16_t actors_stepped;
    uint16_t actors_blocked;
    uint16_t tiles_stepped;
} wl_actor_chases_tic_result;

typedef struct wl_actor_wake_result {
    uint8_t woke;
    uint8_t was_ambush;
    uint8_t chase_dir_selected;
    wl_actor_mode mode_before;
    wl_actor_mode mode_after;
    wl_direction dir_before;
    wl_direction dir_after;
} wl_actor_wake_result;

typedef struct wl_actor_wake_all_result {
    uint16_t actors_considered;
    uint16_t actors_woke;
    uint16_t ambush_cleared;
    uint16_t chase_dir_selected;
    uint16_t first_woken_actor;
    uint16_t last_woken_actor;
} wl_actor_wake_all_result;

typedef struct wl_actor_flag_summary {
    size_t shootable_count;
    size_t ambush_count;
    size_t kill_total_count;
    size_t scene_override_count;
    size_t inert_count;
} wl_actor_flag_summary;

typedef struct wl_actor_position_summary {
    size_t moved_from_spawn_count;
    size_t fine_position_count;
    size_t spawn_out_of_bounds_count;
    size_t tile_out_of_bounds_count;
} wl_actor_position_summary;

typedef struct wl_actor_wake_summary {
    size_t wakeable_count;
    size_t ambush_waiting_count;
    size_t already_chasing_count;
    size_t ineligible_count;
} wl_actor_wake_summary;

typedef struct wl_actor_patrol_path_summary {
    size_t patrol_count;
    size_t path_selected_count;
    size_t path_blocked_count;
    size_t invalid_position_count;
} wl_actor_patrol_path_summary;

typedef struct wl_actor_chase_path_summary {
    size_t chase_count;
    size_t path_selected_count;
    size_t path_blocked_count;
    size_t invalid_position_count;
} wl_actor_chase_path_summary;

typedef struct wl_actor_attack_summary {
    size_t attack_capable_count;
    size_t bite_count;
    size_t shoot_count;
    size_t passive_count;
    size_t invalid_position_count;
} wl_actor_attack_summary;

typedef struct wl_actor_scene_summary {
    size_t actor_count;
    size_t default_source_count;
    size_t override_source_count;
    size_t missing_source_count;
} wl_actor_scene_summary;

typedef struct wl_actor_player_distance_summary {
    size_t considered_count;
    size_t invalid_position_count;
    uint16_t nearest_actor_index;
    uint16_t farthest_actor_index;
    uint16_t nearest_distance;
    uint16_t farthest_distance;
} wl_actor_player_distance_summary;

typedef struct wl_actor_engagement_summary {
    size_t threat_count;
    size_t melee_threat_count;
    size_t ranged_threat_count;
    size_t close_threat_count;
    size_t chasing_threat_count;
    size_t ambush_threat_count;
    size_t invalid_position_count;
    uint16_t nearest_threat_index;
    uint16_t farthest_threat_index;
    uint16_t nearest_threat_distance;
    uint16_t farthest_threat_distance;
} wl_actor_engagement_summary;

typedef struct wl_actor_direction_summary {
    size_t direction_counts[(WL_DIR_NONE + 1)];
    size_t invalid_direction_count;
} wl_actor_direction_summary;

typedef struct wl_actor_motion_summary {
    size_t centered_count;
    size_t offset_count;
    size_t active_remainder_count;
    size_t invalid_position_count;
    uint16_t farthest_offset_actor_index;
    uint32_t largest_axis_offset;
    uint32_t min_active_remainder;
    uint32_t max_active_remainder;
} wl_actor_motion_summary;

typedef struct wl_actor_activity_summary {
    size_t active_ai_count;
    size_t waiting_ai_count;
    size_t inert_count;
    size_t combat_ready_count;
    size_t boss_or_ghost_count;
    size_t invalid_position_count;
} wl_actor_activity_summary;

typedef struct wl_actor_mode_summary {
    size_t stand_count;
    size_t patrol_count;
    size_t chase_count;
    size_t inert_count;
    size_t boss_mode_count;
    size_t ghost_mode_count;
    size_t invalid_mode_count;
    size_t invalid_position_count;
} wl_actor_mode_summary;

typedef struct wl_actor_tile_occupancy_summary {
    size_t occupied_tile_count;
    size_t stacked_actor_count;
    size_t invalid_position_count;
    size_t max_stack_count;
} wl_actor_tile_occupancy_summary;

typedef struct wl_actor_spawn_occupancy_summary {
    size_t occupied_spawn_tile_count;
    size_t stacked_spawn_actor_count;
    size_t invalid_spawn_position_count;
    size_t moved_from_spawn_count;
    size_t max_spawn_stack_count;
} wl_actor_spawn_occupancy_summary;

typedef struct wl_actor_collision_tile_summary {
    size_t open_tile_count;
    size_t wall_tile_count;
    size_t door_tile_count;
    size_t door_adjacent_tile_count;
    size_t invalid_position_count;
} wl_actor_collision_tile_summary;

typedef struct wl_actor_door_proximity_summary {
    size_t on_door_tile_count;
    size_t door_adjacent_tile_count;
    size_t away_from_door_count;
    size_t invalid_position_count;
    size_t unique_door_tile_count;
    uint16_t first_door_index;
    uint16_t last_door_index;
} wl_actor_door_proximity_summary;

typedef struct wl_actor_door_blocker_summary {
    size_t blocking_actor_count;
    size_t shootable_blocker_count;
    size_t nonshootable_blocker_count;
    size_t invalid_position_count;
    size_t unique_blocked_door_count;
    uint16_t first_blocked_door_index;
    uint16_t last_blocked_door_index;
} wl_actor_door_blocker_summary;

typedef struct wl_actor_player_adjacency_summary {
    size_t same_tile_count;
    size_t cardinal_adjacent_count;
    size_t diagonal_adjacent_count;
    size_t same_row_or_column_count;
    size_t distant_count;
    size_t invalid_position_count;
} wl_actor_player_adjacency_summary;

typedef struct wl_actor_facing_summary {
    size_t facing_player_count;
    size_t facing_away_count;
    size_t perpendicular_count;
    size_t same_tile_count;
    size_t no_direction_count;
    size_t invalid_direction_count;
    size_t invalid_position_count;
} wl_actor_facing_summary;

typedef struct wl_actor_line_of_sight_summary {
    size_t clear_cardinal_count;
    size_t blocked_by_wall_count;
    size_t blocked_by_door_count;
    size_t same_tile_count;
    size_t noncardinal_count;
    size_t invalid_position_count;
} wl_actor_line_of_sight_summary;

typedef struct wl_actor_source_tile_summary {
    size_t actor_count;
    size_t unique_source_tile_count;
    size_t zero_source_tile_count;
    uint16_t min_source_tile;
    uint16_t max_source_tile;
} wl_actor_source_tile_summary;

typedef struct wl_actor_combat_class_summary {
    size_t infantry_count;
    size_t dog_count;
    size_t boss_count;
    size_t ghost_count;
    size_t corpse_count;
    size_t shootable_count;
    size_t kill_credit_count;
    size_t noncombat_count;
    size_t invalid_kind_count;
} wl_actor_combat_class_summary;

typedef struct wl_actor_threat_summary {
    size_t immediate_threat_count;
    size_t latent_threat_count;
    size_t ambush_latent_count;
    size_t inert_shootable_count;
    size_t nonshootable_count;
    size_t invalid_mode_count;
} wl_actor_threat_summary;

typedef struct wl_static_state_summary {
    size_t active_count;
    size_t inactive_count;
    size_t blocking_count;
    size_t bonus_count;
    size_t treasure_count;
    size_t active_bonus_count;
    size_t active_blocking_count;
} wl_static_state_summary;

typedef struct wl_static_source_tile_summary {
    size_t static_count;
    size_t unique_source_tile_count;
    size_t zero_source_tile_count;
    uint16_t min_source_tile;
    uint16_t max_source_tile;
} wl_static_source_tile_summary;

typedef struct wl_static_tile_occupancy_summary {
    size_t occupied_tile_count;
    size_t stacked_static_count;
    size_t invalid_position_count;
    uint16_t max_stack_depth;
} wl_static_tile_occupancy_summary;

typedef struct wl_static_player_distance_summary {
    size_t considered_count;
    size_t inactive_count;
    size_t invalid_position_count;
    uint16_t nearest_static_index;
    uint16_t farthest_static_index;
    uint16_t nearest_distance;
    uint16_t farthest_distance;
} wl_static_player_distance_summary;

typedef struct wl_static_player_adjacency_summary {
    size_t same_tile_count;
    size_t cardinal_adjacent_count;
    size_t diagonal_adjacent_count;
    size_t same_row_or_column_count;
    size_t distant_count;
    size_t inactive_count;
    size_t invalid_position_count;
} wl_static_player_adjacency_summary;

typedef struct wl_static_line_of_sight_summary {
    size_t clear_cardinal_count;
    size_t blocked_by_wall_count;
    size_t blocked_by_door_count;
    size_t same_tile_count;
    size_t noncardinal_count;
    size_t inactive_count;
    size_t invalid_position_count;
} wl_static_line_of_sight_summary;

typedef struct wl_door_state_summary {
    size_t vertical_count;
    size_t horizontal_count;
    size_t unlocked_count;
    size_t locked_count;
    size_t open_count;
    size_t closed_count;
    size_t opening_count;
    size_t closing_count;
    size_t moving_count;
    size_t partially_open_count;
    size_t invalid_action_count;
    uint16_t max_position;
} wl_door_state_summary;

typedef struct wl_door_timing_summary {
    size_t waiting_count;
    size_t countdown_count;
    size_t overdue_count;
    size_t moving_with_countdown_count;
    size_t open_with_countdown_count;
    int32_t min_ticcount;
    int32_t max_ticcount;
} wl_door_timing_summary;

typedef struct wl_door_player_distance_summary {
    size_t considered_count;
    size_t invalid_position_count;
    size_t vertical_count;
    size_t horizontal_count;
    uint16_t nearest_door_index;
    uint16_t farthest_door_index;
    uint16_t nearest_distance;
    uint16_t farthest_distance;
} wl_door_player_distance_summary;

typedef struct wl_door_player_adjacency_summary {
    size_t same_tile_count;
    size_t cardinal_adjacent_count;
    size_t diagonal_adjacent_count;
    size_t same_row_or_column_count;
    size_t distant_count;
    size_t invalid_position_count;
} wl_door_player_adjacency_summary;

typedef struct wl_door_line_of_sight_summary {
    size_t clear_cardinal_count;
    size_t blocked_by_wall_count;
    size_t blocked_by_door_count;
    size_t same_tile_count;
    size_t noncardinal_count;
    size_t invalid_position_count;
} wl_door_line_of_sight_summary;

typedef struct wl_door_lock_summary {
    size_t normal_count;
    size_t keyed_count;
    size_t elevator_count;
    size_t invalid_source_tile_count;
    size_t source_lock_mismatch_count;
    size_t unique_lock_count;
    uint8_t min_lock;
    uint8_t max_lock;
} wl_door_lock_summary;

typedef struct wl_door_source_tile_summary {
    size_t door_count;
    size_t unique_source_tile_count;
    size_t vertical_source_count;
    size_t horizontal_source_count;
    size_t invalid_source_tile_count;
    uint16_t min_source_tile;
    uint16_t max_source_tile;
} wl_door_source_tile_summary;

typedef struct wl_door_tile_occupancy_summary {
    size_t door_count;
    size_t unique_tile_count;
    size_t duplicate_tile_count;
    size_t invalid_position_count;
} wl_door_tile_occupancy_summary;

typedef struct wl_door_area_connection_summary {
    size_t door_count;
    size_t connection_count;
    size_t unique_connection_count;
    size_t self_connection_count;
    size_t duplicate_connection_count;
    size_t invalid_area_count;
    uint8_t min_area;
    uint8_t max_area;
} wl_door_area_connection_summary;

typedef struct wl_door_area_matrix_summary {
    size_t directed_link_count;
    size_t undirected_link_count;
    size_t asymmetric_link_count;
    size_t self_link_count;
    uint8_t min_area;
    uint8_t max_area;
    uint8_t max_link_weight;
} wl_door_area_matrix_summary;

typedef struct wl_pushwall_state_summary {
    size_t marker_count;
    size_t invalid_marker_position_count;
    size_t north_count;
    size_t east_count;
    size_t south_count;
    size_t west_count;
    size_t invalid_direction_count;
    uint8_t motion_active;
    uint8_t motion_marker_valid;
    uint8_t motion_position_valid;
    uint16_t motion_tile;
    uint16_t motion_state;
    uint16_t motion_pos;
} wl_pushwall_state_summary;

typedef struct wl_pushwall_player_distance_summary {
    size_t considered_count;
    size_t invalid_marker_position_count;
    uint16_t nearest_pushwall_index;
    uint16_t farthest_pushwall_index;
    uint16_t nearest_distance;
    uint16_t farthest_distance;
} wl_pushwall_player_distance_summary;

typedef struct wl_pushwall_player_adjacency_summary {
    size_t same_tile_count;
    size_t cardinal_adjacent_count;
    size_t diagonal_adjacent_count;
    size_t same_row_or_column_count;
    size_t distant_count;
    size_t invalid_marker_position_count;
} wl_pushwall_player_adjacency_summary;

typedef struct wl_pushwall_line_of_sight_summary {
    size_t clear_cardinal_count;
    size_t blocked_by_wall_count;
    size_t blocked_by_door_count;
    size_t same_tile_count;
    size_t noncardinal_count;
    size_t invalid_marker_position_count;
} wl_pushwall_line_of_sight_summary;

typedef struct wl_pushwall_motion_path_summary {
    uint8_t active;
    uint8_t current_position_valid;
    uint8_t next_position_valid;
    uint8_t next_tile_blocked;
    uint16_t current_x;
    uint16_t current_y;
    uint16_t next_x;
    uint16_t next_y;
    uint16_t next_tile_value;
    uint16_t motion_pos;
} wl_pushwall_motion_path_summary;

typedef struct wl_pushwall_source_tile_summary {
    size_t marker_count;
    size_t unique_source_tile_count;
    size_t expected_marker_source_count;
    size_t zero_source_tile_count;
    size_t unexpected_source_tile_count;
    uint16_t min_source_tile;
    uint16_t max_source_tile;
} wl_pushwall_source_tile_summary;

typedef struct wl_pushwall_tile_occupancy_summary {
    size_t marker_count;
    size_t unique_tile_count;
    size_t duplicate_tile_count;
    size_t invalid_marker_position_count;
} wl_pushwall_tile_occupancy_summary;

typedef struct wl_path_marker_source_tile_summary {
    size_t marker_count;
    size_t unique_source_tile_count;
    size_t expected_marker_source_count;
    size_t zero_source_tile_count;
    size_t unexpected_source_tile_count;
    uint16_t min_source_tile;
    uint16_t max_source_tile;
} wl_path_marker_source_tile_summary;

typedef struct wl_path_marker_tile_occupancy_summary {
    size_t marker_count;
    size_t unique_tile_count;
    size_t duplicate_tile_count;
    size_t invalid_marker_position_count;
} wl_path_marker_tile_occupancy_summary;

typedef struct wl_path_marker_player_distance_summary {
    size_t considered_count;
    size_t invalid_marker_position_count;
    uint16_t nearest_marker_index;
    uint16_t farthest_marker_index;
    uint16_t nearest_distance;
    uint16_t farthest_distance;
} wl_path_marker_player_distance_summary;

typedef struct wl_path_marker_player_adjacency_summary {
    size_t same_tile_count;
    size_t cardinal_adjacent_count;
    size_t diagonal_adjacent_count;
    size_t same_row_or_column_count;
    size_t distant_count;
    size_t invalid_marker_position_count;
} wl_path_marker_player_adjacency_summary;

typedef struct wl_path_marker_line_of_sight_summary {
    size_t clear_cardinal_count;
    size_t blocked_by_wall_count;
    size_t blocked_by_door_count;
    size_t same_tile_count;
    size_t noncardinal_count;
    size_t invalid_marker_position_count;
} wl_path_marker_line_of_sight_summary;

typedef struct wl_path_marker_direction_summary {
    size_t north_count;
    size_t east_count;
    size_t south_count;
    size_t west_count;
    size_t no_direction_count;
    size_t invalid_direction_count;
    size_t invalid_marker_position_count;
} wl_path_marker_direction_summary;

typedef struct wl_path_marker_exit_summary {
    size_t open_exit_count;
    size_t wall_blocked_exit_count;
    size_t out_of_bounds_exit_count;
    size_t no_direction_count;
    size_t invalid_direction_count;
    size_t invalid_marker_position_count;
} wl_path_marker_exit_summary;

typedef struct wl_path_marker_chain_summary {
    size_t linked_marker_count;
    size_t straight_count;
    size_t left_turn_count;
    size_t right_turn_count;
    size_t reverse_turn_count;
    size_t dangling_exit_count;
    size_t no_direction_count;
    size_t invalid_direction_count;
    size_t invalid_marker_position_count;
    uint16_t first_dangling_marker_index;
} wl_path_marker_chain_summary;

typedef struct wl_model_capacity_summary {
    size_t door_count;
    size_t static_count;
    size_t actor_count;
    size_t path_marker_count;
    size_t pushwall_count;
    size_t door_remaining;
    size_t static_remaining;
    size_t actor_remaining;
    size_t path_marker_remaining;
    size_t pushwall_remaining;
    uint8_t door_full;
    uint8_t static_full;
    uint8_t actor_full;
    uint8_t path_marker_full;
    uint8_t pushwall_full;
    size_t unknown_info_tiles;
} wl_model_capacity_summary;

typedef struct wl_unknown_info_summary {
    size_t unknown_info_tiles;
    uint32_t unknown_info_hash;
    uint16_t first_tile;
    uint16_t first_x;
    uint16_t first_y;
    uint16_t last_tile;
    uint16_t last_x;
    uint16_t last_y;
    uint8_t has_unknown_info;
} wl_unknown_info_summary;

typedef struct wl_pushwall_motion {
    uint8_t active;
    uint16_t state;
    uint16_t pos;
    uint16_t x;
    uint16_t y;
    wl_direction dir;
    uint16_t tile;
    size_t marker_index;
} wl_pushwall_motion;

typedef struct wl_game_model {
    uint16_t tilemap[WL_MAP_PLANE_WORDS];
    wl_player_spawn player;

    wl_door_desc doors[WL_MAX_DOORS];
    size_t door_count;
    size_t door_area_connection_count;
    size_t unique_door_area_connection_count;
    uint8_t door_area_connections[WL_NUM_AREAS][WL_NUM_AREAS];

    wl_static_desc statics[WL_MAX_STATS];
    size_t static_count;
    size_t blocking_static_count;
    size_t bonus_static_count;
    size_t treasure_total;

    wl_actor_desc actors[WL_MAX_ACTORS];
    size_t actor_count;
    size_t kill_total;

    wl_marker_desc path_markers[WL_MAX_PATH_MARKERS];
    size_t path_marker_count;

    wl_marker_desc pushwalls[WL_MAX_PUSHWALLS];
    size_t pushwall_count;
    size_t secret_total;
    wl_pushwall_motion pushwall_motion;

    size_t unknown_info_tiles;
    uint32_t unknown_info_hash;
    uint16_t first_unknown_info_tile;
    uint16_t first_unknown_info_x;
    uint16_t first_unknown_info_y;
    uint16_t last_unknown_info_tile;
    uint16_t last_unknown_info_x;
    uint16_t last_unknown_info_y;
} wl_game_model;

int wl_build_game_model(const uint16_t *wall_plane, const uint16_t *info_plane,
                        size_t word_count, wl_difficulty difficulty,
                        wl_game_model *out);
int wl_build_runtime_solid_plane(const wl_game_model *model, uint16_t door_tile,
                                 uint16_t *out);
int wl_render_runtime_camera_wall_view(const wl_game_model *model, uint16_t door_tile,
                                       uint32_t origin_x, uint32_t origin_y,
                                       int32_t forward_x, int32_t forward_y,
                                       int32_t plane_x, int32_t plane_y,
                                       uint16_t first_x, uint16_t x_step,
                                       size_t ray_count,
                                       const unsigned char *const *wall_pages,
                                       const size_t *wall_page_sizes,
                                       size_t wall_page_count,
                                       wl_indexed_surface *dst,
                                       int32_t *directions_x,
                                       int32_t *directions_y,
                                       wl_map_wall_hit *hits,
                                       wl_wall_strip *strips);
int wl_build_door_wall_hit(const wl_door_desc *door, uint16_t vswap_sprite_start,
                           uint32_t intercept, uint16_t x, uint16_t scaled_height,
                           wl_map_wall_hit *out);
int wl_build_pushwall_wall_hit(uint16_t tile, wl_wall_side side,
                               uint32_t intercept, uint16_t pwall_pos,
                               int step_sign, uint16_t x, uint16_t scaled_height,
                               wl_map_wall_hit *out);
int wl_cast_runtime_fixed_wall_ray(const wl_game_model *model,
                                   uint16_t vswap_sprite_start,
                                   uint32_t origin_x, uint32_t origin_y,
                                   int32_t direction_x, int32_t direction_y,
                                   uint16_t x, uint16_t scaled_height,
                                   wl_map_wall_hit *out);
int wl_render_runtime_door_camera_wall_view(const wl_game_model *model,
                                            uint16_t vswap_sprite_start,
                                            uint32_t origin_x, uint32_t origin_y,
                                            int32_t forward_x, int32_t forward_y,
                                            int32_t plane_x, int32_t plane_y,
                                            uint16_t first_x, uint16_t x_step,
                                            size_t ray_count,
                                            const unsigned char *const *wall_pages,
                                            const size_t *wall_page_sizes,
                                            size_t wall_page_count,
                                            wl_indexed_surface *dst,
                                            int32_t *directions_x,
                                            int32_t *directions_y,
                                            wl_map_wall_hit *hits,
                                            wl_wall_strip *strips);
int wl_render_runtime_door_camera_scene_view(const wl_game_model *model,
                                             uint16_t vswap_sprite_start,
                                             uint32_t origin_x, uint32_t origin_y,
                                             int32_t forward_x, int32_t forward_y,
                                             int32_t plane_x, int32_t plane_y,
                                             uint16_t first_x, uint16_t x_step,
                                             size_t ray_count,
                                             const unsigned char *const *wall_pages,
                                             const size_t *wall_page_sizes,
                                             size_t wall_page_count,
                                             const wl_indexed_surface *const *sprite_surfaces,
                                             const uint32_t *sprite_x,
                                             const uint32_t *sprite_y,
                                             const uint16_t *sprite_source_indices,
                                             size_t sprite_count,
                                             unsigned char transparent_index,
                                             wl_indexed_surface *dst,
                                             int32_t *directions_x,
                                             int32_t *directions_y,
                                             wl_map_wall_hit *hits,
                                             wl_wall_strip *strips,
                                             wl_projected_sprite *sprites,
                                             uint16_t *wall_heights);
int wl_collect_scene_sprite_refs(const wl_game_model *model, uint16_t vswap_sprite_start,
                                 wl_scene_sprite_ref *refs, size_t max_refs,
                                 size_t *out_count);
int wl_collect_spear_scene_sprite_refs(const wl_game_model *model, uint16_t vswap_sprite_start,
                                       wl_scene_sprite_ref *refs, size_t max_refs,
                                       size_t *out_count);
int wl_summarize_model_capacity(const wl_game_model *model,
                                wl_model_capacity_summary *out);
int wl_summarize_unknown_info_tiles(const wl_game_model *model,
                                    wl_unknown_info_summary *out);
int wl_count_actors_by_kind(const wl_game_model *model, size_t *counts,
                            size_t count_capacity);
int wl_count_actors_by_mode(const wl_game_model *model, size_t *counts,
                            size_t count_capacity);
int wl_summarize_actor_flags(const wl_game_model *model,
                             wl_actor_flag_summary *out);
int wl_summarize_actor_positions(const wl_game_model *model,
                                 wl_actor_position_summary *out);
int wl_summarize_actor_wake_state(const wl_game_model *model, int include_ambush,
                                  wl_actor_wake_summary *out);
int wl_summarize_actor_patrol_paths(const wl_game_model *model,
                                    wl_actor_patrol_path_summary *out);
int wl_summarize_actor_chase_paths(const wl_game_model *model,
                                   uint16_t player_x, uint16_t player_y,
                                   int search_forward,
                                   wl_actor_chase_path_summary *out);
int wl_summarize_actor_attacks(const wl_game_model *model,
                               wl_actor_attack_summary *out);
int wl_summarize_actor_scene_sources(const wl_game_model *model,
                                     wl_actor_scene_summary *out);
int wl_summarize_actor_player_distances(const wl_game_model *model,
                                        uint16_t player_x, uint16_t player_y,
                                        int shootable_only,
                                        wl_actor_player_distance_summary *out);
int wl_summarize_actor_engagements(const wl_game_model *model,
                                   uint16_t player_x, uint16_t player_y,
                                   uint16_t close_distance,
                                   wl_actor_engagement_summary *out);
int wl_summarize_actor_directions(const wl_game_model *model,
                                  wl_actor_direction_summary *out);
int wl_summarize_actor_motion(const wl_game_model *model,
                              wl_actor_motion_summary *out);
int wl_summarize_actor_activity(const wl_game_model *model,
                                wl_actor_activity_summary *out);
int wl_summarize_actor_modes(const wl_game_model *model,
                             wl_actor_mode_summary *out);
int wl_summarize_actor_tile_occupancy(const wl_game_model *model,
                                      wl_actor_tile_occupancy_summary *out);
int wl_summarize_actor_spawn_occupancy(const wl_game_model *model,
                                       wl_actor_spawn_occupancy_summary *out);
int wl_summarize_actor_collision_tiles(const wl_game_model *model,
                                       wl_actor_collision_tile_summary *out);
int wl_summarize_actor_door_proximity(const wl_game_model *model,
                                      wl_actor_door_proximity_summary *out);
int wl_summarize_actor_door_blockers(const wl_game_model *model,
                                     wl_actor_door_blocker_summary *out);
int wl_summarize_actor_player_adjacency(const wl_game_model *model,
                                        uint16_t player_x, uint16_t player_y,
                                        wl_actor_player_adjacency_summary *out);
int wl_summarize_actor_facing(const wl_game_model *model,
                              uint16_t player_x, uint16_t player_y,
                              wl_actor_facing_summary *out);
int wl_summarize_actor_line_of_sight(const wl_game_model *model,
                                     uint16_t player_x, uint16_t player_y,
                                     wl_actor_line_of_sight_summary *out);
int wl_summarize_actor_source_tiles(const wl_game_model *model,
                                    wl_actor_source_tile_summary *out);
int wl_summarize_actor_combat_classes(const wl_game_model *model,
                                      wl_actor_combat_class_summary *out);
int wl_summarize_actor_threats(const wl_game_model *model,
                               wl_actor_threat_summary *out);
int wl_summarize_static_states(const wl_game_model *model,
                               wl_static_state_summary *out);
int wl_summarize_static_source_tiles(const wl_game_model *model,
                                     wl_static_source_tile_summary *out);
int wl_summarize_static_tile_occupancy(const wl_game_model *model,
                                       wl_static_tile_occupancy_summary *out);
int wl_summarize_static_player_distances(const wl_game_model *model,
                                         uint16_t player_x, uint16_t player_y,
                                         int active_only,
                                         wl_static_player_distance_summary *out);
int wl_summarize_static_player_adjacency(const wl_game_model *model,
                                         uint16_t player_x, uint16_t player_y,
                                         int active_only,
                                         wl_static_player_adjacency_summary *out);
int wl_summarize_static_line_of_sight(const wl_game_model *model,
                                      uint16_t player_x, uint16_t player_y,
                                      int active_only,
                                      wl_static_line_of_sight_summary *out);
int wl_summarize_door_states(const wl_game_model *model,
                             wl_door_state_summary *out);
int wl_summarize_door_timing(const wl_game_model *model,
                             wl_door_timing_summary *out);
int wl_summarize_door_player_distances(const wl_game_model *model,
                                       uint16_t player_x, uint16_t player_y,
                                       wl_door_player_distance_summary *out);
int wl_summarize_door_player_adjacency(const wl_game_model *model,
                                       uint16_t player_x, uint16_t player_y,
                                       wl_door_player_adjacency_summary *out);
int wl_summarize_door_line_of_sight(const wl_game_model *model,
                                    uint16_t player_x, uint16_t player_y,
                                    wl_door_line_of_sight_summary *out);
int wl_summarize_door_locks(const wl_game_model *model,
                            wl_door_lock_summary *out);
int wl_summarize_door_source_tiles(const wl_game_model *model,
                                   wl_door_source_tile_summary *out);
int wl_summarize_door_tile_occupancy(const wl_game_model *model,
                                     wl_door_tile_occupancy_summary *out);
int wl_summarize_door_area_connections(
    const wl_game_model *model, wl_door_area_connection_summary *out);
int wl_summarize_door_area_matrix(
    const wl_game_model *model, wl_door_area_matrix_summary *out);
int wl_summarize_pushwall_state(const wl_game_model *model,
                                wl_pushwall_state_summary *out);
int wl_summarize_pushwall_player_distances(const wl_game_model *model,
                                           uint16_t player_x, uint16_t player_y,
                                           wl_pushwall_player_distance_summary *out);
int wl_summarize_pushwall_player_adjacency(
    const wl_game_model *model, uint16_t player_x, uint16_t player_y,
    wl_pushwall_player_adjacency_summary *out);
int wl_summarize_pushwall_line_of_sight(
    const wl_game_model *model, uint16_t player_x, uint16_t player_y,
    wl_pushwall_line_of_sight_summary *out);
int wl_summarize_pushwall_motion_path(
    const wl_game_model *model, wl_pushwall_motion_path_summary *out);
int wl_summarize_pushwall_source_tiles(const wl_game_model *model,
                                       wl_pushwall_source_tile_summary *out);
int wl_summarize_pushwall_tile_occupancy(
    const wl_game_model *model, wl_pushwall_tile_occupancy_summary *out);
int wl_summarize_path_marker_source_tiles(
    const wl_game_model *model, wl_path_marker_source_tile_summary *out);
int wl_summarize_path_marker_tile_occupancy(
    const wl_game_model *model, wl_path_marker_tile_occupancy_summary *out);
int wl_summarize_path_marker_player_distances(
    const wl_game_model *model, uint16_t player_x, uint16_t player_y,
    wl_path_marker_player_distance_summary *out);
int wl_summarize_path_marker_player_adjacency(
    const wl_game_model *model, uint16_t player_x, uint16_t player_y,
    wl_path_marker_player_adjacency_summary *out);
int wl_summarize_path_marker_line_of_sight(
    const wl_game_model *model, uint16_t player_x, uint16_t player_y,
    wl_path_marker_line_of_sight_summary *out);
int wl_summarize_path_marker_directions(
    const wl_game_model *model, wl_path_marker_direction_summary *out);
int wl_summarize_path_marker_exits(
    const wl_game_model *model, wl_path_marker_exit_summary *out);
int wl_summarize_path_marker_chains(
    const wl_game_model *model, wl_path_marker_chain_summary *out);
int wl_select_path_direction(const wl_game_model *model, uint16_t tile_x,
                             uint16_t tile_y, wl_direction current_dir,
                             wl_direction *out_dir);
int wl_step_patrol_actor(wl_game_model *model, uint16_t actor_index,
                         wl_actor_patrol_step_result *out);
int wl_step_patrol_actor_tics(wl_game_model *model, uint16_t actor_index,
                              uint32_t speed, int32_t tics,
                              wl_actor_patrol_tic_result *out);
int wl_step_patrol_actors_tics(wl_game_model *model, uint32_t speed,
                               int32_t tics,
                               wl_actor_patrols_tic_result *out);
int wl_select_chase_direction(const wl_game_model *model, uint16_t actor_x,
                              uint16_t actor_y, uint16_t player_x,
                              uint16_t player_y, wl_direction current_dir,
                              int search_forward,
                              wl_actor_chase_dir_result *out);
int wl_step_chase_actor(wl_game_model *model, uint16_t actor_index,
                        uint16_t player_x, uint16_t player_y,
                        int search_forward,
                        wl_actor_chase_step_result *out);
int wl_step_chase_actor_tics(wl_game_model *model, uint16_t actor_index,
                             uint16_t player_x, uint16_t player_y,
                             int search_forward, uint32_t speed,
                             int32_t tics,
                             wl_actor_chase_tic_result *out);
int wl_step_chase_actors_tics(wl_game_model *model, uint16_t player_x,
                              uint16_t player_y, int search_forward,
                              uint32_t speed, int32_t tics,
                              wl_actor_chases_tic_result *out);
int wl_wake_actor_for_chase(wl_game_model *model, uint16_t actor_index,
                            uint16_t player_x, uint16_t player_y,
                            int search_forward, wl_actor_wake_result *out);
int wl_wake_actors_for_chase(wl_game_model *model, uint16_t player_x,
                             uint16_t player_y, int include_ambush,
                             int search_forward, wl_actor_wake_all_result *out);

#ifdef __cplusplus
}
#endif

#endif /* WL_GAME_MODEL_H */
