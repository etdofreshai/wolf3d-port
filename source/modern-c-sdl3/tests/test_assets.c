#include "wl_assets.h"
#include "wl_map_semantics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int path(char *out, size_t out_size, const char *dir, const char *name) {
    int ok = wl_join_path(out, out_size, dir, name);
    CHECK(ok == 0);
    return 0;
}

static int expect_file_size(const char *dir, const char *name, size_t expected) {
    char p[1024];
    size_t actual = 0;
    CHECK(path(p, sizeof(p), dir, name) == 0);
    CHECK(wl_file_size(p, &actual) == 0);
    CHECK(actual == expected);
    return 0;
}

static uint32_t fnv1a_words(const uint16_t *words, size_t count) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) {
        hash ^= (uint8_t)(words[i] & 0xffu);
        hash *= 16777619u;
        hash ^= (uint8_t)(words[i] >> 8);
        hash *= 16777619u;
    }
    return hash;
}

static size_t count_value(const uint16_t *words, size_t count, uint16_t value) {
    size_t hits = 0;
    for (size_t i = 0; i < count; ++i) {
        if (words[i] == value) {
            ++hits;
        }
    }
    return hits;
}

static size_t count_nonzero(const uint16_t *words, size_t count) {
    size_t hits = 0;
    for (size_t i = 0; i < count; ++i) {
        if (words[i] != 0) {
            ++hits;
        }
    }
    return hits;
}

static int check_decode_helpers(void) {
    const unsigned char carmack_src[] = {
        0x11, 0x11, 0x22, 0x22, 0x02, 0xa7, 0x02,
        0x00, 0xa7, 0x44, 0x00, 0xa8, 0x55,
    };
    uint16_t carmack_out[6];
    size_t written = 0;
    CHECK(wl_carmack_expand(carmack_src, sizeof(carmack_src), sizeof(carmack_out),
                            carmack_out, 6, &written) == 0);
    CHECK(written == 6);
    CHECK(carmack_out[0] == 0x1111);
    CHECK(carmack_out[1] == 0x2222);
    CHECK(carmack_out[2] == 0x1111);
    CHECK(carmack_out[3] == 0x2222);
    CHECK(carmack_out[4] == 0xa744);
    CHECK(carmack_out[5] == 0xa855);

    const uint16_t rlew_src[] = { 7, 0xabcd, 3, 9, 11 };
    uint16_t rlew_out[5];
    CHECK(wl_rlew_expand(rlew_src, 5, 0xabcd, sizeof(rlew_out),
                         rlew_out, 5, &written) == 0);
    CHECK(written == 5);
    CHECK(rlew_out[0] == 7);
    CHECK(rlew_out[1] == 9);
    CHECK(rlew_out[2] == 9);
    CHECK(rlew_out[3] == 9);
    CHECK(rlew_out[4] == 11);
    return 0;
}

static int check_wl6(const char *dir) {
    CHECK(expect_file_size(dir, "MAPHEAD.WL6", 402) == 0);
    CHECK(expect_file_size(dir, "GAMEMAPS.WL6", 150652) == 0);
    CHECK(expect_file_size(dir, "VSWAP.WL6", 1544376) == 0);
    CHECK(expect_file_size(dir, "VGAHEAD.WL6", 450) == 0);
    CHECK(expect_file_size(dir, "VGADICT.WL6", 1024) == 0);
    CHECK(expect_file_size(dir, "VGAGRAPH.WL6", 275774) == 0);
    CHECK(expect_file_size(dir, "AUDIOHED.WL6", 1156) == 0);
    CHECK(expect_file_size(dir, "AUDIOT.WL6", 320209) == 0);

    char maphead_path[1024];
    char gamemaps_path[1024];
    char vswap_path[1024];
    CHECK(path(maphead_path, sizeof(maphead_path), dir, "MAPHEAD.WL6") == 0);
    CHECK(path(gamemaps_path, sizeof(gamemaps_path), dir, "GAMEMAPS.WL6") == 0);
    CHECK(path(vswap_path, sizeof(vswap_path), dir, "VSWAP.WL6") == 0);

    wl_maphead mh;
    CHECK(wl_read_maphead(maphead_path, &mh) == 0);
    CHECK(mh.file_size == 402);
    CHECK(mh.rlew_tag == 0xabcd);
    CHECK(mh.offsets[0] == 2250);

    wl_map_header map0;
    CHECK(wl_read_map_header(gamemaps_path, mh.offsets[0], &map0) == 0);
    CHECK(strcmp(map0.name, "Wolf1 Map1") == 0);
    CHECK(map0.width == 64);
    CHECK(map0.height == 64);
    CHECK(map0.plane_starts[0] == 11);
    CHECK(map0.plane_starts[1] == 1445);
    CHECK(map0.plane_starts[2] == 2240);
    CHECK(map0.plane_lengths[0] == 1434);
    CHECK(map0.plane_lengths[1] == 795);
    CHECK(map0.plane_lengths[2] == 10);

    uint16_t wall_plane[WL_MAP_PLANE_WORDS];
    uint16_t info_plane[WL_MAP_PLANE_WORDS];
    uint16_t extra_plane[WL_MAP_PLANE_WORDS];
    CHECK(wl_read_map_plane(gamemaps_path, &map0, 0, mh.rlew_tag,
                            wall_plane, WL_MAP_PLANE_WORDS) == 0);
    CHECK(fnv1a_words(wall_plane, WL_MAP_PLANE_WORDS) == 0x5940a18e);
    CHECK(count_nonzero(wall_plane, WL_MAP_PLANE_WORDS) == 4096);
    CHECK(count_value(wall_plane, WL_MAP_PLANE_WORDS, 1) == 2331);
    CHECK(count_value(wall_plane, WL_MAP_PLANE_WORDS, 8) == 230);
    CHECK(wall_plane[0] == 1);
    CHECK(wall_plane[WL_MAP_SIDE - 1] == 1);

    CHECK(wl_read_map_plane(gamemaps_path, &map0, 1, mh.rlew_tag,
                            info_plane, WL_MAP_PLANE_WORDS) == 0);
    CHECK(fnv1a_words(info_plane, WL_MAP_PLANE_WORDS) == 0xacf24351);
    CHECK(count_nonzero(info_plane, WL_MAP_PLANE_WORDS) == 183);
    CHECK(count_value(info_plane, WL_MAP_PLANE_WORDS, 0) == 3913);
    CHECK(count_value(info_plane, WL_MAP_PLANE_WORDS, 37) == 30);
    CHECK(info_plane[0] == 0);

    CHECK(wl_read_map_plane(gamemaps_path, &map0, 2, mh.rlew_tag,
                            extra_plane, WL_MAP_PLANE_WORDS) == 0);
    CHECK(fnv1a_words(extra_plane, WL_MAP_PLANE_WORDS) == 0xbcc31dc5);
    CHECK(count_nonzero(extra_plane, WL_MAP_PLANE_WORDS) == 0);

    wl_map_semantics sem;
    CHECK(wl_classify_map_semantics(wall_plane, info_plane, WL_MAP_PLANE_WORDS, &sem) == 0);
    CHECK(sem.solid_tiles == 3082);
    CHECK(sem.area_tiles == 1014);
    CHECK(sem.door_tiles == 22);
    CHECK(sem.vertical_doors == 14);
    CHECK(sem.horizontal_doors == 8);
    CHECK(sem.locked_doors[0] == 20);
    CHECK(sem.locked_doors[5] == 2);
    CHECK(sem.ambush_tiles == 3);
    CHECK(sem.elevator_tiles == 6);
    CHECK(sem.alt_elevator_tiles == 1);

    CHECK(sem.player_starts == 1);
    CHECK(sem.first_player_x == 29);
    CHECK(sem.first_player_y == 57);
    CHECK(sem.first_player_dir == WL_DIR_EAST);
    CHECK(sem.static_objects == 121);
    CHECK(sem.static_blocking == 34);
    CHECK(sem.static_bonus == 48);
    CHECK(sem.static_treasure == 23);
    CHECK(sem.pushwall_markers == 5);
    CHECK(sem.path_markers == 18);
    CHECK(sem.guard_easy_starts == 10);
    CHECK(sem.guard_medium_starts == 7);
    CHECK(sem.guard_hard_starts == 15);
    CHECK(sem.dog_easy_starts == 1);
    CHECK(sem.dog_medium_starts == 2);
    CHECK(sem.dog_hard_starts == 2);
    CHECK(sem.dead_guards == 1);
    CHECK(sem.officer_starts == 0);
    CHECK(sem.ss_starts == 0);
    CHECK(sem.mutant_starts == 0);
    CHECK(sem.boss_starts == 0);
    CHECK(sem.ghost_starts == 0);
    CHECK(sem.unknown_info_tiles == 0);

    wl_vswap_header vs;
    CHECK(wl_read_vswap_header(vswap_path, &vs) == 0);
    CHECK(vs.file_size == 1544376);
    CHECK(vs.chunks_in_file == 663);
    CHECK(vs.sprite_start == 106);
    CHECK(vs.sound_start == 542);
    CHECK(vs.first_offsets[0] == 4096);
    CHECK(vs.first_offsets[1] == 8192);
    CHECK(vs.first_offsets[2] == 12288);
    CHECK(vs.first_offsets[3] == 16384);
    CHECK(vs.first_offsets[4] == 20480);
    return 0;
}

static int check_optional_sod(const char *dir) {
    char sod_dir[1024];
    CHECK(wl_join_path(sod_dir, sizeof(sod_dir), dir, "m1") == 0);

    char maphead_path[1024];
    char gamemaps_path[1024];
    char vswap_path[1024];
    if (path(maphead_path, sizeof(maphead_path), sod_dir, "MAPHEAD.SOD") != 0 ||
        path(gamemaps_path, sizeof(gamemaps_path), sod_dir, "GAMEMAPS.SOD") != 0 ||
        path(vswap_path, sizeof(vswap_path), sod_dir, "VSWAP.SOD") != 0) {
        return 0;
    }

    size_t sod_size = 0;
    if (wl_file_size(maphead_path, &sod_size) != 0) {
        printf("SOD data absent; skipping optional SOD assertions\n");
        return 0;
    }

    wl_maphead mh;
    CHECK(wl_read_maphead(maphead_path, &mh) == 0);
    CHECK(mh.file_size == 402);
    CHECK(mh.rlew_tag == 0xabcd);
    CHECK(mh.offsets[0] == 2097);

    wl_map_header map0;
    CHECK(wl_read_map_header(gamemaps_path, mh.offsets[0], &map0) == 0);
    CHECK(strcmp(map0.name, "Tunnels 1") == 0);
    CHECK(map0.width == 64);
    CHECK(map0.height == 64);

    wl_vswap_header vs;
    CHECK(wl_read_vswap_header(vswap_path, &vs) == 0);
    CHECK(vs.chunks_in_file == 666);
    CHECK(vs.sprite_start == 134);
    CHECK(vs.sound_start == 555);
    return 0;
}

int main(void) {
    const char *dir = wl_default_data_dir();
    CHECK(check_decode_helpers() == 0);
    CHECK(check_wl6(dir) == 0);
    CHECK(check_optional_sod(dir) == 0);
    printf("asset/decompression/semantics tests passed for %s\n", dir);
    return 0;
}
