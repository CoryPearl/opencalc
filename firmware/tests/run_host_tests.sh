#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CC=${CC:-cc}
OUT=${TMPDIR:-/tmp}/opencalc-host-tests
mkdir -p "$OUT"

CFLAGS="-std=c11 -Wall -Wextra -Werror -I$ROOT/main/components"
EIGEN_FLAGS="$CFLAGS -Wno-unused-parameter -Wno-sign-compare -DOPENCALC_EIGENMATH_EMBEDDED=1"
SYMBOLS="$ROOT/main/components/opencalc_symbols.c"

run_test() {
    name=$1
    shift
    echo "== $name =="
    "$CC" "$@" -lm -o "$OUT/$name"
    (cd "$ROOT" && "$OUT/$name")
}

run_test tiny_python $CFLAGS \
    "$ROOT/tests/tiny_python_regression.c" \
    "$ROOT/main/components/tiny-python.c"
run_test cas $EIGEN_FLAGS \
    "$ROOT/tests/cas_regression.c" \
    "$ROOT/main/components/opencalc_cas.c" \
    "$ROOT/main/components/opencalc_eigenmath.c" \
    "$ROOT/main/components/opencalc_math.c" \
    "$SYMBOLS" \
    "$ROOT/main/components/eigenmath/eigenmath.c"
run_test cas_experience $CFLAGS \
    "$ROOT/tests/cas_experience_regression.c" \
    "$ROOT/main/components/opencalc_cas_experience.c"
run_test eigenmath $EIGEN_FLAGS \
    "$ROOT/tests/eigenmath_regression.c" \
    "$ROOT/main/components/opencalc_eigenmath.c" \
    "$ROOT/main/components/eigenmath/eigenmath.c"
run_test calculator $EIGEN_FLAGS \
    "$ROOT/tests/calc_regression.c" \
    "$ROOT/main/components/opencalc_calc.c" \
    "$ROOT/main/components/opencalc_cas_experience.c" \
    "$ROOT/main/components/opencalc_cas.c" \
    "$ROOT/main/components/opencalc_eigenmath.c" \
    "$ROOT/main/components/opencalc_math.c" \
    "$SYMBOLS" \
    "$ROOT/main/components/opencalc_units.c" \
    "$ROOT/main/components/eigenmath/eigenmath.c"
run_test graph_modes $CFLAGS \
    "$ROOT/tests/graph_modes_regression.c" \
    "$ROOT/main/components/opencalc_math.c" \
    "$SYMBOLS"
run_test graph_model $CFLAGS \
    "$ROOT/tests/graph_model_regression.c" \
    "$ROOT/main/components/opencalc_graph_model.c"
run_test graph_series $CFLAGS \
    "$ROOT/tests/graph_series_regression.c" \
    "$ROOT/main/components/opencalc_graph_series.c" \
    "$ROOT/main/components/opencalc_math.c" \
    "$SYMBOLS"
run_test graph_analysis $CFLAGS \
    "$ROOT/tests/graph_analysis_regression.c" \
    "$ROOT/main/components/opencalc_graph_analysis.c" \
    "$ROOT/main/components/opencalc_graph_series.c" \
    "$ROOT/main/components/opencalc_math.c" \
    "$SYMBOLS"
run_test graph_symbolic $CFLAGS \
    "$ROOT/tests/graph_symbolic_regression.c" \
    "$ROOT/main/components/opencalc_graph_symbolic.c"
run_test graph_controller $CFLAGS \
    "$ROOT/tests/graph_controller_regression.c" \
    "$ROOT/main/components/opencalc_graph_controller.c" \
    "$ROOT/main/components/opencalc_graph_symbolic.c" \
    "$ROOT/main/components/opencalc_graph_series.c" \
    "$ROOT/main/components/opencalc_math.c" \
    "$SYMBOLS"
run_test math_layout $CFLAGS \
    "$ROOT/tests/math_layout_regression.c" \
    "$ROOT/main/components/opencalc_math_layout.c"
run_test stats $CFLAGS \
    "$ROOT/tests/stats_regression.c" \
    "$ROOT/main/components/opencalc_stats.c"
run_test conics $CFLAGS \
    "$ROOT/tests/conics_regression.c" \
    "$ROOT/main/components/opencalc_conics.c"
run_test inequality $CFLAGS \
    "$ROOT/tests/inequality_regression.c" \
    "$ROOT/main/components/opencalc_inequality.c" \
    "$ROOT/main/components/opencalc_math.c" \
    "$SYMBOLS"
run_test variables $CFLAGS \
    "$ROOT/tests/variables_regression.c" \
    "$ROOT/main/components/opencalc_math.c" \
    "$SYMBOLS"
run_test units $CFLAGS \
    "$ROOT/tests/units_regression.c" \
    "$ROOT/main/components/opencalc_units.c"
run_test reference $CFLAGS \
    "$ROOT/tests/reference_regression.c" \
    "$ROOT/main/components/opencalc_reference.c"
run_test workspace_io $CFLAGS \
    "$ROOT/tests/workspace_io_regression.c" \
    "$ROOT/main/components/opencalc_workspace_io.c"

echo "All OpenCalc host regressions passed."
