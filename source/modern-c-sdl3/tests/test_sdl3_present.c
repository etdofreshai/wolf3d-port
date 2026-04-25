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

int main(void) {
    char vswap_path[512];
    wl_vswap_directory directory;
    unsigned char chunk[4096];
    size_t chunk_size = 0;
    unsigned char wall_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface wall;
    unsigned char palette[256 * 3];
    unsigned char rgba[WL_MAP_PLANE_WORDS * 4];
    wl_palette_shift_result shift;
    wl_present_frame_descriptor present;

    for (size_t i = 0; i < 256; ++i) {
        palette[i * 3u + 0u] = (unsigned char)(i & 63u);
        palette[i * 3u + 1u] = (unsigned char)((i * 2u) & 63u);
        palette[i * 3u + 2u] = (unsigned char)((63u - i) & 63u);
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

    SDL_DestroySurface(source);
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("SDL3 Wolf wall present smoke test passed");
    return 0;
}
