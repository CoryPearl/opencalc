#include "opencalc_workspace_io.h"

#include <limits.h>

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
