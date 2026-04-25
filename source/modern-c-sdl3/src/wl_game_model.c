#include "wl_game_model.h"

#include <string.h>

static int in_range(uint16_t value, uint16_t first, uint16_t last) {
    return value >= first && value <= last;
}

static size_t map_index(size_t x, size_t y) {
    return y * WL_MAP_SIDE + x;
}

static int static_traits(uint16_t type, int *blocking, int *bonus, int *treasure) {
    static const unsigned char traits[] = {
        0,       /* 0 puddle */
        1,       /* 1 green barrel */
        1,       /* 2 table/chairs */
        1,       /* 3 floor lamp */
        0,       /* 4 chandelier */
        1,       /* 5 hanged man */
        2,       /* 6 bad food */
        1,       /* 7 red pillar */
        1,       /* 8 tree */
        0,       /* 9 skeleton flat */
        1,       /* 10 sink */
        1,       /* 11 potted plant */
        1,       /* 12 urn */
        1,       /* 13 bare table */
        0,       /* 14 ceiling light */
        0,       /* 15 kitchen stuff */
        1,       /* 16 suit of armor */
        1,       /* 17 hanging cage */
        1,       /* 18 skeleton in cage */
        0,       /* 19 skeleton relax */
        2,       /* 20 key 1 */
        2,       /* 21 key 2 */
        1,       /* 22 stuff */
        0,       /* 23 stuff */
        2,       /* 24 good food */
        2,       /* 25 first aid */
        2,       /* 26 clip */
        2,       /* 27 machine gun */
        2,       /* 28 chaingun */
        2 | 4,   /* 29 cross */
        2 | 4,   /* 30 chalice */
        2 | 4,   /* 31 bible */
        2 | 4,   /* 32 crown */
        2 | 4,   /* 33 full heal / one up */
        2,       /* 34 gibs */
        1,       /* 35 barrel */
        1,       /* 36 well */
        1,       /* 37 empty well */
        2,       /* 38 gibs 2 */
        1,       /* 39 flag */
        1,       /* 40 call apogee */
        0,       /* 41 junk */
        0,       /* 42 junk */
        0,       /* 43 junk */
        0,       /* 44 pots */
        1,       /* 45 stove */
        1,       /* 46 spears */
        0,       /* 47 vines */
        2,       /* 48 clip2 */
    };

    if (type >= sizeof(traits) / sizeof(traits[0])) {
        return -1;
    }
    *blocking = (traits[type] & 1) != 0;
    *bonus = (traits[type] & 2) != 0;
    *treasure = (traits[type] & 4) != 0;
    return 0;
}

static int add_door(wl_game_model *out, const uint16_t *wall_plane,
                    size_t x, size_t y, uint16_t tile) {
    if (out->door_count >= WL_MAX_DOORS) {
        return -1;
    }
    wl_door_desc *door = &out->doors[out->door_count];
    door->x = (uint16_t)x;
    door->y = (uint16_t)y;
    door->source_tile = tile;
    door->vertical = (tile % 2) == 0;
    door->lock = (uint8_t)((tile - (door->vertical ? 90 : 91)) / 2);

    out->tilemap[map_index(x, y)] = (uint16_t)(out->door_count | 0x80u);
    if (door->vertical) {
        if (x == 0 || x + 1 >= WL_MAP_SIDE || y == 0 || y + 1 >= WL_MAP_SIDE) {
            return -1;
        }
        door->area1 = (uint8_t)(wall_plane[map_index(x + 1, y)] - WL_AREATILE);
        door->area2 = (uint8_t)(wall_plane[map_index(x - 1, y)] - WL_AREATILE);
        out->tilemap[map_index(x, y - 1)] |= 0x40u;
        out->tilemap[map_index(x, y + 1)] |= 0x40u;
    } else {
        if (x == 0 || x + 1 >= WL_MAP_SIDE || y == 0 || y + 1 >= WL_MAP_SIDE) {
            return -1;
        }
        door->area1 = (uint8_t)(wall_plane[map_index(x, y - 1)] - WL_AREATILE);
        door->area2 = (uint8_t)(wall_plane[map_index(x, y + 1)] - WL_AREATILE);
        out->tilemap[map_index(x - 1, y)] |= 0x40u;
        out->tilemap[map_index(x + 1, y)] |= 0x40u;
    }
    if (door->area1 >= WL_NUM_AREAS || door->area2 >= WL_NUM_AREAS) {
        return -1;
    }
    if (!out->door_area_connections[door->area1][door->area2] &&
        !out->door_area_connections[door->area2][door->area1]) {
        ++out->unique_door_area_connection_count;
    }
    ++out->door_area_connections[door->area1][door->area2];
    ++out->door_area_connections[door->area2][door->area1];
    ++out->door_area_connection_count;
    ++out->door_count;
    return 0;
}

static int add_static(wl_game_model *out, size_t x, size_t y, uint16_t tile) {
    if (out->static_count >= WL_MAX_STATS) {
        return -1;
    }
    uint16_t type = (uint16_t)(tile - 23);
    int blocking = 0;
    int bonus = 0;
    int treasure = 0;
    if (static_traits(type, &blocking, &bonus, &treasure) != 0) {
        ++out->unknown_info_tiles;
        return 0;
    }

    wl_static_desc *stat = &out->statics[out->static_count++];
    stat->x = (uint16_t)x;
    stat->y = (uint16_t)y;
    stat->source_tile = tile;
    stat->type = type;
    stat->blocking = blocking;
    stat->bonus = bonus;
    stat->treasure = treasure;
    if (blocking) {
        ++out->blocking_static_count;
    }
    if (bonus) {
        ++out->bonus_static_count;
    }
    if (treasure) {
        ++out->treasure_total;
    }
    return 0;
}

static int actor_tile_for_difficulty(uint16_t *tile, wl_difficulty difficulty,
                                     uint16_t easy_first, uint16_t easy_patrol_first,
                                     uint16_t medium_first, uint16_t medium_patrol_first,
                                     uint16_t hard_first, uint16_t hard_patrol_first,
                                     wl_actor_mode *mode, uint16_t *dir) {
    uint16_t t = *tile;
    if (in_range(t, hard_first, (uint16_t)(hard_first + 3))) {
        if (difficulty < WL_DIFFICULTY_HARD) {
            return 0;
        }
        *mode = WL_ACTOR_STAND;
        *dir = (uint16_t)(t - hard_first);
        return 1;
    }
    if (in_range(t, (uint16_t)(hard_patrol_first), (uint16_t)(hard_patrol_first + 3))) {
        if (difficulty < WL_DIFFICULTY_HARD) {
            return 0;
        }
        *mode = WL_ACTOR_PATROL;
        *dir = (uint16_t)(t - hard_patrol_first);
        return 1;
    }
    if (in_range(t, medium_first, (uint16_t)(medium_first + 3))) {
        if (difficulty < WL_DIFFICULTY_MEDIUM) {
            return 0;
        }
        *mode = WL_ACTOR_STAND;
        *dir = (uint16_t)(t - medium_first);
        return 1;
    }
    if (in_range(t, medium_patrol_first, (uint16_t)(medium_patrol_first + 3))) {
        if (difficulty < WL_DIFFICULTY_MEDIUM) {
            return 0;
        }
        *mode = WL_ACTOR_PATROL;
        *dir = (uint16_t)(t - medium_patrol_first);
        return 1;
    }
    if (in_range(t, easy_first, (uint16_t)(easy_first + 3))) {
        *mode = WL_ACTOR_STAND;
        *dir = (uint16_t)(t - easy_first);
        return 1;
    }
    if (in_range(t, easy_patrol_first, (uint16_t)(easy_patrol_first + 3))) {
        *mode = WL_ACTOR_PATROL;
        *dir = (uint16_t)(t - easy_patrol_first);
        return 1;
    }
    return -1;
}

static int add_actor(wl_game_model *out, size_t x, size_t y, uint16_t source_tile,
                     wl_actor_kind kind, wl_actor_mode mode, wl_direction dir,
                     int shootable, int kill_total) {
    if (out->actor_count >= WL_MAX_ACTORS) {
        return -1;
    }

    wl_actor_desc *actor = &out->actors[out->actor_count++];
    actor->spawn_x = (uint16_t)x;
    actor->spawn_y = (uint16_t)y;
    actor->tile_x = (uint16_t)x;
    actor->tile_y = (uint16_t)y;
    actor->source_tile = source_tile;
    actor->kind = kind;
    actor->mode = mode;
    actor->dir = dir;
    actor->shootable = shootable;
    actor->counts_for_kill_total = kill_total;

    if (mode == WL_ACTOR_PATROL) {
        switch (dir) {
        case WL_DIR_NORTH:
            actor->tile_x++;
            break;
        case WL_DIR_EAST:
            actor->tile_y--;
            break;
        case WL_DIR_SOUTH:
            actor->tile_x--;
            break;
        case WL_DIR_WEST:
            actor->tile_y++;
            break;
        case WL_DIR_NONE:
            break;
        }
    }

    if (kill_total) {
        ++out->kill_total;
    }
    return 0;
}

static int add_marker(wl_marker_desc *markers, size_t max_markers, size_t *count,
                      size_t x, size_t y, uint16_t tile, wl_direction dir) {
    if (*count >= max_markers) {
        return -1;
    }
    wl_marker_desc *marker = &markers[(*count)++];
    marker->x = (uint16_t)x;
    marker->y = (uint16_t)y;
    marker->source_tile = tile;
    marker->dir = dir;
    return 0;
}

static uint16_t neighboring_area_tile(const uint16_t *wall_plane, size_t x, size_t y) {
    uint16_t tile = 0;
    if (x + 1 < WL_MAP_SIDE && wall_plane[map_index(x + 1, y)] >= WL_AREATILE) {
        tile = wall_plane[map_index(x + 1, y)];
    }
    if (y > 0 && wall_plane[map_index(x, y - 1)] >= WL_AREATILE) {
        tile = wall_plane[map_index(x, y - 1)];
    }
    if (y + 1 < WL_MAP_SIDE && wall_plane[map_index(x, y + 1)] >= WL_AREATILE) {
        tile = wall_plane[map_index(x, y + 1)];
    }
    if (x > 0 && wall_plane[map_index(x - 1, y)] >= WL_AREATILE) {
        tile = wall_plane[map_index(x - 1, y)];
    }
    return tile;
}

int wl_build_game_model(const uint16_t *wall_plane, const uint16_t *info_plane,
                        size_t word_count, wl_difficulty difficulty,
                        wl_game_model *out) {
    if (!wall_plane || !info_plane || !out || word_count != WL_MAP_PLANE_WORDS ||
        difficulty > WL_DIFFICULTY_HARD) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->player.dir = WL_DIR_NONE;

    for (size_t i = 0; i < word_count; ++i) {
        uint16_t tile = wall_plane[i];
        out->tilemap[i] = tile < WL_AREATILE ? tile : 0;
    }

    for (size_t y = 0; y < WL_MAP_SIDE; ++y) {
        for (size_t x = 0; x < WL_MAP_SIDE; ++x) {
            uint16_t tile = wall_plane[map_index(x, y)];
            if (in_range(tile, 90, 101) && add_door(out, wall_plane, x, y, tile) != 0) {
                return -1;
            }
        }
    }

    for (size_t y = 0; y < WL_MAP_SIDE; ++y) {
        for (size_t x = 0; x < WL_MAP_SIDE; ++x) {
            uint16_t tile = info_plane[map_index(x, y)];
            if (tile == 0) {
                continue;
            }

            if (in_range(tile, 19, 22)) {
                if (!out->player.present) {
                    out->player.present = 1;
                    out->player.x = (uint16_t)x;
                    out->player.y = (uint16_t)y;
                    out->player.dir = (wl_direction)(tile - 19);
                }
            } else if (in_range(tile, 23, 74)) {
                if (add_static(out, x, y, tile) != 0) {
                    return -1;
                }
            } else if (in_range(tile, WL_ICONARROWS, WL_ICONARROWS + 7)) {
                if (add_marker(out->path_markers, WL_MAX_PATH_MARKERS, &out->path_marker_count,
                               x, y, tile, (wl_direction)(tile - WL_ICONARROWS)) != 0) {
                    return -1;
                }
            } else if (tile == WL_PUSHABLETILE) {
                if (add_marker(out->pushwalls, WL_MAX_PUSHWALLS, &out->pushwall_count,
                               x, y, tile, WL_DIR_NONE) != 0) {
                    return -1;
                }
                ++out->secret_total;
            } else if (tile == 124) {
                if (add_actor(out, x, y, tile, WL_ACTOR_DEAD_GUARD, WL_ACTOR_INERT,
                              WL_DIR_NONE, 0, 0) != 0) {
                    return -1;
                }
            } else {
                wl_actor_mode mode = WL_ACTOR_STAND;
                uint16_t dir = 0;
                int match = actor_tile_for_difficulty(&tile, difficulty, 108, 112, 144, 148, 180, 184,
                                                      &mode, &dir);
                if (match == 1) {
                    if (add_actor(out, x, y, tile, WL_ACTOR_GUARD, mode, (wl_direction)dir, 1, 1) != 0) {
                        return -1;
                    }
                    continue;
                }
                match = actor_tile_for_difficulty(&tile, difficulty, 134, 138, 170, 174, 206, 210,
                                                  &mode, &dir);
                if (match == 1) {
                    if (add_actor(out, x, y, tile, WL_ACTOR_DOG, mode, (wl_direction)dir, 1, 1) != 0) {
                        return -1;
                    }
                    continue;
                }
                if ((in_range(tile, 144, 151) && difficulty < WL_DIFFICULTY_MEDIUM) ||
                    (in_range(tile, 180, 187) && difficulty < WL_DIFFICULTY_HARD) ||
                    (in_range(tile, 170, 177) && difficulty < WL_DIFFICULTY_MEDIUM) ||
                    (in_range(tile, 206, 213) && difficulty < WL_DIFFICULTY_HARD)) {
                    continue;
                }
                if (match == -1 ||
                    in_range(tile, 116, 123) || in_range(tile, 152, 159) || in_range(tile, 188, 195) ||
                    in_range(tile, 126, 133) || in_range(tile, 162, 169) || in_range(tile, 198, 205) ||
                    in_range(tile, 216, 223) || in_range(tile, 234, 241) || in_range(tile, 252, 259) ||
                    in_range(tile, 224, 227) || tile == 160 || tile == 178 || tile == 179 ||
                    tile == 196 || tile == 197 || tile == 214 || tile == 215) {
                    ++out->unknown_info_tiles;
                }
            }
        }
    }

    for (size_t y = 0; y < WL_MAP_SIDE; ++y) {
        for (size_t x = 0; x < WL_MAP_SIDE; ++x) {
            if (wall_plane[map_index(x, y)] == WL_AMBUSHTILE) {
                out->tilemap[map_index(x, y)] = 0;
                uint16_t replacement = neighboring_area_tile(wall_plane, x, y);
                (void)replacement; /* Future runtime map data should mirror the original mapsegs mutation. */
            }
        }
    }

    return 0;
}
