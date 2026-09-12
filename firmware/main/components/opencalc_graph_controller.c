#include "opencalc_graph_controller.h"

#include "opencalc_cas.h"
#include "opencalc_giac.h"
#include "opencalc_graph_symbolic.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool symbolic_eval(const char *expression, bool degrees, char *out, size_t out_size)
{
    if (opencalc_giac_eval(expression, degrees, out, out_size) ||
        opencalc_cas_eval(expression, out, out_size)) {
        return true;
    }
    snprintf(out, out_size, "not available");
    return false;
}

static bool symbolic_eval_format(bool degrees, char *out, size_t out_size,
                                 const char *format, ...)
{
    char command[OPENCALC_GRAPH_SYMBOLIC_COMMAND_MAX];
    va_list args;
    va_start(args, format);
    int written = vsnprintf(command, sizeof(command), format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= sizeof(command)) {
        snprintf(out, out_size, "expression too long");
        return false;
    }
    return symbolic_eval(command, degrees, out, out_size);
}

static bool analyze_recurrence(const opencalc_graph_symbolic_request_t *request,
                               opencalc_graph_scalar_eval_fn numeric_eval,
                               opencalc_graph_symbolic_result_t *result,
                               bool *analysis_ok)
{
    if (analysis_ok != NULL) *analysis_ok = false;
    opencalc_graph_sequence_t sequence;
    if (!opencalc_graph_sequence_parse(request->primary, &sequence) ||
        sequence.kind != OPENCALC_GRAPH_SEQUENCE_RECURRENCE) {
        return false;
    }

    int first_index = request->range_first < 0 ? 0 : request->range_first;
    int last_index = request->range_last < first_index ? first_index : request->range_last;
    if (last_index > 4096) last_index = 4096;
    opencalc_graph_sequence_analysis_t analysis;
    if (!opencalc_graph_sequence_analyze(&sequence, first_index, last_index,
                                         numeric_eval, &analysis)) {
        snprintf(result->derivative, sizeof(result->derivative), "numeric analysis failed");
        snprintf(result->integral, sizeof(result->integral), "range n=%d..%d",
                 first_index, last_index);
        snprintf(result->roots, sizeof(result->roots), "none calculated");
        snprintf(result->asymptotes, sizeof(result->asymptotes), "end behavior unavailable");
        return true;
    }

    snprintf(result->derivative, sizeof(result->derivative),
             "delta u(%d)=%.8g", analysis.last_index, analysis.delta);
    snprintf(result->integral, sizeof(result->integral), "sum n=%d..%d = %.10g",
             analysis.first_index, analysis.last_index, analysis.sum);
    if (analysis.zero_count == 0) {
        snprintf(result->roots, sizeof(result->roots), "none in n=%d..%d",
                 analysis.first_index, analysis.last_index);
    } else {
        size_t used = 0;
        for (int i = 0; i < analysis.zero_count; i++) {
            int written = snprintf(result->roots + used, sizeof(result->roots) - used,
                                   "%s%d", i ? ", " : "n=", analysis.zero_indices[i]);
            if (written < 0 || (size_t)written >= sizeof(result->roots) - used) break;
            used += (size_t)written;
        }
    }
    snprintf(result->asymptotes, sizeof(result->asymptotes), "%s; u(%d)=%.10g",
             opencalc_graph_sequence_end_name(analysis.end_behavior),
             analysis.last_index, analysis.last);
    if (analysis_ok != NULL) *analysis_ok = true;
    return true;
}

bool opencalc_graph_symbolic_analyze(
    const opencalc_graph_symbolic_request_t *request,
    bool degrees,
    opencalc_graph_scalar_eval_fn numeric_eval,
    opencalc_graph_symbolic_result_t *result)
{
    if (request == NULL || result == NULL || request->primary[0] == '\0') return false;
    memset(result, 0, sizeof(*result));
    result->graphing_mode = request->graphing_mode;
    result->series = request->series;
    result->fingerprint = request->fingerprint;
    const char *primary = request->primary;
    const char *secondary = request->secondary;
    bool ok = true;

    switch (request->graphing_mode) {
    case 1:
        ok &= symbolic_eval_format(degrees, result->derivative, sizeof(result->derivative),
                                   "normal(diff((%s),t)/diff((%s),t))", secondary, primary);
        ok &= symbolic_eval_format(degrees, result->integral, sizeof(result->integral),
                                   "integrate((%s)*diff((%s),t),t)", secondary, primary);
        ok &= symbolic_eval_format(degrees, result->roots, sizeof(result->roots),
                                   "solve((%s)=0,t)", secondary);
        ok &= symbolic_eval_format(degrees, result->asymptotes, sizeof(result->asymptotes),
                                   "limit([(%s),(%s)],t,infinity)", primary, secondary);
        break;
    case 2: {
        char command[OPENCALC_GRAPH_SYMBOLIC_COMMAND_MAX];
        if (opencalc_graph_symbolic_polar_derivative_command(primary, command, sizeof(command))) {
            ok &= symbolic_eval(command, degrees, result->derivative, sizeof(result->derivative));
        } else {
            snprintf(result->derivative, sizeof(result->derivative), "expression too long");
            ok = false;
        }
        ok &= symbolic_eval_format(degrees, result->integral, sizeof(result->integral),
                                   "integrate((%s)^2/2,t)", primary);
        ok &= symbolic_eval_format(degrees, result->roots, sizeof(result->roots),
                                   "solve((%s)=0,t)", primary);
        ok &= symbolic_eval_format(degrees, result->asymptotes, sizeof(result->asymptotes),
                                   "limit((%s),t,infinity)", primary);
        break;
    }
    case 3: {
        bool recurrence_ok = false;
        if (analyze_recurrence(request, numeric_eval, result, &recurrence_ok)) return recurrence_ok;
        ok &= symbolic_eval_format(degrees, result->derivative, sizeof(result->derivative),
                                   "simplify(subst((%s),n,n+1)-(%s))", primary, primary);
        ok &= symbolic_eval_format(degrees, result->integral, sizeof(result->integral),
                                   "sum((%s),n)", primary);
        ok &= symbolic_eval_format(degrees, result->roots, sizeof(result->roots),
                                   "solve((%s)=0,n)", primary);
        ok &= symbolic_eval_format(degrees, result->asymptotes, sizeof(result->asymptotes),
                                   "limit((%s),n,infinity)", primary);
        break;
    }
    default: {
        char vertical[72];
        char positive[72];
        char negative[72];
        ok &= symbolic_eval_format(degrees, result->derivative, sizeof(result->derivative),
                                   "diff((%s),x)", primary);
        ok &= symbolic_eval_format(degrees, result->integral, sizeof(result->integral),
                                   "integrate((%s),x)", primary);
        ok &= symbolic_eval_format(degrees, result->roots, sizeof(result->roots),
                                   "solve((%s)=0,x)", primary);
        ok &= symbolic_eval_format(degrees, vertical, sizeof(vertical),
                                   "solve(denom(normal((%s)))=0,x)", primary);
        ok &= symbolic_eval_format(degrees, positive, sizeof(positive),
            "[limit((%s)/x,x,infinity),limit((%s)-limit((%s)/x,x,infinity)*x,x,infinity)]",
            primary, primary, primary);
        ok &= symbolic_eval_format(degrees, negative, sizeof(negative),
            "[limit((%s)/x,x,-infinity),limit((%s)-limit((%s)/x,x,-infinity)*x,x,-infinity)]",
            primary, primary, primary);
        snprintf(result->asymptotes, sizeof(result->asymptotes),
                 "vertical %.40s; [slope,offset] +%.40s -%.40s", vertical, positive, negative);
        break;
    }
    }
    return ok;
}
