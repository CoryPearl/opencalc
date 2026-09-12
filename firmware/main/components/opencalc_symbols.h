#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPENCALC_SYMBOL_NAME_MAX 16
#define OPENCALC_SYMBOL_TEXT_MAX 1024
#define OPENCALC_SYMBOL_MAX 96

typedef enum {
    OPENCALC_SYMBOL_NUMBER = 0,
    OPENCALC_SYMBOL_COMPLEX,
    OPENCALC_SYMBOL_EXPRESSION,
    OPENCALC_SYMBOL_UNIT,
    OPENCALC_SYMBOL_LIST,
    OPENCALC_SYMBOL_MATRIX,
    OPENCALC_SYMBOL_STRING,
    OPENCALC_SYMBOL_FUNCTION,
    OPENCALC_SYMBOL_STATISTIC,
    OPENCALC_SYMBOL_GRAPH,
    OPENCALC_SYMBOL_SYSTEM,
} opencalc_symbol_type_t;

typedef enum {
    OPENCALC_SYMBOL_SOURCE_USER = 0,
    OPENCALC_SYMBOL_SOURCE_WORKSHEET_LIST,
    OPENCALC_SYMBOL_SOURCE_WORKSHEET_MATRIX,
    OPENCALC_SYMBOL_SOURCE_GRAPH,
    OPENCALC_SYMBOL_SOURCE_STATISTICS,
    OPENCALC_SYMBOL_SOURCE_SYSTEM,
    OPENCALC_SYMBOL_SOURCE_CAS,
} opencalc_symbol_source_t;

enum {
    OPENCALC_SYMBOL_USER = 1u << 0,
    OPENCALC_SYMBOL_READ_ONLY = 1u << 1,
    OPENCALC_SYMBOL_GIAC_SYNC = 1u << 2,
};

typedef struct {
    char name[OPENCALC_SYMBOL_NAME_MAX];
    opencalc_symbol_type_t type;
    uint8_t flags;
    bool numeric_valid;
    double real;
    double imag;
    opencalc_symbol_source_t source;
    int16_t source_index;
    uint16_t rows;
    uint16_t cols;
    uint32_t element_count;
    uint32_t revision;
    bool structured;
    char text[OPENCALC_SYMBOL_TEXT_MAX];
} opencalc_symbol_t;

typedef bool (*opencalc_symbol_real_visitor_fn)(const opencalc_symbol_t *symbol,
                                                const double *values,
                                                size_t count, void *context);

void opencalc_symbols_init(void);
bool opencalc_symbol_set(const opencalc_symbol_t *symbol);
bool opencalc_symbol_set_text(const char *name, opencalc_symbol_type_t type,
                              uint8_t flags, const char *text);
bool opencalc_symbol_set_owned_text(const char *name, opencalc_symbol_type_t type,
                                    uint8_t flags, opencalc_symbol_source_t source,
                                    int source_index, const char *text);
bool opencalc_symbol_set_number(const char *name, double real, double imag,
                                uint8_t flags, const char *exact_text);
bool opencalc_symbol_set_real_list(const char *name, uint8_t flags,
                                   opencalc_symbol_source_t source, int source_index,
                                   const double *values, size_t count);
bool opencalc_symbol_set_real_matrix(const char *name, uint8_t flags,
                                     opencalc_symbol_source_t source, int source_index,
                                     const double *values, size_t rows, size_t cols,
                                     size_t row_stride);
bool opencalc_symbol_get(const char *name, opencalc_symbol_t *symbol);
bool opencalc_symbol_visit_real_values(const char *name,
                                       opencalc_symbol_real_visitor_fn visitor,
                                       void *context);
bool opencalc_symbol_format_value(const char *name, char *out, size_t out_size);
bool opencalc_symbol_remove(const char *name, bool include_read_only);
bool opencalc_symbol_rename(const char *old_name, const char *new_name);
size_t opencalc_symbol_count(uint8_t required_flags);
bool opencalc_symbol_at(size_t index, uint8_t required_flags,
                        opencalc_symbol_t *symbol);
void opencalc_symbols_clear(uint8_t matching_flags);
uint32_t opencalc_symbols_generation(void);
const char *opencalc_symbol_type_name(opencalc_symbol_type_t type);

#ifdef __cplusplus
}
#endif
