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
