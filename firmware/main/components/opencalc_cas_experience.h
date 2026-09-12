#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPENCALC_CAS_RESULT_ITEMS 24
#define OPENCALC_CAS_ASSUMPTIONS_MAX 96

typedef enum {
    OPENCALC_CAS_DOMAIN_AUTO = 0,
    OPENCALC_CAS_DOMAIN_REAL,
    OPENCALC_CAS_DOMAIN_COMPLEX,
    OPENCALC_CAS_DOMAIN_INTEGER,
    OPENCALC_CAS_DOMAIN_INTERVAL,
    OPENCALC_CAS_DOMAIN_COUNT,
} opencalc_cas_domain_t;

typedef enum {
    OPENCALC_CAS_RESULT_SCALAR = 0,
    OPENCALC_CAS_RESULT_LIST,
    OPENCALC_CAS_RESULT_SOLUTIONS,
    OPENCALC_CAS_RESULT_MATRIX,
    OPENCALC_CAS_RESULT_ERROR,
} opencalc_cas_result_kind_t;

typedef struct {
    unsigned short offset;
    unsigned short length;
    short relation_offset;
} opencalc_cas_result_item_t;

/* Items reference spans in the caller-owned result string. */
typedef struct {
    opencalc_cas_result_kind_t kind;
    int item_count;
    bool truncated;
    bool exact;
    opencalc_cas_result_item_t items[OPENCALC_CAS_RESULT_ITEMS];
} opencalc_cas_result_t;

typedef struct {
    opencalc_cas_domain_t domain;
    double interval_min;
    double interval_max;
    char assumptions[OPENCALC_CAS_ASSUMPTIONS_MAX];
} opencalc_cas_options_t;

typedef struct {
    const char *name;
    const char *insert;
    const char *summary;
    const char *category;
} opencalc_cas_catalog_entry_t;

void opencalc_cas_options_default(opencalc_cas_options_t *options);
const char *opencalc_cas_domain_name(opencalc_cas_domain_t domain);

/* Builds the engine command without changing the user's editor text. */
bool opencalc_cas_prepare_expression(const char *expression,
                                     const opencalc_cas_options_t *options,
                                     char *out, size_t out_size);

void opencalc_cas_analyze_result(const char *text, opencalc_cas_result_t *result);
bool opencalc_cas_result_item_text(const char *text,
                                   const opencalc_cas_result_t *result,
                                   int index, char *out, size_t out_size);

size_t opencalc_cas_catalog_count(void);
const opencalc_cas_catalog_entry_t *opencalc_cas_catalog_at(size_t index);
int opencalc_cas_catalog_complete(const char *prefix, int after_index);

#ifdef __cplusplus
}
#endif
