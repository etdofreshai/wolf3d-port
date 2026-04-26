#include "wl_map_semantics.h"

#include <string.h>

typedef enum wl_static_kind {
    WL_STATIC_DRESSING,
    WL_STATIC_BLOCK,
    WL_STATIC_BONUS,
    WL_STATIC_TREASURE,
    WL_STATIC_UNKNOWN,
} wl_static_kind;

static wl_static_kind wl6_static_kind(uint16_t type) {
    static const wl_static_kind kinds[] = {
        WL_STATIC_DRESSING, /* 0 puddle */
        WL_STATIC_BLOCK,    /* 1 green barrel */
        WL_STATIC_BLOCK,    /* 2 table/chairs */
        WL_STATIC_BLOCK,    /* 3 floor lamp */
        WL_STATIC_DRESSING, /* 4 chandelier */
        WL_STATIC_BLOCK,    /* 5 hanged man */
        WL_STATIC_BONUS,    /* 6 bad food */
        WL_STATIC_BLOCK,    /* 7 red pillar */
        WL_STATIC_BLOCK,    /* 8 tree */
        WL_STATIC_DRESSING, /* 9 skeleton flat */
        WL_STATIC_BLOCK,    /* 10 sink */
        WL_STATIC_BLOCK,    /* 11 potted plant */
        WL_STATIC_BLOCK,    /* 12 urn */
        WL_STATIC_BLOCK,    /* 13 bare table */
        WL_STATIC_DRESSING, /* 14 ceiling light */
        WL_STATIC_DRESSING, /* 15 kitchen stuff */
        WL_STATIC_BLOCK,    /* 16 suit of armor */
        WL_STATIC_BLOCK,    /* 17 hanging cage */
        WL_STATIC_BLOCK,    /* 18 skeleton in cage */
        WL_STATIC_DRESSING, /* 19 skeleton relax */
        WL_STATIC_BONUS,    /* 20 key 1 */
        WL_STATIC_BONUS,    /* 21 key 2 */
        WL_STATIC_BLOCK,    /* 22 stuff */
        WL_STATIC_DRESSING, /* 23 stuff */
        WL_STATIC_BONUS,    /* 24 good food */
        WL_STATIC_BONUS,    /* 25 first aid */
        WL_STATIC_BONUS,    /* 26 clip */
        WL_STATIC_BONUS,    /* 27 machine gun */
        WL_STATIC_BONUS,    /* 28 chaingun */
        WL_STATIC_TREASURE, /* 29 cross */
        WL_STATIC_TREASURE, /* 30 chalice */
        WL_STATIC_TREASURE, /* 31 bible */
        WL_STATIC_TREASURE, /* 32 crown */
        WL_STATIC_TREASURE, /* 33 full heal / one up */
        WL_STATIC_BONUS,    /* 34 gibs */
        WL_STATIC_BLOCK,    /* 35 barrel */
        WL_STATIC_BLOCK,    /* 36 well */
        WL_STATIC_BLOCK,    /* 37 empty well */
        WL_STATIC_BONUS,    /* 38 gibs 2 */
        WL_STATIC_BLOCK,    /* 39 flag */
        WL_STATIC_BLOCK,    /* 40 call apogee */
        WL_STATIC_DRESSING, /* 41 junk */
        WL_STATIC_DRESSING, /* 42 junk */
        WL_STATIC_DRESSING, /* 43 junk */
        WL_STATIC_DRESSING, /* 44 pots */
        WL_STATIC_BLOCK,    /* 45 stove */
        WL_STATIC_BLOCK,    /* 46 spears */
        WL_STATIC_DRESSING, /* 47 vines */
        WL_STATIC_BONUS,    /* 48 clip2 */
        WL_STATIC_BLOCK,    /* 49 marble pillar (Spear) */
        WL_STATIC_BONUS,    /* 50 25-ammo clip (Spear) */
        WL_STATIC_BLOCK,    /* 51 truck (Spear) */
        WL_STATIC_BONUS,    /* 52 Spear of Destiny pickup (Spear) */
    };

    if (type >= sizeof(kinds) / sizeof(kinds[0])) {
        return WL_STATIC_UNKNOWN;
    }
    return kinds[type];
}

static int in_range(uint16_t value, uint16_t first, uint16_t last) {
    return value >= first && value <= last;
}

static int is_boss_info_tile(uint16_t tile) {
    switch (tile) {
    case 107: /* SpawnAngel (Spear) */
    case 125: /* SpawnTrans (Spear) */
    case 142: /* SpawnUber (Spear) */
    case 143: /* SpawnWill (Spear) */
    case 160: /* SpawnFakeHitler */
    case 161: /* SpawnDeath (Spear) */
    case 178: /* SpawnHitler */
    case 179: /* SpawnFat */
    case 196: /* SpawnSchabbs */
    case 197: /* SpawnGretel */
    case 214: /* SpawnBoss */
    case 215: /* SpawnGift */
        return 1;
    default:
        return 0;
    }
}

static int is_ghost_info_tile(uint16_t tile) {
    return in_range(tile, 224, 227) || tile == 106; /* Spectre is Spear's ghost-style actor. */
}

static void count_static(wl_map_semantics *out, uint16_t tile) {
    ++out->static_objects;
    switch (wl6_static_kind((uint16_t)(tile - 23))) {
    case WL_STATIC_BLOCK:
        ++out->static_blocking;
        break;
    case WL_STATIC_BONUS:
        ++out->static_bonus;
        break;
    case WL_STATIC_TREASURE:
        ++out->static_bonus;
        ++out->static_treasure;
        break;
    case WL_STATIC_DRESSING:
        break;
    case WL_STATIC_UNKNOWN:
        ++out->unknown_info_tiles;
        break;
    }
}

int wl_classify_map_semantics(const uint16_t *wall_plane, const uint16_t *info_plane,
                              size_t word_count, wl_map_semantics *out) {
    if (!wall_plane || !info_plane || !out || word_count != WL_MAP_PLANE_WORDS) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->first_player_dir = WL_DIR_NONE;

    for (size_t i = 0; i < word_count; ++i) {
        uint16_t wall = wall_plane[i];
        if (wall < WL_AREATILE) {
            if (wall != 0) {
                ++out->solid_tiles;
            }
        } else {
            ++out->area_tiles;
        }

        if (in_range(wall, 90, 101)) {
            ++out->door_tiles;
            if ((wall % 2) == 0) {
                ++out->vertical_doors;
                ++out->locked_doors[(wall - 90) / 2];
            } else {
                ++out->horizontal_doors;
                ++out->locked_doors[(wall - 91) / 2];
            }
        }
        if (wall == WL_AMBUSHTILE) {
            ++out->ambush_tiles;
        } else if (wall == WL_EXITTILE) {
            ++out->exit_tiles;
        } else if (wall == WL_ELEVATORTILE) {
            ++out->elevator_tiles;
        } else if (wall == WL_ALTELEVATORTILE) {
            ++out->alt_elevator_tiles;
        }
    }

    for (size_t i = 0; i < word_count; ++i) {
        uint16_t tile = info_plane[i];
        if (tile == 0) {
            continue;
        }

        if (in_range(tile, 19, 22)) {
            if (out->player_starts == 0) {
                out->first_player_x = i % WL_MAP_SIDE;
                out->first_player_y = i / WL_MAP_SIDE;
                out->first_player_dir = (wl_direction)(tile - 19);
            }
            ++out->player_starts;
        } else if (in_range(tile, 23, 74)) {
            count_static(out, tile);
        } else if (in_range(tile, WL_ICONARROWS, WL_ICONARROWS + 7)) {
            ++out->path_markers;
        } else if (tile == WL_PUSHABLETILE) {
            ++out->pushwall_markers;
        } else if (tile == WL_EXITTILE) {
            ++out->exit_tiles;
        } else if (in_range(tile, 108, 115)) {
            ++out->guard_easy_starts;
        } else if (in_range(tile, 144, 151)) {
            ++out->guard_medium_starts;
        } else if (in_range(tile, 180, 187)) {
            ++out->guard_hard_starts;
        } else if (tile == 124) {
            ++out->dead_guards;
        } else if (in_range(tile, 116, 123) || in_range(tile, 152, 159) || in_range(tile, 188, 195)) {
            ++out->officer_starts;
        } else if (in_range(tile, 126, 133) || in_range(tile, 162, 169) || in_range(tile, 198, 205)) {
            ++out->ss_starts;
        } else if (in_range(tile, 134, 141)) {
            ++out->dog_easy_starts;
        } else if (in_range(tile, 170, 177)) {
            ++out->dog_medium_starts;
        } else if (in_range(tile, 206, 213)) {
            ++out->dog_hard_starts;
        } else if (is_boss_info_tile(tile)) {
            ++out->boss_starts;
        } else if (in_range(tile, 216, 223) || in_range(tile, 234, 241) || in_range(tile, 252, 259)) {
            ++out->mutant_starts;
        } else if (is_ghost_info_tile(tile)) {
            ++out->ghost_starts;
        } else {
            ++out->unknown_info_tiles;
        }
    }

    return 0;
}
