#include <stdint.h>
#include <stdio.h>

#include <SDL3/SDL.h>

static uint32_t fnv1a_bytes(const unsigned char *data, size_t size) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        h ^= (uint32_t)data[i];
        h *= 16777619u;
    }
    return h;
}

int main(void) {
    static const unsigned char indexed[16] = {
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 10, 11,
        12, 13, 14, 15,
    };
    static const unsigned char palette[16 * 3] = {
        0, 0, 0,      4, 0, 0,      8, 0, 0,      12, 0, 0,
        0, 4, 0,      0, 8, 0,      0, 12, 0,     4, 4, 0,
        0, 0, 4,      0, 0, 8,      0, 0, 12,     4, 0, 4,
        4, 4, 4,      8, 8, 8,      12, 12, 12,   63, 63, 63,
    };
    unsigned char rgba[4 * 4 * 4];

    for (size_t i = 0; i < sizeof(indexed); ++i) {
        const unsigned char *rgb = palette + (size_t)indexed[i] * 3u;
        rgba[i * 4u + 0u] = (unsigned char)((rgb[0] << 2) | (rgb[0] >> 4));
        rgba[i * 4u + 1u] = (unsigned char)((rgb[1] << 2) | (rgb[1] >> 4));
        rgba[i * 4u + 2u] = (unsigned char)((rgb[2] << 2) | (rgb[2] >> 4));
        rgba[i * 4u + 3u] = 255;
    }

    if (fnv1a_bytes(rgba, sizeof(rgba)) != 0xa25957c4u) {
        fprintf(stderr, "unexpected RGBA source hash\n");
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("wolf3d-sdl3-present", 4, 4, SDL_WINDOW_HIDDEN);
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

    SDL_Surface *source = SDL_CreateSurfaceFrom(4, 4, SDL_PIXELFORMAT_RGBA32,
                                               rgba, 4 * 4);
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
    puts("SDL3 present smoke test passed");
    return 0;
}
