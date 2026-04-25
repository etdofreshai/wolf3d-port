#include "wl_assets.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t read_le16(const unsigned char *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

const char *wl_default_data_dir(void) {
    const char *env = getenv("WOLF3D_DATA_DIR");
    return (env && env[0]) ? env : "game-files/base";
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
