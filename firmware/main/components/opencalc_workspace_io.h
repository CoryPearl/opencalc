#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

uint32_t opencalc_workspace_crc32_update(uint32_t crc, const void *data, size_t size);
bool opencalc_workspace_write_payload(FILE *file, const void *data, size_t size,
                                      uint32_t *crc, uint32_t *payload_size);
bool opencalc_workspace_read_payload(FILE *file, void *data, size_t size,
                                     uint32_t *remaining);
