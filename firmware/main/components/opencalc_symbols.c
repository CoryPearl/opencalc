#include "opencalc_symbols.h"

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#else
#define EXT_RAM_BSS_ATTR
#endif

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    bool occupied;
    opencalc_symbol_t value;
    double *real_values;
} symbol_slot_t;

static EXT_RAM_BSS_ATTR symbol_slot_t s_symbols[OPENCALC_SYMBOL_MAX];
#ifdef ESP_PLATFORM
static SemaphoreHandle_t s_symbols_lock;
static StaticSemaphore_t s_symbols_lock_storage;
static portMUX_TYPE s_init_lock = portMUX_INITIALIZER_UNLOCKED;
#endif
static uint32_t s_generation = 1;

static bool set_real_container(const char *name, opencalc_symbol_type_t type,
                               uint8_t flags, opencalc_symbol_source_t source,
                               int source_index, const double *values,
                               size_t rows, size_t cols, size_t row_stride);

static void *symbol_payload_alloc(size_t size)
{
    if (size == 0) return NULL;
#ifdef ESP_PLATFORM
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return malloc(size);
#endif
}

static void symbol_payload_free(void *payload)
{
#ifdef ESP_PLATFORM
    heap_caps_free(payload);
#else
    free(payload);
#endif
}

static bool ensure_lock(void)
{
#ifdef ESP_PLATFORM
    if (s_symbols_lock != NULL) return true;
    taskENTER_CRITICAL(&s_init_lock);
    if (s_symbols_lock == NULL) s_symbols_lock = xSemaphoreCreateMutexStatic(&s_symbols_lock_storage);
    taskEXIT_CRITICAL(&s_init_lock);
    return s_symbols_lock != NULL;
#else
    return true;
#endif
}

static bool lock_symbols(void)
{
#ifdef ESP_PLATFORM
    return ensure_lock() && xSemaphoreTake(s_symbols_lock, portMAX_DELAY) == pdTRUE;
#else
    return true;
#endif
}

static void unlock_symbols(void)
{
#ifdef ESP_PLATFORM
    xSemaphoreGive(s_symbols_lock);
#endif
}

static bool name_is_valid(const char *name)
{
    if (name == NULL || !(isalpha((unsigned char)name[0]) || name[0] == '_')) return false;
    size_t length = strlen(name);
    if (length == 0 || length >= OPENCALC_SYMBOL_NAME_MAX) return false;
    for (size_t i = 1; i < length; i++) {
        if (!(isalnum((unsigned char)name[i]) || name[i] == '_')) return false;
    }
    return true;
}

static int find_locked(const char *name)
{
    for (int i = 0; i < OPENCALC_SYMBOL_MAX; i++) {
        if (s_symbols[i].occupied && strcasecmp(s_symbols[i].value.name, name) == 0) return i;
    }
    return -1;
}

void opencalc_symbols_init(void)
{
    (void)ensure_lock();
}

bool opencalc_symbol_set(const opencalc_symbol_t *symbol)
{
    if (symbol == NULL || !name_is_valid(symbol->name) ||
        symbol->type < OPENCALC_SYMBOL_NUMBER || symbol->type > OPENCALC_SYMBOL_SYSTEM ||
        memchr(symbol->text, '\0', OPENCALC_SYMBOL_TEXT_MAX) == NULL ||
        !ensure_lock()) return false;
    if (symbol->numeric_valid && (!isfinite(symbol->real) || !isfinite(symbol->imag))) return false;
    if (!lock_symbols()) return false;
    int index = find_locked(symbol->name);
    if (index < 0) {
        for (int i = 0; i < OPENCALC_SYMBOL_MAX; i++) {
            if (!s_symbols[i].occupied) { index = i; break; }
        }
    }
    bool ok = index >= 0;
    if (ok) {
        opencalc_symbol_t normalized = *symbol;
        normalized.name[OPENCALC_SYMBOL_NAME_MAX - 1] = '\0';
        normalized.text[OPENCALC_SYMBOL_TEXT_MAX - 1] = '\0';
        normalized.structured = false;
        normalized.rows = 0;
        normalized.cols = 0;
        normalized.element_count = 0;
        normalized.revision = s_symbols[index].occupied
            ? s_symbols[index].value.revision : 0;
        bool changed = !s_symbols[index].occupied ||
            memcmp(&s_symbols[index].value, &normalized, sizeof(normalized)) != 0;
        double *old_values = s_symbols[index].real_values;
        s_symbols[index].real_values = NULL;
        if (changed) normalized.revision = ++s_generation;
        s_symbols[index].occupied = true;
        s_symbols[index].value = normalized;
        symbol_payload_free(old_values);
    }
    unlock_symbols();
    return ok;
}

bool opencalc_symbol_set_text(const char *name, opencalc_symbol_type_t type,
                              uint8_t flags, const char *text)
{
    return opencalc_symbol_set_owned_text(name, type, flags,
                                          OPENCALC_SYMBOL_SOURCE_USER, -1, text);
}

static bool parse_real_container(const char *text, bool matrix, double **values_out,
                                 size_t *rows_out, size_t *cols_out)
{
    size_t capacity = OPENCALC_SYMBOL_TEXT_MAX / 2;
    double *values = symbol_payload_alloc(capacity * sizeof(double));
    if (values == NULL) return false;
    const char *p = text;
    while (isspace((unsigned char)*p)) p++;
    char outer_close = *p == '{' ? '}' : (*p == '[' ? ']' : '\0');
    if (outer_close == '\0') goto fail;
    p++;

    size_t rows = 0;
    size_t cols = 0;
    size_t count = 0;
    while (true) {
        while (isspace((unsigned char)*p)) p++;
        if (*p == outer_close) { p++; break; }
        if (matrix && *p++ != '[') goto fail;

        size_t row_cols = 0;
        while (true) {
            while (isspace((unsigned char)*p)) p++;
            char *end = NULL;
            double value = strtod(p, &end);
            if (end == p || !isfinite(value) || count >= capacity) goto fail;
            values[count++] = value;
            row_cols++;
            p = end;
            while (isspace((unsigned char)*p)) p++;
            if (*p == ',') { p++; continue; }
            if ((matrix && *p == ']') || (!matrix && *p == outer_close)) break;
            goto fail;
        }

        if (matrix) {
            p++;
            if (rows == 0) cols = row_cols;
            else if (row_cols != cols) goto fail;
            rows++;
            while (isspace((unsigned char)*p)) p++;
            if (*p == ',') { p++; continue; }
            if (*p == outer_close) { p++; break; }
            goto fail;
        }
        rows = 1;
        cols = row_cols;
        if (*p == outer_close) { p++; break; }
    }
    while (isspace((unsigned char)*p)) p++;
    if (*p != '\0' || rows == 0 || cols == 0 || count != rows * cols) goto fail;
    *values_out = values;
    *rows_out = rows;
    *cols_out = cols;
    return true;

fail:
    symbol_payload_free(values);
    return false;
}

bool opencalc_symbol_set_owned_text(const char *name, opencalc_symbol_type_t type,
                                    uint8_t flags, opencalc_symbol_source_t source,
                                    int source_index, const char *text)
{
    if (name == NULL || text == NULL || strlen(text) >= OPENCALC_SYMBOL_TEXT_MAX) return false;
    if (type == OPENCALC_SYMBOL_LIST || type == OPENCALC_SYMBOL_MATRIX) {
        double *values = NULL;
        size_t rows = 0;
        size_t cols = 0;
        if (parse_real_container(text, type == OPENCALC_SYMBOL_MATRIX,
                                 &values, &rows, &cols)) {
            bool stored = set_real_container(name, type, flags, source, source_index,
                                              values, rows, cols, cols);
            symbol_payload_free(values);
            return stored;
        }
    }
    opencalc_symbol_t symbol = {
        .type = type,
        .flags = flags,
        .source = source,
        .source_index = (int16_t)source_index,
    };
    snprintf(symbol.name, sizeof(symbol.name), "%s", name);
    snprintf(symbol.text, sizeof(symbol.text), "%s", text);
    return opencalc_symbol_set(&symbol);
}

bool opencalc_symbol_set_number(const char *name, double real, double imag,
                                uint8_t flags, const char *exact_text)
{
    if (name == NULL || !isfinite(real) || !isfinite(imag)) return false;
    opencalc_symbol_t symbol = {
        .type = fabs(imag) > 1e-12 ? OPENCALC_SYMBOL_COMPLEX : OPENCALC_SYMBOL_NUMBER,
        .flags = flags,
        .numeric_valid = true,
        .real = fabs(real) < 1e-12 ? 0.0 : real,
        .imag = fabs(imag) < 1e-12 ? 0.0 : imag,
        .source = OPENCALC_SYMBOL_SOURCE_USER,
        .source_index = -1,
    };
    snprintf(symbol.name, sizeof(symbol.name), "%s", name);
    if (exact_text != NULL && exact_text[0] != '\0') {
        snprintf(symbol.text, sizeof(symbol.text), "%s", exact_text);
    } else if (symbol.type == OPENCALC_SYMBOL_COMPLEX) {
        snprintf(symbol.text, sizeof(symbol.text), "%.17g%+.17g*i", symbol.real, symbol.imag);
    } else {
        snprintf(symbol.text, sizeof(symbol.text), "%.17g", symbol.real);
    }
    return opencalc_symbol_set(&symbol);
}

static bool set_real_container(const char *name, opencalc_symbol_type_t type,
                               uint8_t flags, opencalc_symbol_source_t source,
                               int source_index, const double *values,
                               size_t rows, size_t cols, size_t row_stride)
{
    if (name == NULL || values == NULL || rows == 0 || cols == 0 ||
        rows > UINT16_MAX || cols > UINT16_MAX || rows > UINT32_MAX / cols) return false;
    size_t count = rows * cols;
    for (size_t row = 0; row < rows; row++) {
        for (size_t col = 0; col < cols; col++) {
            if (!isfinite(values[row * row_stride + col])) return false;
        }
    }

    if (!lock_symbols()) return false;
    int existing = find_locked(name);
    if (existing >= 0) {
        symbol_slot_t *slot = &s_symbols[existing];
        bool same = slot->value.type == type && slot->value.flags == flags &&
            slot->value.source == source && slot->value.source_index == source_index &&
            slot->value.rows == rows && slot->value.cols == cols &&
            slot->value.element_count == count && slot->real_values != NULL;
        for (size_t row = 0; same && row < rows; row++) {
            same = memcmp(slot->real_values + row * cols,
                          values + row * row_stride, cols * sizeof(double)) == 0;
        }
        if (same) {
            unlock_symbols();
            return true;
        }
    }
    unlock_symbols();

    double *copy = symbol_payload_alloc(count * sizeof(double));
    if (copy == NULL) return false;
    for (size_t row = 0; row < rows; row++) {
        memcpy(copy + row * cols, values + row * row_stride, cols * sizeof(double));
    }

    if (!lock_symbols()) {
        symbol_payload_free(copy);
        return false;
    }
    int index = find_locked(name);
    if (index < 0) {
        for (int i = 0; i < OPENCALC_SYMBOL_MAX; i++) {
            if (!s_symbols[i].occupied) { index = i; break; }
        }
    }
    if (index < 0) {
        unlock_symbols();
        symbol_payload_free(copy);
        return false;
    }

    double *old_values = s_symbols[index].real_values;
    opencalc_symbol_t symbol = {
        .type = type,
        .flags = flags,
        .source = source,
        .source_index = (int16_t)source_index,
        .rows = (uint16_t)rows,
        .cols = (uint16_t)cols,
        .element_count = (uint32_t)count,
        .revision = ++s_generation,
        .structured = true,
    };
    snprintf(symbol.name, sizeof(symbol.name), "%s", name);
    if (type == OPENCALC_SYMBOL_MATRIX) {
        snprintf(symbol.text, sizeof(symbol.text), "[%ux%u real matrix]",
                 (unsigned)rows, (unsigned)cols);
    } else {
        snprintf(symbol.text, sizeof(symbol.text), "[%u real values]", (unsigned)count);
    }
    s_symbols[index].occupied = true;
    s_symbols[index].value = symbol;
    s_symbols[index].real_values = copy;
    unlock_symbols();
    symbol_payload_free(old_values);
    return true;
}

bool opencalc_symbol_set_real_list(const char *name, uint8_t flags,
                                   opencalc_symbol_source_t source, int source_index,
                                   const double *values, size_t count)
{
    return set_real_container(name, OPENCALC_SYMBOL_LIST, flags, source, source_index,
                              values, 1, count, count);
}

bool opencalc_symbol_set_real_matrix(const char *name, uint8_t flags,
                                     opencalc_symbol_source_t source, int source_index,
                                     const double *values, size_t rows, size_t cols,
                                     size_t row_stride)
{
    if (row_stride < cols) return false;
    return set_real_container(name, OPENCALC_SYMBOL_MATRIX, flags, source, source_index,
                              values, rows, cols, row_stride);
}

bool opencalc_symbol_get(const char *name, opencalc_symbol_t *symbol)
{
    if (name == NULL || symbol == NULL || !lock_symbols()) return false;
    int index = find_locked(name);
    if (index >= 0) *symbol = s_symbols[index].value;
    unlock_symbols();
    return index >= 0;
}

bool opencalc_symbol_visit_real_values(const char *name,
                                       opencalc_symbol_real_visitor_fn visitor,
                                       void *context)
{
    if (name == NULL || visitor == NULL || !lock_symbols()) return false;
    int index = find_locked(name);
    bool visited = index >= 0 && s_symbols[index].value.structured &&
        s_symbols[index].real_values != NULL &&
        visitor(&s_symbols[index].value, s_symbols[index].real_values,
                s_symbols[index].value.element_count, context);
    unlock_symbols();
    return visited;
}

bool opencalc_symbol_format_value(const char *name, char *out, size_t out_size)
{
    if (name == NULL || out == NULL || out_size == 0 || !lock_symbols()) return false;
    int index = find_locked(name);
    if (index < 0) {
        unlock_symbols();
        return false;
    }
    const symbol_slot_t *slot = &s_symbols[index];
    if (!slot->value.structured || slot->real_values == NULL) {
        int written = snprintf(out, out_size, "%s", slot->value.text);
        unlock_symbols();
        return written >= 0 && (size_t)written < out_size;
    }

    size_t used = 0;
    bool matrix = slot->value.type == OPENCALC_SYMBOL_MATRIX;
    if (used + 1 >= out_size) goto too_small;
    out[used++] = '[';
    for (size_t row = 0; row < slot->value.rows; row++) {
        if (matrix) {
            if (used + (row ? 2U : 1U) >= out_size) goto too_small;
            if (row) out[used++] = ',';
            out[used++] = '[';
        }
        for (size_t col = 0; col < slot->value.cols; col++) {
            int written = snprintf(out + used, out_size - used, "%s%.17g",
                                   col ? "," : "",
                                   slot->real_values[row * slot->value.cols + col]);
            if (written < 0 || (size_t)written >= out_size - used) goto too_small;
            used += (size_t)written;
        }
        if (matrix) {
            if (used + 1 >= out_size) goto too_small;
            out[used++] = ']';
        }
    }
    if (used + 2 > out_size) goto too_small;
    out[used++] = ']';
    out[used] = '\0';
    unlock_symbols();
    return true;

too_small:
    out[0] = '\0';
    unlock_symbols();
    return false;
}

bool opencalc_symbol_remove(const char *name, bool include_read_only)
{
    if (name == NULL || !lock_symbols()) return false;
    int index = find_locked(name);
    bool removed = index >= 0 &&
        (include_read_only || !(s_symbols[index].value.flags & OPENCALC_SYMBOL_READ_ONLY));
    if (removed) {
        symbol_payload_free(s_symbols[index].real_values);
        memset(&s_symbols[index], 0, sizeof(s_symbols[index]));
        s_generation++;
    }
    unlock_symbols();
    return removed;
}

bool opencalc_symbol_rename(const char *old_name, const char *new_name)
{
    if (!name_is_valid(new_name) || !lock_symbols()) return false;
    int old_index = find_locked(old_name);
    bool ok = old_index >= 0 && find_locked(new_name) < 0 &&
        !(s_symbols[old_index].value.flags & OPENCALC_SYMBOL_READ_ONLY);
    if (ok) {
        snprintf(s_symbols[old_index].value.name,
                 sizeof(s_symbols[old_index].value.name), "%s", new_name);
        s_symbols[old_index].value.revision = ++s_generation;
    }
    unlock_symbols();
    return ok;
}

size_t opencalc_symbol_count(uint8_t required_flags)
{
    if (!lock_symbols()) return 0;
    size_t count = 0;
    for (int i = 0; i < OPENCALC_SYMBOL_MAX; i++) {
        if (s_symbols[i].occupied &&
            (s_symbols[i].value.flags & required_flags) == required_flags) count++;
    }
    unlock_symbols();
    return count;
}

bool opencalc_symbol_at(size_t index, uint8_t required_flags,
                        opencalc_symbol_t *symbol)
{
    if (symbol == NULL || !lock_symbols()) return false;
    bool found = false;
    for (int i = 0; i < OPENCALC_SYMBOL_MAX; i++) {
        if (!s_symbols[i].occupied ||
            (s_symbols[i].value.flags & required_flags) != required_flags) continue;
        if (index-- == 0) {
            *symbol = s_symbols[i].value;
            found = true;
            break;
        }
    }
    unlock_symbols();
    return found;
}

void opencalc_symbols_clear(uint8_t matching_flags)
{
    if (!lock_symbols()) return;
    bool changed = false;
    for (int i = 0; i < OPENCALC_SYMBOL_MAX; i++) {
        if (s_symbols[i].occupied &&
            (matching_flags == 0 || (s_symbols[i].value.flags & matching_flags) != 0)) {
            symbol_payload_free(s_symbols[i].real_values);
            memset(&s_symbols[i], 0, sizeof(s_symbols[i]));
            changed = true;
        }
    }
    if (changed) s_generation++;
    unlock_symbols();
}

uint32_t opencalc_symbols_generation(void)
{
    if (!lock_symbols()) return 0;
    uint32_t generation = s_generation;
    unlock_symbols();
    return generation;
}

const char *opencalc_symbol_type_name(opencalc_symbol_type_t type)
{
    static const char *const names[] = {
        "Number", "Complex", "Expression", "Unit", "List", "Matrix",
        "String", "Function", "Statistic", "Graph", "System",
    };
    return type >= OPENCALC_SYMBOL_NUMBER && type <= OPENCALC_SYMBOL_SYSTEM
        ? names[type] : "Unknown";
}
