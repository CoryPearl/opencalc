#ifndef OPENCALC_REFERENCE_H
#define OPENCALC_REFERENCE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    OPENCALC_ELEMENT_NONMETAL = 0,
    OPENCALC_ELEMENT_NOBLE_GAS,
    OPENCALC_ELEMENT_ALKALI,
    OPENCALC_ELEMENT_ALKALINE,
    OPENCALC_ELEMENT_METALLOID,
    OPENCALC_ELEMENT_HALOGEN,
    OPENCALC_ELEMENT_POST_TRANSITION,
    OPENCALC_ELEMENT_TRANSITION,
    OPENCALC_ELEMENT_LANTHANIDE,
    OPENCALC_ELEMENT_ACTINIDE,
    OPENCALC_ELEMENT_UNKNOWN,
} opencalc_element_category_t;

typedef struct {
    const char *symbol;
    const char *name;
    const char *mass;
    uint8_t group;
    uint8_t period;
    opencalc_element_category_t category;
} opencalc_element_t;

typedef enum {
    OPENCALC_REFERENCE_MATH = 0,
    OPENCALC_REFERENCE_PHYSICS,
    OPENCALC_REFERENCE_ENGINEERING,
    OPENCALC_REFERENCE_CATEGORY_COUNT,
} opencalc_reference_category_t;

typedef struct {
    const char *title;
    const char *formula;
    const char *units;
    const char *note;
} opencalc_reference_entry_t;

const opencalc_element_t *opencalc_element_get(int atomic_number);
const char *opencalc_element_category_name(opencalc_element_category_t category);
int opencalc_periodic_atomic_number_at(int row, int column);
int opencalc_periodic_find_position(int atomic_number, int *row, int *column);

const char *opencalc_reference_category_name(opencalc_reference_category_t category);
size_t opencalc_reference_count(opencalc_reference_category_t category);
const opencalc_reference_entry_t *opencalc_reference_get(opencalc_reference_category_t category,
                                                          size_t index);

#endif
