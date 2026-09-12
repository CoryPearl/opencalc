#include "opencalc_graph_background.h"

#include "esp_heap_caps.h"

#include <math.h>
#include <stdio.h>

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint16_t read_le16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void set_status(char *status, size_t size, const char *message)
{
    if (status != NULL && size > 0) snprintf(status, size, "%s", message);
}

bool opencalc_graph_background_load_bmp(const char *path, uint32_t *pixels,
                                        int output_width, int output_height,
                                        uint32_t background_color,
                                        opencalc_graph_background_mode_t mode,
                                        char *status, size_t status_size)
{
    if (path == NULL || pixels == NULL || output_width <= 0 || output_height <= 0) {
        set_status(status, status_size, "invalid background target");
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        if (status != NULL && status_size > 0) snprintf(status, status_size, "missing %s", path);
        return false;
    }

    uint8_t header[54];
    bool ok = fread(header, 1, sizeof(header), file) == sizeof(header) &&
        header[0] == 'B' && header[1] == 'M';
    uint32_t pixel_offset = ok ? read_le32(header + 10) : 0;
    int32_t width = ok ? (int32_t)read_le32(header + 18) : 0;
    int32_t height = ok ? (int32_t)read_le32(header + 22) : 0;
    uint16_t planes = ok ? read_le16(header + 26) : 0;
    uint16_t bits = ok ? read_le16(header + 28) : 0;
    uint32_t compression = ok ? read_le32(header + 30) : 1;
    int32_t absolute_height = height < 0 ? -height : height;
    ok = ok && width > 0 && width <= 4096 && absolute_height > 0 &&
        absolute_height <= 4096 && planes == 1 && (bits == 24 || bits == 32) &&
        compression == 0;
    size_t row_bytes = ok ? (((size_t)width * bits + 31u) / 32u) * 4u : 0;
    uint8_t *row = ok ? heap_caps_malloc(row_bytes,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : NULL;
    if (!ok || row == NULL) {
        if (row != NULL) heap_caps_free(row);
        fclose(file);
        set_status(status, status_size, "graph.bmp: RGB BMP up to 4096 px");
        return false;
    }

    double scale_x = (double)output_width / width;
    double scale_y = (double)output_height / absolute_height;
    double scale = mode == OPENCALC_GRAPH_BACKGROUND_FIT ? fmin(scale_x, scale_y) :
        (mode == OPENCALC_GRAPH_BACKGROUND_FILL ? fmax(scale_x, scale_y) : 0.0);
    double drawn_width = mode == OPENCALC_GRAPH_BACKGROUND_STRETCH ? output_width : width * scale;
    double drawn_height = mode == OPENCALC_GRAPH_BACKGROUND_STRETCH ? output_height :
        absolute_height * scale;
    double offset_x = (output_width - drawn_width) * 0.5;
    double offset_y = (output_height - drawn_height) * 0.5;

    for (int y = 0; y < output_height && ok; y++) {
        for (int x = 0; x < output_width; x++) pixels[y * output_width + x] = background_color;
        double source_y_value = mode == OPENCALC_GRAPH_BACKGROUND_STRETCH
            ? (double)y * absolute_height / output_height : ((double)y - offset_y) / scale;
        if (source_y_value < 0.0 || source_y_value >= absolute_height) continue;
        int source_y = (int)source_y_value;
        int file_row = height > 0 ? absolute_height - 1 - source_y : source_y;
        if (fseek(file, (long)(pixel_offset + (uint32_t)((size_t)file_row * row_bytes)),
                  SEEK_SET) != 0 || fread(row, 1, row_bytes, file) != row_bytes) {
            ok = false;
            break;
        }
        int bytes_per_pixel = bits / 8;
        for (int x = 0; x < output_width; x++) {
            double source_x_value = mode == OPENCALC_GRAPH_BACKGROUND_STRETCH
                ? (double)x * width / output_width : ((double)x - offset_x) / scale;
            if (source_x_value < 0.0 || source_x_value >= width) continue;
            const uint8_t *source = row + (int)source_x_value * bytes_per_pixel;
            pixels[y * output_width + x] = ((uint32_t)source[2] << 16) |
                ((uint32_t)source[1] << 8) | source[0];
        }
    }

    heap_caps_free(row);
    fclose(file);
    if (!ok) {
        set_status(status, status_size, "graph.bmp read failed");
        return false;
    }
    if (status != NULL && status_size > 0) {
        snprintf(status, status_size, "background %ldx%ld loaded",
                 (long)width, (long)absolute_height);
    }
    return true;
}
