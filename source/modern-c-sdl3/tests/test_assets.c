#include "wl_assets.h"
#include "wl_game_model.h"
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

static uint32_t fnv1a_bytes(const unsigned char *bytes, size_t count) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static size_t count_byte_not_value(const unsigned char *bytes, size_t count,
                                   unsigned char value) {
    size_t hits = 0;
    for (size_t i = 0; i < count; ++i) {
        if (bytes[i] != value) {
            ++hits;
        }
    }
    return hits;
}

static uint32_t fnv1a_scene_sprite_refs(const wl_scene_sprite_ref *refs, size_t count) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) {
        const uint32_t values[] = {
            (uint32_t)refs[i].kind,
            refs[i].model_index,
            refs[i].source_index,
            refs[i].vswap_chunk_index,
            refs[i].world_x,
            refs[i].world_y,
        };
        for (size_t j = 0; j < sizeof(values) / sizeof(values[0]); ++j) {
            for (size_t b = 0; b < 4; ++b) {
                hash ^= (uint8_t)(values[j] >> (8u * b));
                hash *= 16777619u;
            }
        }
    }
    return hash;
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
    char vgahead_path[1024];
    char vgadict_path[1024];
    char vgagraph_path[1024];
    CHECK(path(maphead_path, sizeof(maphead_path), dir, "MAPHEAD.WL6") == 0);
    CHECK(path(gamemaps_path, sizeof(gamemaps_path), dir, "GAMEMAPS.WL6") == 0);
    CHECK(path(vswap_path, sizeof(vswap_path), dir, "VSWAP.WL6") == 0);
    CHECK(path(vgahead_path, sizeof(vgahead_path), dir, "VGAHEAD.WL6") == 0);
    CHECK(path(vgadict_path, sizeof(vgadict_path), dir, "VGADICT.WL6") == 0);
    CHECK(path(vgagraph_path, sizeof(vgagraph_path), dir, "VGAGRAPH.WL6") == 0);

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

    wl_game_model model;
    CHECK(wl_build_game_model(wall_plane, info_plane, WL_MAP_PLANE_WORDS,
                              WL_DIFFICULTY_EASY, &model) == 0);
    CHECK(model.player.present == 1);
    CHECK(model.player.x == 29);
    CHECK(model.player.y == 57);
    CHECK(model.player.dir == WL_DIR_EAST);
    CHECK(model.door_count == 22);
    CHECK(model.door_area_connection_count == 22);
    CHECK(model.unique_door_area_connection_count == 17);
    CHECK(model.doors[0].x == 28);
    CHECK(model.doors[0].y == 11);
    CHECK(model.doors[0].vertical == 1);
    CHECK(model.doors[0].lock == 0);
    CHECK(model.doors[0].area1 == 36);
    CHECK(model.doors[0].area2 == 35);
    CHECK(model.doors[2].area1 == 36);
    CHECK(model.doors[2].area2 == 3);
    CHECK(model.doors[17].source_tile == 100);
    CHECK(model.doors[17].lock == 5);
    CHECK(model.doors[17].area1 == 33);
    CHECK(model.doors[17].area2 == 0);
    CHECK(model.door_area_connections[35][36] == 2);
    CHECK(model.door_area_connections[2][2] == 8);
    CHECK(model.door_area_connections[0][33] == 1);
    CHECK(model.static_count == 121);
    CHECK(model.blocking_static_count == 34);
    CHECK(model.bonus_static_count == 48);
    CHECK(model.treasure_total == 23);
    CHECK(model.statics[0].x == 29);
    CHECK(model.statics[0].y == 8);
    CHECK(model.statics[0].source_tile == 35);
    CHECK(model.statics[0].blocking == 1);
    CHECK(model.path_marker_count == 18);
    CHECK(model.path_markers[0].x == 2);
    CHECK(model.path_markers[0].y == 33);
    CHECK(model.path_markers[0].source_tile == 96);
    CHECK(model.pushwall_count == 5);
    CHECK(model.secret_total == 5);
    CHECK(model.pushwalls[0].x == 10);
    CHECK(model.pushwalls[0].y == 13);
    CHECK(model.actor_count == 12);
    CHECK(model.kill_total == 11);
    CHECK(model.actors[0].source_tile == 111);
    CHECK(model.actors[0].spawn_x == 33);
    CHECK(model.actors[0].spawn_y == 9);
    CHECK(model.actors[0].kind == WL_ACTOR_GUARD);
    CHECK(model.actors[0].mode == WL_ACTOR_STAND);
    CHECK(model.actors[9].kind == WL_ACTOR_DOG);
    CHECK(model.actors[9].mode == WL_ACTOR_PATROL);
    CHECK(model.actors[9].spawn_x == 54);
    CHECK(model.actors[9].spawn_y == 45);
    CHECK(model.actors[9].tile_x == 55);
    CHECK(model.actors[11].kind == WL_ACTOR_DEAD_GUARD);
    CHECK(model.actors[11].counts_for_kill_total == 0);
    CHECK(model.unknown_info_tiles == 0);
    wl_scene_sprite_ref scene_refs[WL_MAX_STATS + WL_MAX_ACTORS];
    size_t scene_ref_count = 0;
    CHECK(wl_collect_scene_sprite_refs(&model, 106, scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 133);
    CHECK(scene_refs[0].kind == WL_SCENE_SPRITE_STATIC);
    CHECK(scene_refs[0].model_index == 0);
    CHECK(scene_refs[0].source_index == 14);
    CHECK(scene_refs[0].vswap_chunk_index == 120);
    CHECK(scene_refs[0].world_x == 0x1d8000);
    CHECK(scene_refs[0].world_y == 0x088000);
    CHECK(scene_refs[2].source_index == 28);
    CHECK(scene_refs[2].vswap_chunk_index == 134);
    CHECK(scene_refs[128].kind == WL_SCENE_SPRITE_ACTOR);
    CHECK(scene_refs[128].model_index == 7);
    CHECK(scene_refs[128].source_index == 58);
    CHECK(scene_refs[128].vswap_chunk_index == 164);
    CHECK(scene_refs[130].source_index == 99);
    CHECK(scene_refs[130].vswap_chunk_index == 205);
    CHECK(scene_refs[132].source_index == 95);
    CHECK(scene_refs[132].vswap_chunk_index == 201);
    CHECK(fnv1a_scene_sprite_refs(scene_refs, scene_ref_count) == 0x2ab36473);
    wl_vswap_directory sprite_dirinfo;
    CHECK(wl_read_vswap_directory(vswap_path, &sprite_dirinfo) == 0);
    const uint16_t ref_chunks[] = {
        scene_refs[0].vswap_chunk_index,
        scene_refs[2].vswap_chunk_index,
        scene_refs[128].vswap_chunk_index,
        scene_refs[130].vswap_chunk_index,
        scene_refs[132].vswap_chunk_index,
    };
    unsigned char ref_sprite_pixels[5 * WL_MAP_PLANE_WORDS];
    wl_indexed_surface ref_surfaces[5];
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               ref_chunks, 5, 0, ref_sprite_pixels,
                                               sizeof(ref_sprite_pixels),
                                               ref_surfaces) == 0);
    CHECK(fnv1a_bytes(ref_surfaces[0].pixels, ref_surfaces[0].pixel_count) == 0x38769770);
    CHECK(fnv1a_bytes(ref_surfaces[1].pixels, ref_surfaces[1].pixel_count) == 0xbd6176ba);
    CHECK(fnv1a_bytes(ref_surfaces[2].pixels, ref_surfaces[2].pixel_count) == 0x0fe580fa);
    CHECK(fnv1a_bytes(ref_surfaces[3].pixels, ref_surfaces[3].pixel_count) == 0xa875d685);
    CHECK(fnv1a_bytes(ref_surfaces[4].pixels, ref_surfaces[4].pixel_count) == 0x63f7eba2);
    CHECK(fnv1a_bytes(ref_sprite_pixels, sizeof(ref_sprite_pixels)) == 0x4a8eb8db);
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               ref_chunks, 5, 0, ref_sprite_pixels,
                                               sizeof(ref_sprite_pixels) - 1,
                                               ref_surfaces) == -1);
    const uint16_t wall_chunk_ref[] = { 0 };
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               wall_chunk_ref, 1, 0,
                                               ref_sprite_pixels,
                                               sizeof(ref_sprite_pixels),
                                               ref_surfaces) == -1);
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               ref_chunks, 0, 0, NULL, 0,
                                               NULL) == 0);
    CHECK(wl_collect_scene_sprite_refs(&model, 106, scene_refs, 132,
                                       &scene_ref_count) == -1);

    CHECK(wl_build_game_model(wall_plane, info_plane, WL_MAP_PLANE_WORDS,
                              WL_DIFFICULTY_MEDIUM, &model) == 0);
    CHECK(model.actor_count == 21);
    CHECK(model.kill_total == 20);
    CHECK(model.unknown_info_tiles == 0);

    CHECK(wl_build_game_model(wall_plane, info_plane, WL_MAP_PLANE_WORDS,
                              WL_DIFFICULTY_HARD, &model) == 0);
    CHECK(model.actor_count == 38);
    CHECK(model.kill_total == 37);
    CHECK(model.unknown_info_tiles == 0);

    wl_graphics_header gh;
    wl_huffman_node huff[WL_HUFFMAN_NODE_COUNT];
    unsigned char graphics_buf[65536];
    unsigned char indexed_buf[65536];
    unsigned char rgba_buf[65536 * 4];
    unsigned char upload_palette[256 * 3];
    unsigned char fade_target_palette[256 * 3];
    unsigned char fade_palette[256 * 3];
    unsigned char fade_sample_pixels[16];
    wl_indexed_surface fade_sample_surface;
    unsigned char fade_sample_rgba[16 * 4];
    unsigned char canvas_pixels[160 * 120];
    wl_indexed_surface surface;
    wl_indexed_surface canvas;
    wl_texture_upload_descriptor upload;
    wl_texture_upload_descriptor rgba_upload;
    size_t graphics_bytes = 0;
    size_t compressed_bytes = 0;
    for (size_t i = 0; i < 256; ++i) {
        upload_palette[i * 3 + 0] = (unsigned char)(i & 63u);
        upload_palette[i * 3 + 1] = (unsigned char)((i * 2u) & 63u);
        upload_palette[i * 3 + 2] = (unsigned char)((63u - i) & 63u);
        fade_target_palette[i * 3 + 0] = 0;
        fade_target_palette[i * 3 + 1] = 17;
        fade_target_palette[i * 3 + 2] = 17;
    }
    for (size_t i = 0; i < sizeof(fade_sample_pixels); ++i) {
        fade_sample_pixels[i] = (unsigned char)(i * 17u);
    }
    CHECK(wl_interpolate_palette_range(upload_palette, fade_target_palette,
                                       sizeof(upload_palette), 6, 0, 255, 5,
                                       10, fade_palette,
                                       sizeof(fade_palette)) == 0);
    CHECK(fnv1a_bytes(fade_palette, sizeof(fade_palette)) == 0xa93a5ba5);
    CHECK(fade_palette[0] == 0);
    CHECK(fade_palette[1] == 8);
    CHECK(fade_palette[2] == 40);
    CHECK(fade_palette[255u * 3u + 0u] == 32);
    CHECK(fade_palette[255u * 3u + 1u] == 40);
    CHECK(fade_palette[255u * 3u + 2u] == 8);
    CHECK(wl_wrap_indexed_surface(4, 4, fade_sample_pixels,
                                  sizeof(fade_sample_pixels),
                                  &fade_sample_surface) == 0);
    CHECK(wl_expand_indexed_surface_to_rgba(&fade_sample_surface, fade_palette,
                                            sizeof(fade_palette), 6,
                                            fade_sample_rgba,
                                            sizeof(fade_sample_rgba),
                                            NULL) == 0);
    CHECK(fnv1a_bytes(fade_sample_rgba, sizeof(fade_sample_rgba)) == 0x50918d48);
    CHECK(wl_interpolate_palette_range(upload_palette, fade_target_palette,
                                       sizeof(upload_palette), 6, 32, 63, 10,
                                       10, fade_palette,
                                       sizeof(fade_palette)) == 0);
    CHECK(fnv1a_bytes(fade_palette, sizeof(fade_palette)) == 0x91f102c5);
    CHECK(fade_palette[31u * 3u + 0u] == 31);
    CHECK(fade_palette[32u * 3u + 0u] == 0);
    CHECK(fade_palette[32u * 3u + 1u] == 17);
    CHECK(fade_palette[32u * 3u + 2u] == 17);
    CHECK(fade_palette[63u * 3u + 0u] == 0);
    CHECK(fade_palette[63u * 3u + 1u] == 17);
    CHECK(fade_palette[63u * 3u + 2u] == 17);
    CHECK(fade_palette[64u * 3u + 2u] == 63);
    CHECK(wl_interpolate_palette_range(upload_palette, fade_target_palette,
                                       sizeof(upload_palette), 6, 64, 32, 0,
                                       10, fade_palette,
                                       sizeof(fade_palette)) == -1);
    CHECK(wl_interpolate_palette_range(upload_palette, fade_target_palette,
                                       sizeof(upload_palette), 5, 0, 255, 0,
                                       10, fade_palette,
                                       sizeof(fade_palette)) == -1);
    CHECK(wl_interpolate_palette_range(upload_palette, fade_target_palette,
                                       sizeof(upload_palette), 6, 0, 255, 11,
                                       10, fade_palette,
                                       sizeof(fade_palette)) == -1);
    CHECK(wl_build_palette_shift(upload_palette, sizeof(upload_palette), 6,
                                 64, 0, 0, 6, 8, fade_palette,
                                 sizeof(fade_palette)) == 0);
    CHECK(fnv1a_bytes(fade_palette, sizeof(fade_palette)) == 0xb8462fc5);
    CHECK(fade_palette[0] == 48);
    CHECK(fade_palette[1] == 0);
    CHECK(fade_palette[2] == 16);
    CHECK(fade_palette[255u * 3u + 0u] == 63);
    CHECK(fade_palette[255u * 3u + 1u] == 16);
    CHECK(fade_palette[255u * 3u + 2u] == 0);
    CHECK(wl_expand_indexed_surface_to_rgba(&fade_sample_surface, fade_palette,
                                            sizeof(fade_palette), 6,
                                            fade_sample_rgba,
                                            sizeof(fade_sample_rgba),
                                            NULL) == 0);
    CHECK(fnv1a_bytes(fade_sample_rgba, sizeof(fade_sample_rgba)) == 0xfa0a0cd7);
    CHECK(wl_build_palette_shift(upload_palette, sizeof(upload_palette), 6,
                                 64, 62, 0, 3, 20, fade_palette,
                                 sizeof(fade_palette)) == 0);
    CHECK(fnv1a_bytes(fade_palette, sizeof(fade_palette)) == 0x3c8da1ed);
    CHECK(fade_palette[0] == 9);
    CHECK(fade_palette[1] == 9);
    CHECK(fade_palette[2] == 54);
    CHECK(fade_palette[255u * 3u + 0u] == 63);
    CHECK(fade_palette[255u * 3u + 1u] == 62);
    CHECK(fade_palette[255u * 3u + 2u] == 0);
    CHECK(wl_expand_indexed_surface_to_rgba(&fade_sample_surface, fade_palette,
                                            sizeof(fade_palette), 6,
                                            fade_sample_rgba,
                                            sizeof(fade_sample_rgba),
                                            NULL) == 0);
    CHECK(fnv1a_bytes(fade_sample_rgba, sizeof(fade_sample_rgba)) == 0x93adda7f);
    CHECK(wl_build_palette_shift(upload_palette, sizeof(upload_palette), 6,
                                 65, 0, 0, 1, 8, fade_palette,
                                 sizeof(fade_palette)) == -1);
    CHECK(wl_build_palette_shift(upload_palette, sizeof(upload_palette), 5,
                                 64, 0, 0, 1, 8, fade_palette,
                                 sizeof(fade_palette)) == -1);
    CHECK(wl_build_palette_shift(upload_palette, sizeof(upload_palette), 6,
                                 64, 0, 0, 9, 8, fade_palette,
                                 sizeof(fade_palette)) == -1);
    CHECK(wl_read_graphics_header(vgahead_path, &gh) == 0);
    CHECK(gh.chunk_count == 149);
    CHECK(gh.file_size == 450);
    CHECK(gh.offsets[0] == 0);
    CHECK(gh.offsets[1] == 354);
    CHECK(gh.offsets[3] == 9570);
    CHECK(gh.offsets[149] == 275774);
    CHECK(wl_read_huffman_dictionary(vgadict_path, huff) == 0);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 0, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 354);
    CHECK(graphics_bytes == 528);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0x0a6f459a);
    wl_picture_table_metadata pictures;
    CHECK(wl_decode_picture_table(graphics_buf, graphics_bytes, &pictures) == 0);
    CHECK(pictures.picture_count == 132);
    CHECK(pictures.min_width == 8);
    CHECK(pictures.max_width == 320);
    CHECK(pictures.min_height == 8);
    CHECK(pictures.max_height == 200);
    CHECK(pictures.total_pixels == 342464);
    CHECK(pictures.pictures[0].width == 96);
    CHECK(pictures.pictures[0].height == 88);
    CHECK(pictures.pictures[3].width == 320);
    CHECK(pictures.pictures[3].height == 8);
    CHECK(pictures.pictures[84].width == 320);
    CHECK(pictures.pictures[84].height == 200);
    CHECK(pictures.pictures[86].width == 320);
    CHECK(pictures.pictures[86].height == 200);
    CHECK(pictures.pictures[87].width == 224);
    CHECK(pictures.pictures[87].height == 56);
    CHECK(pictures.pictures[131].width == 224);
    CHECK(pictures.pictures[131].height == 48);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 1, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 3467);
    CHECK(graphics_bytes == 8300);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0xdb48ce2b);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 3, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 8057);
    CHECK(graphics_bytes == 8448);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0x5c152b5c);
    CHECK(wl_decode_planar_picture_surface(graphics_buf, graphics_bytes,
                                           pictures.pictures[0].width,
                                           pictures.pictures[0].height,
                                           indexed_buf, sizeof(indexed_buf),
                                           &surface) == 0);
    CHECK(surface.format == WL_SURFACE_INDEXED8);
    CHECK(surface.width == 96);
    CHECK(surface.height == 88);
    CHECK(surface.stride == 96);
    CHECK(surface.pixel_count == graphics_bytes);
    CHECK(surface.pixels == indexed_buf);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0xa9c1ea92);
    CHECK(wl_describe_indexed_texture_upload(&surface, upload_palette,
                                             sizeof(upload_palette), 6,
                                             &upload) == 0);
    CHECK(upload.format == WL_TEXTURE_UPLOAD_INDEXED8_RGB_PALETTE);
    CHECK(upload.width == 96);
    CHECK(upload.height == 88);
    CHECK(upload.pitch == 96);
    CHECK(upload.pixel_bytes == 8448);
    CHECK(upload.pixels == surface.pixels);
    CHECK(upload.palette == upload_palette);
    CHECK(upload.palette_entries == 256);
    CHECK(upload.palette_component_bits == 6);
    CHECK(wl_expand_indexed_surface_to_rgba(&surface, upload_palette,
                                            sizeof(upload_palette), 6, rgba_buf,
                                            sizeof(rgba_buf), &rgba_upload) == 0);
    CHECK(rgba_upload.format == WL_TEXTURE_UPLOAD_RGBA8888);
    CHECK(rgba_upload.width == 96);
    CHECK(rgba_upload.height == 88);
    CHECK(rgba_upload.pitch == 384);
    CHECK(rgba_upload.pixel_bytes == 33792);
    CHECK(rgba_upload.pixels == rgba_buf);
    CHECK(rgba_upload.palette == NULL);
    CHECK(fnv1a_bytes(rgba_buf, rgba_upload.pixel_bytes) == 0xb75bdee9);
    CHECK(wl_describe_indexed_texture_upload(&surface, upload_palette,
                                             sizeof(upload_palette), 5,
                                             &upload) == -1);
    CHECK(wl_expand_indexed_surface_to_rgba(&surface, upload_palette,
                                            sizeof(upload_palette), 6, rgba_buf,
                                            rgba_upload.pixel_bytes - 1,
                                            &rgba_upload) == -1);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(160, 120, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_blit_indexed_surface(&surface, &canvas, 13, 17) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xf7c5e35c);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 87, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 45948);
    CHECK(graphics_bytes == 64000);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0x01643ebc);
    CHECK(wl_decode_planar_picture_surface(graphics_buf, graphics_bytes,
                                           pictures.pictures[84].width,
                                           pictures.pictures[84].height,
                                           indexed_buf, sizeof(indexed_buf),
                                           &surface) == 0);
    CHECK(surface.width == 320);
    CHECK(surface.height == 200);
    CHECK(surface.stride == 320);
    CHECK(surface.pixel_count == graphics_bytes);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x4b172b02);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 134, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 5127);
    CHECK(graphics_bytes == 10752);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0xeb393cc0);
    CHECK(wl_decode_planar_picture_surface(graphics_buf, graphics_bytes,
                                           pictures.pictures[131].width,
                                           pictures.pictures[131].height,
                                           indexed_buf, sizeof(indexed_buf),
                                           &surface) == 0);
    CHECK(surface.width == 224);
    CHECK(surface.height == 48);
    CHECK(surface.pixel_count == graphics_bytes);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x46e4bd08);
    CHECK(wl_blit_indexed_surface(&surface, &canvas, -12, 70) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x338ae9cd);

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

    wl_vswap_directory dirinfo;
    CHECK(wl_read_vswap_directory(vswap_path, &dirinfo) == 0);
    CHECK(dirinfo.header.chunks_in_file == 663);
    CHECK(dirinfo.data_start == 3984);
    CHECK(dirinfo.max_chunk_end == 1544376);
    CHECK(dirinfo.wall_count == 106);
    CHECK(dirinfo.sprite_count == 436);
    CHECK(dirinfo.sound_count == 121);
    CHECK(dirinfo.sparse_count == 0);
    CHECK(dirinfo.chunks[0].offset == 4096);
    CHECK(dirinfo.chunks[0].length == 4096);
    CHECK(dirinfo.chunks[0].kind == WL_VSWAP_CHUNK_WALL);
    CHECK(dirinfo.chunks[105].offset == 434176);
    CHECK(dirinfo.chunks[105].length == 4096);
    CHECK(dirinfo.chunks[106].offset == 438272);
    CHECK(dirinfo.chunks[106].length == 1306);
    CHECK(dirinfo.chunks[106].kind == WL_VSWAP_CHUNK_SPRITE);
    CHECK(dirinfo.chunks[541].offset == 1139200);
    CHECK(dirinfo.chunks[541].length == 650);
    CHECK(dirinfo.chunks[542].offset == 1140224);
    CHECK(dirinfo.chunks[542].length == 4096);
    CHECK(dirinfo.chunks[542].kind == WL_VSWAP_CHUNK_SOUND);
    CHECK(dirinfo.chunks[662].offset == 1544192);
    CHECK(dirinfo.chunks[662].length == 184);

    unsigned char chunk_buf[4096];
    unsigned char wall0_buf[4096];
    unsigned char wall9_buf[4096];
    unsigned char wall14_buf[4096];
    unsigned char wall15_buf[4096];
    unsigned char wall16_buf[4096];
    unsigned char wall17_buf[4096];
    unsigned char wall63_buf[4096];
    unsigned char column_buf[64];
    unsigned char surface_column_buf[64];
    size_t chunk_bytes = 0;
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 0, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x98d020a5);
    memcpy(wall0_buf, chunk_buf, sizeof(wall0_buf));
    wl_vswap_shape_metadata shape;
    CHECK(wl_decode_vswap_shape_metadata(chunk_buf, chunk_bytes, dirinfo.chunks[0].kind,
                                         &shape) == 0);
    CHECK(shape.kind == WL_VSWAP_CHUNK_WALL);
    CHECK(shape.width == 64);
    CHECK(shape.height == 64);
    CHECK(shape.visible_columns == 64);
    wl_wall_page_metadata wallmeta;
    CHECK(wl_decode_wall_page_metadata(chunk_buf, chunk_bytes, &wallmeta) == 0);
    CHECK(wallmeta.width == 64);
    CHECK(wallmeta.height == 64);
    CHECK(wallmeta.column_count == 64);
    CHECK(wallmeta.bytes_per_column == 64);
    CHECK(wallmeta.min_color == 7);
    CHECK(wallmeta.max_color == 31);
    CHECK(wallmeta.unique_color_count == 18);
    CHECK(wl_decode_wall_page_surface(chunk_buf, chunk_bytes, indexed_buf,
                                      sizeof(indexed_buf), &surface) == 0);
    CHECK(surface.format == WL_SURFACE_INDEXED8);
    CHECK(surface.width == 64);
    CHECK(surface.height == 64);
    CHECK(surface.stride == 64);
    CHECK(surface.pixel_count == 4096);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x8fe4d8ff);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0000, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0xc77d483d);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0040, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0x272b5483);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0800, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0x2fbb79bb);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0fc0, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0x19c55a4e);
    CHECK(wl_sample_indexed_surface_column(&surface, 63, surface_column_buf,
                                           sizeof(surface_column_buf)) == 0);
    CHECK(memcmp(column_buf, surface_column_buf, sizeof(column_buf)) == 0);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0000, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &canvas,
                                          7, 64) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xceb8a051);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0fc0, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &canvas,
                                          29, 160) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xf25f51d9);
    CHECK(wl_scale_wall_column_to_surface(column_buf, 63, &canvas, 7, 64) == -1);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &canvas,
                                          80, 64) == -1);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0001, column_buf,
                                     sizeof(column_buf)) == -1);
    CHECK(wl_sample_indexed_surface_column(&surface, 64, surface_column_buf,
                                           sizeof(surface_column_buf)) == -1);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 63, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xa9a1ca8c);
    memcpy(wall63_buf, chunk_buf, sizeof(wall63_buf));
    CHECK(wl_decode_wall_page_metadata(chunk_buf, chunk_bytes, &wallmeta) == 0);
    CHECK(wallmeta.min_color == 26);
    CHECK(wallmeta.max_color == 223);
    CHECK(wallmeta.unique_color_count == 31);
    CHECK(wl_decode_wall_page_surface(chunk_buf, chunk_bytes, indexed_buf,
                                      sizeof(indexed_buf), &surface) == 0);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x5b4d4c38);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0800, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0x8a859220);
    CHECK(wl_sample_indexed_surface_column(&surface, 32, surface_column_buf,
                                           sizeof(surface_column_buf)) == 0);
    CHECK(memcmp(column_buf, surface_column_buf, sizeof(column_buf)) == 0);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &canvas,
                                          13, 96) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xb200118);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    const wl_wall_strip wl6_strips[] = {
        { wall0_buf, sizeof(wall0_buf), 0x0000, 7, 64 },
        { wall0_buf, sizeof(wall0_buf), 0x0fc0, 29, 160 },
        { wall63_buf, sizeof(wall63_buf), 0x0800, 13, 96 },
    };
    CHECK(wl_render_wall_strip_viewport(wl6_strips, 3, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xb200118);
    CHECK(wl_render_wall_strip_viewport(wl6_strips, 0, &canvas) == -1);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 9, wall9_buf, sizeof(wall9_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 14, wall14_buf, sizeof(wall14_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 15, wall15_buf, sizeof(wall15_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 16, wall16_buf, sizeof(wall16_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 17, wall17_buf, sizeof(wall17_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    wl_map_wall_hit hits[4];
    CHECK(wl_build_map_wall_hit(wall_plane, WL_MAP_PLANE_WORDS, 0, 0,
                                WL_WALL_SIDE_HORIZONTAL, 0, 7, 64,
                                &hits[0]) == 0);
    CHECK(hits[0].source_tile == 1);
    CHECK(hits[0].wall_page_index == 0);
    CHECK(hits[0].texture_offset == 0x0000);
    CHECK(wl_build_map_wall_hit(wall_plane, WL_MAP_PLANE_WORDS, 62, 33,
                                WL_WALL_SIDE_VERTICAL, 32, 13, 96,
                                &hits[1]) == 0);
    CHECK(hits[1].source_tile == 5);
    CHECK(hits[1].wall_page_index == 9);
    CHECK(hits[1].texture_offset == 0x0800);
    CHECK(wl_build_map_wall_hit(wall_plane, WL_MAP_PLANE_WORDS, 53, 28,
                                WL_WALL_SIDE_HORIZONTAL, 63, 29, 160,
                                &hits[2]) == 0);
    CHECK(hits[2].source_tile == 8);
    CHECK(hits[2].wall_page_index == 14);
    CHECK(hits[2].texture_offset == 0x0fc0);
    const unsigned char *hit_pages[] = { wall0_buf, wall9_buf, wall14_buf };
    wl_wall_strip hit_strips[3];
    for (size_t i = 0; i < 3; ++i) {
        CHECK(wl_wall_hit_to_strip(&hits[i], hit_pages[i], 4096, &hit_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(hit_strips, 3, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x7ffb21c0);
    CHECK(wl_build_map_wall_hit(wall_plane, WL_MAP_PLANE_WORDS, 29, 57,
                                WL_WALL_SIDE_HORIZONTAL, 0, 0, 64,
                                &hits[0]) == -1);
    CHECK(wl_cast_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS, 29, 57,
                                    WL_RAY_EAST, 32, 5, 96, &hits[0]) == 0);
    CHECK(hits[0].tile_x == 41);
    CHECK(hits[0].tile_y == 57);
    CHECK(hits[0].source_tile == 9);
    CHECK(hits[0].wall_page_index == 17);
    CHECK(hits[0].texture_offset == 0x0800);
    CHECK(wl_cast_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS, 29, 57,
                                    WL_RAY_WEST, 32, 15, 96, &hits[1]) == 0);
    CHECK(hits[1].tile_x == 27);
    CHECK(hits[1].tile_y == 57);
    CHECK(hits[1].wall_page_index == 17);
    CHECK(wl_cast_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS, 29, 57,
                                    WL_RAY_NORTH, 32, 25, 96, &hits[2]) == 0);
    CHECK(hits[2].tile_x == 29);
    CHECK(hits[2].tile_y == 55);
    CHECK(hits[2].wall_page_index == 16);
    CHECK(wl_cast_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS, 29, 57,
                                    WL_RAY_SOUTH, 32, 35, 96, &hits[3]) == 0);
    CHECK(hits[3].tile_x == 29);
    CHECK(hits[3].tile_y == 59);
    CHECK(hits[3].wall_page_index == 16);
    const unsigned char *ray_pages[] = { wall17_buf, wall17_buf, wall16_buf, wall16_buf };
    wl_wall_strip ray_strips[4];
    for (size_t i = 0; i < 4; ++i) {
        CHECK(wl_wall_hit_to_strip(&hits[i], ray_pages[i], 4096, &ray_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(ray_strips, 4, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xa4c9e6e1);
    const uint32_t player_x = (29u << 16) + 0x8000u;
    const uint32_t player_y = (57u << 16) + 0x8000u;
    CHECK(wl_cast_fixed_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                          player_x, player_y, WL_RAY_EAST,
                                          5, 96, &hits[0]) == 0);
    CHECK(hits[0].tile_x == 41);
    CHECK(hits[0].tile_y == 57);
    CHECK(hits[0].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                          player_x, player_y, WL_RAY_WEST,
                                          15, 96, &hits[1]) == 0);
    CHECK(hits[1].tile_x == 27);
    CHECK(hits[1].tile_y == 57);
    CHECK(hits[1].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                          player_x, player_y, WL_RAY_NORTH,
                                          25, 96, &hits[2]) == 0);
    CHECK(hits[2].tile_x == 29);
    CHECK(hits[2].tile_y == 55);
    CHECK(hits[2].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                          player_x, player_y, WL_RAY_SOUTH,
                                          35, 96, &hits[3]) == 0);
    CHECK(hits[3].tile_x == 29);
    CHECK(hits[3].tile_y == 59);
    CHECK(hits[3].texture_offset == 0x0800);
    for (size_t i = 0; i < 4; ++i) {
        CHECK(wl_wall_hit_to_strip(&hits[i], ray_pages[i], 4096, &ray_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(ray_strips, 4, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xa4c9e6e1);
    CHECK(wl_cast_fixed_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                          (64u << 16), player_y, WL_RAY_EAST,
                                          0, 64, &hits[0]) == -1);

    wl_map_wall_hit dda_hits[5];
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                 player_x, player_y, 0x10000, 0,
                                 5, 96, &dda_hits[0]) == 0);
    CHECK(dda_hits[0].tile_x == 41);
    CHECK(dda_hits[0].tile_y == 57);
    CHECK(dda_hits[0].wall_page_index == 17);
    CHECK(dda_hits[0].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                 player_x, player_y, -0x10000, 0,
                                 15, 96, &dda_hits[1]) == 0);
    CHECK(dda_hits[1].tile_x == 27);
    CHECK(dda_hits[1].tile_y == 57);
    CHECK(dda_hits[1].wall_page_index == 17);
    CHECK(dda_hits[1].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                 player_x, player_y, 0, -0x10000,
                                 25, 96, &dda_hits[2]) == 0);
    CHECK(dda_hits[2].tile_x == 29);
    CHECK(dda_hits[2].tile_y == 55);
    CHECK(dda_hits[2].wall_page_index == 16);
    CHECK(dda_hits[2].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                 player_x, player_y, 0, 0x10000,
                                 35, 96, &dda_hits[3]) == 0);
    CHECK(dda_hits[3].tile_x == 29);
    CHECK(dda_hits[3].tile_y == 59);
    CHECK(dda_hits[3].wall_page_index == 16);
    CHECK(dda_hits[3].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                 player_x, player_y, 0x10000, -0x8000,
                                 45, 96, &dda_hits[4]) == 0);
    CHECK(dda_hits[4].tile_x == 32);
    CHECK(dda_hits[4].tile_y == 56);
    CHECK(dda_hits[4].source_tile == 8);
    CHECK(dda_hits[4].wall_page_index == 15);
    CHECK(dda_hits[4].texture_offset == 0x0400);
    const unsigned char *dda_pages[] = { wall17_buf, wall17_buf, wall16_buf,
                                         wall16_buf, wall15_buf };
    wl_wall_strip dda_strips[5];
    for (size_t i = 0; i < 5; ++i) {
        CHECK(wl_wall_hit_to_strip(&dda_hits[i], dda_pages[i], 4096,
                                   &dda_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(dda_strips, 5, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xae40b70c);
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                 player_y, 0, 0, 0, 64, &dda_hits[0]) == -1);
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                 (64u << 16), player_y, 0x10000, 0,
                                 0, 64, &dda_hits[0]) == -1);

    CHECK(wl_project_wall_height(0x5800, 80, 128) == 128);
    CHECK(wl_project_wall_height(0x28000, 80, 128) == 86);
    CHECK(wl_project_wall_height(0x0b8000, 80, 128) == 18);
    wl_map_wall_hit projected_hits[5];
    CHECK(wl_cast_projected_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                     player_x, player_y, 0x10000, 0,
                                     5, 80, 128, &projected_hits[0]) == 0);
    CHECK(projected_hits[0].tile_x == 41);
    CHECK(projected_hits[0].tile_y == 57);
    CHECK(projected_hits[0].distance == 0x0b8000);
    CHECK(projected_hits[0].scaled_height == 18);
    CHECK(wl_cast_projected_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                     player_x, player_y, -0x10000, 0,
                                     15, 80, 128, &projected_hits[1]) == 0);
    CHECK(projected_hits[1].tile_x == 27);
    CHECK(projected_hits[1].tile_y == 57);
    CHECK(projected_hits[1].distance == 0x018000);
    CHECK(projected_hits[1].scaled_height == 128);
    CHECK(wl_cast_projected_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                     player_x, player_y, 0, -0x10000,
                                     25, 80, 128, &projected_hits[2]) == 0);
    CHECK(projected_hits[2].tile_x == 29);
    CHECK(projected_hits[2].tile_y == 55);
    CHECK(projected_hits[2].distance == 0x018000);
    CHECK(projected_hits[2].scaled_height == 128);
    CHECK(wl_cast_projected_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                     player_x, player_y, 0, 0x10000,
                                     35, 80, 128, &projected_hits[3]) == 0);
    CHECK(projected_hits[3].tile_x == 29);
    CHECK(projected_hits[3].tile_y == 59);
    CHECK(projected_hits[3].distance == 0x018000);
    CHECK(projected_hits[3].scaled_height == 128);
    CHECK(wl_cast_projected_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                     player_x, player_y, 0x10000, -0x8000,
                                     45, 80, 128, &projected_hits[4]) == 0);
    CHECK(projected_hits[4].tile_x == 32);
    CHECK(projected_hits[4].tile_y == 56);
    CHECK(projected_hits[4].distance == 0x028000);
    CHECK(projected_hits[4].scaled_height == 86);
    const unsigned char *projected_pages[] = { wall17_buf, wall17_buf, wall16_buf,
                                               wall16_buf, wall15_buf };
    wl_wall_strip projected_strips[5];
    for (size_t i = 0; i < 5; ++i) {
        CHECK(wl_wall_hit_to_strip(&projected_hits[i], projected_pages[i], 4096,
                                   &projected_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(projected_strips, 5, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xd48f2f6d);
    CHECK(wl_cast_projected_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                     player_x, player_y, 0x10000, 0,
                                     80, 80, 128, &projected_hits[0]) == -1);

    const int32_t batch_dirs_x[] = { 0x10000, -0x10000, 0, 0, 0x10000 };
    const int32_t batch_dirs_y[] = { 0, 0, -0x10000, 0x10000, -0x8000 };
    wl_map_wall_hit batch_hits[5];
    CHECK(wl_cast_projected_wall_ray_batch(wall_plane, WL_MAP_PLANE_WORDS,
                                           player_x, player_y, batch_dirs_x,
                                           batch_dirs_y, 5, 20, 1, 80, 128,
                                           batch_hits) == 0);
    CHECK(batch_hits[0].x == 20);
    CHECK(batch_hits[0].tile_x == 41);
    CHECK(batch_hits[0].scaled_height == 18);
    CHECK(batch_hits[1].x == 21);
    CHECK(batch_hits[1].tile_x == 27);
    CHECK(batch_hits[1].scaled_height == 128);
    CHECK(batch_hits[2].x == 22);
    CHECK(batch_hits[2].tile_y == 55);
    CHECK(batch_hits[2].scaled_height == 128);
    CHECK(batch_hits[3].x == 23);
    CHECK(batch_hits[3].tile_y == 59);
    CHECK(batch_hits[3].scaled_height == 128);
    CHECK(batch_hits[4].x == 24);
    CHECK(batch_hits[4].tile_x == 32);
    CHECK(batch_hits[4].tile_y == 56);
    CHECK(batch_hits[4].scaled_height == 86);
    wl_wall_strip batch_strips[5];
    for (size_t i = 0; i < 5; ++i) {
        CHECK(wl_wall_hit_to_strip(&batch_hits[i], projected_pages[i], 4096,
                                   &batch_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(batch_strips, 5, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x7209a9ed);
    CHECK(wl_cast_projected_wall_ray_batch(wall_plane, WL_MAP_PLANE_WORDS,
                                           player_x, player_y, batch_dirs_x,
                                           batch_dirs_y, 5, 78, 1, 80, 128,
                                           batch_hits) == -1);
    CHECK(wl_cast_projected_wall_ray_batch(wall_plane, WL_MAP_PLANE_WORDS,
                                           player_x, player_y, batch_dirs_x,
                                           batch_dirs_y, 0, 20, 1, 80, 128,
                                           batch_hits) == -1);

    int32_t camera_dirs_x[5];
    int32_t camera_dirs_y[5];
    CHECK(wl_build_camera_ray_directions(0x10000, 0, 0, -0x8000, 5, 0, 1, 5,
                                         camera_dirs_x, camera_dirs_y) == 0);
    CHECK(camera_dirs_x[0] == 0x10000);
    CHECK(camera_dirs_y[0] == 26214);
    CHECK(camera_dirs_x[2] == 0x10000);
    CHECK(camera_dirs_y[2] == 0);
    CHECK(camera_dirs_x[4] == 0x10000);
    CHECK(camera_dirs_y[4] == -26214);
    wl_map_wall_hit camera_hits[5];
    CHECK(wl_cast_projected_wall_ray_batch(wall_plane, WL_MAP_PLANE_WORDS,
                                           player_x, player_y, camera_dirs_x,
                                           camera_dirs_y, 5, 30, 1, 80, 128,
                                           camera_hits) == 0);
    CHECK(camera_hits[0].tile_x == 32);
    CHECK(camera_hits[0].tile_y == 58);
    CHECK(camera_hits[0].wall_page_index == 15);
    CHECK(camera_hits[0].texture_offset == 0x07c0);
    CHECK(camera_hits[0].scaled_height == 86);
    CHECK(camera_hits[1].tile_x == 32);
    CHECK(camera_hits[1].tile_y == 58);
    CHECK(camera_hits[1].wall_page_index == 14);
    CHECK(camera_hits[1].texture_offset == 0x0000);
    CHECK(camera_hits[2].tile_x == 41);
    CHECK(camera_hits[2].tile_y == 57);
    CHECK(camera_hits[2].wall_page_index == 17);
    CHECK(camera_hits[2].scaled_height == 18);
    CHECK(camera_hits[3].tile_x == 32);
    CHECK(camera_hits[3].tile_y == 56);
    CHECK(camera_hits[3].wall_page_index == 14);
    CHECK(camera_hits[4].tile_x == 32);
    CHECK(camera_hits[4].tile_y == 56);
    CHECK(camera_hits[4].wall_page_index == 15);
    CHECK(camera_hits[4].texture_offset == 0x0800);
    const unsigned char *camera_pages[] = { wall15_buf, wall14_buf, wall17_buf,
                                            wall14_buf, wall15_buf };
    wl_wall_strip camera_strips[5];
    for (size_t i = 0; i < 5; ++i) {
        CHECK(wl_wall_hit_to_strip(&camera_hits[i], camera_pages[i], 4096,
                                   &camera_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(camera_strips, 5, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x7320f695);
    CHECK(wl_build_camera_ray_directions(0, 0, 0, -0x8000, 5, 0, 1, 5,
                                         camera_dirs_x, camera_dirs_y) == -1);
    CHECK(wl_build_camera_ray_directions(0x10000, 0, 0, -0x8000, 5, 4, 1, 2,
                                         camera_dirs_x, camera_dirs_y) == -1);

    const unsigned char *view_pages[18] = { 0 };
    size_t view_page_sizes[18] = { 0 };
    view_pages[14] = wall14_buf;
    view_page_sizes[14] = sizeof(wall14_buf);
    view_pages[15] = wall15_buf;
    view_page_sizes[15] = sizeof(wall15_buf);
    view_pages[17] = wall17_buf;
    view_page_sizes[17] = sizeof(wall17_buf);
    wl_map_wall_hit view_hits[5];
    wl_wall_strip view_strips[5];
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_camera_wall_view(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                     player_y, 0x10000, 0, 0, -0x8000, 30, 1, 5,
                                     view_pages, view_page_sizes, 18, &canvas,
                                     camera_dirs_x, camera_dirs_y, view_hits,
                                     view_strips) == 0);
    CHECK(view_hits[0].tile_x == 36);
    CHECK(view_hits[0].tile_y == 58);
    CHECK(view_hits[0].wall_page_index == 15);
    CHECK(view_hits[0].scaled_height == 33);
    CHECK(view_hits[2].tile_x == 36);
    CHECK(view_hits[2].tile_y == 58);
    CHECK(view_hits[2].wall_page_index == 15);
    CHECK(view_hits[2].texture_offset == 0x01c0);
    CHECK(view_hits[4].tile_x == 36);
    CHECK(view_hits[4].tile_y == 58);
    CHECK(view_hits[4].wall_page_index == 14);
    CHECK(view_hits[4].scaled_height == 29);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xfad71929);
    view_pages[15] = NULL;
    CHECK(wl_render_camera_wall_view(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                     player_y, 0x10000, 0, 0, -0x8000, 30, 1, 5,
                                     view_pages, view_page_sizes, 18, &canvas,
                                     camera_dirs_x, camera_dirs_y, view_hits,
                                     view_strips) == -1);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 105, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x33b7f33f);
    CHECK(wl_decode_wall_page_metadata(chunk_buf, chunk_bytes, &wallmeta) == 0);
    CHECK(wallmeta.min_color == 0);
    CHECK(wallmeta.max_color == 31);
    CHECK(wallmeta.unique_color_count == 11);
    CHECK(wl_decode_wall_page_surface(chunk_buf, chunk_bytes, indexed_buf,
                                      sizeof(indexed_buf), &surface) == 0);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x66874cf5);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 106, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 1306);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xbf4fcd99);
    CHECK(wl_decode_vswap_shape_metadata(chunk_buf, chunk_bytes, dirinfo.chunks[106].kind,
                                         &shape) == 0);
    CHECK(shape.kind == WL_VSWAP_CHUNK_SPRITE);
    CHECK(shape.width == 64);
    CHECK(shape.height == 64);
    CHECK(shape.leftpix == 4);
    CHECK(shape.rightpix == 58);
    CHECK(shape.visible_columns == 55);
    CHECK(shape.first_column_offset == 800);
    CHECK(shape.last_column_offset == 1298);
    CHECK(shape.min_column_offset == 800);
    CHECK(shape.max_column_offset == 1298);
    CHECK(shape.post_count == 66);
    CHECK(shape.terminal_count == 55);
    CHECK(shape.min_posts_per_column == 1);
    CHECK(shape.max_posts_per_column == 2);
    CHECK(shape.min_post_span == 2);
    CHECK(shape.max_post_span == 40);
    CHECK(shape.max_post_start == 36);
    CHECK(shape.max_post_end == 46);
    CHECK(shape.min_source_offset == 108);
    CHECK(shape.max_source_offset == 782);
    CHECK(shape.total_post_span == 1372);
    CHECK(wl_decode_sprite_shape_surface(chunk_buf, chunk_bytes, 0, indexed_buf,
                                         sizeof(indexed_buf), &surface) == 0);
    CHECK(surface.format == WL_SURFACE_INDEXED8);
    CHECK(surface.width == 64);
    CHECK(surface.height == 64);
    CHECK(surface.stride == 64);
    CHECK(surface.pixel_count == 4096);
    CHECK(surface.pixels == indexed_buf);
    CHECK(count_byte_not_value(surface.pixels, surface.pixel_count, 0) == 614);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x918ed728);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_scaled_sprite(&surface, &canvas, 40, 64, 0, NULL, 0) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x3f753ac8);
    uint16_t wall_heights[80];
    for (size_t i = 0; i < 80; ++i) {
        wall_heights[i] = 0;
    }
    for (size_t i = 36; i < 44; ++i) {
        wall_heights[i] = 80;
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_render_scaled_sprite(&surface, &canvas, 40, 64, 0,
                                  wall_heights, 80) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xaa7c2838);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_render_scaled_sprite(&surface, &canvas, -4, 96, 0, NULL, 0) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x6ff0f5c8);
    CHECK(wl_render_scaled_sprite(&surface, &canvas, 40, 0, 0, NULL, 0) == -1);
    CHECK(wl_render_scaled_sprite(&surface, &canvas, 40, 64, 0, wall_heights, 79) == -1);
    wl_projected_sprite sprites[3];
    CHECK(wl_project_world_sprite(106, player_x, player_y,
                                  (34u << 16) + 0x8000u,
                                  (57u << 16) + 0x8000u,
                                  0x10000, 0, 80, 128, &sprites[0]) == 0);
    CHECK(sprites[0].visible == 1);
    CHECK(sprites[0].view_x == 39);
    CHECK(sprites[0].scaled_height == 42);
    CHECK(sprites[0].distance == 0x51700);
    CHECK(sprites[0].trans_y == 0);
    CHECK(wl_project_world_sprite(107, player_x, player_y,
                                  (36u << 16) + 0x8000u,
                                  (58u << 16) + 0x8000u,
                                  0x10000, 0, 80, 128, &sprites[1]) == 0);
    CHECK(sprites[1].visible == 1);
    CHECK(sprites[1].view_x == 46);
    CHECK(sprites[1].scaled_height == 30);
    CHECK(sprites[1].distance == 0x71700);
    CHECK(sprites[1].trans_y == 0x10000);
    CHECK(wl_project_world_sprite(108, player_x, player_y,
                                  (28u << 16) + 0x8000u,
                                  (57u << 16) + 0x8000u,
                                  0x10000, 0, 80, 128, &sprites[2]) == 0);
    CHECK(sprites[2].visible == 0);
    CHECK(wl_sort_projected_sprites_far_to_near(sprites, 3) == 0);
    CHECK(sprites[0].source_index == 107);
    CHECK(sprites[1].source_index == 106);
    CHECK(sprites[2].source_index == 108);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    for (size_t i = 0; i < 80; ++i) {
        wall_heights[i] = 0;
    }
    for (size_t i = 30; i < 36; ++i) {
        wall_heights[i] = 45;
    }
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    for (size_t i = 0; i < 3; ++i) {
        if (sprites[i].visible) {
            CHECK(wl_render_scaled_sprite(&surface, &canvas, sprites[i].view_x,
                                          sprites[i].scaled_height, 0,
                                          wall_heights, 80) == 0);
        }
    }
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x819b1035);
    view_pages[15] = wall15_buf;
    const wl_indexed_surface *scene_sprites[] = { &surface, &surface };
    const uint32_t scene_sprite_x[] = {
        (34u << 16) + 0x8000u,
        (36u << 16) + 0x8000u,
    };
    const uint32_t scene_sprite_y[] = {
        (57u << 16) + 0x8000u,
        (58u << 16) + 0x8000u,
    };
    const uint16_t scene_sprite_ids[] = { 106, 107 };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_camera_scene_view(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                      player_y, 0x10000, 0, 0, -0x8000, 30, 1, 5,
                                      view_pages, view_page_sizes, 18,
                                      scene_sprites, scene_sprite_x, scene_sprite_y,
                                      scene_sprite_ids, 2, 0, &canvas,
                                      camera_dirs_x, camera_dirs_y, view_hits,
                                      view_strips, sprites, wall_heights) == 0);
    CHECK(sprites[0].source_index == 107);
    CHECK(sprites[0].surface_index == 1);
    CHECK(sprites[0].view_x == 46);
    CHECK(sprites[0].scaled_height == 30);
    CHECK(sprites[1].source_index == 106);
    CHECK(sprites[1].surface_index == 0);
    CHECK(sprites[1].view_x == 39);
    CHECK(sprites[1].scaled_height == 42);
    CHECK(wall_heights[30] == 33);
    CHECK(wall_heights[32] == 33);
    CHECK(wall_heights[34] == 29);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x1e4a8264);
    const wl_indexed_surface *bad_scene_sprites[] = { NULL };
    CHECK(wl_render_camera_scene_view(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                      player_y, 0x10000, 0, 0, -0x8000, 30, 1, 5,
                                      view_pages, view_page_sizes, 18,
                                      bad_scene_sprites, scene_sprite_x, scene_sprite_y,
                                      scene_sprite_ids, 1, 0, &canvas,
                                      camera_dirs_x, camera_dirs_y, view_hits,
                                      view_strips, sprites, wall_heights) == -1);
    const uint16_t visible_ref_chunks[] = {
        scene_refs[113].vswap_chunk_index,
        scene_refs[114].vswap_chunk_index,
    };
    unsigned char visible_ref_pixels[2 * WL_MAP_PLANE_WORDS];
    wl_indexed_surface visible_ref_surfaces[2];
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               visible_ref_chunks, 2, 0,
                                               visible_ref_pixels,
                                               sizeof(visible_ref_pixels),
                                               visible_ref_surfaces) == 0);
    CHECK(fnv1a_bytes(visible_ref_pixels, sizeof(visible_ref_pixels)) == 0xd53b06f5);
    CHECK(fnv1a_bytes(visible_ref_surfaces[0].pixels,
                      visible_ref_surfaces[0].pixel_count) == 0x442facd4);
    CHECK(fnv1a_bytes(visible_ref_surfaces[1].pixels,
                      visible_ref_surfaces[1].pixel_count) == 0xd363bf0c);
    const wl_indexed_surface *visible_scene_sprites[] = {
        &visible_ref_surfaces[0],
        &visible_ref_surfaces[1],
    };
    const uint32_t visible_scene_x[] = {
        scene_refs[113].world_x,
        scene_refs[114].world_x,
    };
    const uint32_t visible_scene_y[] = {
        scene_refs[113].world_y,
        scene_refs[114].world_y,
    };
    const uint16_t visible_scene_ids[] = {
        scene_refs[113].source_index,
        scene_refs[114].source_index,
    };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_camera_scene_view(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                      player_y, 0x10000, 0, 0, -0x8000, 30, 1, 5,
                                      view_pages, view_page_sizes, 18,
                                      visible_scene_sprites, visible_scene_x,
                                      visible_scene_y, visible_scene_ids, 2, 0,
                                      &canvas, camera_dirs_x, camera_dirs_y,
                                      view_hits, view_strips, sprites,
                                      wall_heights) == 0);
    CHECK(sprites[0].source_index == scene_refs[114].source_index);
    CHECK(sprites[0].surface_index == 1);
    CHECK(sprites[0].view_x == 39);
    CHECK(sprites[0].scaled_height == 26);
    CHECK(sprites[1].source_index == scene_refs[113].source_index);
    CHECK(sprites[1].surface_index == 0);
    CHECK(sprites[1].view_x == 39);
    CHECK(sprites[1].scaled_height == 42);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x61f7f78b);
    CHECK(wl_project_world_sprite(106, player_x, player_y, player_x, player_y,
                                  0, 0, 80, 128, &sprites[0]) == -1);
    CHECK(wl_sort_projected_sprites_far_to_near(NULL, 0) == 0);
    CHECK(wl_sort_projected_sprites_far_to_near(NULL, 1) == -1);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 107, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 1556);
    CHECK(wl_decode_vswap_shape_metadata(chunk_buf, chunk_bytes, dirinfo.chunks[107].kind,
                                         &shape) == 0);
    CHECK(shape.leftpix == 1);
    CHECK(shape.rightpix == 62);
    CHECK(shape.visible_columns == 62);
    CHECK(shape.post_count == 85);
    CHECK(shape.terminal_count == 62);
    CHECK(shape.min_posts_per_column == 1);
    CHECK(shape.max_posts_per_column == 3);
    CHECK(shape.max_post_span == 36);
    CHECK(shape.max_post_start == 32);
    CHECK(shape.max_post_end == 36);
    CHECK(shape.min_source_offset == 113);
    CHECK(shape.max_source_offset == 904);
    CHECK(shape.total_post_span == 1586);
    CHECK(wl_decode_sprite_shape_surface(chunk_buf, chunk_bytes, 0, indexed_buf,
                                         sizeof(indexed_buf), &surface) == 0);
    CHECK(count_byte_not_value(surface.pixels, surface.pixel_count, 0) == 384);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x88a2d1b4);
    CHECK(wl_decode_sprite_shape_surface(chunk_buf, chunk_bytes, 0, indexed_buf,
                                         4095, &surface) == -1);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 542, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xaee73350);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 662, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 184);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xfba68c74);
    return 0;
}

static int check_optional_sod(const char *dir) {
    char sod_dir[1024];
    CHECK(wl_join_path(sod_dir, sizeof(sod_dir), dir, "m1") == 0);

    char maphead_path[1024];
    char gamemaps_path[1024];
    char vswap_path[1024];
    char vgahead_path[1024];
    char vgadict_path[1024];
    char vgagraph_path[1024];
    if (path(maphead_path, sizeof(maphead_path), sod_dir, "MAPHEAD.SOD") != 0 ||
        path(gamemaps_path, sizeof(gamemaps_path), sod_dir, "GAMEMAPS.SOD") != 0 ||
        path(vswap_path, sizeof(vswap_path), sod_dir, "VSWAP.SOD") != 0 ||
        path(vgahead_path, sizeof(vgahead_path), sod_dir, "VGAHEAD.SOD") != 0 ||
        path(vgadict_path, sizeof(vgadict_path), sod_dir, "VGADICT.SOD") != 0 ||
        path(vgagraph_path, sizeof(vgagraph_path), sod_dir, "VGAGRAPH.SOD") != 0) {
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

    wl_graphics_header gh;
    wl_huffman_node huff[WL_HUFFMAN_NODE_COUNT];
    unsigned char graphics_buf[65536];
    unsigned char indexed_buf[65536];
    unsigned char scaler_pixels[80 * 128];
    wl_indexed_surface surface;
    wl_indexed_surface scaler;
    size_t graphics_bytes = 0;
    size_t compressed_bytes = 0;
    CHECK(wl_read_graphics_header(vgahead_path, &gh) == 0);
    CHECK(gh.chunk_count == 169);
    CHECK(gh.file_size == 510);
    CHECK(gh.offsets[0] == 0);
    CHECK(gh.offsets[1] == 383);
    CHECK(gh.offsets[169] == 947979);
    CHECK(wl_read_huffman_dictionary(vgadict_path, huff) == 0);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 0, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 383);
    CHECK(graphics_bytes == 588);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0x43f617ea);
    wl_picture_table_metadata pictures;
    CHECK(wl_decode_picture_table(graphics_buf, graphics_bytes, &pictures) == 0);
    CHECK(pictures.picture_count == 147);
    CHECK(pictures.min_width == 8);
    CHECK(pictures.max_width == 320);
    CHECK(pictures.min_height == 8);
    CHECK(pictures.max_height == 200);
    CHECK(pictures.total_pixels == 1105792);
    CHECK(pictures.pictures[0].width == 320);
    CHECK(pictures.pictures[0].height == 200);
    CHECK(pictures.pictures[1].width == 104);
    CHECK(pictures.pictures[1].height == 16);
    CHECK(pictures.pictures[84].width == 320);
    CHECK(pictures.pictures[84].height == 200);
    CHECK(pictures.pictures[90].width == 320);
    CHECK(pictures.pictures[90].height == 80);
    CHECK(pictures.pictures[91].width == 320);
    CHECK(pictures.pictures[91].height == 120);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 1, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 4448);
    CHECK(graphics_bytes == 8300);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0xdb48ce2b);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 3, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 42248);
    CHECK(graphics_bytes == 64000);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0x3a6afac3);
    CHECK(wl_decode_planar_picture_surface(graphics_buf, graphics_bytes,
                                           pictures.pictures[0].width,
                                           pictures.pictures[0].height,
                                           indexed_buf, sizeof(indexed_buf),
                                           &surface) == 0);
    CHECK(surface.format == WL_SURFACE_INDEXED8);
    CHECK(surface.width == 320);
    CHECK(surface.height == 200);
    CHECK(surface.stride == 320);
    CHECK(surface.pixel_count == graphics_bytes);
    CHECK(surface.pixels == indexed_buf);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x5e85d9c1);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 90, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 10561);
    CHECK(graphics_bytes == 12800);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0xa5d5a6f7);
    CHECK(wl_decode_planar_picture_surface(graphics_buf, graphics_bytes,
                                           pictures.pictures[87].width,
                                           pictures.pictures[87].height,
                                           indexed_buf, sizeof(indexed_buf),
                                           &surface) == 0);
    CHECK(surface.width == 320);
    CHECK(surface.height == 40);
    CHECK(surface.pixel_count == graphics_bytes);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0xff61711d);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 149, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 6243);
    CHECK(graphics_bytes == 10752);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0xeb393cc0);
    CHECK(wl_decode_planar_picture_surface(graphics_buf, graphics_bytes,
                                           pictures.pictures[146].width,
                                           pictures.pictures[146].height,
                                           indexed_buf, sizeof(indexed_buf),
                                           &surface) == 0);
    CHECK(surface.width == 224);
    CHECK(surface.height == 48);
    CHECK(surface.pixel_count == graphics_bytes);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x46e4bd08);

    wl_vswap_header vs;
    CHECK(wl_read_vswap_header(vswap_path, &vs) == 0);
    CHECK(vs.chunks_in_file == 666);
    CHECK(vs.sprite_start == 134);
    CHECK(vs.sound_start == 555);

    wl_vswap_directory dirinfo;
    CHECK(wl_read_vswap_directory(vswap_path, &dirinfo) == 0);
    CHECK(dirinfo.header.file_size == 1616544);
    CHECK(dirinfo.data_start == 4002);
    CHECK(dirinfo.max_chunk_end == 1616544);
    CHECK(dirinfo.wall_count == 134);
    CHECK(dirinfo.sprite_count == 421);
    CHECK(dirinfo.sound_count == 111);
    CHECK(dirinfo.sparse_count == 0);
    CHECK(dirinfo.chunks[133].offset == 548864);
    CHECK(dirinfo.chunks[133].length == 4096);
    CHECK(dirinfo.chunks[134].offset == 552960);
    CHECK(dirinfo.chunks[134].length == 1306);
    CHECK(dirinfo.chunks[134].kind == WL_VSWAP_CHUNK_SPRITE);
    CHECK(dirinfo.chunks[555].offset == 1233408);
    CHECK(dirinfo.chunks[555].length == 4096);
    CHECK(dirinfo.chunks[665].offset == 1616384);
    CHECK(dirinfo.chunks[665].length == 160);

    unsigned char chunk_buf[4096];
    unsigned char wall0_buf[4096];
    unsigned char wall105_buf[4096];
    unsigned char column_buf[64];
    unsigned char surface_column_buf[64];
    size_t chunk_bytes = 0;
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 0, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x98d020a5);
    memcpy(wall0_buf, chunk_buf, sizeof(wall0_buf));
    wl_vswap_shape_metadata shape;
    CHECK(wl_decode_vswap_shape_metadata(chunk_buf, chunk_bytes, dirinfo.chunks[0].kind,
                                         &shape) == 0);
    CHECK(shape.kind == WL_VSWAP_CHUNK_WALL);
    CHECK(shape.width == 64);
    CHECK(shape.height == 64);
    CHECK(shape.visible_columns == 64);
    wl_wall_page_metadata wallmeta;
    CHECK(wl_decode_wall_page_metadata(chunk_buf, chunk_bytes, &wallmeta) == 0);
    CHECK(wallmeta.min_color == 7);
    CHECK(wallmeta.max_color == 31);
    CHECK(wallmeta.unique_color_count == 18);
    CHECK(wl_decode_wall_page_surface(chunk_buf, chunk_bytes, indexed_buf,
                                      sizeof(indexed_buf), &surface) == 0);
    CHECK(surface.width == 64);
    CHECK(surface.height == 64);
    CHECK(surface.pixel_count == 4096);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x8fe4d8ff);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0800, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0x2fbb79bb);
    CHECK(wl_sample_indexed_surface_column(&surface, 32, surface_column_buf,
                                           sizeof(surface_column_buf)) == 0);
    CHECK(memcmp(column_buf, surface_column_buf, sizeof(column_buf)) == 0);
    memset(scaler_pixels, 0x2a, sizeof(scaler_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, scaler_pixels, sizeof(scaler_pixels),
                                  &scaler) == 0);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &scaler,
                                          7, 64) == 0);
    CHECK(fnv1a_bytes(scaler.pixels, scaler.pixel_count) == 0x78547277);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 105, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xbbbf5c67);
    memcpy(wall105_buf, chunk_buf, sizeof(wall105_buf));
    CHECK(wl_decode_wall_page_metadata(chunk_buf, chunk_bytes, &wallmeta) == 0);
    CHECK(wallmeta.min_color == 0);
    CHECK(wallmeta.max_color == 237);
    CHECK(wallmeta.unique_color_count == 26);
    CHECK(wl_decode_wall_page_surface(chunk_buf, chunk_bytes, indexed_buf,
                                      sizeof(indexed_buf), &surface) == 0);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x997d475d);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0000, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0xd61f9cbd);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0fc0, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0x3e5f4efd);
    CHECK(wl_sample_indexed_surface_column(&surface, 63, surface_column_buf,
                                           sizeof(surface_column_buf)) == 0);
    CHECK(memcmp(column_buf, surface_column_buf, sizeof(column_buf)) == 0);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &scaler,
                                          29, 160) == 0);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0000, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &scaler,
                                          13, 96) == 0);
    CHECK(fnv1a_bytes(scaler.pixels, scaler.pixel_count) == 0x60ddb236);
    memset(scaler_pixels, 0x2a, sizeof(scaler_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, scaler_pixels, sizeof(scaler_pixels),
                                  &scaler) == 0);
    const wl_wall_strip sod_strips[] = {
        { wall0_buf, sizeof(wall0_buf), 0x0800, 7, 64 },
        { wall105_buf, sizeof(wall105_buf), 0x0fc0, 29, 160 },
        { wall105_buf, sizeof(wall105_buf), 0x0000, 13, 96 },
    };
    CHECK(wl_render_wall_strip_viewport(sod_strips, 3, &scaler) == 0);
    CHECK(fnv1a_bytes(scaler.pixels, scaler.pixel_count) == 0x60ddb236);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 133, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x33b7f33f);
    CHECK(wl_decode_wall_page_metadata(chunk_buf, chunk_bytes, &wallmeta) == 0);
    CHECK(wallmeta.min_color == 0);
    CHECK(wallmeta.max_color == 31);
    CHECK(wallmeta.unique_color_count == 11);
    CHECK(wl_decode_wall_page_surface(chunk_buf, chunk_bytes, indexed_buf,
                                      sizeof(indexed_buf), &surface) == 0);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x66874cf5);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 134, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 1306);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xbf4fcd99);
    CHECK(wl_decode_vswap_shape_metadata(chunk_buf, chunk_bytes, dirinfo.chunks[134].kind,
                                         &shape) == 0);
    CHECK(shape.kind == WL_VSWAP_CHUNK_SPRITE);
    CHECK(shape.leftpix == 4);
    CHECK(shape.rightpix == 58);
    CHECK(shape.visible_columns == 55);
    CHECK(shape.first_column_offset == 800);
    CHECK(shape.last_column_offset == 1298);
    CHECK(shape.post_count == 66);
    CHECK(shape.terminal_count == 55);
    CHECK(shape.min_posts_per_column == 1);
    CHECK(shape.max_posts_per_column == 2);
    CHECK(shape.max_post_span == 40);
    CHECK(shape.min_source_offset == 108);
    CHECK(shape.max_source_offset == 782);
    CHECK(shape.total_post_span == 1372);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 135, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 1556);
    CHECK(wl_decode_vswap_shape_metadata(chunk_buf, chunk_bytes, dirinfo.chunks[135].kind,
                                         &shape) == 0);
    CHECK(shape.leftpix == 1);
    CHECK(shape.rightpix == 62);
    CHECK(shape.visible_columns == 62);
    CHECK(shape.post_count == 85);
    CHECK(shape.terminal_count == 62);
    CHECK(shape.max_posts_per_column == 3);
    CHECK(shape.max_post_span == 36);
    CHECK(shape.max_source_offset == 904);
    CHECK(shape.total_post_span == 1586);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 555, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xaee73350);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 665, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 160);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xbb53ed59);
    return 0;
}

int main(void) {
    const char *dir = wl_default_data_dir();
    CHECK(check_decode_helpers() == 0);
    CHECK(check_wl6(dir) == 0);
    CHECK(check_optional_sod(dir) == 0);
    printf("asset/decompression/semantics/model/vswap/palette-shift tests passed for %s\n", dir);
    return 0;
}
