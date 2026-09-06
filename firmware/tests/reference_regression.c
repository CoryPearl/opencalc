#include "opencalc_reference.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    int seen[119] = {0};

    for (int row = 0; row < 9; ++row) {
        for (int column = 0; column < 18; ++column) {
            int atomic_number = opencalc_periodic_atomic_number_at(row, column);
            if (atomic_number == 0) continue;
            if (atomic_number < 1 || atomic_number > 118 || seen[atomic_number]++) {
                fprintf(stderr, "invalid or duplicate element %d at %d,%d\n",
                        atomic_number, row, column);
                return 1;
            }
        }
    }

    for (int atomic_number = 1; atomic_number <= 118; ++atomic_number) {
        const opencalc_element_t *element = opencalc_element_get(atomic_number);
        int row = -1;
        int column = -1;
        if (element == NULL || element->symbol == NULL || element->symbol[0] == '\0' ||
            element->name == NULL || element->name[0] == '\0' ||
            element->mass == NULL || element->mass[0] == '\0' || seen[atomic_number] != 1 ||
            !opencalc_periodic_find_position(atomic_number, &row, &column) ||
            opencalc_periodic_atomic_number_at(row, column) != atomic_number) {
            fprintf(stderr, "incomplete element record %d\n", atomic_number);
            return 1;
        }
    }

    if (strcmp(opencalc_element_get(1)->symbol, "H") != 0 ||
        strcmp(opencalc_element_get(79)->symbol, "Au") != 0 ||
        strcmp(opencalc_element_get(118)->symbol, "Og") != 0 ||
        opencalc_element_get(0) != NULL || opencalc_element_get(119) != NULL) {
        fprintf(stderr, "element lookup regression\n");
        return 1;
    }

    for (int category = 0; category < OPENCALC_REFERENCE_CATEGORY_COUNT; ++category) {
        size_t count = opencalc_reference_count((opencalc_reference_category_t)category);
        if (count < 8) {
            fprintf(stderr, "reference category %d is too small\n", category);
            return 1;
        }
        for (size_t index = 0; index < count; ++index) {
            const opencalc_reference_entry_t *entry =
                opencalc_reference_get((opencalc_reference_category_t)category, index);
            if (entry == NULL || entry->title[0] == '\0' || entry->formula[0] == '\0' ||
                entry->units[0] == '\0' || entry->note[0] == '\0') {
                fprintf(stderr, "incomplete reference %d:%zu\n", category, index);
                return 1;
            }
        }
    }

    puts("PASS reference database");
    return 0;
}
