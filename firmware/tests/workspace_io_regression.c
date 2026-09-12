#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "opencalc_workspace_io.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void make_path(char *out, size_t out_size, const char *suffix)
{
    snprintf(out, out_size, "/tmp/opencalc-workspace-%ld-%s", (long)getpid(), suffix);
}

int main(void)
{
    char temporary[128];
    char current[128];
    char backup[128];
    make_path(temporary, sizeof(temporary), "temporary");
    make_path(current, sizeof(current), "current");
    make_path(backup, sizeof(backup), "backup");

    FILE *file = fopen(current, "wb");
    assert(file != NULL);
    assert(fwrite("old", 1, 3, file) == 3);
    assert(opencalc_workspace_flush_sync(file));
    assert(fclose(file) == 0);

    file = fopen(temporary, "wb");
    assert(file != NULL);
    uint32_t crc = UINT32_MAX;
    uint32_t payload_size = 0;
    assert(opencalc_workspace_write_payload(file, "new-data", 8, &crc, &payload_size));
    assert(payload_size == 8);
    assert(opencalc_workspace_flush_sync(file));
    assert(fclose(file) == 0);

    assert(opencalc_workspace_replace_checkpoint(temporary, current, backup));
    file = fopen(current, "rb+");
    assert(file != NULL);
    char data[9] = {0};
    assert(fread(data, 1, 8, file) == 8);
    assert(strcmp(data, "new-data") == 0);
    assert(opencalc_workspace_truncate_sync(file, 3));
    assert(fclose(file) == 0);
    assert(opencalc_workspace_clear_path(current));
    file = fopen(current, "rb");
    assert(file != NULL);
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);

    remove(temporary);
    remove(current);
    remove(backup);
    puts("PASS workspace durability helpers");
    return 0;
}
