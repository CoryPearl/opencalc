# OpenCalc OS App Status

This file tracks the user-facing app surface in `main/components/opencalc_ui.c`.
It is intentionally practical: an app is only marked complete when the UI path,
key handling, storage/persistence path where needed, and math/game behavior all
exist in firmware.

## Overall Status

- Current stage: working hardware/software prototype, not a finished production
  release.
- The latest checked ESP-IDF build predates the Giac import and produced a
  1,234,096-byte app image. The Giac-enabled image and link-time DRAM use still
  need to be measured against the 6 MB factory app partition.
- Host regressions cover Tiny Python lifecycle/error recovery, the local
  polynomial CAS, and the embedded Eigenmath fallback. Giac has an optional
  on-device boot smoke test but has not yet completed OpenCalc hardware
  validation.
- Display, keypad, USB storage/serial, software-off power behavior, scripting,
  and all five games have been exercised on current hardware.
- Optional PAM8302A audio remains disabled by default and needs final validation
  on the new audio-equipped PCB.

## Launcher

Implemented:

- 12-icon home screen.
- Arrow-key navigation and Enter open behavior.
- Header icon/title/battery indicators.
- Centered, app-specific icon artwork.
- Game menu opened by `Alpha` then `2nd`.

Needs work:

- Final icon and label polish across different physical LCD batches.

## Calculator

Implemented:

- TI-style history display with expression/result lines.
- Cursor editing, token delete, clear, Ans, variables, fractions, roots, powers,
  trig, inverse trig, sec/csc/cot, logs, random, probability basics, numeric
  derivatives, definite integrals, simple symbolic derivatives, and simple
  symbolic indefinite integrals.
- Complex-number input/display for `i`, `a+bi`, and polar `re^ti` modes,
  including common CPX functions such as `conj`, `real`, `imag`, `abs`, and
  `angle`.
- Calculator variables `A` through `Z` can store and reuse complex values in
  complex mode.
- `nDeriv(expr,value)`, `nDeriv(expr,var,value)`, `fnInt(expr,a,b)`, and
  `fnInt(expr,var,a,b)`. They can use variables such as `x`, `t`, or `n`;
  complex-valued expressions are supported in complex mode along a real input
  path. Supported symbolic patterns are evaluated through the symbolic
  derivative/antiderivative first; more general expressions fall back to
  approximate numerical methods.
- Symbolic `deriv(expr[,var])` and `int(expr[,var])` support common constants,
  powers, linear terms, trig, log, root, and exponential patterns.
- Degree mode defaults on.
- `2nd` and Alpha indicators.
- Giac/KhiCAS-derived primary CAS backend for exact arithmetic, general
  symbolic simplification, equation solving, calculus, complex expressions,
  matrix expressions, and persistent symbolic assignments. The backend is
  serialized through a dedicated 64 KB PSRAM-backed task.
- Embedded Eigenmath and native polynomial routines remain fallback paths for
  unsupported or OpenCalc-specific operations.
- Finite symbolic limits with direct simplification and bounded repeated
  L'Hopital fallback for removable quotient singularities.
- Broad special-function, complex-transform, and matrix-function dispatch,
  including supported functions nested inside larger expressions.
- `CAS` and `ADV` Math-menu tabs expose the primary and advanced symbolic
  commands directly on the calculator.
- Fast local polynomial CAS through degree 10 for nested products, powers,
  expansion, factoring, solving, differentiation, and integration.

Needs work:

- The real Giac engine is integrated in source, but its ESP-IDF link size,
  startup behavior, long-running heap stability, and physical-device result
  rendering still require validation.
- The embedded port is not the complete desktop Xcas application. Desktop GUI,
  plotting, some platform facilities, and UI access to every Giac command are
  outside this integration.
- Results longer than the calculator's 192-byte result field are currently
  clipped with an ellipsis rather than shown in a dedicated scrolling view.
- Real-mode math intentionally rejects complex-valued variables instead of
  dropping the imaginary part.

## Graph

Implemented:

- 10 Cartesian `Y=` slots.
- Parametric graph definitions `X1T/Y1T` through `X6T/Y6T`.
- Polar graph definitions `r1` through `r6`.
- Sequence graph definitions `u1` through `u3`.
- Default `Y1=x`.
- Window controls for x/y max and x/y tick spacing.
- Trace, line cycling, zeros, y-intercepts, extrema, intersections, grid toggle,
  zoom in/out, table handoff, and inequality/conic overlays.
- Graph Calc value/derivative/integral/zero/extrema/intersection analysis runs
  for Cartesian, parametric, polar, and sequence modes. Cartesian intersections
  are refined; non-Cartesian intersections use sampled plotted-point matching.

Needs work:

- Split-screen, image backgrounds, and graph-style editing are planned but not
  complete.

## Table

Implemented:

- Numeric table for enabled Cartesian functions.
- Mode-specific tables for parametric, polar, and sequence graphing.
- Scroll in positive/negative x directions.
- Horizontal function-column paging past `Y3`.

Needs work:

- Table setup is currently minimal.

## Python / Programs

Implemented:

- Program menu with Run, Edit, New, and Delete.
- Script browser reads `/data/scripts`.
- Script output/input screen on LCD.
- `input()` accepts keypad text entry.
- Host regression coverage for arithmetic, loops, recursion, collections,
  `input()`, `int(input(...))`, file execution, repeated interpreter lifecycle,
  and recovery after syntax errors.

Needs work:

- Tiny Python is intentionally small and does not cover the full Python language.
- Script editor is intentionally basic: edit/save/delete work, but it is not a
  full IDE.

## Lists

Implemented:

- `L1` through `L6`.
- Up to 999 values per list.
- Edit, new/select, sum, min/max, sort, and clear.

Needs work:

- User-named lists and list-to-matrix conversion are planned but not complete.

## Statistics

Implemented:

- List editing shortcut.
- Sort ascending/descending.
- Clear lists.
- 1-var stats.
- 2-var stats from `L1`,`L2`.
- Linear, quadratic, exponential, and approximate median-median regression.
- Log, power, cubic, and quartic regression.
- Z-test, T-test, Chi-square summary, Z-interval, and T-interval outputs from
  current list/calculator inputs.
- ANOVA from populated `L1` through `L3`.
- Normal, inverse normal, Student's t, Chi-square, F, binomial, and Poisson
  distribution helpers.
- Dedicated Stats result screen for summaries, regressions, tests, intervals,
  ANOVA, and distribution outputs.
- Basic statistical plot viewer for scatter plots, XY-line plots, and
  histograms from `L1`/`L2`.

Needs work:

- Full AP Stats parity still needs richer per-test setup editors, normal
  probability/box plots, confidence/test variants beyond the core catalog,
  edge-case validation, and more display polish.

## Matrices

Implemented:

- Ten matrices, `A` through `J`, with storage up to 99x99 each.
- Set from calculator text, show, determinant, inverse, RREF, transpose, and
  identity.
- Augment selected matrix with the next matrix when row counts match.
- Convert selected list to a matrix column and matrix column 1 back to the
  selected list.
- Elementary row operations from Calc input:
  `swap,1,2`, `scale,1,2.5`, and `add,1,2,-3`.

Needs work:

- Large 99x99 matrices need hardware stress testing for speed and memory
  behavior.
- A dedicated cell-by-cell editor would be better for real-world matrix entry
  than typing semicolon-separated rows in Calc.

## Solver

Implemented:

- Numeric `E1=E2` solver.
- Guess-based root solving.
- Complex-mode `E1=E2` solving through numerical complex Newton iteration.
- Result-to-fraction helper.
- Polynomial-root helper for degree 1 through 10 coefficient lists entered in
  Calc, including complex roots.
- Dedicated polynomial-root detail screen with one root per row, selected-root
  real/imag detail, and a residual accuracy check.
- Linear-system helper from the selected augmented matrix, with no-solution and
  infinite-solution detection.

Needs work:

- The dedicated Solver app remains primarily numerical. Symbolic `solve(...)`
  is available in Calculator/CAS, but arbitrary symbolic systems are not yet a
  complete standalone Solver workflow.
- Polynomial roots are browsable, but the screen is still numerical-detail
  output rather than a symbolic factoring/proof view.

## Settings / Mode

Implemented:

- Brightness.
- Auto sleep.
- Power save.
- Dark/light theme.
- Audio volume in 5% steps.
- Factory reset.
- Mode menu for display format, print style, angle, graphing mode, and complex
  mode selection.

Needs work:

- Some mode values are stored as UI settings before their full math/display
  behavior exists.

## Finance

Implemented:

- TVM fields.
- Solve N, PV, PMT, and FV.
- Begin/end payments.
- NPV and IRR from selected list cash flows.

Needs work:

- Interest-rate solving and richer cash-flow editing are planned but not
  complete.

## Conics

Implemented:

- Line, circle, parabola, ellipse, and hyperbola templates.
- Add selected conic to graph.
- Basic key-point output.

Needs work:

- Full interactive coefficient solver and multi-conic analysis are planned but
  not complete.

## Inequality

Implemented:

- Free-form relation entry from calculator input, e.g. `y>=x^2` or `x<2`.
- Preset inequalities.
- Strict/inclusive line styles.
- Region shading and overlap shading.
- Vertical `x>=0` support.

Needs work:

- Linear-programming helpers and intersection storage to lists are planned but
  not complete.

## Games

Implemented:

- Game menu with Tetris, Doom, Snake, Breakout, and Mario.
- Persistent high scores through NVS.
- Held-key support for games that need it.
- Optional game audio when enabled in config and supported by PCB profile.

Needs work:

- The five games have been exercised on the current hardware. The optional
  audio path still needs final validation on the new audio-equipped PCB.
