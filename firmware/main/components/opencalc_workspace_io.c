#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "opencalc_workspace_io.h"

#include <errno.h>
#include <limits.h>
#include <unistd.h>

uint32_t opencalc_workspace_crc32_update(uint32_t crc, const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    while (size-- > 0) {
        crc ^= *bytes++;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(crc & 1u));
        }
    }
    return crc;
}

bool opencalc_workspace_write_payload(FILE *file, const void *data, size_t size,
                                      uint32_t *crc, uint32_t *payload_size)
{
    if (file == NULL || crc == NULL || payload_size == NULL ||
        size > UINT32_MAX - *payload_size || fwrite(data, 1, size, file) != size) {
        return false;
    }
    *crc = opencalc_workspace_crc32_update(*crc, data, size);
    *payload_size += (uint32_t)size;
    return true;
}

bool opencalc_workspace_read_payload(FILE *file, void *data, size_t size,
                                     uint32_t *remaining)
{
    if (file == NULL || remaining == NULL || size > *remaining ||
        fread(data, 1, size, file) != size) {
        return false;
    }
    *remaining -= (uint32_t)size;
    return true;
}

bool opencalc_workspace_flush_sync(FILE *file)
{
    if (file == NULL || fflush(file) != 0) return false;
    int fd = fileno(file);
    return fd >= 0 && fsync(fd) == 0;
}

bool opencalc_workspace_truncate_sync(FILE *file, long length)
{
    if (file == NULL || length < 0 || fflush(file) != 0) return false;
    int fd = fileno(file);
    return fd >= 0 && ftruncate(fd, length) == 0 && fsync(fd) == 0;
}

bool opencalc_workspace_sync_path(const char *path)
{
    if (path == NULL) return false;
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    int fd = fileno(file);
    bool ok = fd >= 0 && fsync(fd) == 0;
    if (fclose(file) != 0) ok = false;
    return ok;
}

bool opencalc_workspace_clear_path(const char *path)
{
    if (path == NULL) return false;
    FILE *file = fopen(path, "wb");
    if (file == NULL) return false;
    bool ok = opencalc_workspace_flush_sync(file);
    if (fclose(file) != 0) ok = false;
    return ok;
}

bool opencalc_workspace_replace_checkpoint(const char *temporary_path,
                                            const char *current_path,
                                            const char *backup_path)
{
    if (temporary_path == NULL || current_path == NULL || backup_path == NULL ||
        !opencalc_workspace_sync_path(temporary_path)) {
        return false;
    }

    if (remove(backup_path) != 0 && errno != ENOENT) return false;
    bool had_current = rename(current_path, backup_path) == 0;
    if (!had_current && errno != ENOENT) return false;
    if (rename(temporary_path, current_path) != 0 ||
        !opencalc_workspace_sync_path(current_path)) {
        (void)remove(current_path);
        if (had_current) (void)rename(backup_path, current_path);
        return false;
    }
    return true;
}
