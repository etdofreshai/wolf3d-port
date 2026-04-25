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
