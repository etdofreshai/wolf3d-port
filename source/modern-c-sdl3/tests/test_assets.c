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
    unsigned char canvas_pixels[160 * 120];
    wl_indexed_surface surface;
    wl_indexed_surface canvas;
    size_t graphics_bytes = 0;
    size_t compressed_bytes = 0;
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
    printf("asset/decompression/semantics/model/vswap/fixed-ray tests passed for %s\n", dir);
    return 0;
}
