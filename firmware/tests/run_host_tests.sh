#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CC=${CC:-cc}
OUT=${TMPDIR:-/tmp}/opencalc-host-tests
mkdir -p "$OUT"

CFLAGS="-std=c11 -Wall -Wextra -Werror -I$ROOT/main/components"
EIGEN_FLAGS="$CFLAGS -Wno-unused-parameter -Wno-sign-compare -DOPENCALC_EIGENMATH_EMBEDDED=1"

run_test() {
    name=$1
    shift
    echo "== $name =="
    "$CC" "$@" -lm -o "$OUT/$name"
    "$OUT/$name"
}

run_test tiny_python $CFLAGS \
    "$ROOT/tests/tiny_python_regression.c" \
    "$ROOT/main/components/tiny-python.c"
run_test cas $EIGEN_FLAGS \
    "$ROOT/tests/cas_regression.c" \
    "$ROOT/main/components/opencalc_cas.c" \
    "$ROOT/main/components/opencalc_eigenmath.c" \
    "$ROOT/main/components/opencalc_math.c" \
    "$ROOT/main/components/eigenmath/eigenmath.c"
run_test eigenmath $EIGEN_FLAGS \
    "$ROOT/tests/eigenmath_regression.c" \
    "$ROOT/main/components/opencalc_eigenmath.c" \
    "$ROOT/main/components/eigenmath/eigenmath.c"
run_test calculator $EIGEN_FLAGS \
    "$ROOT/tests/calc_regression.c" \
    "$ROOT/main/components/opencalc_calc.c" \
    "$ROOT/main/components/opencalc_cas.c" \
    "$ROOT/main/components/opencalc_eigenmath.c" \
    "$ROOT/main/components/opencalc_math.c" \
    "$ROOT/main/components/opencalc_units.c" \
    "$ROOT/main/components/eigenmath/eigenmath.c"
run_test graph_modes $CFLAGS \
    "$ROOT/tests/graph_modes_regression.c" \
    "$ROOT/main/components/opencalc_math.c"
run_test graph_model $CFLAGS \
    "$ROOT/tests/graph_model_regression.c" \
    "$ROOT/main/components/opencalc_graph_model.c"
run_test stats $CFLAGS \
    "$ROOT/tests/stats_regression.c" \
    "$ROOT/main/components/opencalc_stats.c"
run_test conics $CFLAGS \
    "$ROOT/tests/conics_regression.c" \
    "$ROOT/main/components/opencalc_conics.c"
run_test inequality $CFLAGS \
    "$ROOT/tests/inequality_regression.c" \
    "$ROOT/main/components/opencalc_inequality.c" \
    "$ROOT/main/components/opencalc_math.c"
run_test variables $CFLAGS \
    "$ROOT/tests/variables_regression.c" \
    "$ROOT/main/components/opencalc_math.c"
run_test units $CFLAGS \
    "$ROOT/tests/units_regression.c" \
    "$ROOT/main/components/opencalc_units.c"
run_test reference $CFLAGS \
    "$ROOT/tests/reference_regression.c" \
    "$ROOT/main/components/opencalc_reference.c"

echo "All OpenCalc host regressions passed."
