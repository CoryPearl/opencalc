#include "opencalc_graph_series.h"
#include "opencalc_math.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    opencalc_graph_sequence_t sequence;
    double value = 0.0;
    assert(opencalc_graph_sequence_parse("n^2+n", &sequence));
    assert(sequence.kind == OPENCALC_GRAPH_SEQUENCE_DIRECT);
    assert(opencalc_graph_sequence_eval(&sequence, 5, graph_eval_expression_var, &value));
    assert(fabs(value - 30.0) < 1e-9);

    assert(opencalc_graph_sequence_parse("rec(u(n-1)+u(n-2),0,1)", &sequence));
    assert(sequence.kind == OPENCALC_GRAPH_SEQUENCE_RECURRENCE);
    assert(opencalc_graph_sequence_eval(&sequence, 10, graph_eval_expression_var, &value));
    assert(fabs(value - 55.0) < 1e-9);
    opencalc_graph_sequence_analysis_t analysis;
    assert(opencalc_graph_sequence_analyze(&sequence, 0, 10,
                                           graph_eval_expression_var, &analysis));
    assert(analysis.evaluated == 11 && fabs(analysis.last - 55.0) < 1e-9);
    assert(analysis.zero_count == 1 && analysis.zero_indices[0] == 0);
    assert(analysis.end_behavior == OPENCALC_GRAPH_SEQUENCE_END_INCREASING);

    assert(opencalc_graph_sequence_parse("rec(u(n-1)/2+1,0,1)", &sequence));
    assert(opencalc_graph_sequence_analyze(&sequence, 0, 32,
                                           graph_eval_expression_var, &analysis));
    assert(fabs(analysis.last - 2.0) < 1e-7);
    assert(analysis.end_behavior == OPENCALC_GRAPH_SEQUENCE_END_STEADY);
    assert(!opencalc_graph_sequence_parse("rec(u(n-1),0)", &sequence));

    assert(opencalc_graph_segment_is_continuous(0, 0, 0.5, 0.5, 1, 1, 10, 10));
    assert(!opencalc_graph_segment_is_continuous(-0.01, -100, 0, 100000, 0.01, 100,
                                                  20, 12));
    puts("PASS graph series evaluation and continuity");
    return 0;
}
