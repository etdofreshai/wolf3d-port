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
#define WL_AUDIO_MAX_CHUNKS 512
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

typedef struct wl_picture_size {
    uint16_t width;
    uint16_t height;
} wl_picture_size;

typedef struct wl_picture_table_metadata {
    size_t picture_count;
    uint16_t min_width;
    uint16_t max_width;
    uint16_t min_height;
    uint16_t max_height;
    uint32_t total_pixels;
    wl_picture_size pictures[WL_GRAPHICS_MAX_CHUNKS];
} wl_picture_table_metadata;

typedef enum wl_surface_format {
    WL_SURFACE_INDEXED8 = 1,
} wl_surface_format;

typedef struct wl_indexed_surface {
    wl_surface_format format;
    uint16_t width;
    uint16_t height;
    uint16_t stride;
    size_t pixel_count;
    unsigned char *pixels;
} wl_indexed_surface;

typedef enum wl_texture_upload_format {
    WL_TEXTURE_UPLOAD_INDEXED8_RGB_PALETTE = 1,
    WL_TEXTURE_UPLOAD_RGBA8888 = 2,
} wl_texture_upload_format;

typedef struct wl_texture_upload_descriptor {
    wl_texture_upload_format format;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    size_t pixel_bytes;
    const unsigned char *pixels;
    const unsigned char *palette;
    size_t palette_entries;
    uint8_t palette_component_bits;
} wl_texture_upload_descriptor;

typedef struct wl_present_frame_descriptor {
    wl_texture_upload_descriptor texture;
    uint16_t viewport_width;
    uint16_t viewport_height;
    uint32_t pixel_hash;
    uint32_t palette_hash;
    uint8_t palette_shift_kind;
    uint8_t palette_shift_index;
} wl_present_frame_descriptor;

enum {
    WL_NUM_RED_SHIFTS = 6,
    WL_RED_SHIFT_STEPS = 8,
    WL_NUM_WHITE_SHIFTS = 3,
    WL_WHITE_SHIFT_STEPS = 20,
    WL_WHITE_SHIFT_TICS = 6,
};

typedef enum wl_palette_shift_kind {
    WL_PALETTE_SHIFT_NONE = 0,
    WL_PALETTE_SHIFT_BASE = 1,
    WL_PALETTE_SHIFT_RED = 2,
    WL_PALETTE_SHIFT_WHITE = 3,
} wl_palette_shift_kind;

typedef struct wl_palette_shift_state {
    int32_t damage_count;
    int32_t bonus_count;
    uint8_t palette_shifted;
} wl_palette_shift_state;

typedef struct wl_palette_shift_result {
    wl_palette_shift_kind kind;
    uint8_t shift_index;
    int32_t damage_count;
    int32_t bonus_count;
    uint8_t palette_shifted;
} wl_palette_shift_result;

int wl_interpolate_palette_range(const unsigned char *from_palette,
                                 const unsigned char *to_palette,
                                 size_t palette_size,
                                 uint8_t palette_component_bits,
                                 uint16_t start_index,
                                 uint16_t end_index,
                                 uint16_t step, uint16_t steps,
                                 unsigned char *out_palette,
                                 size_t out_palette_size);

int wl_build_palette_shift(const unsigned char *base_palette,
                           size_t palette_size,
                           uint8_t palette_component_bits,
                           uint8_t target_red, uint8_t target_green,
                           uint8_t target_blue,
                           uint16_t step, uint16_t steps,
                           unsigned char *out_palette,
                           size_t out_palette_size);

int wl_reset_palette_shift_state(wl_palette_shift_state *state);
int wl_start_bonus_palette_shift(wl_palette_shift_state *state);
int wl_start_damage_palette_shift(wl_palette_shift_state *state,
                                  int32_t damage);
int wl_update_palette_shift_state(wl_palette_shift_state *state, int32_t tics,
                                  wl_palette_shift_result *out);

int wl_select_palette_for_shift(const wl_palette_shift_result *shift,
                                const unsigned char *base_palette,
                                const unsigned char *red_palettes,
                                size_t red_palette_count,
                                const unsigned char *white_palettes,
                                size_t white_palette_count,
                                size_t palette_size,
                                const unsigned char **out_palette);
int wl_describe_palette_shifted_texture_upload(
    const wl_indexed_surface *surface, const wl_palette_shift_result *shift,
    const unsigned char *base_palette, const unsigned char *red_palettes,
    size_t red_palette_count, const unsigned char *white_palettes,
    size_t white_palette_count, size_t palette_size,
    uint8_t palette_component_bits, wl_texture_upload_descriptor *out);

typedef struct wl_wall_page_metadata {
    uint16_t width;
    uint16_t height;
    uint16_t column_count;
    uint16_t bytes_per_column;
    uint16_t min_color;
    uint16_t max_color;
    uint16_t unique_color_count;
} wl_wall_page_metadata;

typedef enum wl_wall_side {
    WL_WALL_SIDE_HORIZONTAL = 0,
    WL_WALL_SIDE_VERTICAL = 1,
} wl_wall_side;

typedef enum wl_cardinal_ray_direction {
    WL_RAY_NORTH = 0,
    WL_RAY_EAST = 1,
    WL_RAY_SOUTH = 2,
    WL_RAY_WEST = 3,
} wl_cardinal_ray_direction;

typedef struct wl_wall_strip {
    const unsigned char *wall_page;
    size_t wall_page_size;
    uint16_t texture_offset;
    uint16_t x;
    uint16_t scaled_height;
} wl_wall_strip;

typedef struct wl_map_wall_hit {
    uint16_t tile_x;
    uint16_t tile_y;
    uint16_t source_tile;
    wl_wall_side side;
    uint16_t wall_page_index;
    uint16_t texture_offset;
    uint32_t distance;
    uint16_t x;
    uint16_t scaled_height;
} wl_map_wall_hit;

typedef struct wl_projected_sprite {
    uint16_t source_index;
    size_t surface_index;
    uint8_t visible;
    int32_t view_x;
    uint16_t scaled_height;
    uint32_t distance;
    int32_t trans_x;
    int32_t trans_y;
} wl_projected_sprite;

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
int wl_decode_wall_page_metadata(const unsigned char *chunk, size_t chunk_size,
                                 wl_wall_page_metadata *out);
int wl_decode_wall_page_to_indexed(const unsigned char *chunk, size_t chunk_size,
                                   unsigned char *indexed, size_t indexed_size);
int wl_decode_wall_page_surface(const unsigned char *chunk, size_t chunk_size,
                                unsigned char *pixels, size_t pixel_size,
                                wl_indexed_surface *out);
int wl_decode_sprite_shape_to_indexed(const unsigned char *chunk, size_t chunk_size,
                                      unsigned char transparent_index,
                                      unsigned char *indexed, size_t indexed_size);
int wl_decode_sprite_shape_surface(const unsigned char *chunk, size_t chunk_size,
                                   unsigned char transparent_index,
                                   unsigned char *pixels, size_t pixel_size,
                                   wl_indexed_surface *out);
int wl_decode_vswap_sprite_surface_cache(const char *path,
                                         const wl_vswap_directory *directory,
                                         const uint16_t *chunk_indices,
                                         size_t surface_count,
                                         unsigned char transparent_index,
                                         unsigned char *pixel_storage,
                                         size_t pixel_storage_size,
                                         wl_indexed_surface *surfaces);
int wl_sample_wall_page_column(const unsigned char *chunk, size_t chunk_size,
                               uint16_t texture_offset, unsigned char *out,
                               size_t out_size);
int wl_sample_indexed_surface_column(const wl_indexed_surface *surface, uint16_t x,
                                     unsigned char *out, size_t out_size);
int wl_scale_wall_column_to_surface(const unsigned char *column, size_t column_size,
                                    wl_indexed_surface *dst, uint16_t x,
                                    uint16_t scaled_height);
int wl_render_scaled_sprite(const wl_indexed_surface *sprite, wl_indexed_surface *dst,
                            int x_center, uint16_t scaled_height,
                            unsigned char transparent_index,
                            const uint16_t *wall_heights, size_t wall_height_count);
int wl_project_world_sprite(uint16_t source_index, uint32_t origin_x, uint32_t origin_y,
                            uint32_t sprite_x, uint32_t sprite_y,
                            int32_t forward_x, int32_t forward_y,
                            uint16_t view_width, uint16_t view_height,
                            wl_projected_sprite *out);
int wl_sort_projected_sprites_far_to_near(wl_projected_sprite *sprites, size_t count);
int wl_render_wall_strip_viewport(const wl_wall_strip *strips, size_t strip_count,
                                  wl_indexed_surface *dst);
int wl_build_map_wall_hit(const uint16_t *wall_plane, size_t wall_count,
                          uint16_t tile_x, uint16_t tile_y, wl_wall_side side,
                          uint16_t texture_column, uint16_t x,
                          uint16_t scaled_height, wl_map_wall_hit *out);
int wl_wall_hit_to_strip(const wl_map_wall_hit *hit, const unsigned char *wall_page,
                         size_t wall_page_size, wl_wall_strip *out);
int wl_cast_cardinal_wall_ray(const uint16_t *wall_plane, size_t wall_count,
                              uint16_t start_tile_x, uint16_t start_tile_y,
                              wl_cardinal_ray_direction direction,
                              uint16_t texture_column, uint16_t x,
                              uint16_t scaled_height, wl_map_wall_hit *out);
int wl_cast_fixed_cardinal_wall_ray(const uint16_t *wall_plane, size_t wall_count,
                                    uint32_t origin_x, uint32_t origin_y,
                                    wl_cardinal_ray_direction direction,
                                    uint16_t x, uint16_t scaled_height,
                                    wl_map_wall_hit *out);
uint16_t wl_project_wall_height(uint32_t forward_distance, uint16_t view_width,
                                uint16_t view_height);
int wl_cast_fixed_wall_ray(const uint16_t *wall_plane, size_t wall_count,
                           uint32_t origin_x, uint32_t origin_y,
                           int32_t direction_x, int32_t direction_y,
                           uint16_t x, uint16_t scaled_height,
                           wl_map_wall_hit *out);
int wl_cast_projected_wall_ray(const uint16_t *wall_plane, size_t wall_count,
                               uint32_t origin_x, uint32_t origin_y,
                               int32_t direction_x, int32_t direction_y,
                               uint16_t x, uint16_t view_width,
                               uint16_t view_height, wl_map_wall_hit *out);
int wl_build_camera_ray_directions(int32_t forward_x, int32_t forward_y,
                                   int32_t plane_x, int32_t plane_y,
                                   uint16_t view_width, uint16_t first_x,
                                   uint16_t x_step, size_t ray_count,
                                   int32_t *out_x, int32_t *out_y);
int wl_cast_projected_wall_ray_batch(const uint16_t *wall_plane, size_t wall_count,
                                     uint32_t origin_x, uint32_t origin_y,
                                     const int32_t *directions_x,
                                     const int32_t *directions_y,
                                     size_t ray_count, uint16_t first_x,
                                     uint16_t x_step, uint16_t view_width,
                                     uint16_t view_height, wl_map_wall_hit *out);
int wl_render_camera_wall_view(const uint16_t *wall_plane, size_t wall_count,
                               uint32_t origin_x, uint32_t origin_y,
                               int32_t forward_x, int32_t forward_y,
                               int32_t plane_x, int32_t plane_y,
                               uint16_t first_x, uint16_t x_step, size_t ray_count,
                               const unsigned char *const *wall_pages,
                               const size_t *wall_page_sizes, size_t wall_page_count,
                               wl_indexed_surface *dst, int32_t *directions_x,
                               int32_t *directions_y, wl_map_wall_hit *hits,
                               wl_wall_strip *strips);
int wl_render_camera_scene_view(const uint16_t *wall_plane, size_t wall_count,
                                uint32_t origin_x, uint32_t origin_y,
                                int32_t forward_x, int32_t forward_y,
                                int32_t plane_x, int32_t plane_y,
                                uint16_t first_x, uint16_t x_step, size_t ray_count,
                                const unsigned char *const *wall_pages,
                                const size_t *wall_page_sizes, size_t wall_page_count,
                                const wl_indexed_surface *const *sprite_surfaces,
                                const uint32_t *sprite_x, const uint32_t *sprite_y,
                                const uint16_t *sprite_source_indices, size_t sprite_count,
                                unsigned char transparent_index, wl_indexed_surface *dst,
                                int32_t *directions_x, int32_t *directions_y,
                                wl_map_wall_hit *hits, wl_wall_strip *strips,
                                wl_projected_sprite *sprites, uint16_t *wall_heights);
int wl_read_graphics_header(const char *path, wl_graphics_header *out);
int wl_read_huffman_dictionary(const char *path, wl_huffman_node nodes[WL_HUFFMAN_NODE_COUNT]);
int wl_huff_expand(const unsigned char *src, size_t src_len,
                   const wl_huffman_node nodes[WL_HUFFMAN_NODE_COUNT],
                   unsigned char *out, size_t out_size, size_t *bytes_consumed);
int wl_read_graphics_chunk(const char *vgagraph_path, const wl_graphics_header *header,
                           const wl_huffman_node nodes[WL_HUFFMAN_NODE_COUNT],
                           size_t chunk_index, unsigned char *out, size_t out_size,
                           size_t *bytes_read, size_t *compressed_size);
int wl_decode_picture_table(const unsigned char *chunk, size_t chunk_size,
                            wl_picture_table_metadata *out);
int wl_wrap_indexed_surface(uint16_t width, uint16_t height,
                            unsigned char *pixels, size_t pixel_size,
                            wl_indexed_surface *out);
int wl_describe_indexed_texture_upload(const wl_indexed_surface *surface,
                                       const unsigned char *palette,
                                       size_t palette_size,
                                       uint8_t palette_component_bits,
                                       wl_texture_upload_descriptor *out);
int wl_expand_indexed_surface_to_rgba(const wl_indexed_surface *surface,
                                      const unsigned char *palette,
                                      size_t palette_size,
                                      uint8_t palette_component_bits,
                                      unsigned char *rgba, size_t rgba_size,
                                      wl_texture_upload_descriptor *out);
int wl_describe_present_frame(const wl_indexed_surface *surface,
                              const wl_palette_shift_result *shift,
                              const unsigned char *base_palette,
                              const unsigned char *red_palettes,
                              size_t red_palette_count,
                              const unsigned char *white_palettes,
                              size_t white_palette_count,
                              size_t palette_size,
                              uint8_t palette_component_bits,
                              wl_present_frame_descriptor *out);
int wl_expand_present_frame_to_rgba(const wl_present_frame_descriptor *present,
                                    unsigned char *rgba, size_t rgba_size,
                                    wl_texture_upload_descriptor *out);
int wl_decode_planar_picture_to_indexed(const unsigned char *planar, size_t planar_size,
                                        uint16_t width, uint16_t height,
                                        unsigned char *indexed, size_t indexed_size);
int wl_decode_planar_picture_surface(const unsigned char *planar, size_t planar_size,
                                     uint16_t width, uint16_t height,
                                     unsigned char *pixels, size_t pixel_size,
                                     wl_indexed_surface *out);
int wl_blit_indexed_surface(const wl_indexed_surface *src, wl_indexed_surface *dst,
                            int dst_x, int dst_y);
int wl_carmack_expand(const unsigned char *src, size_t src_len, size_t expanded_bytes,
                      uint16_t *out, size_t out_words, size_t *words_written);
int wl_rlew_expand(const uint16_t *src, size_t src_words, uint16_t rlew_tag,
                   size_t expanded_bytes, uint16_t *out, size_t out_words,
                   size_t *words_written);
int wl_read_map_plane(const char *gamemaps_path, const wl_map_header *header,
                      size_t plane_index, uint16_t rlew_tag,
                      uint16_t *out, size_t out_words);

typedef struct wl_audio_header {
    size_t chunk_count;
    uint32_t offsets[WL_AUDIO_MAX_CHUNKS + 1];
    size_t file_size;
} wl_audio_header;

typedef enum wl_audio_chunk_kind {
    WL_AUDIO_CHUNK_PC_SPEAKER = 1,
    WL_AUDIO_CHUNK_ADLIB = 2,
    WL_AUDIO_CHUNK_DIGITAL = 3,
    WL_AUDIO_CHUNK_MUSIC = 4,
} wl_audio_chunk_kind;

typedef struct wl_audio_chunk_metadata {
    wl_audio_chunk_kind kind;
    uint8_t is_empty;
    size_t raw_size;
    uint32_t declared_length;
    uint16_t priority;
    size_t payload_offset;
    size_t payload_size;
} wl_audio_chunk_metadata;

typedef struct wl_audio_range_summary {
    size_t start_chunk;
    size_t chunk_count;
    size_t non_empty_chunks;
    size_t total_bytes;
    size_t first_non_empty_chunk;
    size_t last_non_empty_chunk;
    size_t largest_chunk;
    size_t largest_chunk_bytes;
} wl_audio_range_summary;

typedef struct wl_sound_priority_decision {
    uint8_t current_active;
    uint16_t current_priority;
    uint16_t candidate_priority;
    uint8_t should_start;
} wl_sound_priority_decision;

typedef struct wl_sound_channel_decision {
    uint8_t current_active;
    size_t current_chunk;
    uint16_t current_priority;
    size_t candidate_chunk;
    wl_audio_chunk_kind candidate_kind;
    uint16_t candidate_priority;
    uint8_t should_start;
    uint8_t next_active;
    size_t next_chunk;
    uint16_t next_priority;
} wl_sound_channel_decision;

typedef struct wl_sound_channel_state {
    uint8_t active;
    uint16_t sound_index;
    uint16_t priority;
    size_t sample_position;
} wl_sound_channel_state;

typedef struct wl_sound_channel_start_result {
    wl_sound_channel_state state;
    uint8_t started;
    uint8_t replaced;
} wl_sound_channel_start_result;

typedef struct wl_sound_channel_schedule_result {
    wl_sound_channel_state state;
    wl_audio_chunk_kind candidate_kind;
    uint16_t candidate_priority;
    uint8_t started;
    uint8_t replaced;
    uint8_t held;
} wl_sound_channel_schedule_result;

typedef struct wl_sound_channel_advance_result {
    wl_sound_channel_state state;
    uint32_t samples_consumed;
    uint8_t completed;
} wl_sound_channel_advance_result;

typedef struct wl_sound_channel_tick_result {
    wl_sound_channel_state state;
    uint32_t samples_consumed;
    uint8_t current_sample;
    uint8_t completed;
} wl_sound_channel_tick_result;

typedef struct wl_pc_speaker_sound_metadata {
    uint32_t sample_count;
    uint8_t first_sample;
    uint8_t last_sample;
    uint8_t terminator;
    size_t trailing_bytes;
} wl_pc_speaker_sound_metadata;

typedef struct wl_pc_speaker_playback_cursor {
    size_t sample_index;
    uint32_t samples_consumed;
    uint8_t current_sample;
    uint8_t completed;
} wl_pc_speaker_playback_cursor;

typedef struct wl_adlib_sound_metadata {
    uint32_t sample_count;
    uint16_t priority;
    uint8_t instrument_bytes;
    uint8_t first_instrument_byte;
    uint8_t last_instrument_byte;
    uint8_t first_sample;
    uint8_t last_sample;
    size_t trailing_bytes;
} wl_adlib_sound_metadata;

typedef struct wl_adlib_instrument_registers {
    uint8_t modulator_regs[5];
    uint8_t modulator_values[5];
    uint8_t carrier_regs[5];
    uint8_t carrier_values[5];
    uint8_t feedback_reg;
    uint8_t feedback_value;
    uint8_t voice;
    uint8_t mode;
} wl_adlib_instrument_registers;

typedef struct wl_adlib_playback_cursor {
    size_t sample_index;
    uint32_t samples_consumed;
    uint8_t current_sample;
    uint8_t completed;
} wl_adlib_playback_cursor;

typedef struct wl_sample_playback_window {
    size_t start_sample;
    size_t samples_available;
    size_t samples_in_window;
    size_t next_sample;
    uint8_t first_sample;
    uint8_t last_sample;
    uint8_t completed;
} wl_sample_playback_window;

typedef struct wl_sample_playback_position {
    size_t sample_position;
    size_t sample_count;
    uint8_t current_sample;
    uint8_t completed;
} wl_sample_playback_position;

typedef struct wl_imf_music_command {
    uint8_t reg;
    uint8_t value;
    uint16_t delay;
} wl_imf_music_command;

typedef struct wl_imf_music_metadata {
    uint32_t declared_bytes;
    size_t command_count;
    uint8_t first_register;
    uint8_t first_value;
    uint16_t first_delay;
    uint8_t last_register;
    uint8_t last_value;
    uint16_t last_delay;
    uint16_t max_delay;
    size_t zero_delay_count;
    uint32_t total_delay;
    size_t trailing_bytes;
} wl_imf_music_metadata;

typedef struct wl_imf_playback_window {
    size_t start_command;
    size_t commands_available;
    size_t commands_in_window;
    size_t next_command;
    uint32_t elapsed_delay;
    uint8_t completed;
} wl_imf_playback_window;

typedef struct wl_imf_playback_position {
    uint32_t tick_position;
    size_t command_index;
    uint16_t command_delay;
    uint16_t delay_elapsed;
    uint16_t delay_remaining;
    uint8_t completed;
} wl_imf_playback_position;

typedef struct wl_imf_playback_cursor {
    size_t command_index;
    uint16_t command_delay;
    uint16_t delay_elapsed;
    uint16_t delay_remaining;
    uint32_t ticks_consumed;
    size_t commands_advanced;
    uint8_t completed;
} wl_imf_playback_cursor;

int wl_read_audio_header(const char *path, wl_audio_header *out);
int wl_read_audio_chunk(const char *audiot_path,
                        const wl_audio_header *header,
                        size_t chunk_index,
                        unsigned char *out, size_t out_size,
                        size_t *bytes_read);
int wl_describe_audio_chunk(size_t chunk_index,
                            const unsigned char *chunk, size_t chunk_size,
                            wl_audio_chunk_metadata *out);
int wl_describe_audio_chunk_with_ranges(size_t chunk_index,
                                        size_t pc_speaker_count,
                                        size_t adlib_count,
                                        size_t digital_count,
                                        const unsigned char *chunk,
                                        size_t chunk_size,
                                        wl_audio_chunk_metadata *out);
int wl_summarize_audio_range(const wl_audio_header *header,
                             size_t start_chunk, size_t chunk_count,
                             wl_audio_range_summary *out);
int wl_describe_sound_priority_decision(uint8_t current_active,
                                        uint16_t current_priority,
                                        uint16_t candidate_priority,
                                        wl_sound_priority_decision *out);
int wl_describe_sound_channel_decision(uint8_t current_active,
                                       size_t current_chunk,
                                       uint16_t current_priority,
                                       size_t candidate_chunk,
                                       const wl_audio_chunk_metadata *candidate,
                                       wl_sound_channel_decision *out);
int wl_start_sound_channel(const wl_sound_channel_state *current,
                           uint16_t candidate_sound_index,
                           uint16_t candidate_priority,
                           wl_sound_channel_start_result *out);
int wl_start_sound_channel_from_chunk(const wl_sound_channel_state *current,
                                      size_t candidate_chunk,
                                      const wl_audio_chunk_metadata *candidate,
                                      wl_sound_channel_start_result *out);
int wl_schedule_sound_channel(const wl_sound_channel_state *current,
                              uint16_t candidate_sound_index,
                              const wl_audio_chunk_metadata *candidate,
                              wl_sound_channel_schedule_result *out);
int wl_schedule_sound_channel_from_chunk(const wl_sound_channel_state *current,
                                         size_t candidate_chunk,
                                         const wl_audio_chunk_metadata *candidate,
                                         wl_sound_channel_schedule_result *out);
int wl_advance_sound_channel(const wl_sound_channel_state *current,
                             size_t sample_count,
                             uint32_t sample_delta,
                             wl_sound_channel_advance_result *out);
int wl_tick_sound_channel(const wl_sound_channel_state *current,
                          wl_audio_chunk_kind kind,
                          const unsigned char *chunk, size_t chunk_size,
                          uint32_t sample_delta,
                          wl_sound_channel_tick_result *out);
int wl_describe_pc_speaker_sound(const unsigned char *chunk, size_t chunk_size,
                                  wl_pc_speaker_sound_metadata *out);
int wl_get_pc_speaker_sound_sample(const unsigned char *chunk, size_t chunk_size,
                                   size_t sample_index, uint8_t *out_sample);
int wl_advance_pc_speaker_playback_cursor(const unsigned char *chunk, size_t chunk_size,
                                          size_t start_sample, uint32_t sample_delta,
                                          wl_pc_speaker_playback_cursor *out);
int wl_describe_pc_speaker_playback_window(const unsigned char *chunk, size_t chunk_size,
                                           size_t start_sample, size_t sample_budget,
                                           wl_sample_playback_window *out);
int wl_describe_pc_speaker_playback_position(const unsigned char *chunk, size_t chunk_size,
                                             size_t sample_position,
                                             wl_sample_playback_position *out);
int wl_describe_adlib_sound(const unsigned char *chunk, size_t chunk_size,
                            wl_adlib_sound_metadata *out);
int wl_describe_adlib_instrument_registers(const unsigned char *chunk, size_t chunk_size,
                                           wl_adlib_instrument_registers *out);
int wl_get_adlib_instrument_byte(const unsigned char *chunk, size_t chunk_size,
                                 size_t instrument_index, uint8_t *out_byte);
int wl_get_adlib_sound_sample(const unsigned char *chunk, size_t chunk_size,
                              size_t sample_index, uint8_t *out_sample);
int wl_advance_adlib_playback_cursor(const unsigned char *chunk, size_t chunk_size,
                                     size_t start_sample, uint32_t sample_delta,
                                     wl_adlib_playback_cursor *out);
int wl_describe_adlib_playback_window(const unsigned char *chunk, size_t chunk_size,
                                      size_t start_sample, size_t sample_budget,
                                      wl_sample_playback_window *out);
int wl_describe_adlib_playback_position(const unsigned char *chunk, size_t chunk_size,
                                        size_t sample_position,
                                        wl_sample_playback_position *out);
int wl_describe_imf_music_chunk(const unsigned char *chunk, size_t chunk_size,
                                wl_imf_music_metadata *out);
int wl_get_imf_music_command(const unsigned char *chunk, size_t chunk_size,
                             size_t command_index, wl_imf_music_command *out);
int wl_describe_imf_playback_window(const unsigned char *chunk, size_t chunk_size,
                                    size_t start_command, uint32_t tick_budget,
                                    wl_imf_playback_window *out);
int wl_describe_imf_playback_position(const unsigned char *chunk, size_t chunk_size,
                                      uint32_t tick_position,
                                      wl_imf_playback_position *out);
int wl_advance_imf_playback_cursor(const unsigned char *chunk, size_t chunk_size,
                                   size_t start_command, uint16_t start_delay_elapsed,
                                   uint32_t tick_delta,
                                   wl_imf_playback_cursor *out);

#ifdef __cplusplus
}
#endif

#endif /* WL_ASSETS_H */
