#include "wl_game_model.h"

#include <stdlib.h>
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
        1,       /* 49 marble pillar (Spear) */
        2,       /* 50 25-ammo clip (Spear) */
        1,       /* 51 truck (Spear) */
        2,       /* 52 Spear of Destiny pickup (Spear) */
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
    door->action = WL_DOOR_CLOSED;
    door->position = 0;
    door->ticcount = 0;

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

static void record_unknown_info_tile(wl_game_model *out, size_t x, size_t y, uint16_t tile);

static int add_static(wl_game_model *out, size_t x, size_t y, uint16_t tile) {
    if (out->static_count >= WL_MAX_STATS) {
        return -1;
    }
    uint16_t type = (uint16_t)(tile - 23);
    int blocking = 0;
    int bonus = 0;
    int treasure = 0;
    if (static_traits(type, &blocking, &bonus, &treasure) != 0) {
        record_unknown_info_tile(out, x, y, tile);
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
    stat->active = 1;
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

static uint32_t fnv1a_u16(uint32_t hash, uint16_t value) {
    hash ^= (uint32_t)(value & 0xffu);
    hash *= 16777619u;
    hash ^= (uint32_t)(value >> 8);
    hash *= 16777619u;
    return hash;
}

static void record_unknown_info_tile(wl_game_model *out, size_t x, size_t y, uint16_t tile) {
    if (out->unknown_info_tiles == 0) {
        out->first_unknown_info_tile = tile;
        out->first_unknown_info_x = (uint16_t)x;
        out->first_unknown_info_y = (uint16_t)y;
        out->unknown_info_hash = 2166136261u;
    }
    out->last_unknown_info_tile = tile;
    out->last_unknown_info_x = (uint16_t)x;
    out->last_unknown_info_y = (uint16_t)y;
    out->unknown_info_hash = fnv1a_u16(out->unknown_info_hash, tile);
    out->unknown_info_hash = fnv1a_u16(out->unknown_info_hash, (uint16_t)x);
    out->unknown_info_hash = fnv1a_u16(out->unknown_info_hash, (uint16_t)y);
    ++out->unknown_info_tiles;
}

static int boss_tile_direction(uint16_t tile, wl_direction *dir) {
    if (!dir) {
        return 0;
    }
    switch (tile) {
    case 160: /* SpawnFakeHitler */
    case 197: /* SpawnGretel */
    case 215: /* SpawnGift */
        *dir = WL_DIR_NORTH;
        return 1;
    case 107: /* SpawnAngel (Spear) */
    case 125: /* SpawnTrans (Spear) */
    case 142: /* SpawnUber (Spear) */
    case 143: /* SpawnWill (Spear) */
    case 161: /* SpawnDeath (Spear) */
    case 178: /* SpawnHitler */
    case 179: /* SpawnFat */
    case 196: /* SpawnSchabbs */
    case 214: /* SpawnBoss */
        *dir = WL_DIR_SOUTH;
        return 1;
    case 106: /* SpawnSpectre (Spear) */
        *dir = WL_DIR_EAST;
        return 1;
    default:
        return 0;
    }
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
            } else if (tile == WL_EXITTILE) {
                /*
                 * Some boss maps place the original exit trigger on the info plane
                 * instead of as a wall-plane special. It is not an actor/static and
                 * should not remain as an unknown info tile in the runtime model.
                 */
                continue;
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
                if (match == 0) {
                    continue;
                }
                match = actor_tile_for_difficulty(&tile, difficulty, 116, 120, 152, 156, 188, 192,
                                                  &mode, &dir);
                if (match == 1) {
                    if (add_actor(out, x, y, tile, WL_ACTOR_OFFICER, mode, (wl_direction)dir, 1, 1) != 0) {
                        return -1;
                    }
                    continue;
                }
                if (match == 0) {
                    continue;
                }
                match = actor_tile_for_difficulty(&tile, difficulty, 126, 130, 162, 166, 198, 202,
                                                  &mode, &dir);
                if (match == 1) {
                    if (add_actor(out, x, y, tile, WL_ACTOR_SS, mode, (wl_direction)dir, 1, 1) != 0) {
                        return -1;
                    }
                    continue;
                }
                if (match == 0) {
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
                if (match == 0) {
                    continue;
                }
                match = actor_tile_for_difficulty(&tile, difficulty, 216, 220, 234, 238, 252, 256,
                                                  &mode, &dir);
                if (match == 1) {
                    if (add_actor(out, x, y, tile, WL_ACTOR_MUTANT, mode, (wl_direction)dir, 1, 1) != 0) {
                        return -1;
                    }
                    continue;
                }
                if (match == 0) {
                    continue;
                }
                wl_direction boss_dir = WL_DIR_NONE;
                if (boss_tile_direction(tile, &boss_dir)) {
                    if (add_actor(out, x, y, tile, WL_ACTOR_BOSS, WL_ACTOR_BOSS_MODE,
                                  boss_dir, 1, 1) != 0) {
                        return -1;
                    }
                    continue;
                }
                if (in_range(tile, 224, 227)) {
                    if (add_actor(out, x, y, tile, WL_ACTOR_GHOST, WL_ACTOR_GHOST_MODE,
                                  WL_DIR_EAST, 0, 1) != 0) {
                        return -1;
                    }
                    continue;
                }
                if (match == -1) {
                    record_unknown_info_tile(out, x, y, tile);
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

static uint16_t static_type_to_sprite_index(uint16_t type) {
    static const uint16_t table[] = {
        2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25,
        26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 36, 37, 38, 39, 40, 41,
        42, 43, 44, 45, 46, 47, 48, 49,
        28, 50, 51, 52,
        53,
    };
    if (type >= sizeof(table) / sizeof(table[0])) {
        return UINT16_MAX;
    }
    return table[type];
}

static uint16_t actor_to_sprite_index(const wl_actor_desc *actor, int spear) {
    if (!actor) {
        return UINT16_MAX;
    }
    if (actor->scene_source_override) {
        return actor->scene_source_index;
    }
    if (spear) {
        switch (actor->kind) {
        case WL_ACTOR_GUARD:
            return (actor->mode == WL_ACTOR_PATROL || actor->mode == WL_ACTOR_CHASE) ? 62 : 54; /* SPEAR SPR_GRD_W1_1 / SPR_GRD_S_1 */
        case WL_ACTOR_OFFICER:
            return (actor->mode == WL_ACTOR_PATROL || actor->mode == WL_ACTOR_CHASE) ? 250 : 242; /* SPEAR SPR_OFC_W1_1 / SPR_OFC_S_1 */
        case WL_ACTOR_SS:
            return (actor->mode == WL_ACTOR_PATROL || actor->mode == WL_ACTOR_CHASE) ? 150 : 142; /* SPEAR SPR_SS_W1_1 / SPR_SS_S_1 */
        case WL_ACTOR_DOG:
            return 103; /* SPEAR SPR_DOG_W1_1 */
        case WL_ACTOR_MUTANT:
            return (actor->mode == WL_ACTOR_PATROL || actor->mode == WL_ACTOR_CHASE) ? 199 : 191; /* SPEAR SPR_MUT_W1_1 / SPR_MUT_S_1 */
        case WL_ACTOR_DEAD_GUARD:
            return 99; /* SPEAR SPR_GRD_DEAD */
        case WL_ACTOR_BOSS:
            switch (actor->source_tile) {
            case 106:
                return 377; /* SPEAR SPR_SPECTRE_W1 */
            case 107:
                return 385; /* SPEAR SPR_ANGEL_W1 */
            case 125:
                return 326; /* SPEAR SPR_TRANS_W1 */
            case 142:
                return 349; /* SPEAR SPR_UBER_W1 */
            case 143:
                return 337; /* SPEAR SPR_WILL_W1 */
            case 161:
                return 362; /* SPEAR SPR_DEATH_W1 */
            default:
                return UINT16_MAX;
            }
        case WL_ACTOR_GHOST:
            return UINT16_MAX;
        }
    }
    switch (actor->kind) {
    case WL_ACTOR_GUARD:
        return (actor->mode == WL_ACTOR_PATROL || actor->mode == WL_ACTOR_CHASE) ? 58 : 50; /* SPR_GRD_W1_1 / SPR_GRD_S_1 */
    case WL_ACTOR_OFFICER:
        return (actor->mode == WL_ACTOR_PATROL || actor->mode == WL_ACTOR_CHASE) ? 246 : 238; /* SPR_OFC_W1_1 / SPR_OFC_S_1 */
    case WL_ACTOR_SS:
        return (actor->mode == WL_ACTOR_PATROL || actor->mode == WL_ACTOR_CHASE) ? 146 : 138; /* SPR_SS_W1_1 / SPR_SS_S_1 */
    case WL_ACTOR_DOG:
        return 99; /* SPR_DOG_W1_1 */
    case WL_ACTOR_MUTANT:
        return (actor->mode == WL_ACTOR_PATROL || actor->mode == WL_ACTOR_CHASE) ? 195 : 187; /* SPR_MUT_W1_1 / SPR_MUT_S_1 */
    case WL_ACTOR_DEAD_GUARD:
        return 95; /* SPR_GRD_DEAD */
    case WL_ACTOR_BOSS:
        switch (actor->source_tile) {
        case 160:
            return 321; /* SPR_FAKE_W1 */
        case 178:
            return 334; /* SPR_MECHA_W1 */
        case 179:
            return 415; /* SPR_FAT_W1 */
        case 196:
            return 307; /* SPR_SCHABB_W1 */
        case 197:
            return 404; /* SPR_GRETEL_W1 */
        case 214:
            return 296; /* SPR_BOSS_W1 */
        case 215:
            return 360; /* SPR_GIFT_W1 */
        default:
            return UINT16_MAX;
        }
    case WL_ACTOR_GHOST:
        switch (actor->source_tile) {
        case 224:
            return 288; /* SPR_BLINKY_W1 */
        case 225:
            return 292; /* SPR_CLYDE_W1 */
        case 226:
            return 290; /* SPR_PINKY_W1 */
        case 227:
            return 294; /* SPR_INKY_W1 */
        default:
            return UINT16_MAX;
        }
    }
    return UINT16_MAX;
}

int wl_build_runtime_solid_plane(const wl_game_model *model, uint16_t door_tile,
                                 uint16_t *out) {
    if (!model || !out || door_tile == 0 || door_tile >= 64) {
        return -1;
    }
    for (size_t i = 0; i < WL_MAP_PLANE_WORDS; ++i) {
        uint16_t tile = model->tilemap[i];
        if (tile == 0 || tile == 0x40u) {
            out[i] = 0;
        } else if (tile & 0x80u) {
            uint16_t moving_tile = (uint16_t)(tile & 63u);
            out[i] = moving_tile ? moving_tile : door_tile;
        } else if (tile < 64u) {
            out[i] = tile;
        } else {
            out[i] = 0;
        }
    }
    return 0;
}

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
                                       wl_wall_strip *strips) {
    uint16_t runtime_plane[WL_MAP_PLANE_WORDS];
    if (wl_build_runtime_solid_plane(model, door_tile, runtime_plane) != 0) {
        return -1;
    }
    return wl_render_camera_wall_view(runtime_plane, WL_MAP_PLANE_WORDS,
                                      origin_x, origin_y, forward_x, forward_y,
                                      plane_x, plane_y, first_x, x_step, ray_count,
                                      wall_pages, wall_page_sizes, wall_page_count,
                                      dst, directions_x, directions_y, hits, strips);
}

int wl_build_door_wall_hit(const wl_door_desc *door, uint16_t vswap_sprite_start,
                           uint32_t intercept, uint16_t x, uint16_t scaled_height,
                           wl_map_wall_hit *out) {
    if (!door || !out || scaled_height == 0 || vswap_sprite_start < 8u) {
        return -1;
    }

    uint16_t door_page = (uint16_t)(vswap_sprite_start - 8u);
    if (door->lock >= 1u && door->lock <= 4u) {
        door_page = (uint16_t)(door_page + 6u);
    } else if (door->lock >= 5u) {
        door_page = (uint16_t)(door_page + 4u);
    }
    if (door->vertical) {
        door_page = (uint16_t)(door_page + 1u);
    }

    memset(out, 0, sizeof(*out));
    out->tile_x = door->x;
    out->tile_y = door->y;
    out->source_tile = door->source_tile;
    out->side = door->vertical ? WL_WALL_SIDE_VERTICAL : WL_WALL_SIDE_HORIZONTAL;
    out->wall_page_index = door_page;
    out->texture_offset = (uint16_t)(((intercept - door->position) >> 4) & 0xfc0u);
    out->x = x;
    out->scaled_height = scaled_height;
    return 0;
}

int wl_build_pushwall_wall_hit(uint16_t tile, wl_wall_side side,
                               uint32_t intercept, uint16_t pwall_pos,
                               int step_sign, uint16_t x, uint16_t scaled_height,
                               wl_map_wall_hit *out) {
    if (!out || tile == 0 || tile >= 64u || pwall_pos >= WL_MAP_SIDE ||
        scaled_height == 0 ||
        (side != WL_WALL_SIDE_HORIZONTAL && side != WL_WALL_SIDE_VERTICAL) ||
        (step_sign != -1 && step_sign != 1)) {
        return -1;
    }

    uint16_t texture = (uint16_t)((intercept >> 4) & 0xfc0u);
    if ((side == WL_WALL_SIDE_HORIZONTAL && step_sign != -1) ||
        (side == WL_WALL_SIDE_VERTICAL && step_sign == -1)) {
        texture = (uint16_t)(0xfc0u - texture);
    }

    memset(out, 0, sizeof(*out));
    out->source_tile = tile;
    out->side = side;
    out->wall_page_index = (uint16_t)((tile - 1u) * 2u +
                                      (side == WL_WALL_SIDE_VERTICAL ? 1u : 0u));
    out->texture_offset = texture;
    out->x = x;
    out->scaled_height = scaled_height;
    (void)pwall_pos;
    return 0;
}

int wl_cast_runtime_fixed_wall_ray(const wl_game_model *model,
                                   uint16_t vswap_sprite_start,
                                   uint32_t origin_x, uint32_t origin_y,
                                   int32_t direction_x, int32_t direction_y,
                                   uint16_t x, uint16_t scaled_height,
                                   wl_map_wall_hit *out) {
    if (!model || !out || scaled_height == 0 || vswap_sprite_start < 8u ||
        origin_x >= ((uint32_t)WL_MAP_SIDE << 16) ||
        origin_y >= ((uint32_t)WL_MAP_SIDE << 16) ||
        (direction_x == 0 && direction_y == 0)) {
        return -1;
    }

    int tile_x = (int)(origin_x >> 16);
    int tile_y = (int)(origin_y >> 16);
    int step_x = 0;
    int step_y = 0;
    int64_t side_t_x = INT64_MAX;
    int64_t side_t_y = INT64_MAX;
    int64_t delta_t_x = INT64_MAX;
    int64_t delta_t_y = INT64_MAX;

    if (direction_x > 0) {
        step_x = 1;
        uint32_t next_boundary = (uint32_t)(tile_x + 1) << 16;
        side_t_x = ((int64_t)(next_boundary - origin_x) << 16) / direction_x;
        delta_t_x = ((int64_t)1 << 32) / direction_x;
    } else if (direction_x < 0) {
        step_x = -1;
        uint32_t next_boundary = (uint32_t)tile_x << 16;
        side_t_x = ((int64_t)(origin_x - next_boundary) << 16) / -(int64_t)direction_x;
        delta_t_x = ((int64_t)1 << 32) / -(int64_t)direction_x;
    }

    if (direction_y > 0) {
        step_y = 1;
        uint32_t next_boundary = (uint32_t)(tile_y + 1) << 16;
        side_t_y = ((int64_t)(next_boundary - origin_y) << 16) / direction_y;
        delta_t_y = ((int64_t)1 << 32) / direction_y;
    } else if (direction_y < 0) {
        step_y = -1;
        uint32_t next_boundary = (uint32_t)tile_y << 16;
        side_t_y = ((int64_t)(origin_y - next_boundary) << 16) / -(int64_t)direction_y;
        delta_t_y = ((int64_t)1 << 32) / -(int64_t)direction_y;
    }

    for (size_t step_count = 0; step_count < WL_MAP_SIDE * 2u; ++step_count) {
        wl_wall_side side = WL_WALL_SIDE_HORIZONTAL;
        int64_t hit_t = 0;
        if (side_t_x <= side_t_y) {
            tile_x += step_x;
            hit_t = side_t_x;
            side_t_x += delta_t_x;
            side = WL_WALL_SIDE_VERTICAL;
        } else {
            tile_y += step_y;
            hit_t = side_t_y;
            side_t_y += delta_t_y;
            side = WL_WALL_SIDE_HORIZONTAL;
        }

        if (tile_x < 0 || tile_y < 0 || tile_x >= WL_MAP_SIDE || tile_y >= WL_MAP_SIDE) {
            return -1;
        }

        uint16_t tile = model->tilemap[map_index((size_t)tile_x, (size_t)tile_y)];
        if (tile == 0 || tile == 0x40u) {
            continue;
        }

        int64_t hit_coord = 0;
        if (side == WL_WALL_SIDE_VERTICAL) {
            hit_coord = (int64_t)origin_y + (((int64_t)direction_y * hit_t) >> 16);
        } else {
            hit_coord = (int64_t)origin_x + (((int64_t)direction_x * hit_t) >> 16);
        }

        if ((tile & 0xc0u) == 0xc0u) {
            tile = (uint16_t)(tile & 63u);
            uint16_t pwall_pos = model->pushwall_motion.active ? model->pushwall_motion.pos : 0;
            int step_sign = (side == WL_WALL_SIDE_VERTICAL) ? step_x : step_y;
            if (wl_build_pushwall_wall_hit(tile, side, (uint32_t)hit_coord, pwall_pos,
                                           step_sign, x, scaled_height, out) != 0) {
                return -1;
            }
            uint32_t offset = (uint32_t)pwall_pos << 10;
            uint32_t axis_delta = (step_sign == -1) ? (0x10000u - offset) : offset;
            int64_t axis_direction = (side == WL_WALL_SIDE_VERTICAL) ? direction_x : direction_y;
            int64_t adjusted_hit_t = hit_t;
            if (axis_direction < 0) {
                axis_direction = -axis_direction;
            }
            if (axis_direction != 0) {
                adjusted_hit_t += ((int64_t)axis_delta << 16) / axis_direction;
            }
            out->tile_x = (uint16_t)tile_x;
            out->tile_y = (uint16_t)tile_y;
            out->distance = (adjusted_hit_t > UINT32_MAX) ? UINT32_MAX : (uint32_t)adjusted_hit_t;
            return 0;
        } else if (tile & 0x80u) {
            uint16_t door_index = (uint16_t)(tile & 0x7fu);
            if (door_index >= model->door_count ||
                wl_build_door_wall_hit(&model->doors[door_index], vswap_sprite_start,
                                       (uint32_t)hit_coord, x, scaled_height, out) != 0) {
                return -1;
            }
            out->distance = (hit_t > UINT32_MAX) ? UINT32_MAX : (uint32_t)hit_t;
            return 0;
        } else if (tile >= 64u) {
            continue;
        }

        if (tile == 0 || tile >= 64u) {
            continue;
        }
        memset(out, 0, sizeof(*out));
        out->tile_x = (uint16_t)tile_x;
        out->tile_y = (uint16_t)tile_y;
        out->source_tile = tile;
        out->side = side;
        out->wall_page_index = (uint16_t)((tile - 1u) * 2u +
                                          (side == WL_WALL_SIDE_VERTICAL ? 1u : 0u));
        out->texture_offset = (uint16_t)(((uint64_t)(hit_coord >> 10) &
                                          (WL_MAP_SIDE - 1u)) * WL_MAP_SIDE);
        out->x = x;
        out->scaled_height = scaled_height;
        out->distance = (hit_t > UINT32_MAX) ? UINT32_MAX : (uint32_t)hit_t;
        return 0;
    }

    return -1;
}

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
                                            wl_wall_strip *strips) {
    if (!model || !wall_pages || !wall_page_sizes || !dst || !directions_x ||
        !directions_y || !hits || !strips || ray_count == 0 || x_step == 0) {
        return -1;
    }

    if (wl_build_camera_ray_directions(forward_x, forward_y, plane_x, plane_y,
                                       dst->width, first_x, x_step, ray_count,
                                       directions_x, directions_y) != 0) {
        return -1;
    }

    for (size_t i = 0; i < ray_count; ++i) {
        uint32_t x = (uint32_t)first_x + (uint32_t)i * x_step;
        if (x >= dst->width) {
            return -1;
        }
        if (wl_cast_runtime_fixed_wall_ray(model, vswap_sprite_start, origin_x, origin_y,
                                           directions_x[i], directions_y[i], (uint16_t)x,
                                           1, &hits[i]) != 0) {
            return -1;
        }
        hits[i].scaled_height = wl_project_wall_height(hits[i].distance, dst->width,
                                                       dst->height);
        if (hits[i].scaled_height == 0) {
            return -1;
        }
        uint16_t page = hits[i].wall_page_index;
        if (page >= wall_page_count || !wall_pages[page] || wall_page_sizes[page] == 0) {
            return -1;
        }
        if (wl_wall_hit_to_strip(&hits[i], wall_pages[page], wall_page_sizes[page],
                                 &strips[i]) != 0) {
            return -1;
        }
    }

    return wl_render_wall_strip_viewport(strips, ray_count, dst);
}

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
                                             uint16_t *wall_heights) {
    if (!dst || !wall_heights || (sprite_count != 0 &&
        (!sprite_surfaces || !sprite_x || !sprite_y || !sprites))) {
        return -1;
    }

    if (wl_render_runtime_door_camera_wall_view(model, vswap_sprite_start, origin_x,
                                                origin_y, forward_x, forward_y,
                                                plane_x, plane_y, first_x, x_step,
                                                ray_count, wall_pages, wall_page_sizes,
                                                wall_page_count, dst, directions_x,
                                                directions_y, hits, strips) != 0) {
        return -1;
    }

    for (uint16_t x = 0; x < dst->width; ++x) {
        wall_heights[x] = 0;
    }
    for (size_t i = 0; i < ray_count; ++i) {
        uint16_t x0 = hits[i].x;
        uint16_t x1 = (uint16_t)(x0 + x_step);
        if (x1 > dst->width) {
            x1 = dst->width;
        }
        for (uint16_t x = x0; x < x1; ++x) {
            wall_heights[x] = hits[i].scaled_height;
        }
    }

    for (size_t i = 0; i < sprite_count; ++i) {
        if (!sprite_surfaces[i]) {
            return -1;
        }
        uint16_t source_index = sprite_source_indices ? sprite_source_indices[i] : (uint16_t)i;
        if (wl_project_world_sprite(source_index, origin_x, origin_y, sprite_x[i], sprite_y[i],
                                    forward_x, forward_y, dst->width, dst->height,
                                    &sprites[i]) != 0) {
            return -1;
        }
        sprites[i].surface_index = i;
    }
    if (wl_sort_projected_sprites_far_to_near(sprites, sprite_count) != 0) {
        return -1;
    }
    for (size_t i = 0; i < sprite_count; ++i) {
        if (!sprites[i].visible) {
            continue;
        }
        size_t surface_index = sprites[i].surface_index;
        if (surface_index >= sprite_count || !sprite_surfaces[surface_index]) {
            return -1;
        }
        if (wl_render_scaled_sprite(sprite_surfaces[surface_index], dst, sprites[i].view_x,
                                    sprites[i].scaled_height, transparent_index,
                                    wall_heights, dst->width) != 0) {
            return -1;
        }
    }
    return 0;
}

static int collect_scene_sprite_refs_impl(const wl_game_model *model, uint16_t vswap_sprite_start,
                                          wl_scene_sprite_ref *refs, size_t max_refs,
                                          size_t *out_count, int spear) {
    if (!model || !refs || !out_count) {
        return -1;
    }

    size_t count = 0;
    for (size_t i = 0; i < model->static_count; ++i) {
        if (!model->statics[i].active) {
            continue;
        }
        uint16_t sprite = static_type_to_sprite_index(model->statics[i].type);
        if (sprite == UINT16_MAX) {
            continue;
        }
        if (count >= max_refs) {
            return -1;
        }
        refs[count].kind = WL_SCENE_SPRITE_STATIC;
        refs[count].model_index = (uint16_t)i;
        refs[count].source_index = sprite;
        refs[count].vswap_chunk_index = (uint16_t)(vswap_sprite_start + sprite);
        refs[count].world_x = ((uint32_t)model->statics[i].x << 16) + 0x8000u;
        refs[count].world_y = ((uint32_t)model->statics[i].y << 16) + 0x8000u;
        ++count;
    }

    for (size_t i = 0; i < model->actor_count; ++i) {
        uint16_t sprite = actor_to_sprite_index(&model->actors[i], spear);
        if (sprite == UINT16_MAX) {
            continue;
        }
        if (count >= max_refs) {
            return -1;
        }
        refs[count].kind = WL_SCENE_SPRITE_ACTOR;
        refs[count].model_index = (uint16_t)i;
        refs[count].source_index = sprite;
        refs[count].vswap_chunk_index = (uint16_t)(vswap_sprite_start + sprite);
        refs[count].world_x = model->actors[i].fine_x ?
            model->actors[i].fine_x : (((uint32_t)model->actors[i].tile_x << 16) + 0x8000u);
        refs[count].world_y = model->actors[i].fine_y ?
            model->actors[i].fine_y : (((uint32_t)model->actors[i].tile_y << 16) + 0x8000u);
        ++count;
    }

    *out_count = count;
    return 0;
}

static int path_step(wl_direction dir, int *dx, int *dy) {
    if (!dx || !dy) {
        return -1;
    }
    *dx = 0;
    *dy = 0;
    switch (dir) {
    case WL_DIR_NORTH:
        *dy = -1;
        return 0;
    case WL_DIR_EAST:
        *dx = 1;
        return 0;
    case WL_DIR_SOUTH:
        *dy = 1;
        return 0;
    case WL_DIR_WEST:
        *dx = -1;
        return 0;
    case WL_DIR_NONE:
        return 0;
    default:
        return -1;
    }
}


int wl_collect_scene_sprite_refs(const wl_game_model *model, uint16_t vswap_sprite_start,
                                 wl_scene_sprite_ref *refs, size_t max_refs,
                                 size_t *out_count) {
    return collect_scene_sprite_refs_impl(model, vswap_sprite_start, refs, max_refs,
                                          out_count, 0);
}

int wl_collect_spear_scene_sprite_refs(const wl_game_model *model, uint16_t vswap_sprite_start,
                                       wl_scene_sprite_ref *refs, size_t max_refs,
                                       size_t *out_count) {
    return collect_scene_sprite_refs_impl(model, vswap_sprite_start, refs, max_refs,
                                          out_count, 1);
}

int wl_select_path_direction(const wl_game_model *model, uint16_t tile_x,
                             uint16_t tile_y, wl_direction current_dir,
                             wl_direction *out_dir) {
    if (!model || !out_dir || tile_x >= WL_MAP_SIDE || tile_y >= WL_MAP_SIDE ||
        current_dir > WL_DIR_NONE) {
        return -1;
    }

    wl_direction selected = current_dir;
    for (size_t i = 0; i < model->path_marker_count; ++i) {
        if (model->path_markers[i].x == tile_x && model->path_markers[i].y == tile_y) {
            selected = model->path_markers[i].dir <= WL_DIR_WEST ?
                model->path_markers[i].dir : WL_DIR_NONE;
            break;
        }
    }

    int dx = 0;
    int dy = 0;
    if (path_step(selected, &dx, &dy) != 0) {
        return -1;
    }
    if (selected == WL_DIR_NONE) {
        *out_dir = WL_DIR_NONE;
        return 0;
    }

    int next_x = (int)tile_x + dx;
    int next_y = (int)tile_y + dy;
    if (next_x < 0 || next_y < 0 || next_x >= WL_MAP_SIDE || next_y >= WL_MAP_SIDE ||
        model->tilemap[map_index((size_t)next_x, (size_t)next_y)] != 0) {
        *out_dir = WL_DIR_NONE;
        return 0;
    }

    *out_dir = selected;
    return 0;
}


static wl_direction opposite_dir(wl_direction dir) {
    switch (dir) {
    case WL_DIR_NORTH: return WL_DIR_SOUTH;
    case WL_DIR_EAST: return WL_DIR_WEST;
    case WL_DIR_SOUTH: return WL_DIR_NORTH;
    case WL_DIR_WEST: return WL_DIR_EAST;
    case WL_DIR_NONE: return WL_DIR_NONE;
    default: return WL_DIR_NONE;
    }
}

static int chase_try_direction(const wl_game_model *model, uint16_t actor_x,
                              uint16_t actor_y, wl_direction dir,
                              wl_actor_chase_dir_result *out) {
    int dx = 0;
    int dy = 0;
    if (path_step(dir, &dx, &dy) != 0 || dir == WL_DIR_NONE) {
        return 0;
    }
    int next_x = (int)actor_x + dx;
    int next_y = (int)actor_y + dy;
    if (next_x < 0 || next_y < 0 || next_x >= WL_MAP_SIDE || next_y >= WL_MAP_SIDE) {
        return 0;
    }
    if (model->tilemap[map_index((size_t)next_x, (size_t)next_y)] != 0) {
        return 0;
    }
    out->selected = 1;
    out->blocked = 0;
    out->dir = dir;
    out->next_x = (uint16_t)next_x;
    out->next_y = (uint16_t)next_y;
    return 1;
}

int wl_select_chase_direction(const wl_game_model *model, uint16_t actor_x,
                              uint16_t actor_y, uint16_t player_x,
                              uint16_t player_y, wl_direction current_dir,
                              int search_forward,
                              wl_actor_chase_dir_result *out) {
    if (!model || !out || actor_x >= WL_MAP_SIDE || actor_y >= WL_MAP_SIDE ||
        player_x >= WL_MAP_SIDE || player_y >= WL_MAP_SIDE || current_dir > WL_DIR_NONE) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->dir = WL_DIR_NONE;
    out->next_x = actor_x;
    out->next_y = actor_y;

    wl_direction turnaround = opposite_dir(current_dir);
    wl_direction primary = WL_DIR_NONE;
    wl_direction secondary = WL_DIR_NONE;
    int deltax = (int)player_x - (int)actor_x;
    int deltay = (int)player_y - (int)actor_y;
    if (deltax > 0) {
        primary = WL_DIR_EAST;
    } else if (deltax < 0) {
        primary = WL_DIR_WEST;
    }
    if (deltay > 0) {
        secondary = WL_DIR_SOUTH;
    } else if (deltay < 0) {
        secondary = WL_DIR_NORTH;
    }
    if (abs(deltay) > abs(deltax)) {
        wl_direction tmp = primary;
        primary = secondary;
        secondary = tmp;
    }
    if (primary == turnaround) {
        primary = WL_DIR_NONE;
    }
    if (secondary == turnaround) {
        secondary = WL_DIR_NONE;
    }

    if (chase_try_direction(model, actor_x, actor_y, primary, out)) {
        return 0;
    }
    if (chase_try_direction(model, actor_x, actor_y, secondary, out)) {
        return 0;
    }
    if (current_dir != WL_DIR_NONE && chase_try_direction(model, actor_x, actor_y, current_dir, out)) {
        return 0;
    }

    if (search_forward) {
        for (wl_direction dir = WL_DIR_NORTH; dir <= WL_DIR_WEST; dir = (wl_direction)(dir + 1)) {
            if (dir != turnaround && chase_try_direction(model, actor_x, actor_y, dir, out)) {
                return 0;
            }
        }
    } else {
        for (int dir = WL_DIR_WEST; dir >= WL_DIR_NORTH; --dir) {
            if ((wl_direction)dir != turnaround &&
                chase_try_direction(model, actor_x, actor_y, (wl_direction)dir, out)) {
                return 0;
            }
        }
    }
    if (turnaround != WL_DIR_NONE && chase_try_direction(model, actor_x, actor_y, turnaround, out)) {
        return 0;
    }
    out->blocked = 1;
    return 0;
}

int wl_step_chase_actor(wl_game_model *model, uint16_t actor_index,
                        uint16_t player_x, uint16_t player_y,
                        int search_forward,
                        wl_actor_chase_step_result *out) {
    if (!model || !out || actor_index >= model->actor_count) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    wl_actor_desc *actor = &model->actors[actor_index];
    out->dir = actor->dir;
    out->tile_x = actor->tile_x;
    out->tile_y = actor->tile_y;
    if (actor->mode != WL_ACTOR_CHASE || actor->tile_x >= WL_MAP_SIDE ||
        actor->tile_y >= WL_MAP_SIDE) {
        return -1;
    }
    wl_actor_chase_dir_result selected;
    if (wl_select_chase_direction(model, actor->tile_x, actor->tile_y,
                                  player_x, player_y, actor->dir,
                                  search_forward, &selected) != 0) {
        return -1;
    }
    out->dir = selected.dir;
    if (!selected.selected) {
        out->blocked = 1;
        return 0;
    }
    actor->dir = selected.dir;
    actor->tile_x = selected.next_x;
    actor->tile_y = selected.next_y;
    actor->fine_x = ((uint32_t)actor->tile_x << 16) + 0x8000u;
    actor->fine_y = ((uint32_t)actor->tile_y << 16) + 0x8000u;
    out->stepped = 1;
    out->tile_x = actor->tile_x;
    out->tile_y = actor->tile_y;
    return 0;
}

int wl_step_chase_actor_tics(wl_game_model *model, uint16_t actor_index,
                             uint16_t player_x, uint16_t player_y,
                             int search_forward, uint32_t speed,
                             int32_t tics,
                             wl_actor_chase_tic_result *out) {
    if (!model || !out || actor_index >= model->actor_count || tics < 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    wl_actor_desc *actor = &model->actors[actor_index];
    if (actor->mode != WL_ACTOR_CHASE || actor->tile_x >= WL_MAP_SIDE ||
        actor->tile_y >= WL_MAP_SIDE) {
        return -1;
    }
    int continuing_move = actor->patrol_remainder > 0;
    uint64_t move64 = (uint64_t)actor->patrol_remainder +
        ((uint64_t)speed * (uint32_t)tics);
    if (move64 > UINT32_MAX) {
        move64 = UINT32_MAX;
    }
    uint32_t move = (uint32_t)move64;
    out->requested_move = move;
    out->dir = actor->dir;
    out->tile_x = actor->tile_x;
    out->tile_y = actor->tile_y;

    wl_actor_chase_dir_result selected;
    memset(&selected, 0, sizeof(selected));
    if (actor->patrol_remainder > 0) {
        selected.dir = WL_DIR_NONE;
        selected.next_x = actor->tile_x;
        selected.next_y = actor->tile_y;
        chase_try_direction(model, actor->tile_x, actor->tile_y, actor->dir, &selected);
    } else if (wl_select_chase_direction(model, actor->tile_x, actor->tile_y,
                                         player_x, player_y, actor->dir,
                                         search_forward, &selected) != 0) {
        return -1;
    }
    out->dir = selected.dir;
    if (!selected.selected) {
        out->blocked = 1;
        out->leftover_move = actor->patrol_remainder;
        out->fine_x = actor->fine_x;
        out->fine_y = actor->fine_y;
        return 0;
    }
    actor->dir = selected.dir;
    if (move < 0x10000u) {
        int dx = 0;
        int dy = 0;
        if (path_step(selected.dir, &dx, &dy) != 0) {
            return -1;
        }
        actor->patrol_remainder = move;
        actor->fine_x = ((uint32_t)actor->tile_x << 16) + 0x8000u + (uint32_t)(dx * (int32_t)move);
        actor->fine_y = ((uint32_t)actor->tile_y << 16) + 0x8000u + (uint32_t)(dy * (int32_t)move);
        out->leftover_move = move;
        out->fine_x = actor->fine_x;
        out->fine_y = actor->fine_y;
        return 0;
    }

    if (continuing_move && move >= 0x10000u) {
        actor->tile_x = selected.next_x;
        actor->tile_y = selected.next_y;
        actor->fine_x = ((uint32_t)actor->tile_x << 16) + 0x8000u;
        actor->fine_y = ((uint32_t)actor->tile_y << 16) + 0x8000u;
        ++out->tiles_stepped;
        move -= 0x10000u;
        actor->patrol_remainder = 0;
    }

    wl_actor_chase_step_result step;
    while (move >= 0x10000u) {
        if (wl_step_chase_actor(model, actor_index, player_x, player_y,
                                search_forward, &step) != 0) {
            return -1;
        }
        if (!step.stepped) {
            out->blocked = 1;
            break;
        }
        ++out->tiles_stepped;
        move -= 0x10000u;
    }
    actor->patrol_remainder = out->blocked ? 0u : move;
    if (!out->blocked && move > 0) {
        int dx = 0;
        int dy = 0;
        if (path_step(actor->dir, &dx, &dy) != 0) {
            return -1;
        }
        actor->fine_x = ((uint32_t)actor->tile_x << 16) + 0x8000u + (uint32_t)(dx * (int32_t)move);
        actor->fine_y = ((uint32_t)actor->tile_y << 16) + 0x8000u + (uint32_t)(dy * (int32_t)move);
    }
    out->leftover_move = actor->patrol_remainder;
    out->tile_x = actor->tile_x;
    out->tile_y = actor->tile_y;
    out->fine_x = actor->fine_x;
    out->fine_y = actor->fine_y;
    out->dir = actor->dir;
    return 0;
}

int wl_step_chase_actors_tics(wl_game_model *model, uint16_t player_x,
                              uint16_t player_y, int search_forward,
                              uint32_t speed, int32_t tics,
                              wl_actor_chases_tic_result *out) {
    if (!model || !out || tics < 0 || player_x >= WL_MAP_SIDE || player_y >= WL_MAP_SIDE) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < model->actor_count; ++i) {
        if (model->actors[i].mode != WL_ACTOR_CHASE) {
            continue;
        }
        ++out->actors_considered;
        wl_actor_chase_tic_result step;
        if (wl_step_chase_actor_tics(model, (uint16_t)i, player_x, player_y,
                                     search_forward, speed, tics, &step) != 0) {
            return -1;
        }
        if (step.tiles_stepped > 0 || step.leftover_move > 0) {
            ++out->actors_stepped;
        }
        if (step.blocked) {
            ++out->actors_blocked;
        }
        out->tiles_stepped = (uint16_t)(out->tiles_stepped + step.tiles_stepped);
    }
    return 0;
}

int wl_step_patrol_actor(wl_game_model *model, uint16_t actor_index,
                         wl_actor_patrol_step_result *out) {
    if (!model || !out || actor_index >= model->actor_count) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    wl_actor_desc *actor = &model->actors[actor_index];
    out->dir = actor->dir;
    out->tile_x = actor->tile_x;
    out->tile_y = actor->tile_y;
    if (actor->mode != WL_ACTOR_PATROL || actor->tile_x >= WL_MAP_SIDE ||
        actor->tile_y >= WL_MAP_SIDE) {
        return -1;
    }

    wl_direction selected = WL_DIR_NONE;
    if (wl_select_path_direction(model, actor->tile_x, actor->tile_y,
                                 actor->dir, &selected) != 0) {
        return -1;
    }
    out->dir = selected;
    if (selected == WL_DIR_NONE) {
        out->blocked = 1;
        return 0;
    }

    int dx = 0;
    int dy = 0;
    if (path_step(selected, &dx, &dy) != 0) {
        return -1;
    }
    actor->dir = selected;
    actor->tile_x = (uint16_t)((int)actor->tile_x + dx);
    actor->tile_y = (uint16_t)((int)actor->tile_y + dy);
    actor->fine_x = ((uint32_t)actor->tile_x << 16) + 0x8000u;
    actor->fine_y = ((uint32_t)actor->tile_y << 16) + 0x8000u;
    out->stepped = 1;
    out->tile_x = actor->tile_x;
    out->tile_y = actor->tile_y;
    return 0;
}

int wl_step_patrol_actor_tics(wl_game_model *model, uint16_t actor_index,
                              uint32_t speed, int32_t tics,
                              wl_actor_patrol_tic_result *out) {
    if (!model || !out || actor_index >= model->actor_count || tics < 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    wl_actor_desc *actor = &model->actors[actor_index];
    if (actor->mode != WL_ACTOR_PATROL || actor->tile_x >= WL_MAP_SIDE ||
        actor->tile_y >= WL_MAP_SIDE) {
        return -1;
    }

    uint64_t move64 = (uint64_t)actor->patrol_remainder +
        ((uint64_t)speed * (uint32_t)tics);
    if (move64 > UINT32_MAX) {
        move64 = UINT32_MAX;
    }
    uint32_t move = (uint32_t)move64;
    out->requested_move = move;
    out->dir = actor->dir;
    out->tile_x = actor->tile_x;
    out->tile_y = actor->tile_y;

    while (move >= 0x10000u) {
        wl_actor_patrol_step_result step;
        if (wl_step_patrol_actor(model, actor_index, &step) != 0) {
            return -1;
        }
        out->dir = step.dir;
        out->tile_x = step.tile_x;
        out->tile_y = step.tile_y;
        if (step.blocked) {
            out->blocked = 1;
            break;
        }
        if (!step.stepped) {
            break;
        }
        ++out->tiles_stepped;
        actor->patrol_remainder = 0;
        move -= 0x10000u;
    }
    out->leftover_move = move;
    actor->patrol_remainder = move;
    actor->fine_x = ((uint32_t)actor->tile_x << 16) + 0x8000u;
    actor->fine_y = ((uint32_t)actor->tile_y << 16) + 0x8000u;
    if (!out->blocked && move > 0) {
        int dx = 0;
        int dy = 0;
        if (path_step(actor->dir, &dx, &dy) != 0) {
            return -1;
        }
        if (dx > 0) {
            actor->fine_x += move;
        } else if (dx < 0) {
            actor->fine_x -= move;
        }
        if (dy > 0) {
            actor->fine_y += move;
        } else if (dy < 0) {
            actor->fine_y -= move;
        }
    }
    out->fine_x = actor->fine_x;
    out->fine_y = actor->fine_y;
    return 0;
}

int wl_step_patrol_actors_tics(wl_game_model *model, uint32_t speed,
                               int32_t tics,
                               wl_actor_patrols_tic_result *out) {
    if (!model || !out || tics < 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < model->actor_count; ++i) {
        if (model->actors[i].mode != WL_ACTOR_PATROL) {
            continue;
        }
        ++out->actors_considered;
        wl_actor_patrol_tic_result step;
        if (wl_step_patrol_actor_tics(model, (uint16_t)i, speed, tics, &step) != 0) {
            return -1;
        }
        if (step.tiles_stepped) {
            ++out->actors_stepped;
            out->tiles_stepped = (uint16_t)(out->tiles_stepped + step.tiles_stepped);
        }
        if (step.blocked) {
            ++out->actors_blocked;
        }
    }
    return 0;
}

int wl_wake_actor_for_chase(wl_game_model *model, uint16_t actor_index,
                            uint16_t player_x, uint16_t player_y,
                            int search_forward, wl_actor_wake_result *out) {
    if (!model || !out || actor_index >= model->actor_count ||
        player_x >= WL_MAP_SIDE || player_y >= WL_MAP_SIDE) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    wl_actor_desc *actor = &model->actors[actor_index];
    out->mode_before = actor->mode;
    out->mode_after = actor->mode;
    out->dir_before = actor->dir;
    out->dir_after = actor->dir;
    out->was_ambush = actor->ambush ? 1u : 0u;

    if (actor->tile_x >= WL_MAP_SIDE || actor->tile_y >= WL_MAP_SIDE ||
        actor->mode == WL_ACTOR_INERT || actor->kind == WL_ACTOR_DEAD_GUARD ||
        !actor->shootable) {
        return -1;
    }

    if (actor->mode == WL_ACTOR_CHASE) {
        return 0;
    }

    actor->mode = WL_ACTOR_CHASE;
    actor->ambush = 0;
    actor->patrol_remainder = 0;
    out->woke = 1;
    out->mode_after = actor->mode;

    wl_actor_chase_dir_result selected;
    if (wl_select_chase_direction(model, actor->tile_x, actor->tile_y,
                                  player_x, player_y, actor->dir,
                                  search_forward, &selected) != 0) {
        return -1;
    }
    if (selected.selected) {
        actor->dir = selected.dir;
        out->chase_dir_selected = 1;
    }
    out->dir_after = actor->dir;
    return 0;
}
