#include "opencalc_graph_model.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    opencalc_graph_model_reset();
    opencalc_graph_model_t *model = opencalc_graph_model();
    assert(model != NULL);
    assert(strcmp(model->expressions[0], "x") == 0 && model->enabled[0]);
    assert(strcmp(model->param_x[0], "cos(t)") == 0);
    assert(strcmp(model->param_y[0], "sin(t)") == 0);
    assert(strcmp(model->polar_expressions[0], "1") == 0);
    assert(strcmp(model->sequence_expressions[0], "n") == 0);
    assert(fabs(model->x_min + 10.0) < 1e-12 && fabs(model->x_max - 10.0) < 1e-12);
    assert(model->table_rows == 9 && model->table_precision == 2 && model->grid);
    puts("PASS graph model defaults");
    return 0;
}
