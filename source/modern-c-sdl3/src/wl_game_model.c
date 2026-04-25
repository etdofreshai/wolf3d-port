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
    case 178: /* SpawnHitler */
    case 179: /* SpawnFat */
    case 196: /* SpawnSchabbs */
    case 214: /* SpawnBoss */
        *dir = WL_DIR_SOUTH;
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

static uint16_t static_type_to_sprite_index(uint16_t type) {
    static const uint16_t table[] = {
        2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25,
        26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 36, 37, 38, 39, 40, 41,
        42, 43, 44, 45, 46, 47, 48, 49,
        28,
    };
    if (type >= sizeof(table) / sizeof(table[0])) {
        return UINT16_MAX;
    }
    return table[type];
}

static uint16_t actor_to_sprite_index(const wl_actor_desc *actor) {
    if (!actor) {
        return UINT16_MAX;
    }
    switch (actor->kind) {
    case WL_ACTOR_GUARD:
        return actor->mode == WL_ACTOR_PATROL ? 58 : 50; /* SPR_GRD_W1_1 / SPR_GRD_S_1 */
    case WL_ACTOR_OFFICER:
        return actor->mode == WL_ACTOR_PATROL ? 246 : 238; /* SPR_OFC_W1_1 / SPR_OFC_S_1 */
    case WL_ACTOR_SS:
        return actor->mode == WL_ACTOR_PATROL ? 146 : 138; /* SPR_SS_W1_1 / SPR_SS_S_1 */
    case WL_ACTOR_DOG:
        return 99; /* SPR_DOG_W1_1 */
    case WL_ACTOR_MUTANT:
        return actor->mode == WL_ACTOR_PATROL ? 195 : 187; /* SPR_MUT_W1_1 / SPR_MUT_S_1 */
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

int wl_collect_scene_sprite_refs(const wl_game_model *model, uint16_t vswap_sprite_start,
                                 wl_scene_sprite_ref *refs, size_t max_refs,
                                 size_t *out_count) {
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
        uint16_t sprite = actor_to_sprite_index(&model->actors[i]);
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
        refs[count].world_x = ((uint32_t)model->actors[i].tile_x << 16) + 0x8000u;
        refs[count].world_y = ((uint32_t)model->actors[i].tile_y << 16) + 0x8000u;
        ++count;
    }

    *out_count = count;
    return 0;
}
