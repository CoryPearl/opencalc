#include "opencalc_cas_experience.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static const opencalc_cas_catalog_entry_t CATALOG[] = {
    {"simplify", "simplify(", "Canonical simplification", "Algebra"},
    {"factor", "factor(", "Factor an expression", "Algebra"},
    {"expand", "expand(", "Expand products and powers", "Algebra"},
    {"normal", "normal(", "Reduce a rational expression", "Algebra"},
    {"partfrac", "partfrac(", "Partial fraction decomposition", "Algebra"},
    {"solve", "solve(", "Exact equation or system solutions", "Solve"},
    {"fsolve", "fsolve(", "Numerical solution near a guess", "Solve"},
    {"isolve", "isolve(", "Integer solutions", "Solve"},
    {"zeros", "zeros(", "Polynomial zero set", "Solve"},
    {"diff", "diff(", "Symbolic derivative", "Calculus"},
    {"integrate", "integrate(", "Indefinite or definite integral", "Calculus"},
    {"limit", "limit(", "Symbolic limit", "Calculus"},
    {"sum", "sum(", "Symbolic or finite sum", "Calculus"},
    {"product", "product(", "Symbolic or finite product", "Calculus"},
    {"taylor", "taylor(", "Taylor expansion", "Calculus"},
    {"desolve", "desolve(", "Differential equation solution", "Calculus"},
    {"subst", "subst(", "Substitute into an expression", "Transform"},
    {"assume", "assume(", "Set a symbol assumption", "Transform"},
    {"purge", "purge(", "Remove a stored CAS symbol", "Transform"},
    {"evalf", "evalf(", "Decimal approximation", "Numeric"},
    {"exact", "exact(", "Recover an exact value", "Numeric"},
    {"re", "re(", "Real part", "Complex"},
    {"im", "im(", "Imaginary part", "Complex"},
    {"conj", "conj(", "Complex conjugate", "Complex"},
    {"arg", "arg(", "Complex argument", "Complex"},
    {"det", "det(", "Matrix determinant", "Matrix"},
    {"inv", "inv(", "Matrix inverse", "Matrix"},
    {"rref", "rref(", "Reduced row echelon form", "Matrix"},
    {"rank", "rank(", "Matrix rank", "Matrix"},
    {"tran", "tran(", "Matrix transpose", "Matrix"},
    {"eigenvals", "eigenvals(", "Matrix eigenvalues", "Matrix"},
    {"eigenvects", "eigenvects(", "Matrix eigenvectors", "Matrix"},
};

void opencalc_cas_options_default(opencalc_cas_options_t *options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->domain = OPENCALC_CAS_DOMAIN_AUTO;
    options->interval_min = -10.0;
    options->interval_max = 10.0;
}

const char *opencalc_cas_domain_name(opencalc_cas_domain_t domain)
{
    static const char *const names[] = {"Auto", "Real", "Complex", "Integer", "Interval"};
    return domain >= 0 && domain < OPENCALC_CAS_DOMAIN_COUNT ? names[domain] : "Auto";
}

static const char *skip_space(const char *text)
{
    while (text != NULL && isspace((unsigned char)*text)) text++;
    return text;
}

static bool starts_command(const char *text, const char *name)
{
    text = skip_space(text);
    size_t length = strlen(name);
    return strncasecmp(text, name, length) == 0 && text[length] == '(';
}

static bool replace_solve(const char *expression, const char *command,
                          char *out, size_t out_size)
{
    const char *start = skip_space(expression);
    if (!starts_command(start, "solve")) return false;
    int written = snprintf(out, out_size, "%s%s", command, start + 5);
    return written >= 0 && (size_t)written < out_size;
}

static bool interval_solve(const char *expression, const opencalc_cas_options_t *options,
                           char *out, size_t out_size)
{
    const char *start = skip_space(expression);
    if (!starts_command(start, "solve")) return false;
    const char *args = strchr(start, '(') + 1;
    const char *end = strrchr(args, ')');
    if (end == NULL) return false;
    int depth = 0;
    const char *comma = NULL;
    for (const char *p = args; p < end; p++) {
        if (*p == '(' || *p == '[' || *p == '{') depth++;
        else if (*p == ')' || *p == ']' || *p == '}') depth--;
        else if (*p == ',' && depth == 0) comma = p;
    }
    if (comma == NULL) return false;
    const char *variable = skip_space(comma + 1);
    const char *variable_end = end;
    while (variable_end > variable && isspace((unsigned char)variable_end[-1])) variable_end--;
    if (variable_end <= variable) return false;
    int written = snprintf(out, out_size, "fsolve(%.*s,%.*s=%.12g..%.12g)",
                           (int)(comma - args), args,
                           (int)(variable_end - variable), variable,
                           options->interval_min, options->interval_max);
    return written >= 0 && (size_t)written < out_size;
}

bool opencalc_cas_prepare_expression(const char *expression,
                                     const opencalc_cas_options_t *options,
                                     char *out, size_t out_size)
{
    if (expression == NULL || options == NULL || out == NULL || out_size == 0) return false;
    char domain_expression[1152];
    bool transformed = false;
    if (options->domain == OPENCALC_CAS_DOMAIN_INTEGER) {
        transformed = replace_solve(expression, "isolve", domain_expression,
                                    sizeof(domain_expression));
    } else if (options->domain == OPENCALC_CAS_DOMAIN_INTERVAL) {
        transformed = interval_solve(expression, options, domain_expression,
                                     sizeof(domain_expression));
    }
    if (!transformed) {
        if (strlen(expression) >= sizeof(domain_expression)) return false;
        memcpy(domain_expression, expression, strlen(expression) + 1);
    }
    if (strlen(domain_expression) >= out_size) return false;
    memcpy(out, domain_expression, strlen(domain_expression) + 1);
    return true;
}

static bool text_is_exact(const char *text)
{
    for (const char *p = text; p != NULL && *p != '\0'; p++) {
        if (*p == '.' && isdigit((unsigned char)p[1]) && p > text && isdigit((unsigned char)p[-1])) {
            return false;
        }
    }
    return true;
}

static short find_relation(const char *text, size_t start, size_t end)
{
    int depth = 0;
    for (size_t i = start; i < end; i++) {
        char c = text[i];
        if (c == '(' || c == '[' || c == '{') depth++;
        else if (c == ')' || c == ']' || c == '}') depth--;
        else if (depth == 0 && c == '=') return (short)(i - start);
    }
    return -1;
}

static void add_item(const char *text, size_t start, size_t end,
                     opencalc_cas_result_t *result)
{
    while (start < end && isspace((unsigned char)text[start])) start++;
    while (end > start && isspace((unsigned char)text[end - 1])) end--;
    if (end <= start) return;
    if (result->item_count >= OPENCALC_CAS_RESULT_ITEMS || start > 65535 || end - start > 65535) {
        result->truncated = true;
        return;
    }
    opencalc_cas_result_item_t *item = &result->items[result->item_count++];
    item->offset = (unsigned short)start;
    item->length = (unsigned short)(end - start);
    item->relation_offset = find_relation(text, start, end);
}

void opencalc_cas_analyze_result(const char *text, opencalc_cas_result_t *result)
{
    if (result == NULL) return;
    memset(result, 0, sizeof(*result));
    result->kind = OPENCALC_CAS_RESULT_SCALAR;
    if (text == NULL) return;
    size_t length = strlen(text);
    result->exact = text_is_exact(text);
    if (length == 0) return;
    if (strncasecmp(text, "error", 5) == 0 || strncasecmp(text, "CAS ", 4) == 0) {
        result->kind = OPENCALC_CAS_RESULT_ERROR;
        add_item(text, 0, length, result);
        return;
    }

    size_t start = 0, end = length;
    while (start < end && isspace((unsigned char)text[start])) start++;
    while (end > start && isspace((unsigned char)text[end - 1])) end--;
    bool container = end > start + 1 &&
        ((text[start] == '[' && text[end - 1] == ']') ||
         (text[start] == '{' && text[end - 1] == '}'));
    if (!container) {
        add_item(text, start, end, result);
        if (result->item_count > 0 && result->items[0].relation_offset >= 0) {
            result->kind = OPENCALC_CAS_RESULT_SOLUTIONS;
        }
        return;
    }

    bool matrix = start + 1 < end && (text[start + 1] == '[' || text[start + 1] == '{');
    int depth = 0;
    size_t item_start = start + 1;
    for (size_t i = item_start; i < end - 1; i++) {
        char c = text[i];
        if (c == '(' || c == '[' || c == '{') depth++;
        else if (c == ')' || c == ']' || c == '}') depth--;
        else if (c == ',' && depth == 0) {
            add_item(text, item_start, i, result);
            item_start = i + 1;
        }
    }
    add_item(text, item_start, end - 1, result);
    result->kind = matrix ? OPENCALC_CAS_RESULT_MATRIX : OPENCALC_CAS_RESULT_LIST;
    for (int i = 0; i < result->item_count; i++) {
        if (result->items[i].relation_offset >= 0) {
            result->kind = OPENCALC_CAS_RESULT_SOLUTIONS;
            break;
        }
    }
}

bool opencalc_cas_result_item_text(const char *text,
                                   const opencalc_cas_result_t *result,
                                   int index, char *out, size_t out_size)
{
    if (text == NULL || result == NULL || out == NULL || out_size == 0 ||
        index < 0 || index >= result->item_count) return false;
    size_t count = result->items[index].length;
    if (count >= out_size) count = out_size - 1;
    memcpy(out, text + result->items[index].offset, count);
    out[count] = '\0';
    return true;
}

size_t opencalc_cas_catalog_count(void)
{
    return sizeof(CATALOG) / sizeof(CATALOG[0]);
}

const opencalc_cas_catalog_entry_t *opencalc_cas_catalog_at(size_t index)
{
    return index < opencalc_cas_catalog_count() ? &CATALOG[index] : NULL;
}

int opencalc_cas_catalog_complete(const char *prefix, int after_index)
{
    size_t count = opencalc_cas_catalog_count();
    size_t length = prefix != NULL ? strlen(prefix) : 0;
    for (size_t step = 1; step <= count; step++) {
        size_t index = ((size_t)(after_index < -1 ? -1 : after_index) + step) % count;
        if (length == 0 || strncasecmp(CATALOG[index].name, prefix, length) == 0) return (int)index;
    }
    return -1;
}
