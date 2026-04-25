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
    WL_ACTOR_INERT,
    WL_ACTOR_BOSS_MODE,
    WL_ACTOR_GHOST_MODE,
} wl_actor_mode;

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
} wl_door_desc;

typedef struct wl_static_desc {
    uint16_t x;
    uint16_t y;
    uint16_t source_tile;
    uint16_t type;
    int blocking;
    int bonus;
    int treasure;
} wl_static_desc;

typedef struct wl_actor_desc {
    uint16_t spawn_x;
    uint16_t spawn_y;
    uint16_t tile_x;
    uint16_t tile_y;
    uint16_t source_tile;
    wl_actor_kind kind;
    wl_actor_mode mode;
    wl_direction dir;
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

    size_t unknown_info_tiles;
} wl_game_model;

int wl_build_game_model(const uint16_t *wall_plane, const uint16_t *info_plane,
                        size_t word_count, wl_difficulty difficulty,
                        wl_game_model *out);

#ifdef __cplusplus
}
#endif

#endif /* WL_GAME_MODEL_H */
