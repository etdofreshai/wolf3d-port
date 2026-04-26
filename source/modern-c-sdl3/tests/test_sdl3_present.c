#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "wl_assets.h"
#include "wl_game_model.h"

static uint32_t fnv1a_bytes(const unsigned char *data, size_t size) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        h ^= (uint32_t)data[i];
        h *= 16777619u;
    }
    return h;
}

static int file_stats(const char *path, long *out_size, uint32_t *out_hash) {
    FILE *f = fopen(path, "rb");
    unsigned char buffer[4096];
    uint32_t h = 2166136261u;
    long size = 0;
    if (!f || !out_size || !out_hash) {
        if (f) {
            fclose(f);
        }
        return -1;
    }
    for (;;) {
        size_t n = fread(buffer, 1, sizeof(buffer), f);
        if (n > 0) {
            for (size_t i = 0; i < n; ++i) {
                h ^= (uint32_t)buffer[i];
                h *= 16777619u;
            }
            size += (long)n;
        }
        if (n < sizeof(buffer)) {
            if (ferror(f)) {
                fclose(f);
                return -1;
            }
            break;
        }
    }
    fclose(f);
    *out_size = size;
    *out_hash = h;
    return 0;
}

static int save_rgba_bmp_artifact(const char *path, uint16_t width,
                                  uint16_t height, unsigned char *rgba,
                                  long expected_size, uint32_t expected_hash) {
    long bmp_size = 0;
    uint32_t bmp_hash = 0;
    SDL_Surface *surface = SDL_CreateSurfaceFrom(width, height,
                                                 SDL_PIXELFORMAT_RGBA32, rgba,
                                                 (int)width * 4);
    if (!surface) {
        fprintf(stderr, "SDL_CreateSurfaceFrom artifact failed for %s: %s\n",
                path, SDL_GetError());
        return -1;
    }
    if (!SDL_SaveBMP(surface, path)) {
        fprintf(stderr, "SDL_SaveBMP artifact failed for %s: %s\n", path,
                SDL_GetError());
        SDL_DestroySurface(surface);
        return -1;
    }
    SDL_DestroySurface(surface);
    if (file_stats(path, &bmp_size, &bmp_hash) != 0 ||
        bmp_size != expected_size || bmp_hash != expected_hash) {
        fprintf(stderr,
                "unexpected artifact stats for %s: size=%ld hash=0x%08x\n",
                path, bmp_size, bmp_hash);
        return -1;
    }
    return 0;
}

int main(void) {
    char vswap_path[512];
    wl_vswap_directory directory;
    unsigned char chunk[4096];
    size_t chunk_size = 0;
    unsigned char wall_pixels[WL_MAP_PLANE_WORDS];
    unsigned char wall_pixels_1[WL_MAP_PLANE_WORDS];
    unsigned char sprite_pixels[WL_MAP_PLANE_WORDS];
    unsigned char sprite_canvas_pixels[WL_MAP_SIDE * 2u * WL_MAP_SIDE];
    unsigned char atlas_pixels[WL_MAP_SIDE * 2u * WL_MAP_SIDE];
    wl_indexed_surface wall;
    wl_indexed_surface wall_1;
    wl_indexed_surface sprite;
    wl_indexed_surface sprite_canvas;
    wl_indexed_surface atlas;
    wl_indexed_surface live_scene;
    wl_indexed_surface wl6_scene;
    wl_indexed_surface wl6_sprites_surface[5];
    unsigned char palette[256 * 3];
    unsigned char red_palettes[WL_NUM_RED_SHIFTS * 256u * 3u];
    unsigned char rgba[WL_MAP_PLANE_WORDS * 4];
    unsigned char atlas_rgba[WL_MAP_SIDE * 2u * WL_MAP_SIDE * 4u];
    unsigned char sprite_rgba[WL_MAP_SIDE * 2u * WL_MAP_SIDE * 4u];
    unsigned char live_scene_pixels[80u * 128u];
    unsigned char live_scene_rgba[80u * 128u * 4u];
    unsigned char wl6_scene_pixels[80u * 128u];
    unsigned char wl6_scene_rgba[80u * 128u * 4u];
    unsigned char wl6_sprite_pixels[5][WL_MAP_PLANE_WORDS];
    wl_palette_shift_result shift;
    wl_present_frame_descriptor present;
    wl_game_model live_model;
    wl_game_model wl6_model;
    wl_maphead maphead;
    wl_map_header map0;
    uint16_t wall_plane[WL_MAP_PLANE_WORDS];
    uint16_t info_plane[WL_MAP_PLANE_WORDS];
    char maphead_path[512];
    char gamemaps_path[512];
    const unsigned char *live_wall_pages[106];
    size_t live_wall_page_sizes[106];
    const wl_indexed_surface *live_sprite_surfaces[1];
    uint32_t live_sprite_x[1];
    uint32_t live_sprite_y[1];
    uint16_t live_sprite_ids[1];
    int32_t live_dirs_x[3];
    int32_t live_dirs_y[3];
    wl_map_wall_hit live_hits[3];
    wl_wall_strip live_strips[3];
    wl_projected_sprite live_sprites[1];
    uint16_t live_wall_heights[80];
    wl_scene_sprite_ref wl6_refs[160];
    size_t wl6_ref_count = 0;
    const wl_indexed_surface *wl6_sprite_surfaces[5];
    uint32_t wl6_sprite_x[5];
    uint32_t wl6_sprite_y[5];
    uint16_t wl6_sprite_ids[5];
    const size_t wl6_visible_ref_indices[5] = { 110u, 111u, 113u, 114u, 115u };
    int32_t wl6_dirs_x[3];
    int32_t wl6_dirs_y[3];
    wl_map_wall_hit wl6_hits[3];
    wl_wall_strip wl6_strips[3];
    wl_projected_sprite wl6_sprites[5];
    uint16_t wl6_wall_heights[80];
    long bmp_size = 0;
    uint32_t bmp_hash = 0;

    for (size_t i = 0; i < 256; ++i) {
        palette[i * 3u + 0u] = (unsigned char)(i & 63u);
        palette[i * 3u + 1u] = (unsigned char)((i * 2u) & 63u);
        palette[i * 3u + 2u] = (unsigned char)((63u - i) & 63u);
    }
    for (uint8_t i = 1; i <= WL_NUM_RED_SHIFTS; ++i) {
        if (wl_build_palette_shift(palette, sizeof(palette), 6, 63, 0, 0,
                                   i, WL_RED_SHIFT_STEPS,
                                   red_palettes +
                                       (size_t)(i - 1u) * sizeof(palette),
                                   sizeof(palette)) != 0) {
            fprintf(stderr, "could not build red palette shift\n");
            return 1;
        }
    }

    if (wl_join_path(vswap_path, sizeof(vswap_path), wl_default_data_dir(),
                     "VSWAP.WL6") != 0) {
        fprintf(stderr, "could not build VSWAP path\n");
        return 1;
    }
    if (wl_read_vswap_directory(vswap_path, &directory) != 0) {
        fprintf(stderr, "could not read VSWAP directory: %s\n", vswap_path);
        return 1;
    }
    if (wl_read_vswap_chunk(vswap_path, &directory, 0, chunk, sizeof(chunk),
                            &chunk_size) != 0) {
        fprintf(stderr, "could not read VSWAP wall chunk 0\n");
        return 1;
    }
    if (wl_decode_wall_page_surface(chunk, chunk_size, wall_pixels,
                                    sizeof(wall_pixels), &wall) != 0) {
        fprintf(stderr, "could not decode VSWAP wall chunk 0\n");
        return 1;
    }
    if (fnv1a_bytes(wall.pixels, wall.pixel_count) != 0x8fe4d8ffu) {
        fprintf(stderr, "unexpected wall-page pixel hash\n");
        return 1;
    }
    if (wl_read_vswap_chunk(vswap_path, &directory, 1, chunk, sizeof(chunk),
                            &chunk_size) != 0) {
        fprintf(stderr, "could not read VSWAP wall chunk 1\n");
        return 1;
    }
    if (wl_decode_wall_page_surface(chunk, chunk_size, wall_pixels_1,
                                    sizeof(wall_pixels_1), &wall_1) != 0) {
        fprintf(stderr, "could not decode VSWAP wall chunk 1\n");
        return 1;
    }
    for (size_t y = 0; y < WL_MAP_SIDE; ++y) {
        memcpy(atlas_pixels + y * WL_MAP_SIDE * 2u,
               wall.pixels + y * WL_MAP_SIDE, WL_MAP_SIDE);
        memcpy(atlas_pixels + y * WL_MAP_SIDE * 2u + WL_MAP_SIDE,
               wall_1.pixels + y * WL_MAP_SIDE, WL_MAP_SIDE);
    }
    if (wl_wrap_indexed_surface(WL_MAP_SIDE * 2u, WL_MAP_SIDE, atlas_pixels,
                                sizeof(atlas_pixels), &atlas) != 0) {
        fprintf(stderr, "could not wrap wall atlas\n");
        return 1;
    }
    if (wl_read_vswap_chunk(vswap_path, &directory, 106, chunk, sizeof(chunk),
                            &chunk_size) != 0) {
        fprintf(stderr, "could not read VSWAP sprite chunk 106\n");
        return 1;
    }
    if (wl_decode_sprite_shape_surface(chunk, chunk_size, 0, sprite_pixels,
                                       sizeof(sprite_pixels), &sprite) != 0) {
        fprintf(stderr, "could not decode VSWAP sprite chunk 106\n");
        return 1;
    }
    memset(sprite_canvas_pixels, 0x2a, sizeof(sprite_canvas_pixels));
    if (wl_wrap_indexed_surface(WL_MAP_SIDE * 2u, WL_MAP_SIDE,
                                sprite_canvas_pixels,
                                sizeof(sprite_canvas_pixels),
                                &sprite_canvas) != 0) {
        fprintf(stderr, "could not wrap sprite canvas\n");
        return 1;
    }
    if (wl_render_scaled_sprite(&sprite, &sprite_canvas, 64, 64, 0, NULL, 0) != 0) {
        fprintf(stderr, "could not render sprite canvas\n");
        return 1;
    }
    if (fnv1a_bytes(sprite.pixels, sprite.pixel_count) != 0x918ed728u) {
        fprintf(stderr, "unexpected sprite pixel hash\n");
        return 1;
    }
    if (fnv1a_bytes(sprite_canvas.pixels, sprite_canvas.pixel_count) !=
        0xb7087e58u) {
        fprintf(stderr, "unexpected sprite canvas hash\n");
        return 1;
    }
    if (fnv1a_bytes(wall_1.pixels, wall_1.pixel_count) != 0xcc7509fdu ||
        fnv1a_bytes(atlas.pixels, atlas.pixel_count) != 0x223d2cafu) {
        fprintf(stderr, "unexpected wall atlas pixel hash\n");
        return 1;
    }

    memset(&shift, 0, sizeof(shift));
    shift.kind = WL_PALETTE_SHIFT_NONE;
    if (wl_describe_present_frame(&wall, &shift, palette, NULL, 0, NULL, 0,
                                  sizeof(palette), 6, &present) != 0) {
        fprintf(stderr, "could not describe present frame\n");
        return 1;
    }
    if (present.viewport_width != WL_MAP_SIDE ||
        present.viewport_height != WL_MAP_SIDE ||
        present.pixel_hash != 0x8fe4d8ffu ||
        present.palette_hash != fnv1a_bytes(palette, sizeof(palette))) {
        fprintf(stderr, "unexpected present descriptor\n");
        return 1;
    }
    if (wl_expand_present_frame_to_rgba(&present, rgba, sizeof(rgba), NULL) != 0) {
        fprintf(stderr, "could not expand wall frame to RGBA\n");
        return 1;
    }
    if (fnv1a_bytes(rgba, sizeof(rgba)) != 0x71d4b5b6u) {
        fprintf(stderr, "unexpected wall RGBA hash\n");
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("wolf3d-sdl3-present",
                                          present.viewport_width,
                                          present.viewport_height,
                                          SDL_WINDOW_HIDDEN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Surface *surface = SDL_GetWindowSurface(window);
    if (!surface) {
        fprintf(stderr, "SDL_GetWindowSurface failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Surface *source = SDL_CreateSurfaceFrom(present.viewport_width,
                                               present.viewport_height,
                                               SDL_PIXELFORMAT_RGBA32,
                                               rgba,
                                               present.viewport_width * 4);
    if (!source) {
        fprintf(stderr, "SDL_CreateSurfaceFrom failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!SDL_BlitSurface(source, NULL, surface, NULL)) {
        fprintf(stderr, "SDL_BlitSurface failed: %s\n", SDL_GetError());
        SDL_DestroySurface(source);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!SDL_UpdateWindowSurface(window)) {
        fprintf(stderr, "SDL_UpdateWindowSurface failed: %s\n", SDL_GetError());
        SDL_DestroySurface(source);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!SDL_SaveBMP(source, "build/wolf-wall-present.bmp")) {
        fprintf(stderr, "SDL_SaveBMP failed: %s\n", SDL_GetError());
        SDL_DestroySurface(source);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (file_stats("build/wolf-wall-present.bmp", &bmp_size, &bmp_hash) != 0 ||
        bmp_size != 16522 || bmp_hash != 0xb49b4cbfu) {
        fprintf(stderr, "unexpected screenshot artifact stats: size=%ld hash=0x%08x\n", bmp_size, bmp_hash);
        SDL_DestroySurface(source);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_DestroySurface(source);
    source = NULL;

    memset(&shift, 0, sizeof(shift));
    shift.kind = WL_PALETTE_SHIFT_RED;
    shift.shift_index = 2;
    if (wl_describe_present_frame(&wall, &shift, palette,
                                  red_palettes, WL_NUM_RED_SHIFTS,
                                  NULL, 0, sizeof(palette), 6,
                                  &present) != 0) {
        fprintf(stderr, "could not describe red present frame\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (present.pixel_hash != 0x8fe4d8ffu ||
        present.palette_hash != 0xd0d5c585u ||
        present.palette_shift_kind != WL_PALETTE_SHIFT_RED ||
        present.palette_shift_index != 2) {
        fprintf(stderr, "unexpected red present descriptor: pixel=0x%08x palette=0x%08x kind=%u index=%u\n",
                present.pixel_hash, present.palette_hash,
                present.palette_shift_kind, present.palette_shift_index);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (wl_expand_present_frame_to_rgba(&present, rgba, sizeof(rgba), NULL) != 0) {
        fprintf(stderr, "could not expand red wall frame to RGBA\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (fnv1a_bytes(rgba, sizeof(rgba)) != 0x1dcaf8c4u) {
        fprintf(stderr, "unexpected red wall RGBA hash\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    source = SDL_CreateSurfaceFrom(present.viewport_width,
                                   present.viewport_height,
                                   SDL_PIXELFORMAT_RGBA32, rgba,
                                   present.viewport_width * 4);
    if (!source) {
        fprintf(stderr, "SDL_CreateSurfaceFrom red failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (!SDL_BlitSurface(source, NULL, surface, NULL)) {
        fprintf(stderr, "SDL_BlitSurface red failed: %s\n", SDL_GetError());
        SDL_DestroySurface(source);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (!SDL_UpdateWindowSurface(window)) {
        fprintf(stderr, "SDL_UpdateWindowSurface red failed: %s\n", SDL_GetError());
        SDL_DestroySurface(source);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (!SDL_SaveBMP(source, "build/wolf-wall-present-red.bmp")) {
        fprintf(stderr, "SDL_SaveBMP red failed: %s\n", SDL_GetError());
        SDL_DestroySurface(source);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (file_stats("build/wolf-wall-present-red.bmp", &bmp_size, &bmp_hash) != 0 ||
        bmp_size != 16522 || bmp_hash != 0xaa1c75c5u) {
        fprintf(stderr, "unexpected red screenshot artifact stats: size=%ld hash=0x%08x\n", bmp_size, bmp_hash);
        SDL_DestroySurface(source);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_DestroySurface(source);
    source = NULL;

    memset(&shift, 0, sizeof(shift));
    shift.kind = WL_PALETTE_SHIFT_NONE;
    if (wl_describe_present_frame(&atlas, &shift, palette, NULL, 0, NULL, 0,
                                  sizeof(palette), 6, &present) != 0) {
        fprintf(stderr, "could not describe atlas present frame\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (wl_expand_present_frame_to_rgba(&present, atlas_rgba,
                                        sizeof(atlas_rgba), NULL) != 0) {
        fprintf(stderr, "could not expand atlas frame to RGBA\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (present.viewport_width != WL_MAP_SIDE * 2u ||
        present.viewport_height != WL_MAP_SIDE ||
        present.pixel_hash != 0x223d2cafu ||
        fnv1a_bytes(atlas_rgba, sizeof(atlas_rgba)) != 0x3a8ae4e9u) {
        fprintf(stderr, "unexpected atlas present metadata\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (save_rgba_bmp_artifact("build/wolf-wall-atlas-present.bmp",
                               present.viewport_width, present.viewport_height,
                               atlas_rgba, 32906, 0xaf70162cu) != 0) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    memset(&shift, 0, sizeof(shift));
    shift.kind = WL_PALETTE_SHIFT_NONE;
    if (wl_describe_present_frame(&sprite_canvas, &shift, palette,
                                  NULL, 0, NULL, 0, sizeof(palette), 6,
                                  &present) != 0) {
        fprintf(stderr, "could not describe sprite present frame\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (wl_expand_present_frame_to_rgba(&present, sprite_rgba,
                                        sizeof(sprite_rgba), NULL) != 0) {
        fprintf(stderr, "could not expand sprite frame to RGBA\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (present.viewport_width != WL_MAP_SIDE * 2u ||
        present.viewport_height != WL_MAP_SIDE ||
        present.pixel_hash != 0xb7087e58u ||
        fnv1a_bytes(sprite_rgba, sizeof(sprite_rgba)) != 0x6159f78fu) {
        fprintf(stderr, "unexpected sprite present metadata\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (save_rgba_bmp_artifact("build/wolf-sprite-present.bmp",
                               present.viewport_width, present.viewport_height,
                               sprite_rgba, 32906, 0xbaeda862u) != 0) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    memset(&live_model, 0, sizeof(live_model));
    live_model.tilemap[4u + 4u * WL_MAP_SIDE] = 0x80u;
    live_model.door_count = 1;
    live_model.doors[0].x = 4;
    live_model.doors[0].y = 4;
    live_model.doors[0].source_tile = 90;
    live_model.doors[0].vertical = 1;
    live_model.doors[0].lock = 0;
    live_model.doors[0].position = 0;
    live_model.doors[0].action = WL_DOOR_CLOSED;
    for (size_t i = 0; i < 106u; ++i) {
        live_wall_pages[i] = wall.pixels;
        live_wall_page_sizes[i] = wall.pixel_count;
    }
    live_wall_pages[0] = wall.pixels;
    live_wall_pages[98] = wall_1.pixels;
    live_wall_page_sizes[98] = wall_1.pixel_count;
    live_sprite_surfaces[0] = &sprite;
    live_sprite_x[0] = (5u << 16) + 0x8000u;
    live_sprite_y[0] = (4u << 16) + 0x8000u;
    live_sprite_ids[0] = 106;
    memset(live_scene_pixels, 0x2a, sizeof(live_scene_pixels));
    if (wl_wrap_indexed_surface(80, 128, live_scene_pixels,
                                sizeof(live_scene_pixels), &live_scene) != 0) {
        fprintf(stderr, "could not wrap live scene canvas\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (wl_render_runtime_door_camera_scene_view(&live_model,
                                                 directory.header.sprite_start,
                                                 (3u << 16) + 0x8000u,
                                                 (4u << 16) + 0x8000u,
                                                 0x10000, 0, 0, -0x8000,
                                                 39, 1, 3, live_wall_pages,
                                                 live_wall_page_sizes, 106,
                                                 live_sprite_surfaces, live_sprite_x,
                                                 live_sprite_y, live_sprite_ids, 1,
                                                 0, &live_scene, live_dirs_x,
                                                 live_dirs_y, live_hits, live_strips,
                                                 live_sprites, live_wall_heights) != 0) {
        fprintf(stderr, "could not render live gameplay scene\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (fnv1a_bytes(live_scene.pixels, live_scene.pixel_count) != 0x8463d905u ||
        live_sprites[0].source_index != 106 || live_sprites[0].surface_index != 0) {
        fprintf(stderr, "unexpected live scene metadata: hash=0x%08x source=%u surface=%zu\n",
                fnv1a_bytes(live_scene.pixels, live_scene.pixel_count),
                live_sprites[0].source_index, live_sprites[0].surface_index);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    memset(&shift, 0, sizeof(shift));
    shift.kind = WL_PALETTE_SHIFT_NONE;
    if (wl_describe_present_frame(&live_scene, &shift, palette, NULL, 0,
                                  NULL, 0, sizeof(palette), 6, &present) != 0 ||
        wl_expand_present_frame_to_rgba(&present, live_scene_rgba,
                                        sizeof(live_scene_rgba), NULL) != 0) {
        fprintf(stderr, "could not describe/expand live scene present frame\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (present.pixel_hash != 0x8463d905u ||
        fnv1a_bytes(live_scene_rgba, sizeof(live_scene_rgba)) != 0xd6cf31acu) {
        fprintf(stderr, "unexpected live scene present hashes: pixel=0x%08x rgba=0x%08x\n",
                present.pixel_hash,
                fnv1a_bytes(live_scene_rgba, sizeof(live_scene_rgba)));
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (save_rgba_bmp_artifact("build/wolf-live-scene-present.bmp",
                               present.viewport_width, present.viewport_height,
                               live_scene_rgba, 41098, 0x2ac02cbdu) != 0) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (wl_join_path(maphead_path, sizeof(maphead_path), wl_default_data_dir(),
                     "MAPHEAD.WL6") != 0 ||
        wl_join_path(gamemaps_path, sizeof(gamemaps_path), wl_default_data_dir(),
                     "GAMEMAPS.WL6") != 0 ||
        wl_read_maphead(maphead_path, &maphead) != 0 ||
        wl_read_map_header(gamemaps_path, maphead.offsets[0], &map0) != 0 ||
        wl_read_map_plane(gamemaps_path, &map0, 0, maphead.rlew_tag,
                          wall_plane, WL_MAP_PLANE_WORDS) != 0 ||
        wl_read_map_plane(gamemaps_path, &map0, 1, maphead.rlew_tag,
                          info_plane, WL_MAP_PLANE_WORDS) != 0 ||
        wl_build_game_model(wall_plane, info_plane, WL_MAP_PLANE_WORDS,
                            WL_DIFFICULTY_EASY, &wl6_model) != 0 ||
        wl_collect_scene_sprite_refs(&wl6_model, directory.header.sprite_start,
                                     wl6_refs, 160, &wl6_ref_count) != 0 ||
        wl6_ref_count <= 113u) {
        fprintf(stderr, "could not build WL6 map scene model\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    for (size_t i = 0; i < 5u; ++i) {
        const size_t ref_index = wl6_visible_ref_indices[i];
        if (wl_read_vswap_chunk(vswap_path, &directory,
                                wl6_refs[ref_index].vswap_chunk_index, chunk,
                                sizeof(chunk), &chunk_size) != 0 ||
            wl_decode_sprite_shape_surface(chunk, chunk_size, 0,
                                           wl6_sprite_pixels[i],
                                           sizeof(wl6_sprite_pixels[i]),
                                           &wl6_sprites_surface[i]) != 0) {
            fprintf(stderr, "could not decode WL6 map scene sprite %zu\n", i);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        wl6_sprite_surfaces[i] = &wl6_sprites_surface[i];
        wl6_sprite_x[i] = wl6_refs[ref_index].world_x;
        wl6_sprite_y[i] = wl6_refs[ref_index].world_y;
        wl6_sprite_ids[i] = wl6_refs[ref_index].source_index;
    }
    memset(wl6_scene_pixels, 0x2a, sizeof(wl6_scene_pixels));
    if (wl_wrap_indexed_surface(80, 128, wl6_scene_pixels,
                                sizeof(wl6_scene_pixels), &wl6_scene) != 0 ||
        wl_render_runtime_door_camera_scene_view(&wl6_model,
                                                 directory.header.sprite_start,
                                                 (29u << 16) + 0x8000u,
                                                 (57u << 16) + 0x8000u,
                                                 0x10000, 0, 0, -0x8000,
                                                 39, 1, 3, live_wall_pages,
                                                 live_wall_page_sizes, 106,
                                                 wl6_sprite_surfaces, wl6_sprite_x,
                                                 wl6_sprite_y, wl6_sprite_ids, 5,
                                                 0, &wl6_scene, wl6_dirs_x,
                                                 wl6_dirs_y, wl6_hits, wl6_strips,
                                                 wl6_sprites, wl6_wall_heights) != 0) {
        fprintf(stderr, "could not render WL6 map scene\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (fnv1a_bytes(wl6_scene.pixels, wl6_scene.pixel_count) != 0xe9277582u) {
        fprintf(stderr, "unexpected WL6 scene hash: 0x%08x refs=%zu sprite=%u\n",
                fnv1a_bytes(wl6_scene.pixels, wl6_scene.pixel_count),
                wl6_ref_count, wl6_refs[wl6_visible_ref_indices[2]].vswap_chunk_index);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    memset(&shift, 0, sizeof(shift));
    shift.kind = WL_PALETTE_SHIFT_NONE;
    if (wl_describe_present_frame(&wl6_scene, &shift, palette, NULL, 0,
                                  NULL, 0, sizeof(palette), 6, &present) != 0 ||
        wl_expand_present_frame_to_rgba(&present, wl6_scene_rgba,
                                        sizeof(wl6_scene_rgba), NULL) != 0) {
        fprintf(stderr, "could not describe/expand WL6 map scene\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (fnv1a_bytes(wl6_scene_rgba, sizeof(wl6_scene_rgba)) != 0xc093aab4u) {
        fprintf(stderr, "unexpected WL6 scene RGBA hash: 0x%08x\n",
                fnv1a_bytes(wl6_scene_rgba, sizeof(wl6_scene_rgba)));
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (save_rgba_bmp_artifact("build/wolf-wl6-map0-scene-present.bmp",
                               present.viewport_width, present.viewport_height,
                               wl6_scene_rgba, 41098, 0xe6ebf3ddu) != 0) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("SDL3 Wolf WL6 map scene screenshot smoke test passed");
    return 0;
}
