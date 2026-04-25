#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "wl_assets.h"

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

int main(void) {
    char vswap_path[512];
    wl_vswap_directory directory;
    unsigned char chunk[4096];
    size_t chunk_size = 0;
    unsigned char wall_pixels[WL_MAP_PLANE_WORDS];
    unsigned char wall_pixels_1[WL_MAP_PLANE_WORDS];
    unsigned char atlas_pixels[WL_MAP_SIDE * 2u * WL_MAP_SIDE];
    wl_indexed_surface wall;
    wl_indexed_surface wall_1;
    wl_indexed_surface atlas;
    unsigned char palette[256 * 3];
    unsigned char red_palettes[WL_NUM_RED_SHIFTS * 256u * 3u];
    unsigned char rgba[WL_MAP_PLANE_WORDS * 4];
    unsigned char atlas_rgba[WL_MAP_SIDE * 2u * WL_MAP_SIDE * 4u];
    wl_palette_shift_result shift;
    wl_present_frame_descriptor present;
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
    if (wl_expand_indexed_surface_to_rgba(&wall, present.texture.palette,
                                          sizeof(palette), 6, rgba,
                                          sizeof(rgba), NULL) != 0) {
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
    if (wl_expand_indexed_surface_to_rgba(&wall, present.texture.palette,
                                          sizeof(palette), 6, rgba,
                                          sizeof(rgba), NULL) != 0) {
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
    if (wl_expand_indexed_surface_to_rgba(&atlas, present.texture.palette,
                                          sizeof(palette), 6, atlas_rgba,
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
    source = SDL_CreateSurfaceFrom(present.viewport_width,
                                   present.viewport_height,
                                   SDL_PIXELFORMAT_RGBA32, atlas_rgba,
                                   present.viewport_width * 4);
    if (!source) {
        fprintf(stderr, "SDL_CreateSurfaceFrom atlas failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (!SDL_SaveBMP(source, "build/wolf-wall-atlas-present.bmp")) {
        fprintf(stderr, "SDL_SaveBMP atlas failed: %s\n", SDL_GetError());
        SDL_DestroySurface(source);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (file_stats("build/wolf-wall-atlas-present.bmp", &bmp_size, &bmp_hash) != 0 ||
        bmp_size != 32906 || bmp_hash != 0xaf70162cu) {
        fprintf(stderr, "unexpected atlas screenshot artifact stats\n");
        SDL_DestroySurface(source);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_DestroySurface(source);
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("SDL3 Wolf wall atlas screenshot smoke test passed");
    return 0;
}
