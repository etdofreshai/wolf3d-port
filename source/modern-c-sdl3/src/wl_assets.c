#include "wl_assets.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t read_le16(const unsigned char *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int wl_data_dir_has_wl6(const char *dir) {
    char path[512];
    if (!dir) {
        return 0;
    }
    int n = snprintf(path, sizeof(path), "%s/%s", dir, "MAPHEAD.WL6");
    if (n < 0 || (size_t)n >= sizeof(path)) {
        return 0;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        return 0;
    }
    fclose(f);
    return 1;
}

const char *wl_default_data_dir(void) {
    const char *env = getenv("WOLF3D_DATA_DIR");
    if (env && env[0]) {
        return env;
    }

    static const char *candidates[] = {
        "game-files/base",
        "../../game-files/base",
        "../../../game-files/base",
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (wl_data_dir_has_wl6(candidates[i])) {
            return candidates[i];
        }
    }
    return candidates[0];
}

int wl_join_path(char *out, size_t out_size, const char *dir, const char *file_name) {
    if (!out || out_size == 0 || !dir || !file_name) {
        return -1;
    }
    int n = snprintf(out, out_size, "%s/%s", dir, file_name);
    return (n >= 0 && (size_t)n < out_size) ? 0 : -1;
}

int wl_file_size(const char *path, size_t *size_out) {
    if (!path || !size_out) {
        return -1;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long n = ftell(f);
    fclose(f);
    if (n < 0) {
        return -1;
    }
    *size_out = (size_t)n;
    return 0;
}

static int read_exact_at(const char *path, long offset, void *buf, size_t len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, offset, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    size_t got = fread(buf, 1, len, f);
    fclose(f);
    return got == len ? 0 : -1;
}

int wl_read_maphead(const char *path, wl_maphead *out) {
    if (!path || !out) {
        return -1;
    }

    size_t size = 0;
    if (wl_file_size(path, &size) != 0) {
        return -1;
    }
    if (size < 2 + WL_MAPHEAD_OFFSET_COUNT * 4) {
        return -1;
    }

    unsigned char buf[2 + WL_MAPHEAD_OFFSET_COUNT * 4];
    if (read_exact_at(path, 0, buf, sizeof(buf)) != 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->file_size = size;
    out->rlew_tag = read_le16(buf);
    for (size_t i = 0; i < WL_MAPHEAD_OFFSET_COUNT; ++i) {
        out->offsets[i] = read_le32(buf + 2 + i * 4);
    }
    return 0;
}

int wl_read_map_header(const char *gamemaps_path, uint32_t offset, wl_map_header *out) {
    if (!gamemaps_path || !out) {
        return -1;
    }

    unsigned char buf[38];
    if (read_exact_at(gamemaps_path, (long)offset, buf, sizeof(buf)) != 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < 3; ++i) {
        out->plane_starts[i] = read_le32(buf + i * 4);
    }
    for (size_t i = 0; i < 3; ++i) {
        out->plane_lengths[i] = read_le16(buf + 12 + i * 2);
    }
    out->width = read_le16(buf + 18);
    out->height = read_le16(buf + 20);
    memcpy(out->name, buf + 22, WL_MAP_NAME_SIZE);
    out->name[WL_MAP_NAME_SIZE] = '\0';
    return 0;
}


int wl_read_graphics_header(const char *path, wl_graphics_header *out) {
    if (!path || !out) {
        return -1;
    }

    size_t size = 0;
    if (wl_file_size(path, &size) != 0 || size < 6 || (size % 3) != 0) {
        return -1;
    }
    size_t entries = size / 3;
    if (entries < 2 || entries > WL_GRAPHICS_MAX_CHUNKS + 1) {
        return -1;
    }

    unsigned char *buf = (unsigned char *)malloc(size);
    if (!buf) {
        return -1;
    }
    if (read_exact_at(path, 0, buf, size) != 0) {
        free(buf);
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->chunk_count = entries - 1;
    out->file_size = size;
    for (size_t i = 0; i < entries; ++i) {
        uint32_t offset = (uint32_t)buf[i * 3] | ((uint32_t)buf[i * 3 + 1] << 8) |
                          ((uint32_t)buf[i * 3 + 2] << 16);
        out->offsets[i] = (offset == 0x00ffffffu) ? -1 : (int32_t)offset;
    }
    free(buf);
    return 0;
}

int wl_read_huffman_dictionary(const char *path, wl_huffman_node nodes[WL_HUFFMAN_NODE_COUNT]) {
    if (!path || !nodes) {
        return -1;
    }

    unsigned char buf[WL_HUFFMAN_NODE_COUNT * 4];
    if (read_exact_at(path, 0, buf, sizeof(buf)) != 0) {
        return -1;
    }
    for (size_t i = 0; i < WL_HUFFMAN_NODE_COUNT; ++i) {
        nodes[i].bit0 = read_le16(buf + i * 4);
        nodes[i].bit1 = read_le16(buf + i * 4 + 2);
        if ((nodes[i].bit0 >= 256 && nodes[i].bit0 >= 256 + WL_HUFFMAN_NODE_COUNT) ||
            (nodes[i].bit1 >= 256 && nodes[i].bit1 >= 256 + WL_HUFFMAN_NODE_COUNT)) {
            return -1;
        }
    }
    return 0;
}

int wl_huff_expand(const unsigned char *src, size_t src_len,
                   const wl_huffman_node nodes[WL_HUFFMAN_NODE_COUNT],
                   unsigned char *out, size_t out_size, size_t *bytes_consumed) {
    if (!src || !nodes || !out) {
        return -1;
    }
    if (out_size == 0) {
        if (bytes_consumed) {
            *bytes_consumed = 0;
        }
        return 0;
    }
    if (src_len == 0) {
        return -1;
    }

    size_t src_pos = 1;
    unsigned char current = src[0];
    unsigned char mask = 1;
    uint16_t node = WL_HUFFMAN_NODE_COUNT - 1;
    size_t out_pos = 0;
    while (out_pos < out_size) {
        uint16_t code = (current & mask) ? nodes[node].bit1 : nodes[node].bit0;
        mask = (unsigned char)(mask << 1);
        if (mask == 0) {
            if (src_pos >= src_len) {
                return -1;
            }
            current = src[src_pos++];
            mask = 1;
        }

        if (code < 256) {
            out[out_pos++] = (unsigned char)code;
            node = WL_HUFFMAN_NODE_COUNT - 1;
        } else {
            code = (uint16_t)(code - 256);
            if (code >= WL_HUFFMAN_NODE_COUNT) {
                return -1;
            }
            node = code;
        }
    }
    if (bytes_consumed) {
        *bytes_consumed = src_pos;
    }
    return 0;
}

int wl_read_graphics_chunk(const char *vgagraph_path, const wl_graphics_header *header,
                           const wl_huffman_node nodes[WL_HUFFMAN_NODE_COUNT],
                           size_t chunk_index, unsigned char *out, size_t out_size,
                           size_t *bytes_read, size_t *compressed_size) {
    if (!vgagraph_path || !header || !nodes || !out || chunk_index >= header->chunk_count) {
        return -1;
    }
    int32_t pos = header->offsets[chunk_index];
    if (pos < 0) {
        if (bytes_read) {
            *bytes_read = 0;
        }
        if (compressed_size) {
            *compressed_size = 0;
        }
        return 0;
    }

    size_t next = chunk_index + 1;
    while (next <= header->chunk_count && header->offsets[next] < 0) {
        ++next;
    }
    if (next > header->chunk_count || header->offsets[next] < pos) {
        return -1;
    }

    size_t compressed = (size_t)(header->offsets[next] - pos);
    if (compressed < sizeof(uint32_t)) {
        return -1;
    }
    unsigned char *buf = (unsigned char *)malloc(compressed);
    if (!buf) {
        return -1;
    }
    if (read_exact_at(vgagraph_path, pos, buf, compressed) != 0) {
        free(buf);
        return -1;
    }

    uint32_t expanded = read_le32(buf);
    if (expanded > out_size) {
        free(buf);
        return -1;
    }
    size_t consumed = 0;
    int rc = wl_huff_expand(buf + sizeof(uint32_t), compressed - sizeof(uint32_t), nodes,
                            out, expanded, &consumed);
    free(buf);
    if (rc != 0) {
        return -1;
    }
    if (bytes_read) {
        *bytes_read = expanded;
    }
    if (compressed_size) {
        *compressed_size = compressed;
    }
    return 0;
}


int wl_decode_picture_table(const unsigned char *chunk, size_t chunk_size,
                            wl_picture_table_metadata *out) {
    if (!chunk || !out || chunk_size == 0 || (chunk_size % 4) != 0) {
        return -1;
    }

    size_t count = chunk_size / 4;
    if (count > WL_GRAPHICS_MAX_CHUNKS) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->picture_count = count;
    out->min_width = UINT16_MAX;
    out->min_height = UINT16_MAX;
    for (size_t i = 0; i < count; ++i) {
        uint16_t width = read_le16(chunk + i * 4);
        uint16_t height = read_le16(chunk + i * 4 + 2);
        if (width == 0 || height == 0) {
            return -1;
        }
        out->pictures[i].width = width;
        out->pictures[i].height = height;
        if (width < out->min_width) {
            out->min_width = width;
        }
        if (width > out->max_width) {
            out->max_width = width;
        }
        if (height < out->min_height) {
            out->min_height = height;
        }
        if (height > out->max_height) {
            out->max_height = height;
        }
        out->total_pixels += (uint32_t)width * (uint32_t)height;
    }
    return 0;
}


int wl_wrap_indexed_surface(uint16_t width, uint16_t height,
                            unsigned char *pixels, size_t pixel_size,
                            wl_indexed_surface *out) {
    if (!pixels || !out || width == 0 || height == 0) {
        return -1;
    }
    size_t count = (size_t)width * (size_t)height;
    if (pixel_size < count) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->format = WL_SURFACE_INDEXED8;
    out->width = width;
    out->height = height;
    out->stride = width;
    out->pixel_count = count;
    out->pixels = pixels;
    return 0;
}

static unsigned char expand_palette_component(unsigned char value, uint8_t bits) {
    if (bits == 6) {
        return (unsigned char)((value << 2) | (value >> 4));
    }
    return value;
}

int wl_interpolate_palette_range(const unsigned char *from_palette,
                                 const unsigned char *to_palette,
                                 size_t palette_size,
                                 uint8_t palette_component_bits,
                                 uint16_t start_index,
                                 uint16_t end_index,
                                 uint16_t step, uint16_t steps,
                                 unsigned char *out_palette,
                                 size_t out_palette_size) {
    if (!from_palette || !to_palette || !out_palette ||
        (palette_component_bits != 6 && palette_component_bits != 8) ||
        palette_size < 256u * 3u || out_palette_size < 256u * 3u ||
        start_index > end_index || end_index >= 256 || steps == 0 ||
        step > steps) {
        return -1;
    }

    memcpy(out_palette, from_palette, 256u * 3u);
    for (uint16_t index = start_index; index <= end_index; ++index) {
        size_t base = (size_t)index * 3u;
        for (size_t channel = 0; channel < 3u; ++channel) {
            int from = from_palette[base + channel];
            int to = to_palette[base + channel];
            int delta = to - from;
            out_palette[base + channel] =
                (unsigned char)(from + (delta * (int)step) / (int)steps);
        }
    }
    return 0;
}

int wl_build_palette_shift(const unsigned char *base_palette,
                           size_t palette_size,
                           uint8_t palette_component_bits,
                           uint8_t target_red, uint8_t target_green,
                           uint8_t target_blue,
                           uint16_t step, uint16_t steps,
                           unsigned char *out_palette,
                           size_t out_palette_size) {
    if (!base_palette || !out_palette ||
        (palette_component_bits != 6 && palette_component_bits != 8) ||
        palette_size < 256u * 3u || out_palette_size < 256u * 3u ||
        steps == 0 || step > steps) {
        return -1;
    }

    unsigned int max_target = palette_component_bits == 6 ? 64u : 255u;
    if (target_red > max_target || target_green > max_target ||
        target_blue > max_target) {
        return -1;
    }

    const int targets[3] = {
        (int)target_red,
        (int)target_green,
        (int)target_blue,
    };
    for (size_t index = 0; index < 256u; ++index) {
        size_t base = index * 3u;
        for (size_t channel = 0; channel < 3u; ++channel) {
            int from = base_palette[base + channel];
            int delta = targets[channel] - from;
            out_palette[base + channel] =
                (unsigned char)(from + (delta * (int)step) / (int)steps);
        }
    }
    return 0;
}


int wl_reset_palette_shift_state(wl_palette_shift_state *state) {
    if (!state) {
        return -1;
    }
    memset(state, 0, sizeof(*state));
    return 0;
}

int wl_start_bonus_palette_shift(wl_palette_shift_state *state) {
    if (!state) {
        return -1;
    }
    state->bonus_count = WL_NUM_WHITE_SHIFTS * WL_WHITE_SHIFT_TICS;
    return 0;
}

int wl_start_damage_palette_shift(wl_palette_shift_state *state,
                                  int32_t damage) {
    if (!state || damage < 0) {
        return -1;
    }
    state->damage_count += damage;
    return 0;
}

int wl_update_palette_shift_state(wl_palette_shift_state *state, int32_t tics,
                                  wl_palette_shift_result *out) {
    if (!state || !out || tics < 0) {
        return -1;
    }

    int white = 0;
    if (state->bonus_count > 0) {
        white = state->bonus_count / WL_WHITE_SHIFT_TICS + 1;
        if (white > WL_NUM_WHITE_SHIFTS) {
            white = WL_NUM_WHITE_SHIFTS;
        }
        state->bonus_count -= tics;
        if (state->bonus_count < 0) {
            state->bonus_count = 0;
        }
    }

    int red = 0;
    if (state->damage_count > 0) {
        red = state->damage_count / 10 + 1;
        if (red > WL_NUM_RED_SHIFTS) {
            red = WL_NUM_RED_SHIFTS;
        }
        state->damage_count -= tics;
        if (state->damage_count < 0) {
            state->damage_count = 0;
        }
    }

    memset(out, 0, sizeof(*out));
    if (red > 0) {
        out->kind = WL_PALETTE_SHIFT_RED;
        out->shift_index = (uint8_t)(red - 1);
        state->palette_shifted = 1;
    } else if (white > 0) {
        out->kind = WL_PALETTE_SHIFT_WHITE;
        out->shift_index = (uint8_t)(white - 1);
        state->palette_shifted = 1;
    } else if (state->palette_shifted) {
        out->kind = WL_PALETTE_SHIFT_BASE;
        state->palette_shifted = 0;
    } else {
        out->kind = WL_PALETTE_SHIFT_NONE;
    }
    out->damage_count = state->damage_count;
    out->bonus_count = state->bonus_count;
    out->palette_shifted = state->palette_shifted;
    return 0;
}


int wl_select_palette_for_shift(const wl_palette_shift_result *shift,
                                const unsigned char *base_palette,
                                const unsigned char *red_palettes,
                                size_t red_palette_count,
                                const unsigned char *white_palettes,
                                size_t white_palette_count,
                                size_t palette_size,
                                const unsigned char **out_palette) {
    if (!shift || !base_palette || !out_palette || palette_size < 256u * 3u) {
        return -1;
    }

    switch (shift->kind) {
    case WL_PALETTE_SHIFT_NONE:
    case WL_PALETTE_SHIFT_BASE:
        *out_palette = base_palette;
        return 0;
    case WL_PALETTE_SHIFT_RED:
        if (!red_palettes || shift->shift_index >= red_palette_count) {
            return -1;
        }
        *out_palette = red_palettes + (size_t)shift->shift_index * palette_size;
        return 0;
    case WL_PALETTE_SHIFT_WHITE:
        if (!white_palettes || shift->shift_index >= white_palette_count) {
            return -1;
        }
        *out_palette = white_palettes + (size_t)shift->shift_index * palette_size;
        return 0;
    default:
        return -1;
    }
}

int wl_describe_palette_shifted_texture_upload(
    const wl_indexed_surface *surface, const wl_palette_shift_result *shift,
    const unsigned char *base_palette, const unsigned char *red_palettes,
    size_t red_palette_count, const unsigned char *white_palettes,
    size_t white_palette_count, size_t palette_size,
    uint8_t palette_component_bits, wl_texture_upload_descriptor *out) {
    const unsigned char *selected = NULL;
    if (wl_select_palette_for_shift(shift, base_palette, red_palettes,
                                    red_palette_count, white_palettes,
                                    white_palette_count, palette_size,
                                    &selected) != 0) {
        return -1;
    }
    return wl_describe_indexed_texture_upload(surface, selected, palette_size,
                                              palette_component_bits, out);
}

int wl_describe_indexed_texture_upload(const wl_indexed_surface *surface,
                                       const unsigned char *palette,
                                       size_t palette_size,
                                       uint8_t palette_component_bits,
                                       wl_texture_upload_descriptor *out) {
    if (!surface || !surface->pixels || !palette || !out ||
        surface->format != WL_SURFACE_INDEXED8 || surface->width == 0 ||
        surface->height == 0 || surface->stride < surface->width ||
        (palette_component_bits != 6 && palette_component_bits != 8) ||
        palette_size < 256u * 3u) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->format = WL_TEXTURE_UPLOAD_INDEXED8_RGB_PALETTE;
    out->width = surface->width;
    out->height = surface->height;
    out->pitch = surface->stride;
    out->pixel_bytes = (size_t)surface->stride * (size_t)surface->height;
    out->pixels = surface->pixels;
    out->palette = palette;
    out->palette_entries = 256;
    out->palette_component_bits = palette_component_bits;
    return 0;
}

int wl_expand_indexed_surface_to_rgba(const wl_indexed_surface *surface,
                                      const unsigned char *palette,
                                      size_t palette_size,
                                      uint8_t palette_component_bits,
                                      unsigned char *rgba, size_t rgba_size,
                                      wl_texture_upload_descriptor *out) {
    wl_texture_upload_descriptor indexed;
    if (!rgba || wl_describe_indexed_texture_upload(surface, palette, palette_size,
                                                    palette_component_bits,
                                                    &indexed) != 0) {
        return -1;
    }

    size_t required = (size_t)surface->width * (size_t)surface->height * 4u;
    if (rgba_size < required) {
        return -1;
    }

    for (uint16_t y = 0; y < surface->height; ++y) {
        const unsigned char *src_row = surface->pixels + (size_t)y * surface->stride;
        unsigned char *dst_row = rgba + (size_t)y * (size_t)surface->width * 4u;
        for (uint16_t x = 0; x < surface->width; ++x) {
            unsigned char index = src_row[x];
            const unsigned char *entry = palette + (size_t)index * 3u;
            dst_row[(size_t)x * 4u + 0u] = expand_palette_component(entry[0], palette_component_bits);
            dst_row[(size_t)x * 4u + 1u] = expand_palette_component(entry[1], palette_component_bits);
            dst_row[(size_t)x * 4u + 2u] = expand_palette_component(entry[2], palette_component_bits);
            dst_row[(size_t)x * 4u + 3u] = 255;
        }
    }

    if (out) {
        memset(out, 0, sizeof(*out));
        out->format = WL_TEXTURE_UPLOAD_RGBA8888;
        out->width = surface->width;
        out->height = surface->height;
        out->pitch = (uint16_t)(surface->width * 4u);
        out->pixel_bytes = required;
        out->pixels = rgba;
        out->palette = NULL;
        out->palette_entries = 0;
        out->palette_component_bits = 8;
    }
    return 0;
}


static uint32_t wl_fnv1a_local(const unsigned char *bytes, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

int wl_describe_present_frame(const wl_indexed_surface *surface,
                              const wl_palette_shift_result *shift,
                              const unsigned char *base_palette,
                              const unsigned char *red_palettes,
                              size_t red_palette_count,
                              const unsigned char *white_palettes,
                              size_t white_palette_count,
                              size_t palette_size,
                              uint8_t palette_component_bits,
                              wl_present_frame_descriptor *out) {
    if (!surface || !shift || !out || palette_size < 256u * 3u) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (wl_describe_palette_shifted_texture_upload(surface, shift, base_palette,
                                                   red_palettes, red_palette_count,
                                                   white_palettes, white_palette_count,
                                                   palette_size, palette_component_bits,
                                                   &out->texture) != 0) {
        return -1;
    }
    out->viewport_width = surface->width;
    out->viewport_height = surface->height;
    out->pixel_hash = wl_fnv1a_local(out->texture.pixels, out->texture.pixel_bytes);
    out->palette_hash = wl_fnv1a_local(out->texture.palette, palette_size);
    out->palette_shift_kind = (uint8_t)shift->kind;
    out->palette_shift_index = shift->shift_index;
    return 0;
}

int wl_expand_present_frame_to_rgba(const wl_present_frame_descriptor *present,
                                    unsigned char *rgba, size_t rgba_size,
                                    wl_texture_upload_descriptor *out) {
    if (!present || !rgba || present->texture.format != WL_TEXTURE_UPLOAD_INDEXED8_RGB_PALETTE ||
        !present->texture.pixels || !present->texture.palette ||
        present->texture.width == 0 || present->texture.height == 0 ||
        present->viewport_width != present->texture.width ||
        present->viewport_height != present->texture.height ||
        present->texture.pitch < present->texture.width ||
        present->texture.pixel_bytes <
            (size_t)present->texture.pitch * (size_t)present->texture.height ||
        present->texture.palette_entries < 256u) {
        return -1;
    }

    wl_indexed_surface surface;
    memset(&surface, 0, sizeof(surface));
    surface.format = WL_SURFACE_INDEXED8;
    surface.width = present->texture.width;
    surface.height = present->texture.height;
    surface.stride = present->texture.pitch;
    surface.pixel_count = present->texture.pixel_bytes;
    surface.pixels = (unsigned char *)present->texture.pixels;

    return wl_expand_indexed_surface_to_rgba(&surface, present->texture.palette,
                                             present->texture.palette_entries * 3u,
                                             present->texture.palette_component_bits,
                                             rgba, rgba_size, out);
}

int wl_decode_planar_picture_to_indexed(const unsigned char *planar, size_t planar_size,
                                        uint16_t width, uint16_t height,
                                        unsigned char *indexed, size_t indexed_size) {
    if (!planar || !indexed || width == 0 || height == 0 || (width % 4) != 0) {
        return -1;
    }

    size_t pixels = (size_t)width * (size_t)height;
    if (indexed_size < pixels || planar_size != pixels) {
        return -1;
    }

    size_t bytes_per_row_plane = (size_t)width / 4;
    size_t plane_size = bytes_per_row_plane * (size_t)height;
    for (size_t plane = 0; plane < 4; ++plane) {
        const unsigned char *plane_base = planar + plane * plane_size;
        for (size_t y = 0; y < height; ++y) {
            const unsigned char *src_row = plane_base + y * bytes_per_row_plane;
            unsigned char *dst_row = indexed + y * (size_t)width;
            for (size_t x = 0; x < bytes_per_row_plane; ++x) {
                dst_row[x * 4 + plane] = src_row[x];
            }
        }
    }
    return 0;
}

int wl_decode_planar_picture_surface(const unsigned char *planar, size_t planar_size,
                                     uint16_t width, uint16_t height,
                                     unsigned char *pixels, size_t pixel_size,
                                     wl_indexed_surface *out) {
    if (wl_decode_planar_picture_to_indexed(planar, planar_size, width, height,
                                            pixels, pixel_size) != 0) {
        return -1;
    }
    return wl_wrap_indexed_surface(width, height, pixels, pixel_size, out);
}

int wl_blit_indexed_surface(const wl_indexed_surface *src, wl_indexed_surface *dst,
                            int dst_x, int dst_y) {
    if (!src || !dst || !src->pixels || !dst->pixels ||
        src->format != WL_SURFACE_INDEXED8 || dst->format != WL_SURFACE_INDEXED8) {
        return -1;
    }

    int src_x0 = 0;
    int src_y0 = 0;
    int src_x1 = src->width;
    int src_y1 = src->height;
    if (dst_x < 0) {
        src_x0 = -dst_x;
        dst_x = 0;
    }
    if (dst_y < 0) {
        src_y0 = -dst_y;
        dst_y = 0;
    }
    if (dst_x + (src_x1 - src_x0) > dst->width) {
        src_x1 = src_x0 + (dst->width - dst_x);
    }
    if (dst_y + (src_y1 - src_y0) > dst->height) {
        src_y1 = src_y0 + (dst->height - dst_y);
    }
    if (src_x0 >= src_x1 || src_y0 >= src_y1) {
        return 0;
    }

    size_t copy_width = (size_t)(src_x1 - src_x0);
    for (int y = src_y0; y < src_y1; ++y) {
        const unsigned char *src_row = src->pixels + (size_t)y * src->stride + (size_t)src_x0;
        unsigned char *dst_row = dst->pixels + (size_t)(dst_y + y - src_y0) * dst->stride + (size_t)dst_x;
        memcpy(dst_row, src_row, copy_width);
    }
    return 0;
}

int wl_read_vswap_header(const char *path, wl_vswap_header *out) {
    if (!path || !out) {
        return -1;
    }

    size_t size = 0;
    if (wl_file_size(path, &size) != 0) {
        return -1;
    }
    unsigned char head[6 + 5 * 4];
    if (read_exact_at(path, 0, head, sizeof(head)) != 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->file_size = size;
    out->chunks_in_file = read_le16(head);
    out->sprite_start = read_le16(head + 2);
    out->sound_start = read_le16(head + 4);
    for (size_t i = 0; i < 5; ++i) {
        out->first_offsets[i] = read_le32(head + 6 + i * 4);
    }
    return 0;
}


static wl_vswap_chunk_kind vswap_kind(size_t index, const wl_vswap_header *header,
                                      uint32_t offset, uint16_t length) {
    if (offset == 0 || length == 0) {
        return WL_VSWAP_CHUNK_SPARSE;
    }
    if (index < header->sprite_start) {
        return WL_VSWAP_CHUNK_WALL;
    }
    if (index < header->sound_start) {
        return WL_VSWAP_CHUNK_SPRITE;
    }
    return WL_VSWAP_CHUNK_SOUND;
}

int wl_read_vswap_directory(const char *path, wl_vswap_directory *out) {
    if (!path || !out) {
        return -1;
    }

    wl_vswap_header header;
    if (wl_read_vswap_header(path, &header) != 0) {
        return -1;
    }
    if (header.chunks_in_file == 0 || header.chunks_in_file > WL_VSWAP_MAX_CHUNKS ||
        header.sprite_start >= header.sound_start || header.sound_start > header.chunks_in_file) {
        return -1;
    }

    size_t table_bytes = (size_t)header.chunks_in_file * (sizeof(uint32_t) + sizeof(uint16_t));
    size_t data_start = 6 + table_bytes;
    if (data_start > header.file_size) {
        return -1;
    }

    unsigned char *tables = (unsigned char *)malloc(table_bytes);
    if (!tables) {
        return -1;
    }
    if (read_exact_at(path, 6, tables, table_bytes) != 0) {
        free(tables);
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->header = header;
    out->data_start = data_start;

    uint32_t previous_end = 0;
    const unsigned char *offsets = tables;
    const unsigned char *lengths = tables + (size_t)header.chunks_in_file * sizeof(uint32_t);
    for (size_t i = 0; i < header.chunks_in_file; ++i) {
        uint32_t offset = read_le32(offsets + i * sizeof(uint32_t));
        uint16_t length = read_le16(lengths + i * sizeof(uint16_t));
        wl_vswap_chunk_kind kind = vswap_kind(i, &header, offset, length);

        out->chunks[i].offset = offset;
        out->chunks[i].length = length;
        out->chunks[i].kind = kind;

        if (kind == WL_VSWAP_CHUNK_SPARSE) {
            ++out->sparse_count;
            continue;
        }

        if (offset < data_start || offset > header.file_size ||
            (size_t)length > header.file_size - (size_t)offset) {
            free(tables);
            return -1;
        }
        if (previous_end != 0 && offset < previous_end) {
            free(tables);
            return -1;
        }
        previous_end = offset + length;
        if (previous_end > out->max_chunk_end) {
            out->max_chunk_end = previous_end;
        }

        switch (kind) {
        case WL_VSWAP_CHUNK_WALL:
            ++out->wall_count;
            break;
        case WL_VSWAP_CHUNK_SPRITE:
            ++out->sprite_count;
            break;
        case WL_VSWAP_CHUNK_SOUND:
            ++out->sound_count;
            break;
        case WL_VSWAP_CHUNK_SPARSE:
            break;
        }
    }

    free(tables);
    return out->max_chunk_end <= out->header.file_size ? 0 : -1;
}


int wl_read_vswap_chunk(const char *path, const wl_vswap_directory *directory,
                        size_t chunk_index, unsigned char *out, size_t out_size,
                        size_t *bytes_read) {
    if (!path || !directory || !out || chunk_index >= directory->header.chunks_in_file) {
        return -1;
    }

    const wl_vswap_chunk *chunk = &directory->chunks[chunk_index];
    if (chunk->kind == WL_VSWAP_CHUNK_SPARSE || chunk->offset == 0 || chunk->length == 0) {
        if (bytes_read) {
            *bytes_read = 0;
        }
        return 0;
    }
    if (chunk->length > out_size || chunk->offset < directory->data_start ||
        (size_t)chunk->offset > directory->header.file_size ||
        (size_t)chunk->length > directory->header.file_size - (size_t)chunk->offset) {
        return -1;
    }

    if (read_exact_at(path, (long)chunk->offset, out, chunk->length) != 0) {
        return -1;
    }
    if (bytes_read) {
        *bytes_read = chunk->length;
    }
    return 0;
}


int wl_decode_vswap_shape_metadata(const unsigned char *chunk, size_t chunk_size,
                                   wl_vswap_chunk_kind kind,
                                   wl_vswap_shape_metadata *out) {
    if (!chunk || !out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->kind = kind;

    if (kind == WL_VSWAP_CHUNK_WALL) {
        if (chunk_size != WL_MAP_PLANE_WORDS) {
            return -1;
        }
        out->width = WL_MAP_SIDE;
        out->height = WL_MAP_SIDE;
        out->leftpix = 0;
        out->rightpix = WL_MAP_SIDE - 1;
        out->visible_columns = WL_MAP_SIDE;
        return 0;
    }

    if (kind != WL_VSWAP_CHUNK_SPRITE || chunk_size < 6) {
        return -1;
    }

    uint16_t left = read_le16(chunk);
    uint16_t right = read_le16(chunk + 2);
    if (left > right || right >= WL_MAP_SIDE) {
        return -1;
    }

    uint16_t columns = (uint16_t)(right - left + 1);
    size_t table_bytes = 4 + (size_t)columns * sizeof(uint16_t);
    if (table_bytes > chunk_size) {
        return -1;
    }

    uint16_t first_offset = read_le16(chunk + 4);
    uint16_t previous = 0;
    uint16_t min_offset = UINT16_MAX;
    uint16_t max_offset = 0;
    uint16_t last_offset = first_offset;
    uint16_t post_count = 0;
    uint16_t terminal_count = 0;
    uint16_t min_posts_per_column = UINT16_MAX;
    uint16_t max_posts_per_column = 0;
    uint16_t min_post_span = UINT16_MAX;
    uint16_t max_post_span = 0;
    uint16_t max_post_start = 0;
    uint16_t max_post_end = 0;
    uint16_t min_source_offset = UINT16_MAX;
    uint16_t max_source_offset = 0;
    uint32_t total_post_span = 0;
    for (uint16_t i = 0; i < columns; ++i) {
        uint16_t offset = read_le16(chunk + 4 + (size_t)i * sizeof(uint16_t));
        if (offset < table_bytes || offset >= chunk_size || (offset % 2) != 0) {
            return -1;
        }
        if (i > 0 && offset < previous) {
            return -1;
        }
        if (offset < min_offset) {
            min_offset = offset;
        }
        if (offset > max_offset) {
            max_offset = offset;
        }
        previous = offset;
        last_offset = offset;

        uint16_t column_posts = 0;
        size_t pos = offset;
        while (1) {
            if (pos + sizeof(uint16_t) > chunk_size) {
                return -1;
            }
            uint16_t end = read_le16(chunk + pos);
            pos += sizeof(uint16_t);
            if (end == 0) {
                ++terminal_count;
                break;
            }
            if (pos + 2 * sizeof(uint16_t) > chunk_size) {
                return -1;
            }
            uint16_t source_offset = read_le16(chunk + pos);
            uint16_t start = read_le16(chunk + pos + sizeof(uint16_t));
            pos += 2 * sizeof(uint16_t);

            if ((start % 2) != 0 || (end % 2) != 0 || start >= end || end > WL_MAP_SIDE * 2u) {
                return -1;
            }
            uint16_t span = (uint16_t)(end - start);
            ++column_posts;
            ++post_count;
            total_post_span += span;
            if (span < min_post_span) {
                min_post_span = span;
            }
            if (span > max_post_span) {
                max_post_span = span;
            }
            if (start > max_post_start) {
                max_post_start = start;
            }
            if (end > max_post_end) {
                max_post_end = end;
            }
            if (source_offset < min_source_offset) {
                min_source_offset = source_offset;
            }
            if (source_offset > max_source_offset) {
                max_source_offset = source_offset;
            }
        }
        if (column_posts < min_posts_per_column) {
            min_posts_per_column = column_posts;
        }
        if (column_posts > max_posts_per_column) {
            max_posts_per_column = column_posts;
        }
    }

    if (terminal_count != columns || post_count == 0) {
        return -1;
    }

    out->width = WL_MAP_SIDE;
    out->height = WL_MAP_SIDE;
    out->leftpix = left;
    out->rightpix = right;
    out->visible_columns = columns;
    out->first_column_offset = first_offset;
    out->last_column_offset = last_offset;
    out->min_column_offset = min_offset;
    out->max_column_offset = max_offset;
    out->post_count = post_count;
    out->terminal_count = terminal_count;
    out->min_posts_per_column = min_posts_per_column;
    out->max_posts_per_column = max_posts_per_column;
    out->min_post_span = min_post_span;
    out->max_post_span = max_post_span;
    out->max_post_start = max_post_start;
    out->max_post_end = max_post_end;
    out->min_source_offset = min_source_offset;
    out->max_source_offset = max_source_offset;
    out->total_post_span = total_post_span;
    return 0;
}

int wl_decode_wall_page_metadata(const unsigned char *chunk, size_t chunk_size,
                                 wl_wall_page_metadata *out) {
    if (!chunk || !out || chunk_size != WL_MAP_PLANE_WORDS) {
        return -1;
    }

    unsigned char seen[256] = { 0 };
    uint16_t min_color = UINT16_MAX;
    uint16_t max_color = 0;
    uint16_t unique = 0;
    for (size_t i = 0; i < chunk_size; ++i) {
        uint16_t color = chunk[i];
        if (!seen[color]) {
            seen[color] = 1;
            ++unique;
        }
        if (color < min_color) {
            min_color = color;
        }
        if (color > max_color) {
            max_color = color;
        }
    }

    memset(out, 0, sizeof(*out));
    out->width = WL_MAP_SIDE;
    out->height = WL_MAP_SIDE;
    out->column_count = WL_MAP_SIDE;
    out->bytes_per_column = WL_MAP_SIDE;
    out->min_color = min_color;
    out->max_color = max_color;
    out->unique_color_count = unique;
    return 0;
}

int wl_decode_wall_page_to_indexed(const unsigned char *chunk, size_t chunk_size,
                                   unsigned char *indexed, size_t indexed_size) {
    if (!chunk || !indexed || chunk_size != WL_MAP_PLANE_WORDS ||
        indexed_size < WL_MAP_PLANE_WORDS) {
        return -1;
    }

    for (size_t x = 0; x < WL_MAP_SIDE; ++x) {
        const unsigned char *src_col = chunk + x * WL_MAP_SIDE;
        for (size_t y = 0; y < WL_MAP_SIDE; ++y) {
            indexed[y * WL_MAP_SIDE + x] = src_col[y];
        }
    }
    return 0;
}

int wl_decode_wall_page_surface(const unsigned char *chunk, size_t chunk_size,
                                unsigned char *pixels, size_t pixel_size,
                                wl_indexed_surface *out) {
    if (wl_decode_wall_page_to_indexed(chunk, chunk_size, pixels, pixel_size) != 0) {
        return -1;
    }
    return wl_wrap_indexed_surface(WL_MAP_SIDE, WL_MAP_SIDE, pixels, pixel_size, out);
}

int wl_decode_sprite_shape_to_indexed(const unsigned char *chunk, size_t chunk_size,
                                      unsigned char transparent_index,
                                      unsigned char *indexed, size_t indexed_size) {
    if (!chunk || !indexed || indexed_size < WL_MAP_PLANE_WORDS || chunk_size < 6) {
        return -1;
    }

    uint16_t left = read_le16(chunk);
    uint16_t right = read_le16(chunk + 2);
    if (left > right || right >= WL_MAP_SIDE) {
        return -1;
    }

    uint16_t columns = (uint16_t)(right - left + 1);
    size_t table_bytes = 4 + (size_t)columns * sizeof(uint16_t);
    if (table_bytes > chunk_size) {
        return -1;
    }

    memset(indexed, transparent_index, WL_MAP_PLANE_WORDS);
    uint16_t previous = 0;
    for (uint16_t i = 0; i < columns; ++i) {
        uint16_t x = (uint16_t)(left + i);
        uint16_t offset = read_le16(chunk + 4 + (size_t)i * sizeof(uint16_t));
        if (offset < table_bytes || offset >= chunk_size || (offset % 2) != 0 ||
            (i > 0 && offset < previous)) {
            return -1;
        }
        previous = offset;

        size_t pos = offset;
        while (1) {
            if (pos + sizeof(uint16_t) > chunk_size) {
                return -1;
            }
            uint16_t end = read_le16(chunk + pos);
            pos += sizeof(uint16_t);
            if (end == 0) {
                break;
            }
            if (pos + 2 * sizeof(uint16_t) > chunk_size) {
                return -1;
            }
            uint16_t source_offset = read_le16(chunk + pos);
            uint16_t start = read_le16(chunk + pos + sizeof(uint16_t));
            pos += 2 * sizeof(uint16_t);
            if ((start % 2) != 0 || (end % 2) != 0 || start >= end ||
                end > WL_MAP_SIDE * 2u) {
                return -1;
            }
            uint16_t start_y = (uint16_t)(start / 2u);
            uint16_t end_y = (uint16_t)(end / 2u);
            for (uint16_t y = start_y; y < end_y; ++y) {
                uint16_t wrapped_offset = (uint16_t)(source_offset + y);
                if ((size_t)wrapped_offset >= chunk_size) {
                    return -1;
                }
                indexed[(size_t)y * WL_MAP_SIDE + x] = chunk[wrapped_offset];
            }
        }
    }
    return 0;
}

int wl_decode_sprite_shape_surface(const unsigned char *chunk, size_t chunk_size,
                                   unsigned char transparent_index,
                                   unsigned char *pixels, size_t pixel_size,
                                   wl_indexed_surface *out) {
    if (wl_decode_sprite_shape_to_indexed(chunk, chunk_size, transparent_index,
                                          pixels, pixel_size) != 0) {
        return -1;
    }
    return wl_wrap_indexed_surface(WL_MAP_SIDE, WL_MAP_SIDE, pixels, pixel_size, out);
}

int wl_decode_vswap_sprite_surface_cache(const char *path,
                                         const wl_vswap_directory *directory,
                                         const uint16_t *chunk_indices,
                                         size_t surface_count,
                                         unsigned char transparent_index,
                                         unsigned char *pixel_storage,
                                         size_t pixel_storage_size,
                                         wl_indexed_surface *surfaces) {
    if (!path || !directory || (!chunk_indices && surface_count != 0) ||
        (!pixel_storage && surface_count != 0) || (!surfaces && surface_count != 0)) {
        return -1;
    }
    if (surface_count == 0) {
        return 0;
    }
    if (surface_count > SIZE_MAX / (size_t)WL_MAP_PLANE_WORDS ||
        pixel_storage_size < surface_count * (size_t)WL_MAP_PLANE_WORDS) {
        return -1;
    }

    for (size_t i = 0; i < surface_count; ++i) {
        uint16_t chunk_index = chunk_indices[i];
        if (chunk_index >= directory->header.chunks_in_file ||
            directory->chunks[chunk_index].kind != WL_VSWAP_CHUNK_SPRITE ||
            directory->chunks[chunk_index].length == 0) {
            return -1;
        }

        size_t chunk_size = directory->chunks[chunk_index].length;
        unsigned char *chunk = (unsigned char *)malloc(chunk_size);
        if (!chunk) {
            return -1;
        }
        size_t bytes_read = 0;
        int ok = wl_read_vswap_chunk(path, directory, chunk_index, chunk, chunk_size,
                                     &bytes_read);
        if (ok == 0 && bytes_read != chunk_size) {
            ok = -1;
        }
        if (ok == 0) {
            ok = wl_decode_sprite_shape_surface(chunk, bytes_read, transparent_index,
                                                pixel_storage + i * (size_t)WL_MAP_PLANE_WORDS,
                                                pixel_storage_size - i * (size_t)WL_MAP_PLANE_WORDS,
                                                &surfaces[i]);
        }
        free(chunk);
        if (ok != 0) {
            return -1;
        }
    }
    return 0;
}

int wl_sample_wall_page_column(const unsigned char *chunk, size_t chunk_size,
                               uint16_t texture_offset, unsigned char *out,
                               size_t out_size) {
    if (!chunk || !out || chunk_size != WL_MAP_PLANE_WORDS || out_size < WL_MAP_SIDE ||
        texture_offset > (WL_MAP_SIDE - 1u) * WL_MAP_SIDE ||
        (texture_offset % WL_MAP_SIDE) != 0) {
        return -1;
    }

    memcpy(out, chunk + texture_offset, WL_MAP_SIDE);
    return 0;
}

int wl_sample_indexed_surface_column(const wl_indexed_surface *surface, uint16_t x,
                                     unsigned char *out, size_t out_size) {
    if (!surface || !surface->pixels || !out || surface->format != WL_SURFACE_INDEXED8 ||
        x >= surface->width || out_size < surface->height || surface->stride < surface->width) {
        return -1;
    }

    for (uint16_t y = 0; y < surface->height; ++y) {
        out[y] = surface->pixels[(size_t)y * surface->stride + x];
    }
    return 0;
}

int wl_scale_wall_column_to_surface(const unsigned char *column, size_t column_size,
                                    wl_indexed_surface *dst, uint16_t x,
                                    uint16_t scaled_height) {
    if (!column || !dst || !dst->pixels || dst->format != WL_SURFACE_INDEXED8 ||
        dst->stride < dst->width || column_size < WL_MAP_SIDE || x >= dst->width ||
        scaled_height == 0) {
        return -1;
    }

    int top = ((int)dst->height - (int)scaled_height) / 2;
    uint32_t step = ((uint32_t)scaled_height << 16) / WL_MAP_SIDE;
    uint32_t fix = 0;
    for (uint16_t src = 0; src < WL_MAP_SIDE; ++src) {
        int start = (int)(fix >> 16);
        fix += step;
        int end = (int)(fix >> 16);
        if (end <= start) {
            continue;
        }
        start += top;
        end += top;
        if (end <= 0 || start >= dst->height) {
            continue;
        }
        if (start < 0) {
            start = 0;
        }
        if (end > dst->height) {
            end = dst->height;
        }
        for (int y = start; y < end; ++y) {
            dst->pixels[(size_t)y * dst->stride + x] = column[src];
        }
    }
    return 0;
}

int wl_render_scaled_sprite(const wl_indexed_surface *sprite, wl_indexed_surface *dst,
                            int x_center, uint16_t scaled_height,
                            unsigned char transparent_index,
                            const uint16_t *wall_heights, size_t wall_height_count) {
    if (!sprite || !dst || !sprite->pixels || !dst->pixels ||
        sprite->format != WL_SURFACE_INDEXED8 || dst->format != WL_SURFACE_INDEXED8 ||
        sprite->width == 0 || sprite->height == 0 || sprite->stride < sprite->width ||
        dst->stride < dst->width || scaled_height == 0 ||
        (wall_heights && wall_height_count < dst->width)) {
        return -1;
    }

    int left = x_center - (int)scaled_height / 2;
    int top = ((int)dst->height - (int)scaled_height) / 2;
    uint32_t x_step = ((uint32_t)scaled_height << 16) / sprite->width;
    uint32_t y_step = ((uint32_t)scaled_height << 16) / sprite->height;
    uint32_t x_fix = 0;

    for (uint16_t src_x = 0; src_x < sprite->width; ++src_x) {
        int x_start = (int)(x_fix >> 16);
        x_fix += x_step;
        int x_end = (int)(x_fix >> 16);
        if (x_end <= x_start) {
            continue;
        }

        int dst_x0 = left + x_start;
        int dst_x1 = left + x_end;
        if (dst_x1 <= 0 || dst_x0 >= dst->width) {
            continue;
        }
        if (dst_x0 < 0) {
            dst_x0 = 0;
        }
        if (dst_x1 > dst->width) {
            dst_x1 = dst->width;
        }

        uint32_t y_fix = 0;
        for (uint16_t src_y = 0; src_y < sprite->height; ++src_y) {
            unsigned char color = sprite->pixels[(size_t)src_y * sprite->stride + src_x];
            int y_start = (int)(y_fix >> 16);
            y_fix += y_step;
            int y_end = (int)(y_fix >> 16);
            if (color == transparent_index || y_end <= y_start) {
                continue;
            }

            int dst_y0 = top + y_start;
            int dst_y1 = top + y_end;
            if (dst_y1 <= 0 || dst_y0 >= dst->height) {
                continue;
            }
            if (dst_y0 < 0) {
                dst_y0 = 0;
            }
            if (dst_y1 > dst->height) {
                dst_y1 = dst->height;
            }

            for (int dst_x = dst_x0; dst_x < dst_x1; ++dst_x) {
                if (wall_heights && wall_heights[dst_x] >= scaled_height) {
                    continue;
                }
                for (int dst_y = dst_y0; dst_y < dst_y1; ++dst_y) {
                    dst->pixels[(size_t)dst_y * dst->stride + (size_t)dst_x] = color;
                }
            }
        }
    }
    return 0;
}

static int32_t fixed_mul_shift(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
}

static uint32_t projection_scale_for_width(uint16_t view_width) {
    const uint32_t mindist = 0x5800u;
    const uint32_t focallength = 0x5700u;
    const uint32_t viewglobal = 0x10000u;
    uint32_t halfview = (uint32_t)view_width / 2u;
    return (uint32_t)(((uint64_t)halfview * (uint64_t)(focallength + mindist)) /
                      (viewglobal / 2u));
}

int wl_project_world_sprite(uint16_t source_index, uint32_t origin_x, uint32_t origin_y,
                            uint32_t sprite_x, uint32_t sprite_y,
                            int32_t forward_x, int32_t forward_y,
                            uint16_t view_width, uint16_t view_height,
                            wl_projected_sprite *out) {
    const int32_t mindist = 0x5800;
    const int32_t focallength = 0x5700;
    const int32_t actorsize = 0x4000;
    const int32_t tileglobal = 1 << 16;

    if (!out || view_width == 0 || view_height == 0 ||
        (forward_x == 0 && forward_y == 0) ||
        origin_x >= ((uint32_t)WL_MAP_SIDE << 16) ||
        origin_y >= ((uint32_t)WL_MAP_SIDE << 16) ||
        sprite_x >= ((uint32_t)WL_MAP_SIDE << 16) ||
        sprite_y >= ((uint32_t)WL_MAP_SIDE << 16)) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->source_index = source_index;
    out->surface_index = 0;

    int32_t focal_x = (int32_t)origin_x - fixed_mul_shift(focallength, forward_x);
    int32_t focal_y = (int32_t)origin_y - fixed_mul_shift(focallength, forward_y);
    int32_t gx = (int32_t)sprite_x - focal_x;
    int32_t gy = (int32_t)sprite_y - focal_y;

    int32_t forward_distance = fixed_mul_shift(gx, forward_x) + fixed_mul_shift(gy, forward_y);
    int32_t lateral = fixed_mul_shift(gy, forward_x) - fixed_mul_shift(gx, forward_y);
    int32_t nx = forward_distance - actorsize;

    out->trans_x = nx;
    out->trans_y = lateral;
    if (nx < mindist) {
        out->visible = 0;
        return 0;
    }

    uint32_t scale = projection_scale_for_width(view_width);
    int32_t centerx = (int32_t)view_width / 2 - 1;
    out->view_x = (int16_t)(centerx + (int32_t)(((int64_t)lateral * scale) / nx));
    uint32_t denominator = (uint32_t)nx >> 8;
    if (denominator == 0) {
        denominator = 1;
    }
    uint32_t heightnumerator = (uint32_t)(((uint64_t)tileglobal * scale) >> 6);
    uint32_t projected = heightnumerator / denominator;
    if (projected == 0) {
        projected = 1;
    }
    if (projected > UINT16_MAX) {
        projected = UINT16_MAX;
    }
    out->scaled_height = (uint16_t)projected;
    out->distance = (uint32_t)nx;
    out->visible = 1;
    return 0;
}

int wl_sort_projected_sprites_far_to_near(wl_projected_sprite *sprites, size_t count) {
    if (!sprites && count != 0) {
        return -1;
    }

    for (size_t i = 1; i < count; ++i) {
        wl_projected_sprite key = sprites[i];
        size_t j = i;
        while (j > 0) {
            int move = 0;
            if (!sprites[j - 1].visible && key.visible) {
                move = 1;
            } else if (sprites[j - 1].visible == key.visible &&
                       sprites[j - 1].distance < key.distance) {
                move = 1;
            }
            if (!move) {
                break;
            }
            sprites[j] = sprites[j - 1];
            --j;
        }
        sprites[j] = key;
    }
    return 0;
}

int wl_render_wall_strip_viewport(const wl_wall_strip *strips, size_t strip_count,
                                  wl_indexed_surface *dst) {
    if (!strips || !dst || strip_count == 0) {
        return -1;
    }

    unsigned char column[WL_MAP_SIDE];
    for (size_t i = 0; i < strip_count; ++i) {
        if (wl_sample_wall_page_column(strips[i].wall_page, strips[i].wall_page_size,
                                       strips[i].texture_offset, column,
                                       sizeof(column)) != 0) {
            return -1;
        }
        if (wl_scale_wall_column_to_surface(column, sizeof(column), dst,
                                            strips[i].x, strips[i].scaled_height) != 0) {
            return -1;
        }
    }
    return 0;
}

int wl_build_map_wall_hit(const uint16_t *wall_plane, size_t wall_count,
                          uint16_t tile_x, uint16_t tile_y, wl_wall_side side,
                          uint16_t texture_column, uint16_t x,
                          uint16_t scaled_height, wl_map_wall_hit *out) {
    if (!wall_plane || !out || wall_count < WL_MAP_PLANE_WORDS ||
        tile_x >= WL_MAP_SIDE || tile_y >= WL_MAP_SIDE || texture_column >= WL_MAP_SIDE ||
        scaled_height == 0 ||
        (side != WL_WALL_SIDE_HORIZONTAL && side != WL_WALL_SIDE_VERTICAL)) {
        return -1;
    }

    uint16_t tile = wall_plane[(size_t)tile_y * WL_MAP_SIDE + tile_x];
    if (tile == 0 || tile >= 64) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->tile_x = tile_x;
    out->tile_y = tile_y;
    out->source_tile = tile;
    out->side = side;
    out->wall_page_index = (uint16_t)((tile - 1u) * 2u +
                                      (side == WL_WALL_SIDE_VERTICAL ? 1u : 0u));
    out->texture_offset = (uint16_t)(texture_column * WL_MAP_SIDE);
    out->x = x;
    out->scaled_height = scaled_height;
    return 0;
}

int wl_wall_hit_to_strip(const wl_map_wall_hit *hit, const unsigned char *wall_page,
                         size_t wall_page_size, wl_wall_strip *out) {
    if (!hit || !wall_page || !out || wall_page_size != WL_MAP_PLANE_WORDS) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->wall_page = wall_page;
    out->wall_page_size = wall_page_size;
    out->texture_offset = hit->texture_offset;
    out->x = hit->x;
    out->scaled_height = hit->scaled_height;
    return 0;
}

int wl_cast_cardinal_wall_ray(const uint16_t *wall_plane, size_t wall_count,
                              uint16_t start_tile_x, uint16_t start_tile_y,
                              wl_cardinal_ray_direction direction,
                              uint16_t texture_column, uint16_t x,
                              uint16_t scaled_height, wl_map_wall_hit *out) {
    if (!wall_plane || !out || wall_count < WL_MAP_PLANE_WORDS ||
        start_tile_x >= WL_MAP_SIDE || start_tile_y >= WL_MAP_SIDE ||
        texture_column >= WL_MAP_SIDE || scaled_height == 0) {
        return -1;
    }

    int dx = 0;
    int dy = 0;
    wl_wall_side side = WL_WALL_SIDE_HORIZONTAL;
    switch (direction) {
    case WL_RAY_NORTH:
        dy = -1;
        side = WL_WALL_SIDE_HORIZONTAL;
        break;
    case WL_RAY_EAST:
        dx = 1;
        side = WL_WALL_SIDE_VERTICAL;
        break;
    case WL_RAY_SOUTH:
        dy = 1;
        side = WL_WALL_SIDE_HORIZONTAL;
        break;
    case WL_RAY_WEST:
        dx = -1;
        side = WL_WALL_SIDE_VERTICAL;
        break;
    default:
        return -1;
    }

    int tx = start_tile_x;
    int ty = start_tile_y;
    while (1) {
        tx += dx;
        ty += dy;
        if (tx < 0 || ty < 0 || tx >= WL_MAP_SIDE || ty >= WL_MAP_SIDE) {
            return -1;
        }
        uint16_t tile = wall_plane[(size_t)ty * WL_MAP_SIDE + (size_t)tx];
        if (tile == 0 || tile >= 64) {
            continue;
        }
        return wl_build_map_wall_hit(wall_plane, wall_count, (uint16_t)tx, (uint16_t)ty,
                                     side, texture_column, x, scaled_height, out);
    }
}

int wl_cast_fixed_cardinal_wall_ray(const uint16_t *wall_plane, size_t wall_count,
                                    uint32_t origin_x, uint32_t origin_y,
                                    wl_cardinal_ray_direction direction,
                                    uint16_t x, uint16_t scaled_height,
                                    wl_map_wall_hit *out) {
    if (!wall_plane || !out || wall_count < WL_MAP_PLANE_WORDS || scaled_height == 0 ||
        origin_x >= ((uint32_t)WL_MAP_SIDE << 16) ||
        origin_y >= ((uint32_t)WL_MAP_SIDE << 16)) {
        return -1;
    }

    uint16_t start_x = (uint16_t)(origin_x >> 16);
    uint16_t start_y = (uint16_t)(origin_y >> 16);
    uint16_t texture_column = 0;
    switch (direction) {
    case WL_RAY_NORTH:
    case WL_RAY_SOUTH:
        texture_column = (uint16_t)((origin_x >> 10) & (WL_MAP_SIDE - 1u));
        break;
    case WL_RAY_EAST:
    case WL_RAY_WEST:
        texture_column = (uint16_t)((origin_y >> 10) & (WL_MAP_SIDE - 1u));
        break;
    default:
        return -1;
    }

    return wl_cast_cardinal_wall_ray(wall_plane, wall_count, start_x, start_y,
                                     direction, texture_column, x, scaled_height,
                                     out);
}

uint16_t wl_project_wall_height(uint32_t forward_distance, uint16_t view_width,
                                uint16_t view_height) {
    const uint32_t tileglobal = 1u << 16;
    const uint32_t mindist = 0x5800u;
    const uint32_t focallength = 0x5700u;
    const uint32_t viewglobal = 0x10000u;

    if (view_width == 0 || view_height == 0) {
        return 0;
    }

    uint32_t clamped_distance = forward_distance < mindist ? mindist : forward_distance;
    uint32_t denominator = clamped_distance >> 8;
    if (denominator == 0) {
        denominator = 1;
    }

    uint32_t halfview = (uint32_t)view_width / 2u;
    uint32_t scale = (uint32_t)(((uint64_t)halfview * (focallength + mindist)) /
                               (viewglobal / 2u));
    uint32_t heightnumerator = (uint32_t)(((uint64_t)tileglobal * scale) >> 6);
    uint32_t projected = heightnumerator / denominator;
    if (projected == 0) {
        projected = 1;
    }
    if (projected > view_height) {
        projected = view_height;
    }
    return (uint16_t)projected;
}

int wl_cast_fixed_wall_ray(const uint16_t *wall_plane, size_t wall_count,
                           uint32_t origin_x, uint32_t origin_y,
                           int32_t direction_x, int32_t direction_y,
                           uint16_t x, uint16_t scaled_height,
                           wl_map_wall_hit *out) {
    if (!wall_plane || !out || wall_count < WL_MAP_PLANE_WORDS || scaled_height == 0 ||
        origin_x >= ((uint32_t)WL_MAP_SIDE << 16) ||
        origin_y >= ((uint32_t)WL_MAP_SIDE << 16) ||
        (direction_x == 0 && direction_y == 0)) {
        return -1;
    }

    int tile_x = (int)(origin_x >> 16);
    int tile_y = (int)(origin_y >> 16);
    int step_x = 0;
    int step_y = 0;
    int64_t side_t_x = INT64_MAX;
    int64_t side_t_y = INT64_MAX;
    int64_t delta_t_x = INT64_MAX;
    int64_t delta_t_y = INT64_MAX;

    if (direction_x > 0) {
        step_x = 1;
        uint32_t next_boundary = (uint32_t)(tile_x + 1) << 16;
        side_t_x = ((int64_t)(next_boundary - origin_x) << 16) / direction_x;
        delta_t_x = ((int64_t)1 << 32) / direction_x;
    } else if (direction_x < 0) {
        step_x = -1;
        uint32_t next_boundary = (uint32_t)tile_x << 16;
        side_t_x = ((int64_t)(origin_x - next_boundary) << 16) / -(int64_t)direction_x;
        delta_t_x = ((int64_t)1 << 32) / -(int64_t)direction_x;
    }

    if (direction_y > 0) {
        step_y = 1;
        uint32_t next_boundary = (uint32_t)(tile_y + 1) << 16;
        side_t_y = ((int64_t)(next_boundary - origin_y) << 16) / direction_y;
        delta_t_y = ((int64_t)1 << 32) / direction_y;
    } else if (direction_y < 0) {
        step_y = -1;
        uint32_t next_boundary = (uint32_t)tile_y << 16;
        side_t_y = ((int64_t)(origin_y - next_boundary) << 16) / -(int64_t)direction_y;
        delta_t_y = ((int64_t)1 << 32) / -(int64_t)direction_y;
    }

    for (size_t step_count = 0; step_count < WL_MAP_SIDE * 2u; ++step_count) {
        wl_wall_side side = WL_WALL_SIDE_HORIZONTAL;
        int64_t hit_t = 0;
        if (side_t_x <= side_t_y) {
            tile_x += step_x;
            hit_t = side_t_x;
            side_t_x += delta_t_x;
            side = WL_WALL_SIDE_VERTICAL;
        } else {
            tile_y += step_y;
            hit_t = side_t_y;
            side_t_y += delta_t_y;
            side = WL_WALL_SIDE_HORIZONTAL;
        }

        if (tile_x < 0 || tile_y < 0 || tile_x >= WL_MAP_SIDE || tile_y >= WL_MAP_SIDE) {
            return -1;
        }

        uint16_t tile = wall_plane[(size_t)tile_y * WL_MAP_SIDE + (size_t)tile_x];
        if (tile == 0 || tile >= 64) {
            continue;
        }

        int64_t hit_coord = 0;
        if (side == WL_WALL_SIDE_VERTICAL) {
            hit_coord = (int64_t)origin_y + (((int64_t)direction_y * hit_t) >> 16);
        } else {
            hit_coord = (int64_t)origin_x + (((int64_t)direction_x * hit_t) >> 16);
        }
        uint16_t texture_column = (uint16_t)((uint64_t)(hit_coord >> 10) &
                                             (WL_MAP_SIDE - 1u));
        if (wl_build_map_wall_hit(wall_plane, wall_count, (uint16_t)tile_x,
                                  (uint16_t)tile_y, side, texture_column, x,
                                  scaled_height, out) != 0) {
            return -1;
        }
        out->distance = (hit_t > UINT32_MAX) ? UINT32_MAX : (uint32_t)hit_t;
        return 0;
    }

    return -1;
}

int wl_cast_projected_wall_ray(const uint16_t *wall_plane, size_t wall_count,
                               uint32_t origin_x, uint32_t origin_y,
                               int32_t direction_x, int32_t direction_y,
                               uint16_t x, uint16_t view_width,
                               uint16_t view_height, wl_map_wall_hit *out) {
    if (!out || view_width == 0 || view_height == 0 || x >= view_width) {
        return -1;
    }

    if (wl_cast_fixed_wall_ray(wall_plane, wall_count, origin_x, origin_y,
                               direction_x, direction_y, x, 1, out) != 0) {
        return -1;
    }
    out->scaled_height = wl_project_wall_height(out->distance, view_width, view_height);
    return out->scaled_height == 0 ? -1 : 0;
}

int wl_build_camera_ray_directions(int32_t forward_x, int32_t forward_y,
                                   int32_t plane_x, int32_t plane_y,
                                   uint16_t view_width, uint16_t first_x,
                                   uint16_t x_step, size_t ray_count,
                                   int32_t *out_x, int32_t *out_y) {
    if (!out_x || !out_y || view_width == 0 || x_step == 0 || ray_count == 0 ||
        (forward_x == 0 && forward_y == 0)) {
        return -1;
    }

    for (size_t i = 0; i < ray_count; ++i) {
        uint32_t x = (uint32_t)first_x + (uint32_t)i * x_step;
        if (x >= view_width) {
            return -1;
        }
        int64_t screen_numerator = ((int64_t)2 * (int64_t)x + 1 - view_width) << 16;
        int64_t camera_x = screen_numerator / view_width;
        int64_t ray_x = (int64_t)forward_x + (((int64_t)plane_x * camera_x) >> 16);
        int64_t ray_y = (int64_t)forward_y + (((int64_t)plane_y * camera_x) >> 16);
        if ((ray_x == 0 && ray_y == 0) || ray_x < INT32_MIN || ray_x > INT32_MAX ||
            ray_y < INT32_MIN || ray_y > INT32_MAX) {
            return -1;
        }
        out_x[i] = (int32_t)ray_x;
        out_y[i] = (int32_t)ray_y;
    }
    return 0;
}

int wl_cast_projected_wall_ray_batch(const uint16_t *wall_plane, size_t wall_count,
                                     uint32_t origin_x, uint32_t origin_y,
                                     const int32_t *directions_x,
                                     const int32_t *directions_y,
                                     size_t ray_count, uint16_t first_x,
                                     uint16_t x_step, uint16_t view_width,
                                     uint16_t view_height, wl_map_wall_hit *out) {
    if (!directions_x || !directions_y || !out || ray_count == 0 || x_step == 0 ||
        view_width == 0 || view_height == 0) {
        return -1;
    }

    for (size_t i = 0; i < ray_count; ++i) {
        uint32_t x = (uint32_t)first_x + (uint32_t)i * x_step;
        if (x >= view_width) {
            return -1;
        }
        if (wl_cast_projected_wall_ray(wall_plane, wall_count, origin_x, origin_y,
                                       directions_x[i], directions_y[i], (uint16_t)x,
                                       view_width, view_height, &out[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

int wl_render_camera_wall_view(const uint16_t *wall_plane, size_t wall_count,
                               uint32_t origin_x, uint32_t origin_y,
                               int32_t forward_x, int32_t forward_y,
                               int32_t plane_x, int32_t plane_y,
                               uint16_t first_x, uint16_t x_step, size_t ray_count,
                               const unsigned char *const *wall_pages,
                               const size_t *wall_page_sizes, size_t wall_page_count,
                               wl_indexed_surface *dst, int32_t *directions_x,
                               int32_t *directions_y, wl_map_wall_hit *hits,
                               wl_wall_strip *strips) {
    if (!wall_pages || !wall_page_sizes || !dst || !directions_x || !directions_y ||
        !hits || !strips || ray_count == 0) {
        return -1;
    }

    if (wl_build_camera_ray_directions(forward_x, forward_y, plane_x, plane_y,
                                       dst->width, first_x, x_step, ray_count,
                                       directions_x, directions_y) != 0) {
        return -1;
    }

    if (wl_cast_projected_wall_ray_batch(wall_plane, wall_count, origin_x, origin_y,
                                         directions_x, directions_y, ray_count,
                                         first_x, x_step, dst->width, dst->height,
                                         hits) != 0) {
        return -1;
    }

    for (size_t i = 0; i < ray_count; ++i) {
        uint16_t page = hits[i].wall_page_index;
        if (page >= wall_page_count || !wall_pages[page] || wall_page_sizes[page] == 0) {
            return -1;
        }
        if (wl_wall_hit_to_strip(&hits[i], wall_pages[page], wall_page_sizes[page],
                                 &strips[i]) != 0) {
            return -1;
        }
    }

    return wl_render_wall_strip_viewport(strips, ray_count, dst);
}

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
                                wl_projected_sprite *sprites, uint16_t *wall_heights) {
    if (!dst || !wall_heights || (sprite_count != 0 &&
        (!sprite_surfaces || !sprite_x || !sprite_y || !sprites))) {
        return -1;
    }

    if (wl_render_camera_wall_view(wall_plane, wall_count, origin_x, origin_y,
                                   forward_x, forward_y, plane_x, plane_y,
                                   first_x, x_step, ray_count, wall_pages,
                                   wall_page_sizes, wall_page_count, dst,
                                   directions_x, directions_y, hits, strips) != 0) {
        return -1;
    }

    for (uint16_t x = 0; x < dst->width; ++x) {
        wall_heights[x] = 0;
    }
    for (size_t i = 0; i < ray_count; ++i) {
        uint16_t x0 = hits[i].x;
        uint16_t x1 = (uint16_t)(x0 + x_step);
        if (x1 > dst->width) {
            x1 = dst->width;
        }
        for (uint16_t x = x0; x < x1; ++x) {
            wall_heights[x] = hits[i].scaled_height;
        }
    }

    for (size_t i = 0; i < sprite_count; ++i) {
        if (!sprite_surfaces[i]) {
            return -1;
        }
        uint16_t source_index = sprite_source_indices ? sprite_source_indices[i] : (uint16_t)i;
        if (wl_project_world_sprite(source_index, origin_x, origin_y, sprite_x[i], sprite_y[i],
                                    forward_x, forward_y, dst->width, dst->height,
                                    &sprites[i]) != 0) {
            return -1;
        }
        sprites[i].surface_index = i;
    }
    if (wl_sort_projected_sprites_far_to_near(sprites, sprite_count) != 0) {
        return -1;
    }
    for (size_t i = 0; i < sprite_count; ++i) {
        if (!sprites[i].visible) {
            continue;
        }
        size_t surface_index = sprites[i].surface_index;
        if (surface_index >= sprite_count || !sprite_surfaces[surface_index]) {
            return -1;
        }
        if (wl_render_scaled_sprite(sprite_surfaces[surface_index], dst, sprites[i].view_x,
                                    sprites[i].scaled_height, transparent_index,
                                    wall_heights, dst->width) != 0) {
            return -1;
        }
    }
    return 0;
}

int wl_carmack_expand(const unsigned char *src, size_t src_len, size_t expanded_bytes,
                      uint16_t *out, size_t out_words, size_t *words_written) {
    const uint16_t near_tag = 0xa7;
    const uint16_t far_tag = 0xa8;

    if (!src || !out || (expanded_bytes % 2) != 0) {
        return -1;
    }

    size_t target_words = expanded_bytes / 2;
    if (target_words > out_words) {
        return -1;
    }

    size_t src_pos = 0;
    size_t out_pos = 0;
    while (out_pos < target_words) {
        if (src_pos + 2 > src_len) {
            return -1;
        }

        uint16_t ch = read_le16(src + src_pos);
        src_pos += 2;
        uint16_t ch_high = ch >> 8;
        if (ch_high == near_tag) {
            uint16_t count = ch & 0xff;
            if (count == 0) {
                if (src_pos >= src_len) {
                    return -1;
                }
                out[out_pos++] = ch | src[src_pos++];
            } else {
                if (src_pos >= src_len) {
                    return -1;
                }
                size_t offset = src[src_pos++];
                if (offset == 0 || offset > out_pos || out_pos + count > target_words) {
                    return -1;
                }
                size_t copy_pos = out_pos - offset;
                while (count--) {
                    if (copy_pos >= out_pos) {
                        return -1;
                    }
                    out[out_pos++] = out[copy_pos++];
                }
            }
        } else if (ch_high == far_tag) {
            uint16_t count = ch & 0xff;
            if (count == 0) {
                if (src_pos >= src_len) {
                    return -1;
                }
                out[out_pos++] = ch | src[src_pos++];
            } else {
                if (src_pos + 2 > src_len || out_pos + count > target_words) {
                    return -1;
                }
                size_t copy_pos = read_le16(src + src_pos);
                src_pos += 2;
                while (count--) {
                    if (copy_pos >= out_pos) {
                        return -1;
                    }
                    out[out_pos++] = out[copy_pos++];
                }
            }
        } else {
            out[out_pos++] = ch;
        }
    }

    if (words_written) {
        *words_written = out_pos;
    }
    return 0;
}

int wl_rlew_expand(const uint16_t *src, size_t src_words, uint16_t rlew_tag,
                   size_t expanded_bytes, uint16_t *out, size_t out_words,
                   size_t *words_written) {
    if (!src || !out || (expanded_bytes % 2) != 0) {
        return -1;
    }

    size_t target_words = expanded_bytes / 2;
    if (target_words > out_words) {
        return -1;
    }

    size_t src_pos = 0;
    size_t out_pos = 0;
    while (out_pos < target_words) {
        if (src_pos >= src_words) {
            return -1;
        }
        uint16_t value = src[src_pos++];
        if (value != rlew_tag) {
            out[out_pos++] = value;
        } else {
            if (src_pos + 2 > src_words) {
                return -1;
            }
            uint16_t count = src[src_pos++];
            value = src[src_pos++];
            if (out_pos + count > target_words) {
                return -1;
            }
            while (count--) {
                out[out_pos++] = value;
            }
        }
    }

    if (words_written) {
        *words_written = out_pos;
    }
    return 0;
}

int wl_read_map_plane(const char *gamemaps_path, const wl_map_header *header,
                      size_t plane_index, uint16_t rlew_tag,
                      uint16_t *out, size_t out_words) {
    if (!gamemaps_path || !header || !out || plane_index >= WL_MAP_PLANE_COUNT ||
        out_words < WL_MAP_PLANE_WORDS) {
        return -1;
    }

    uint16_t compressed_len = header->plane_lengths[plane_index];
    if (compressed_len < 2) {
        return -1;
    }

    unsigned char *compressed = (unsigned char *)malloc(compressed_len);
    if (!compressed) {
        return -1;
    }

    int ok = read_exact_at(gamemaps_path, (long)header->plane_starts[plane_index],
                           compressed, compressed_len);
    if (ok != 0) {
        free(compressed);
        return -1;
    }

    size_t carmack_bytes = read_le16(compressed);
    if ((carmack_bytes % 2) != 0 || carmack_bytes < 2) {
        free(compressed);
        return -1;
    }

    size_t carmack_words = carmack_bytes / 2;
    uint16_t *rlew_words = (uint16_t *)malloc(carmack_words * sizeof(*rlew_words));
    if (!rlew_words) {
        free(compressed);
        return -1;
    }

    size_t words_written = 0;
    ok = wl_carmack_expand(compressed + 2, compressed_len - 2, carmack_bytes,
                           rlew_words, carmack_words, &words_written);
    free(compressed);
    if (ok != 0 || words_written != carmack_words || rlew_words[0] != WL_MAP_PLANE_WORDS * 2) {
        free(rlew_words);
        return -1;
    }

    ok = wl_rlew_expand(rlew_words + 1, carmack_words - 1, rlew_tag,
                        WL_MAP_PLANE_WORDS * 2, out, out_words, &words_written);
    free(rlew_words);
    if (ok != 0 || words_written != WL_MAP_PLANE_WORDS) {
        return -1;
    }

    return 0;
}

int wl_read_audio_header(const char *path, wl_audio_header *out) {
    if (!path || !out) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    size_t file_size = 0;
    if (wl_file_size(path, &file_size) != 0) {
        return -1;
    }
    if (file_size < 4 || (file_size % 4) != 0) {
        return -1;
    }
    size_t offset_count = file_size / 4;
    if (offset_count < 2 || offset_count - 1 > WL_AUDIO_MAX_CHUNKS) {
        return -1;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    out->chunk_count = offset_count - 1;
    out->file_size = file_size;
    unsigned char buf[4];
    for (size_t i = 0; i < offset_count; ++i) {
        if (fread(buf, 1, 4, f) != 4) {
            fclose(f);
            return -1;
        }
        out->offsets[i] = read_le32(buf);
    }
    fclose(f);
    return 0;
}

int wl_read_audio_chunk(const char *audiot_path,
                        const wl_audio_header *header,
                        size_t chunk_index,
                        unsigned char *out, size_t out_size,
                        size_t *bytes_read) {
    if (!audiot_path || !header || !out || !bytes_read) {
        return -1;
    }
    *bytes_read = 0;
    if (chunk_index >= header->chunk_count) {
        return -1;
    }
    uint32_t start = header->offsets[chunk_index];
    uint32_t end = header->offsets[chunk_index + 1];
    if (end < start) {
        return -1;
    }
    uint32_t chunk_size = end - start;
    if (chunk_size == 0) {
        return 0;
    }
    if (chunk_size > out_size) {
        return -1;
    }
    FILE *f = fopen(audiot_path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, (long)start, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    size_t read_count = fread(out, 1, chunk_size, f);
    fclose(f);
    if (read_count != chunk_size) {
        return -1;
    }
    *bytes_read = chunk_size;
    return 0;
}

int wl_describe_audio_chunk(size_t chunk_index,
                            const unsigned char *chunk, size_t chunk_size,
                            wl_audio_chunk_metadata *out) {
    return wl_describe_audio_chunk_with_ranges(chunk_index, 87, 87, 87,
                                               chunk, chunk_size, out);
}

int wl_describe_audio_chunk_with_ranges(size_t chunk_index,
                                        size_t pc_speaker_count,
                                        size_t adlib_count,
                                        size_t digital_count,
                                        const unsigned char *chunk,
                                        size_t chunk_size,
                                        wl_audio_chunk_metadata *out) {
    size_t adlib_start = pc_speaker_count;
    size_t digital_start = adlib_start + adlib_count;
    size_t music_start = digital_start + digital_count;
    if (!out || (chunk_size > 0 && !chunk) || adlib_start < pc_speaker_count ||
        digital_start < adlib_start || music_start < digital_start) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->raw_size = chunk_size;
    if (chunk_index < adlib_start) {
        out->kind = WL_AUDIO_CHUNK_PC_SPEAKER;
    } else if (chunk_index < digital_start) {
        out->kind = WL_AUDIO_CHUNK_ADLIB;
    } else if (chunk_index < music_start) {
        out->kind = WL_AUDIO_CHUNK_DIGITAL;
    } else {
        out->kind = WL_AUDIO_CHUNK_MUSIC;
    }
    if (chunk_size == 0) {
        out->is_empty = 1;
        return 0;
    }

    if (chunk_size < sizeof(uint32_t)) {
        return -1;
    }
    out->declared_length = read_le32(chunk);
    out->payload_offset = sizeof(uint32_t);
    if (out->kind == WL_AUDIO_CHUNK_PC_SPEAKER || out->kind == WL_AUDIO_CHUNK_ADLIB) {
        if (chunk_size < sizeof(uint32_t) + sizeof(uint16_t)) {
            return -1;
        }
        out->priority = read_le16(chunk + sizeof(uint32_t));
        out->payload_offset += sizeof(uint16_t);
    }
    if (out->payload_offset > chunk_size) {
        return -1;
    }
    out->payload_size = chunk_size - out->payload_offset;
    return 0;
}

int wl_summarize_audio_range(const wl_audio_header *header,
                             size_t start_chunk, size_t chunk_count,
                             wl_audio_range_summary *out) {
    if (!header || !out || start_chunk > header->chunk_count ||
        chunk_count > header->chunk_count - start_chunk) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->start_chunk = start_chunk;
    out->chunk_count = chunk_count;
    out->first_non_empty_chunk = header->chunk_count;
    for (size_t i = 0; i < chunk_count; ++i) {
        size_t chunk_index = start_chunk + i;
        uint32_t start = header->offsets[chunk_index];
        uint32_t end = header->offsets[chunk_index + 1u];
        size_t bytes;
        if (end < start) {
            return -1;
        }
        bytes = (size_t)(end - start);
        out->total_bytes += bytes;
        if (bytes > 0) {
            ++out->non_empty_chunks;
            if (out->first_non_empty_chunk == header->chunk_count) {
                out->first_non_empty_chunk = chunk_index;
            }
            out->last_non_empty_chunk = chunk_index;
            if (bytes > out->largest_chunk_bytes) {
                out->largest_chunk = chunk_index;
                out->largest_chunk_bytes = bytes;
            }
        }
    }
    if (out->non_empty_chunks == 0) {
        out->first_non_empty_chunk = start_chunk + chunk_count;
    }
    return 0;
}

int wl_describe_sound_priority_decision(uint8_t current_active,
                                        uint16_t current_priority,
                                        uint16_t candidate_priority,
                                        wl_sound_priority_decision *out) {
    if (!out || current_active > 1u) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->current_active = current_active;
    out->current_priority = current_priority;
    out->candidate_priority = candidate_priority;
    out->should_start = (!current_active || candidate_priority >= current_priority) ? 1u : 0u;
    return 0;
}

int wl_describe_sound_channel_decision(uint8_t current_active,
                                       size_t current_chunk,
                                       uint16_t current_priority,
                                       size_t candidate_chunk,
                                       const wl_audio_chunk_metadata *candidate,
                                       wl_sound_channel_decision *out) {
    wl_sound_priority_decision priority;
    if (!out || !candidate || current_active > 1u || candidate->is_empty ||
        (candidate->kind != WL_AUDIO_CHUNK_PC_SPEAKER && candidate->kind != WL_AUDIO_CHUNK_ADLIB)) {
        return -1;
    }
    if (wl_describe_sound_priority_decision(current_active, current_priority,
                                            candidate->priority, &priority) != 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->current_active = current_active;
    out->current_chunk = current_chunk;
    out->current_priority = current_priority;
    out->candidate_chunk = candidate_chunk;
    out->candidate_kind = candidate->kind;
    out->candidate_priority = candidate->priority;
    out->should_start = priority.should_start;
    out->next_active = current_active;
    out->next_chunk = current_chunk;
    out->next_priority = current_priority;
    if (priority.should_start) {
        out->next_active = 1u;
        out->next_chunk = candidate_chunk;
        out->next_priority = candidate->priority;
    }
    return 0;
}

int wl_start_sound_channel(const wl_sound_channel_state *current,
                           uint16_t candidate_sound_index,
                           uint16_t candidate_priority,
                           wl_sound_channel_start_result *out) {
    wl_sound_priority_decision decision;
    if (!current || !out || current->active > 1u) {
        return -1;
    }
    if (wl_describe_sound_priority_decision(current->active, current->priority,
                                            candidate_priority, &decision) != 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->state = *current;
    if (!decision.should_start) {
        return 0;
    }
    out->started = 1u;
    out->replaced = current->active ? 1u : 0u;
    out->state.active = 1u;
    out->state.sound_index = candidate_sound_index;
    out->state.priority = candidate_priority;
    out->state.sample_position = 0;
    return 0;
}

int wl_start_sound_channel_from_chunk(const wl_sound_channel_state *current,
                                      size_t candidate_chunk,
                                      const wl_audio_chunk_metadata *candidate,
                                      wl_sound_channel_start_result *out) {
    if (!current || !candidate || !out || candidate_chunk > UINT16_MAX ||
        candidate->is_empty ||
        (candidate->kind != WL_AUDIO_CHUNK_PC_SPEAKER &&
         candidate->kind != WL_AUDIO_CHUNK_ADLIB)) {
        return -1;
    }
    return wl_start_sound_channel(current, (uint16_t)candidate_chunk,
                                  candidate->priority, out);
}

int wl_schedule_sound_channel(const wl_sound_channel_state *current,
                              uint16_t candidate_sound_index,
                              const wl_audio_chunk_metadata *candidate,
                              wl_sound_channel_schedule_result *out) {
    wl_sound_channel_decision decision;
    if (!current || !candidate || !out || current->active > 1u) {
        return -1;
    }
    if (wl_describe_sound_channel_decision(current->active, current->sound_index,
                                           current->priority, candidate_sound_index,
                                           candidate, &decision) != 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->state = *current;
    out->candidate_kind = decision.candidate_kind;
    out->candidate_priority = decision.candidate_priority;
    if (!decision.should_start) {
        out->held = 1u;
        return 0;
    }
    out->started = 1u;
    out->replaced = current->active ? 1u : 0u;
    out->state.active = 1u;
    out->state.sound_index = candidate_sound_index;
    out->state.priority = candidate->priority;
    out->state.sample_position = 0;
    return 0;
}

int wl_schedule_sound_channel_from_chunk(const wl_sound_channel_state *current,
                                         size_t candidate_chunk,
                                         const wl_audio_chunk_metadata *candidate,
                                         wl_sound_channel_schedule_result *out) {
    if (candidate_chunk > UINT16_MAX) {
        return -1;
    }
    return wl_schedule_sound_channel(current, (uint16_t)candidate_chunk, candidate, out);
}

int wl_advance_sound_channel(const wl_sound_channel_state *current,
                             size_t sample_count,
                             uint32_t sample_delta,
                             wl_sound_channel_advance_result *out) {
    wl_sound_channel_state state;
    size_t remaining;
    if (!current || !out || current->active > 1u) {
        return -1;
    }
    state = *current;
    if (state.active && state.sample_position > sample_count) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->state = state;
    if (!state.active) {
        return 0;
    }
    remaining = sample_count - state.sample_position;
    if ((size_t)sample_delta < remaining) {
        out->samples_consumed = sample_delta;
        out->state.sample_position += (size_t)sample_delta;
        return 0;
    }
    out->samples_consumed = (uint32_t)remaining;
    out->completed = 1u;
    out->state.active = 0;
    out->state.sample_position = sample_count;
    return 0;
}

int wl_advance_sound_channel_from_chunk(const wl_sound_channel_state *current,
                                        const wl_audio_chunk_metadata *metadata,
                                        const unsigned char *chunk, size_t chunk_size,
                                        uint32_t sample_delta,
                                        wl_sound_channel_advance_result *out) {
    uint32_t sample_count;
    if (!metadata || !chunk || metadata->is_empty ||
        (metadata->kind != WL_AUDIO_CHUNK_PC_SPEAKER &&
         metadata->kind != WL_AUDIO_CHUNK_ADLIB) ||
        metadata->payload_size == 0 || metadata->payload_size > chunk_size ||
        chunk_size < sizeof(uint32_t)) {
        return -1;
    }
    sample_count = read_le32(chunk);
    if (metadata->kind == WL_AUDIO_CHUNK_PC_SPEAKER) {
        const size_t payload_offset = sizeof(uint32_t) + sizeof(uint16_t);
        if (chunk_size < payload_offset || (size_t)sample_count >= chunk_size - payload_offset) {
            return -1;
        }
    } else {
        const size_t payload_offset = sizeof(uint32_t) + sizeof(uint16_t) + 16u;
        if (chunk_size < payload_offset || (size_t)sample_count > chunk_size - payload_offset) {
            return -1;
        }
    }
    return wl_advance_sound_channel(current, (size_t)sample_count, sample_delta, out);
}

int wl_tick_sound_channel(const wl_sound_channel_state *current,
                          wl_audio_chunk_kind kind,
                          const unsigned char *chunk, size_t chunk_size,
                          uint32_t sample_delta,
                          wl_sound_channel_tick_result *out) {
    wl_pc_speaker_playback_cursor pc_cursor;
    wl_adlib_playback_cursor adlib_cursor;
    wl_sound_channel_state state;
    if (!current || !out || current->active > 1u ||
        (kind != WL_AUDIO_CHUNK_PC_SPEAKER && kind != WL_AUDIO_CHUNK_ADLIB)) {
        return -1;
    }
    state = *current;
    memset(out, 0, sizeof(*out));
    out->state = state;
    if (!state.active) {
        return 0;
    }
    if (kind == WL_AUDIO_CHUNK_PC_SPEAKER) {
        if (wl_advance_pc_speaker_playback_cursor(chunk, chunk_size,
                                                  state.sample_position,
                                                  sample_delta, &pc_cursor) != 0) {
            return -1;
        }
        out->state.sample_position = pc_cursor.sample_index;
        out->samples_consumed = pc_cursor.samples_consumed;
        out->current_sample = pc_cursor.current_sample;
        out->completed = pc_cursor.completed;
    } else {
        if (wl_advance_adlib_playback_cursor(chunk, chunk_size,
                                             state.sample_position,
                                             sample_delta, &adlib_cursor) != 0) {
            return -1;
        }
        out->state.sample_position = adlib_cursor.sample_index;
        out->samples_consumed = adlib_cursor.samples_consumed;
        out->current_sample = adlib_cursor.current_sample;
        out->completed = adlib_cursor.completed;
    }
    if (out->completed) {
        out->state.active = 0;
    }
    return 0;
}

int wl_tick_sound_channel_from_chunk(const wl_sound_channel_state *current,
                                     const wl_audio_chunk_metadata *metadata,
                                     const unsigned char *chunk, size_t chunk_size,
                                     uint32_t sample_delta,
                                     wl_sound_channel_tick_result *out) {
    if (!metadata || metadata->is_empty ||
        (metadata->kind != WL_AUDIO_CHUNK_PC_SPEAKER &&
         metadata->kind != WL_AUDIO_CHUNK_ADLIB) ||
        metadata->payload_size == 0 || metadata->payload_size > chunk_size) {
        return -1;
    }
    return wl_tick_sound_channel(current, metadata->kind, chunk, chunk_size,
                                 sample_delta, out);
}

static int describe_sample_playback_window(size_t sample_count,
                                           int (*getter)(const unsigned char *, size_t, size_t, uint8_t *),
                                           const unsigned char *chunk, size_t chunk_size,
                                           size_t start_sample, size_t sample_budget,
                                           wl_sample_playback_window *out) {
    size_t remaining;
    if (!chunk || !getter || !out || start_sample > sample_count) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->start_sample = start_sample;
    out->samples_available = sample_count - start_sample;
    remaining = out->samples_available;
    out->samples_in_window = sample_budget < remaining ? sample_budget : remaining;
    out->next_sample = start_sample + out->samples_in_window;
    out->completed = (out->next_sample == sample_count) ? 1u : 0u;
    if (out->samples_in_window > 0) {
        if (getter(chunk, chunk_size, start_sample, &out->first_sample) != 0 ||
            getter(chunk, chunk_size, out->next_sample - 1u, &out->last_sample) != 0) {
            return -1;
        }
    }
    return 0;
}


static int describe_sample_playback_position(size_t sample_count,
                                             int (*getter)(const unsigned char *, size_t, size_t, uint8_t *),
                                             const unsigned char *chunk, size_t chunk_size,
                                             size_t sample_position,
                                             wl_sample_playback_position *out) {
    if (!getter || !chunk || !out || sample_position > sample_count) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->sample_position = sample_position;
    out->sample_count = sample_count;
    if (sample_position == sample_count) {
        out->completed = 1u;
        return 0;
    }
    return getter(chunk, chunk_size, sample_position, &out->current_sample);
}

static int advance_sample_playback_cursor(size_t sample_count,
                                          int (*getter)(const unsigned char *, size_t, size_t, uint8_t *),
                                          const unsigned char *chunk, size_t chunk_size,
                                          size_t start_sample, uint32_t sample_delta,
                                          size_t *out_sample_index, uint32_t *out_consumed,
                                          uint8_t *out_current_sample, uint8_t *out_completed) {
    size_t next_sample;
    uint32_t consumed;
    if (!getter || !chunk || !out_sample_index || !out_consumed || !out_current_sample ||
        !out_completed || start_sample > sample_count) {
        return -1;
    }
    *out_sample_index = start_sample;
    *out_consumed = 0;
    *out_current_sample = 0;
    *out_completed = 0;
    if (start_sample == sample_count) {
        *out_completed = 1u;
        return (sample_delta == 0u) ? 0 : -1;
    }
    consumed = sample_delta;
    if ((uint64_t)start_sample + (uint64_t)consumed > (uint64_t)sample_count) {
        consumed = (uint32_t)(sample_count - start_sample);
    }
    next_sample = start_sample + (size_t)consumed;
    *out_sample_index = next_sample;
    *out_consumed = consumed;
    if (next_sample >= sample_count) {
        *out_completed = 1u;
        return 0;
    }
    if (getter(chunk, chunk_size, next_sample, out_current_sample) != 0) {
        return -1;
    }
    return 0;
}

int wl_describe_pc_speaker_sound(const unsigned char *chunk, size_t chunk_size,
                                  wl_pc_speaker_sound_metadata *out) {
    uint32_t sample_count;
    size_t payload_offset = sizeof(uint32_t) + sizeof(uint16_t);
    if (!chunk || !out || chunk_size < payload_offset) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    sample_count = read_le32(chunk);
    if ((size_t)sample_count > chunk_size - payload_offset) {
        return -1;
    }
    if ((size_t)sample_count == chunk_size - payload_offset) {
        return -1;
    }
    out->sample_count = sample_count;
    if (sample_count > 0) {
        if (wl_get_pc_speaker_sound_sample(chunk, chunk_size, 0, &out->first_sample) != 0 ||
            wl_get_pc_speaker_sound_sample(chunk, chunk_size, (size_t)sample_count - 1u,
                                           &out->last_sample) != 0) {
            return -1;
        }
    }
    out->terminator = chunk[payload_offset + (size_t)sample_count];
    out->trailing_bytes = chunk_size - payload_offset - (size_t)sample_count - 1u;
    return 0;
}

int wl_get_pc_speaker_sound_sample(const unsigned char *chunk, size_t chunk_size,
                                   size_t sample_index, uint8_t *out_sample) {
    uint32_t sample_count;
    const size_t payload_offset = sizeof(uint32_t) + sizeof(uint16_t);
    if (!chunk || !out_sample || chunk_size < payload_offset) {
        return -1;
    }
    sample_count = read_le32(chunk);
    if ((size_t)sample_count >= chunk_size - payload_offset ||
        sample_index >= (size_t)sample_count) {
        return -1;
    }
    *out_sample = chunk[payload_offset + sample_index];
    return 0;
}

int wl_advance_pc_speaker_playback_cursor(const unsigned char *chunk, size_t chunk_size,
                                          size_t start_sample, uint32_t sample_delta,
                                          wl_pc_speaker_playback_cursor *out) {
    uint32_t sample_count;
    const size_t payload_offset = sizeof(uint32_t) + sizeof(uint16_t);
    if (!chunk || !out || chunk_size < payload_offset) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    sample_count = read_le32(chunk);
    if ((size_t)sample_count >= chunk_size - payload_offset) {
        return -1;
    }
    return advance_sample_playback_cursor((size_t)sample_count, wl_get_pc_speaker_sound_sample,
                                          chunk, chunk_size, start_sample, sample_delta,
                                          &out->sample_index, &out->samples_consumed,
                                          &out->current_sample, &out->completed);
}

int wl_describe_pc_speaker_playback_window(const unsigned char *chunk, size_t chunk_size,
                                           size_t start_sample, size_t sample_budget,
                                           wl_sample_playback_window *out) {
    uint32_t sample_count;
    const size_t payload_offset = sizeof(uint32_t) + sizeof(uint16_t);
    if (!chunk || chunk_size < payload_offset) {
        return -1;
    }
    sample_count = read_le32(chunk);
    if ((size_t)sample_count >= chunk_size - payload_offset) {
        return -1;
    }
    return describe_sample_playback_window((size_t)sample_count, wl_get_pc_speaker_sound_sample,
                                           chunk, chunk_size, start_sample, sample_budget, out);
}

int wl_describe_pc_speaker_playback_position(const unsigned char *chunk, size_t chunk_size,
                                             size_t sample_position,
                                             wl_sample_playback_position *out) {
    uint32_t sample_count;
    const size_t payload_offset = sizeof(uint32_t) + sizeof(uint16_t);
    if (!chunk || chunk_size < payload_offset) {
        return -1;
    }
    sample_count = read_le32(chunk);
    if ((size_t)sample_count >= chunk_size - payload_offset) {
        return -1;
    }
    return describe_sample_playback_position((size_t)sample_count, wl_get_pc_speaker_sound_sample,
                                             chunk, chunk_size, sample_position, out);
}

int wl_describe_adlib_sound(const unsigned char *chunk, size_t chunk_size,
                            wl_adlib_sound_metadata *out) {
    uint32_t sample_count;
    const size_t common_bytes = sizeof(uint32_t) + sizeof(uint16_t);
    const size_t instrument_bytes = 16u;
    const size_t data_offset = common_bytes + instrument_bytes;
    if (!chunk || !out || chunk_size < data_offset) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    sample_count = read_le32(chunk);
    if ((size_t)sample_count > chunk_size - data_offset) {
        return -1;
    }
    out->sample_count = sample_count;
    out->priority = read_le16(chunk + sizeof(uint32_t));
    out->instrument_bytes = (uint8_t)instrument_bytes;
    if (wl_get_adlib_instrument_byte(chunk, chunk_size, 0, &out->first_instrument_byte) != 0 ||
        wl_get_adlib_instrument_byte(chunk, chunk_size, instrument_bytes - 1u,
                                     &out->last_instrument_byte) != 0) {
        return -1;
    }
    if (sample_count > 0) {
        if (wl_get_adlib_sound_sample(chunk, chunk_size, 0, &out->first_sample) != 0 ||
            wl_get_adlib_sound_sample(chunk, chunk_size, (size_t)sample_count - 1u,
                                      &out->last_sample) != 0) {
            return -1;
        }
    }
    out->trailing_bytes = chunk_size - data_offset - (size_t)sample_count;
    return 0;
}

int wl_describe_adlib_instrument_registers(const unsigned char *chunk, size_t chunk_size,
                                           wl_adlib_instrument_registers *out) {
    static const uint8_t base_regs[5] = {0x20u, 0x40u, 0x60u, 0x80u, 0xe0u};
    const size_t common_bytes = sizeof(uint32_t) + sizeof(uint16_t);
    const size_t data_offset = common_bytes + 16u;
    if (!chunk || !out || chunk_size < data_offset) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < 5u; ++i) {
        out->modulator_regs[i] = base_regs[i];
        out->modulator_values[i] = chunk[common_bytes + (i * 2u)];
        out->carrier_regs[i] = (uint8_t)(base_regs[i] + 3u);
        out->carrier_values[i] = chunk[common_bytes + (i * 2u) + 1u];
    }
    out->feedback_reg = 0xc0u;
    out->feedback_value = chunk[common_bytes + 10u];
    out->voice = chunk[common_bytes + 11u];
    out->mode = chunk[common_bytes + 12u];
    return 0;
}

int wl_get_adlib_instrument_byte(const unsigned char *chunk, size_t chunk_size,
                                 size_t instrument_index, uint8_t *out_byte) {
    const size_t common_bytes = sizeof(uint32_t) + sizeof(uint16_t);
    const size_t instrument_bytes = 16u;
    const size_t data_offset = common_bytes + instrument_bytes;
    if (!chunk || !out_byte || chunk_size < data_offset || instrument_index >= instrument_bytes) {
        return -1;
    }
    *out_byte = chunk[common_bytes + instrument_index];
    return 0;
}

int wl_get_adlib_sound_sample(const unsigned char *chunk, size_t chunk_size,
                              size_t sample_index, uint8_t *out_sample) {
    uint32_t sample_count;
    const size_t common_bytes = sizeof(uint32_t) + sizeof(uint16_t);
    const size_t data_offset = common_bytes + 16u;
    if (!chunk || !out_sample || chunk_size < data_offset) {
        return -1;
    }
    sample_count = read_le32(chunk);
    if ((size_t)sample_count > chunk_size - data_offset || sample_index >= (size_t)sample_count) {
        return -1;
    }
    *out_sample = chunk[data_offset + sample_index];
    return 0;
}

int wl_advance_adlib_playback_cursor(const unsigned char *chunk, size_t chunk_size,
                                     size_t start_sample, uint32_t sample_delta,
                                     wl_adlib_playback_cursor *out) {
    uint32_t sample_count;
    const size_t common_bytes = sizeof(uint32_t) + sizeof(uint16_t);
    const size_t data_offset = common_bytes + 16u;
    if (!chunk || !out || chunk_size < data_offset) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    sample_count = read_le32(chunk);
    if ((size_t)sample_count > chunk_size - data_offset) {
        return -1;
    }
    return advance_sample_playback_cursor((size_t)sample_count, wl_get_adlib_sound_sample,
                                          chunk, chunk_size, start_sample, sample_delta,
                                          &out->sample_index, &out->samples_consumed,
                                          &out->current_sample, &out->completed);
}

int wl_describe_adlib_playback_window(const unsigned char *chunk, size_t chunk_size,
                                      size_t start_sample, size_t sample_budget,
                                      wl_sample_playback_window *out) {
    uint32_t sample_count;
    const size_t common_bytes = sizeof(uint32_t) + sizeof(uint16_t);
    const size_t data_offset = common_bytes + 16u;
    if (!chunk || chunk_size < data_offset) {
        return -1;
    }
    sample_count = read_le32(chunk);
    if ((size_t)sample_count > chunk_size - data_offset) {
        return -1;
    }
    return describe_sample_playback_window((size_t)sample_count, wl_get_adlib_sound_sample,
                                           chunk, chunk_size, start_sample, sample_budget, out);
}

int wl_describe_adlib_playback_position(const unsigned char *chunk, size_t chunk_size,
                                        size_t sample_position,
                                        wl_sample_playback_position *out) {
    uint32_t sample_count;
    const size_t common_bytes = sizeof(uint32_t) + sizeof(uint16_t);
    const size_t data_offset = common_bytes + 16u;
    if (!chunk || chunk_size < data_offset) {
        return -1;
    }
    sample_count = read_le32(chunk);
    if ((size_t)sample_count > chunk_size - data_offset) {
        return -1;
    }
    return describe_sample_playback_position((size_t)sample_count, wl_get_adlib_sound_sample,
                                             chunk, chunk_size, sample_position, out);
}

int wl_describe_sound_sample_count_from_chunk(const wl_audio_chunk_metadata *metadata,
                                             const unsigned char *chunk, size_t chunk_size,
                                             size_t *out_sample_count) {
    wl_pc_speaker_sound_metadata pc_meta;
    wl_adlib_sound_metadata adlib_meta;
    if (!metadata || !out_sample_count || metadata->is_empty ||
        metadata->payload_size == 0 || metadata->payload_size > chunk_size) {
        return -1;
    }
    if (metadata->kind == WL_AUDIO_CHUNK_PC_SPEAKER) {
        if (wl_describe_pc_speaker_sound(chunk, chunk_size, &pc_meta) != 0) {
            return -1;
        }
        *out_sample_count = pc_meta.sample_count;
        return 0;
    }
    if (metadata->kind == WL_AUDIO_CHUNK_ADLIB) {
        if (wl_describe_adlib_sound(chunk, chunk_size, &adlib_meta) != 0) {
            return -1;
        }
        *out_sample_count = adlib_meta.sample_count;
        return 0;
    }
    return -1;
}

int wl_describe_sound_playback_position_from_chunk(const wl_audio_chunk_metadata *metadata,
                                                   const unsigned char *chunk, size_t chunk_size,
                                                   size_t sample_position,
                                                   wl_sample_playback_position *out) {
    if (!metadata || metadata->is_empty ||
        metadata->payload_size == 0 || metadata->payload_size > chunk_size) {
        return -1;
    }
    if (metadata->kind == WL_AUDIO_CHUNK_PC_SPEAKER) {
        return wl_describe_pc_speaker_playback_position(chunk, chunk_size,
                                                        sample_position, out);
    }
    if (metadata->kind == WL_AUDIO_CHUNK_ADLIB) {
        return wl_describe_adlib_playback_position(chunk, chunk_size,
                                                   sample_position, out);
    }
    return -1;
}

int wl_describe_sound_playback_window_from_chunk(
    const wl_audio_chunk_metadata *metadata, const unsigned char *chunk,
    size_t chunk_size, size_t start_sample, size_t sample_budget,
    wl_sample_playback_window *out) {
    if (!metadata || !out || metadata->is_empty || metadata->payload_size == 0 ||
        metadata->payload_size > chunk_size) {
        return -1;
    }
    if (metadata->kind == WL_AUDIO_CHUNK_PC_SPEAKER) {
        return wl_describe_pc_speaker_playback_window(
            chunk, chunk_size, start_sample, sample_budget, out);
    }
    if (metadata->kind == WL_AUDIO_CHUNK_ADLIB) {
        return wl_describe_adlib_playback_window(
            chunk, chunk_size, start_sample, sample_budget, out);
    }
    return -1;
}

int wl_describe_imf_music_chunk(const unsigned char *chunk, size_t chunk_size,
                                wl_imf_music_metadata *out) {
    uint64_t total_delay = 0;
    if (!out || !chunk) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (chunk_size < sizeof(uint32_t)) {
        return -1;
    }
    out->declared_bytes = read_le32(chunk);
    if ((out->declared_bytes % 4u) != 0u) {
        return -1;
    }
    if ((size_t)out->declared_bytes > chunk_size - sizeof(uint32_t)) {
        return -1;
    }
    out->command_count = (size_t)out->declared_bytes / 4u;
    out->trailing_bytes = chunk_size - sizeof(uint32_t) - (size_t)out->declared_bytes;
    if (out->command_count > 0) {
        wl_imf_music_command command;
        const unsigned char *payload = chunk + sizeof(uint32_t);
        out->first_register = payload[0];
        out->first_value = payload[1];
        out->first_delay = read_le16(payload + 2);
        for (size_t i = 0; i < out->command_count; ++i) {
            if (wl_get_imf_music_command(chunk, chunk_size, i, &command) != 0) {
                return -1;
            }
            total_delay += command.delay;
            if (command.delay == 0) {
                ++out->zero_delay_count;
            }
            if (command.delay > out->max_delay) {
                out->max_delay = command.delay;
            }
        }
        if (wl_get_imf_music_command(chunk, chunk_size, out->command_count - 1u, &command) != 0) {
            return -1;
        }
        out->last_register = command.reg;
        out->last_value = command.value;
        out->last_delay = command.delay;
        if (total_delay > UINT32_MAX) {
            return -1;
        }
        out->total_delay = (uint32_t)total_delay;
    }
    return 0;
}

int wl_get_imf_music_command(const unsigned char *chunk, size_t chunk_size,
                             size_t command_index, wl_imf_music_command *out) {
    uint32_t declared_bytes;
    const unsigned char *cmd;
    if (!chunk || !out || chunk_size < sizeof(uint32_t)) {
        return -1;
    }
    declared_bytes = read_le32(chunk);
    if ((declared_bytes % 4u) != 0u || (size_t)declared_bytes > chunk_size - sizeof(uint32_t) ||
        command_index >= (size_t)declared_bytes / 4u) {
        return -1;
    }
    cmd = chunk + sizeof(uint32_t) + (command_index * 4u);
    out->reg = cmd[0];
    out->value = cmd[1];
    out->delay = read_le16(cmd + 2u);
    return 0;
}


int wl_describe_imf_playback_window(const unsigned char *chunk, size_t chunk_size,
                                    size_t start_command, uint32_t tick_budget,
                                    wl_imf_playback_window *out) {
    uint32_t declared_bytes;
    size_t command_count;
    uint64_t elapsed = 0;
    if (!chunk || !out || chunk_size < sizeof(uint32_t)) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    declared_bytes = read_le32(chunk);
    if ((declared_bytes % 4u) != 0u || (size_t)declared_bytes > chunk_size - sizeof(uint32_t)) {
        return -1;
    }
    command_count = (size_t)declared_bytes / 4u;
    if (start_command > command_count) {
        return -1;
    }
    out->start_command = start_command;
    out->commands_available = command_count - start_command;
    out->next_command = start_command;
    while (out->next_command < command_count) {
        wl_imf_music_command command;
        if (wl_get_imf_music_command(chunk, chunk_size, out->next_command, &command) != 0) {
            return -1;
        }
        if (out->commands_in_window > 0 && elapsed + command.delay > tick_budget) {
            break;
        }
        elapsed += command.delay;
        if (elapsed > UINT32_MAX) {
            return -1;
        }
        ++out->commands_in_window;
        ++out->next_command;
        if (elapsed >= tick_budget) {
            break;
        }
    }
    out->elapsed_delay = (uint32_t)elapsed;
    out->completed = (out->next_command == command_count) ? 1u : 0u;
    return 0;
}

int wl_describe_imf_playback_position(const unsigned char *chunk, size_t chunk_size,
                                      uint32_t tick_position,
                                      wl_imf_playback_position *out) {
    uint32_t declared_bytes;
    size_t command_count;
    uint64_t elapsed = 0;
    if (!chunk || !out || chunk_size < sizeof(uint32_t)) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    declared_bytes = read_le32(chunk);
    if ((declared_bytes % 4u) != 0u || (size_t)declared_bytes > chunk_size - sizeof(uint32_t)) {
        return -1;
    }
    command_count = (size_t)declared_bytes / 4u;
    out->tick_position = tick_position;
    out->command_index = command_count;
    for (size_t i = 0; i < command_count; ++i) {
        wl_imf_music_command command;
        if (wl_get_imf_music_command(chunk, chunk_size, i, &command) != 0) {
            return -1;
        }
        if (elapsed + command.delay > tick_position) {
            out->command_index = i;
            out->command_delay = command.delay;
            out->delay_elapsed = (uint16_t)(tick_position - elapsed);
            out->delay_remaining = (uint16_t)(command.delay - out->delay_elapsed);
            return 0;
        }
        elapsed += command.delay;
        if (elapsed > UINT32_MAX) {
            return -1;
        }
    }
    out->completed = 1u;
    return 0;
}

int wl_advance_imf_playback_cursor(const unsigned char *chunk, size_t chunk_size,
                                   size_t start_command, uint16_t start_delay_elapsed,
                                   uint32_t tick_delta,
                                   wl_imf_playback_cursor *out) {
    uint32_t declared_bytes;
    size_t command_count;
    uint32_t remaining = tick_delta;
    size_t index = start_command;
    if (!chunk || !out || chunk_size < sizeof(uint32_t)) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    declared_bytes = read_le32(chunk);
    if ((declared_bytes % 4u) != 0u || (size_t)declared_bytes > chunk_size - sizeof(uint32_t)) {
        return -1;
    }
    command_count = (size_t)declared_bytes / 4u;
    if (start_command > command_count) {
        return -1;
    }
    if (start_command == command_count) {
        if (start_delay_elapsed != 0u) {
            return -1;
        }
        out->command_index = command_count;
        out->completed = 1u;
        return 0;
    }

    while (index < command_count) {
        wl_imf_music_command command;
        uint32_t elapsed = (index == start_command) ? (uint32_t)start_delay_elapsed : 0u;
        uint32_t available;
        if (wl_get_imf_music_command(chunk, chunk_size, index, &command) != 0) {
            return -1;
        }
        if (elapsed > command.delay) {
            return -1;
        }
        available = (uint32_t)command.delay - elapsed;
        if (remaining < available) {
            out->command_index = index;
            out->command_delay = command.delay;
            out->delay_elapsed = (uint16_t)(elapsed + remaining);
            out->delay_remaining = (uint16_t)(available - remaining);
            out->ticks_consumed = tick_delta;
            return 0;
        }
        remaining -= available;
        out->ticks_consumed += available;
        ++out->commands_advanced;
        ++index;
        if (remaining == 0u) {
            break;
        }
    }

    out->command_index = index;
    if (index >= command_count) {
        out->completed = 1u;
        return 0;
    }
    {
        wl_imf_music_command command;
        if (wl_get_imf_music_command(chunk, chunk_size, index, &command) != 0) {
            return -1;
        }
        out->command_delay = command.delay;
        out->delay_remaining = command.delay;
    }
    return 0;
}
