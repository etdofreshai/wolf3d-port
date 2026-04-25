#ifndef WL_MAP_SEMANTICS_H
#define WL_MAP_SEMANTICS_H

#include "wl_assets.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WL_AREATILE 107
#define WL_AMBUSHTILE 106
#define WL_ICONARROWS 90
#define WL_PUSHABLETILE 98
#define WL_EXITTILE 99
#define WL_ELEVATORTILE 21
#define WL_ALTELEVATORTILE 107

typedef enum wl_direction {
    WL_DIR_NORTH = 0,
    WL_DIR_EAST = 1,
    WL_DIR_SOUTH = 2,
    WL_DIR_WEST = 3,
    WL_DIR_NONE = 4,
} wl_direction;

typedef struct wl_map_semantics {
    size_t solid_tiles;
    size_t area_tiles;
    size_t door_tiles;
    size_t vertical_doors;
    size_t horizontal_doors;
    size_t locked_doors[6];
    size_t ambush_tiles;
    size_t exit_tiles;
    size_t elevator_tiles;
    size_t alt_elevator_tiles;

    size_t player_starts;
    size_t first_player_x;
    size_t first_player_y;
    wl_direction first_player_dir;

    size_t static_objects;
    size_t static_blocking;
    size_t static_bonus;
    size_t static_treasure;
    size_t pushwall_markers;
    size_t path_markers;

    size_t guard_easy_starts;
    size_t guard_medium_starts;
    size_t guard_hard_starts;
    size_t dog_easy_starts;
    size_t dog_medium_starts;
    size_t dog_hard_starts;
    size_t officer_starts;
    size_t ss_starts;
    size_t mutant_starts;
    size_t boss_starts;
    size_t ghost_starts;
    size_t dead_guards;
    size_t unknown_info_tiles;
} wl_map_semantics;

int wl_classify_map_semantics(const uint16_t *wall_plane, const uint16_t *info_plane,
                              size_t word_count, wl_map_semantics *out);

#ifdef __cplusplus
}
#endif

#endif /* WL_MAP_SEMANTICS_H */
