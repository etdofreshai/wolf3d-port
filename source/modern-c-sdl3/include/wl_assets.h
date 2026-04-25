#ifndef WL_ASSETS_H
#define WL_ASSETS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WL_MAPHEAD_OFFSET_COUNT 100
#define WL_MAP_NAME_SIZE 16
#define WL_MAP_PLANE_COUNT 3
#define WL_MAP_SIDE 64
#define WL_MAP_PLANE_WORDS (WL_MAP_SIDE * WL_MAP_SIDE)

typedef struct wl_maphead {
    uint16_t rlew_tag;
    uint32_t offsets[WL_MAPHEAD_OFFSET_COUNT];
    size_t file_size;
} wl_maphead;

typedef struct wl_map_header {
    uint32_t plane_starts[3];
    uint16_t plane_lengths[3];
    uint16_t width;
    uint16_t height;
    char name[WL_MAP_NAME_SIZE + 1];
} wl_map_header;

typedef struct wl_vswap_header {
    uint16_t chunks_in_file;
    uint16_t sprite_start;
    uint16_t sound_start;
    uint32_t first_offsets[5];
    size_t file_size;
} wl_vswap_header;

typedef struct wl_required_file {
    const char *name;
    size_t expected_size;
} wl_required_file;

const char *wl_default_data_dir(void);
int wl_join_path(char *out, size_t out_size, const char *dir, const char *file_name);
int wl_file_size(const char *path, size_t *size_out);
int wl_read_maphead(const char *path, wl_maphead *out);
int wl_read_map_header(const char *gamemaps_path, uint32_t offset, wl_map_header *out);
int wl_read_vswap_header(const char *path, wl_vswap_header *out);
int wl_carmack_expand(const unsigned char *src, size_t src_len, size_t expanded_bytes,
                      uint16_t *out, size_t out_words, size_t *words_written);
int wl_rlew_expand(const uint16_t *src, size_t src_words, uint16_t rlew_tag,
                   size_t expanded_bytes, uint16_t *out, size_t out_words,
                   size_t *words_written);
int wl_read_map_plane(const char *gamemaps_path, const wl_map_header *header,
                      size_t plane_index, uint16_t rlew_tag,
                      uint16_t *out, size_t out_words);

#ifdef __cplusplus
}
#endif

#endif /* WL_ASSETS_H */
