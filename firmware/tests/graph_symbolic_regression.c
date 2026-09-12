#include "opencalc_graph_symbolic.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int occurrence_count(const char *text, const char *needle)
{
    int count = 0;
    size_t length = strlen(needle);
    while ((text = strstr(text, needle)) != NULL) {
        count++;
        text += length;
    }
    return count;
}

int main(void)
{
    char expression[OPENCALC_GRAPH_EXPRESSION_MAX];
    for (size_t i = 0; i + 1 < sizeof(expression); i++) {
        expression[i] = (i & 1U) == 0 ? 't' : '+';
    }
    expression[sizeof(expression) - 1] = '\0';

    char command[OPENCALC_GRAPH_SYMBOLIC_COMMAND_MAX];
    assert(opencalc_graph_symbolic_polar_derivative_command(
        expression, command, sizeof(command)));
    assert(strlen(command) > 384);
    assert(occurrence_count(command, expression) == 4);

    char truncated[384];
    assert(!opencalc_graph_symbolic_polar_derivative_command(
        expression, truncated, sizeof(truncated)));
    assert(truncated[sizeof(truncated) - 1] == '\0');

    puts("PASS graph symbolic command construction");
    return 0;
}
