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
#define WL_VSWAP_MAX_CHUNKS 2048
#define WL_GRAPHICS_MAX_CHUNKS 2048
#define WL_HUFFMAN_NODE_COUNT 255

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

typedef enum wl_vswap_chunk_kind {
    WL_VSWAP_CHUNK_WALL,
    WL_VSWAP_CHUNK_SPRITE,
    WL_VSWAP_CHUNK_SOUND,
    WL_VSWAP_CHUNK_SPARSE,
} wl_vswap_chunk_kind;

typedef struct wl_vswap_header {
    uint16_t chunks_in_file;
    uint16_t sprite_start;
    uint16_t sound_start;
    uint32_t first_offsets[5];
    size_t file_size;
} wl_vswap_header;

typedef struct wl_vswap_chunk {
    uint32_t offset;
    uint16_t length;
    wl_vswap_chunk_kind kind;
} wl_vswap_chunk;

typedef struct wl_vswap_directory {
    wl_vswap_header header;
    wl_vswap_chunk chunks[WL_VSWAP_MAX_CHUNKS];
    size_t data_start;
    size_t max_chunk_end;
    size_t wall_count;
    size_t sprite_count;
    size_t sound_count;
    size_t sparse_count;
} wl_vswap_directory;

typedef struct wl_huffman_node {
    uint16_t bit0;
    uint16_t bit1;
} wl_huffman_node;

typedef struct wl_graphics_header {
    size_t chunk_count;
    int32_t offsets[WL_GRAPHICS_MAX_CHUNKS + 1];
    size_t file_size;
} wl_graphics_header;

typedef struct wl_vswap_shape_metadata {
    wl_vswap_chunk_kind kind;
    uint16_t width;
    uint16_t height;
    uint16_t leftpix;
    uint16_t rightpix;
    uint16_t visible_columns;
    uint16_t first_column_offset;
    uint16_t last_column_offset;
    uint16_t min_column_offset;
    uint16_t max_column_offset;
    uint16_t post_count;
    uint16_t terminal_count;
    uint16_t min_posts_per_column;
    uint16_t max_posts_per_column;
    uint16_t min_post_span;
    uint16_t max_post_span;
    uint16_t max_post_start;
    uint16_t max_post_end;
    uint16_t min_source_offset;
    uint16_t max_source_offset;
    uint32_t total_post_span;
} wl_vswap_shape_metadata;

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
int wl_read_vswap_directory(const char *path, wl_vswap_directory *out);
int wl_read_vswap_chunk(const char *path, const wl_vswap_directory *directory,
                        size_t chunk_index, unsigned char *out, size_t out_size,
                        size_t *bytes_read);
int wl_decode_vswap_shape_metadata(const unsigned char *chunk, size_t chunk_size,
                                   wl_vswap_chunk_kind kind,
                                   wl_vswap_shape_metadata *out);
int wl_read_graphics_header(const char *path, wl_graphics_header *out);
int wl_read_huffman_dictionary(const char *path, wl_huffman_node nodes[WL_HUFFMAN_NODE_COUNT]);
int wl_huff_expand(const unsigned char *src, size_t src_len,
                   const wl_huffman_node nodes[WL_HUFFMAN_NODE_COUNT],
                   unsigned char *out, size_t out_size, size_t *bytes_consumed);
int wl_read_graphics_chunk(const char *vgagraph_path, const wl_graphics_header *header,
                           const wl_huffman_node nodes[WL_HUFFMAN_NODE_COUNT],
                           size_t chunk_index, unsigned char *out, size_t out_size,
                           size_t *bytes_read, size_t *compressed_size);
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
