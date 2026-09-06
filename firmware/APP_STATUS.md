# OpenCalc OS App Status

This file tracks the user-facing app surface coordinated by
`main/components/opencalc_ui.c` and its app/service modules.
It is intentionally practical: an app is only marked complete when the UI path,
key handling, storage/persistence path where needed, and math/game behavior all
exist in firmware.

## Overall Status

- Current stage: working hardware/software prototype, not a finished production
  release.
- The September 4, 2026 Giac-enabled ESP-IDF build succeeds at 5,204,208 bytes,
  leaving 1,087,248 bytes (17%) free in the 6 MB factory app partition. Large CAS
  and math-worker stacks are allocated from PSRAM.
- Host regressions cover Tiny Python lifecycle/error recovery, the local
  polynomial CAS, embedded Eigenmath fallback, all four graph evaluator modes,
  and statistics distributions/intervals/edge cases. Giac has an optional
  on-device boot smoke test but still needs broader long-running hardware
  validation.
- Display, keypad, USB storage/serial, software-off power behavior, the earlier
  scripting path, and all five games have been exercised on current hardware.
  The current asynchronous scripting path is built and host-tested but needs a
  fresh physical-device soak pass.
- Optional PAM8302A audio remains disabled by default and needs final validation
  on the new audio-equipped PCB.

## Launcher

Implemented:

- 12-icon home screen.
- Arrow-key navigation and Enter open behavior.
- Header icon/title/battery indicators.
- Centered, app-specific icon artwork.
- Game menu opened by `Alpha` then `2nd`.
- `Back` unwinds the current workflow and then returns to the previously active
  app; `On/Home` always returns directly to the launcher.
- Scrollable browsers and action menus show a position scrollbar.

Needs work:

- Final icon and label polish across different physical LCD batches.

## Reference Center

Implemented:

- `Alpha` + `Zoom` opens a dedicated reference hub without changing the
  balanced 12-icon launcher.
- Spatial 9x18 periodic-table layout containing every element from hydrogen
  through oganesson exactly once.
- Arrow navigation, color-coded classifications, selected-element summary,
  and a larger detail card with atomic number, mass, group, and period.
- Scrollable Math, Physics, and Engineering catalogs with formula, units, and
  explanatory detail views.
- Host data-integrity regression covering all 118 elements and every formula entry.

Needs work:

- Physical LCD review for the smallest periodic-table cell labels.

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
- `STO` stores the current value or expression into `A` through `Z` or a
  custom named variable; `VARS` inserts a stored reference and `2nd` + `STO`
  (`GET`) inserts its current value.
- The Variables browser groups user variables, lists, matrices, functions,
  strings, statistics results, graph variables, and system variables, with
  type/value previews plus rename and delete controls.
- User variables support real and complex values, are usable directly in
  expressions, and persist in on-chip NVS across power cycles and firmware
  restarts. Custom identifiers are case-insensitive and may contain letters,
  digits, and underscores.
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
- Unit-aware scalar arithmetic for common SI and US customary units, including
  compound dimensions, integer powers, compatible-unit conversion, and clear
  errors when dimensions do not match.
- Calculator evaluation runs asynchronously through the PSRAM-backed math
  worker. Each request has an ID, `Back` cancels the active request, stale
  completions are discarded, and Giac waits time out instead of freezing the UI.
- Expressions support 768 bytes and answers/history support 1024 bytes. Answers
  too wide for a history row open in a scrollable full-result screen.

Needs work:

- The real Giac engine builds and its task/context startup path is integrated,
  but long-running heap stability, broad command coverage, and physical-device
  rendering still require validation.
- The embedded port is not the complete desktop Xcas application. Desktop GUI,
  plotting, some platform facilities, and UI access to every Giac command are
  outside this integration.
- Giac currently formats at most 1024 bytes per result; larger desktop-scale
  symbolic output is still truncated at the engine boundary.
- Real-mode math intentionally rejects complex-valued variables instead of
  dropping the imaginary part.
- Unit values currently normalize to SI base units. Affine temperatures such
  as Celsius/Fahrenheit, user-defined units, unit-bearing stored variables, and
  symbolic unit algebra through every CAS command are not yet implemented.

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
- Full-height and split graph/table views.
- Per-series line, thick, dotted, and point rendering styles.
- Optional uncompressed 24-bit or 32-bit `320x240` BMP background loaded from
  `/data/graph.bmp`; graph format controls are under `2nd` + `Zoom`.
- Graph Calc value/derivative/integral/zero/extrema/intersection analysis runs
  for Cartesian, parametric, polar, and sequence modes. Parametric and polar
  derivatives report `dy/dx`; parametric integration computes `integral y dx`,
  polar integration computes enclosed area, and sequence calculus uses forward
  differences and discrete sums. Cartesian intersections are refined;
  non-Cartesian intersections use sampled plotted-point matching.
- Trace can cycle among enabled series, and the active series is identified in
  the trace readout rather than by a full-width selection bar.
- `Alpha` + `Graph` opens linked symbolic analysis for the active series. It
  reports exact CAS derivatives, integrals, roots, and asymptotic/end behavior;
  Cartesian tangent lines and integral shading can be toggled interactively and
  continue to use the same equation data as Graph and Table.

Needs work:

- Non-Cartesian intersections and points of interest are sampled rather than
  analytically refined, so closely spaced or tangent points can still be missed.

## Table

Implemented:

- Numeric table for enabled Cartesian functions.
- Mode-specific tables for parametric, polar, and sequence graphing.
- Compact live table embedded below the graph in split view.
- Scroll in positive/negative x directions.
- Horizontal function-column paging past `Y3`.
- `2nd` + `Window` opens table setup for start, step, visible rows, and decimal
  precision; `2nd` + `Graph` opens the table from any normal app screen.

Needs work:

- Table generation still evaluates rows on demand rather than caching a large
  dataset, so very expensive expressions can make scrolling feel uneven.

## Python / Programs

Implemented:

- Run, Debug, Edit, New, and Delete workflows with editor line breakpoints.
- Statement stepping and continue controls with live variable inspection,
  compact traceback viewing, and statement/function/depth/time profiling.
- A PSRAM-backed worker with cooperative statement, call-depth, active-time, and
  cancellation limits; input and debugger waits remain interruptible.
- Preloaded bounded `graphics`, `keys`, `storage`, `audio`, and `math` modules.
- Graphics commands are replayed by the UI task, and script storage is confined
  to simple file names under `/data/user/`.

- Program menu with Run, Debug, Edit, New, and Delete.
- Script browser reads `/data/scripts`.
- Script output/input screen on LCD.
- `input()` accepts keypad text entry.
- Host regression coverage for arithmetic, range and container iteration,
  membership, sequence operations, collection methods, common builtins,
  recursion, input, file execution, prolonged interpreter lifecycle, repeated
  same-runtime execution, syntax-error recovery, debugger/profile callbacks,
  native module dispatch, execution limits, and post-error runtime reuse.
- Lists, tuples, strings, and dictionaries are iterable; list/dictionary/string
  methods and Python-style negative division, modulo, and powers cover common
  calculator scripts.
- Script evaluation runs on a dedicated 32 KB internal-RAM worker task reserved early so FATFS
  calls remain safe during flash-cache operations. Parser/program storage and
  dynamic containers use PSRAM. The UI task alone owns LCD drawing and keypad
  dispatch; output and `input()` use synchronized buffers and notifications.

Needs work:

- Tiny Python is intentionally small and does not provide CPython's imports,
  exceptions, classes, comprehensions, generators, full scope model, standard
  library, or garbage collector.
- Script editor remains compact: it has source editing and line breakpoints, but
  not desktop features such as project-wide search or conditional breakpoints.
- The asynchronous worker architecture needs repeated on-device run, input,
  syntax-error, cancel, and exit testing before the former crash history can be
  considered closed.

## Lists

Implemented:

- `L1` through `L6`.
- Up to 999 values per list.
- Edit, new/select, sum, min/max, and clear.
- Dedicated list workspace with an `L1`-through-`L6` slot strip, selected-list
  preview, and compact action list. Left/right changes the selected list while
  up/down changes the selected action.

Needs work:

- User-named lists are not implemented. Matrix column/list conversion is
  available from the Matrix workspace.

## Statistics

Implemented:

- List editing shortcut.
- Sort ascending/descending.
- Clear lists.
- 1-var stats.
- 2-var stats from `L1`,`L2`.
- Linear, quadratic, exponential, and approximate median-median regression.
- Log, power, cubic, and quartic regression.
- Dedicated field-by-field setup screens for every hypothesis test,
  confidence interval, ANOVA workflow, and probability distribution. Setup
  fields support list selection, editable parameters, confidence levels, and
  `<`, `>`, or `not equal` alternatives where applicable.
- One-sample Z and T tests, Chi-square goodness-of-fit, one- and two-proportion
  Z tests, and a Welch two-sample T test.
- One-sample Z and T intervals, one- and two-proportion Z intervals,
  two-sample Z intervals with known sigmas, and Welch two-sample T intervals.
- One-way ANOVA from three selectable lists.
- Normal, inverse normal, Student's t, Chi-square, F, binomial, and Poisson
  distribution helpers.
- Dedicated Stats result screen for summaries, regressions, tests, intervals,
  ANOVA, and distribution outputs.
- Statistical plot viewer for scatter, XY-line, histogram, five-number box,
  and normal probability plots from `L1`/`L2`.
- AP Statistics input validation for sample sizes, confidence levels, degrees
  of freedom, success counts, zero variance, distribution domains, and
  observed/expected list compatibility. Results warn when expected Chi-square
  counts are below 5 or proportion success/failure counts are below 10.
- Stable beta/gamma-based Student's t, Chi-square, and F CDFs, plus log-space
  binomial and Poisson calculations. Host regression tests cover reference
  quantiles, intervals, invalid inputs, and extreme normal tails.

Needs work:

- Remaining parity work is dedicated Chi-square contingency-table editing,
  paired-data inference, regression inference/diagnostic plots, and broader
  physical-device validation against released AP Statistics questions.

## Matrices

Implemented:

- Ten matrices, `A` through `J`, with storage up to 99x99 each.
- Dedicated matrix workspace with an `A`-through-`J` slot strip, live matrix
  preview, and matrix-specific action list. Left/right changes the selected
  matrix while up/down changes the selected action.
- Cell-by-cell grid editor with arrow navigation, signed decimal/scientific
  entry, row-major `Enter` advancement, and independent row/column scrolling.
- Dimension editor from 1x1 through 99x99 that preserves overlapping cells and
  clears newly exposed or out-of-range storage.
- Read-only scrolling result browser that opens automatically after arithmetic,
  inverse, REF/RREF, transpose, identity, augment, extraction, row-operation,
  and list-import results.
- Matrix addition, subtraction, and multiplication using the next matrix slot
  as the right-hand operand, with dimension validation before mutation.
- Scalar multiplication and integer powers from Calculator input or `Ans`,
  including zero and negative powers for nonsingular square matrices.
- Determinant, inverse, transpose, REF, RREF, dimension editing, identity, and
  zero-matrix creation.
- Augment selected matrix with the next matrix when row counts match.
- Extract a row (`row,2`), column (`col,3`), or inclusive submatrix
  (`sub,1,2,3,4`) using Calculator input.
- Convert selected list to a matrix column and matrix column 1 back to the
  selected list.
- Elementary row operations from Calc input:
  `swap,1,2`, `scale,1,2.5`, and `add,1,2,-3`.

Needs work:

- Large 99x99 matrices still need a prolonged physical-device stress pass for
  LCD navigation speed, PSRAM behavior, and the heaviest inverse/REF/RREF and
  multiplication cases.

## Solver

Implemented:

- Dedicated dashboard for equation, system, polynomial, numeric, and saved
  problem workflows.
- Exact equation and polynomial solving/factoring through the embedded Giac
  CAS, plus an advanced command path for parameterized, nonlinear, complex,
  interval-restricted, and other supported `solve(...)` forms.
- Numeric `E1=E2` solving near a guess or across configurable lower/upper
  bounds, with adjustable precision and multiple-real-root detection.
- Complex-mode `E1=E2` solving through numerical complex Newton iteration.
- Polynomial-root helper for degree 1 through 10 coefficient lists entered in
  Calc, including complex roots.
- Shared root-detail screen with one root per row, selected-root real/imag
  detail, and polynomial or equation residual checks.
- Linear-system solver from the selected augmented matrix, with unique,
  inconsistent, and dependent-system classification, free-variable formulas,
  and a scrollable detail view for up to 99 variables.
- Unique matrix solutions can be exported to the selected list. Solutions can
  be copied to Calculator, while equation sides can be sent to Graph.
- Five on-chip saved-problem slots retain equation and workflow data across
  power cycles.

Needs work:

- Giac supplies exact symbolic results, but the Solver does not yet generate a
  pedagogical step-by-step derivation or independently prove solution
  completeness and extraneous-root removal for every equation class.
- Symbolic nonlinear systems use CAS command syntax rather than a dedicated
  multi-equation form editor. Long CAS outputs are browsable text, not a
  structured solution-object viewer.
- Interval scans are numerical and sampling-based; tightly clustered roots or
  difficult discontinuities still require exact CAS solving or multiple
  targeted guesses.

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

- Dedicated finance dashboard with TVM summary tiles, payment/compounding
  settings, cash-flow profile chart, and finance-specific actions.
- Directly editable TVM worksheet for `N`, `I%`, `PV`, `PMT`, `FV`, `P/Y`, and
  `C/Y`, including signed decimal and scientific values.
- Solve `N`, `I%`, `PV`, `PMT`, and `FV`; interest-rate solving uses a bracketed
  numerical search and selects the root nearest the current rate estimate.
- Correct nominal-rate conversion when payment frequency and compounding
  frequency differ.
- Begin/end payment timing.
- Dedicated cash-flow ledger backed by `L1` through `L6`, with insertion,
  deletion, clearing, list switching, and a live present-value column.
- Dedicated NPV and IRR result screen with rate and cash-flow context.

Needs work:

- Multiple-IRR cash-flow sets can have several mathematically valid roots; the
  compact result screen reports one bracketed root and does not enumerate all
  possible IRRs.
- Final physical-device validation is still needed for unusually long cash-flow
  lists and extreme-rate TVM inputs.

## Conics

Implemented:

- Dedicated six-item Conics workspace for Circle, Parabola, Ellipse,
  Hyperbola, General Conic, and Conic Graphs.
- Direct coefficient/parameter worksheets for axis-aligned and rotated conics,
  including the general `Ax^2+Bxy+Cy^2+Dx+Ey+F=0` form.
- Classification with `B^2-4AC`, rotation angle, center, real/degenerate status,
  principal coefficients, intercepts, and standard/general-form summaries.
- Circle radius, diameter, area, circumference, parametric/polar forms;
  parabola vertex, focus, directrix, axis, latus rectum, and parametric form;
  ellipse/hyperbola vertices, foci, axes, eccentricity, latus rectum, area or
  asymptotes, and rotated parametric form.
- Point-membership residuals plus tangent and normal calculations.
- Construction from three circle points, a parabola vertex/focus, an
  ellipse/hyperbola center/vertex/focus, or five independent points for a
  general conic supplied through Calculator input.
- Four-slot conic overlay manager, automatic graph windows, and export into
  Graph, where pan, zoom, trace, tables, and numerical intersections remain
  available.
- Equation and selected-result-row handoff into Calculator.
- Host regression coverage for classification, rotation, construction,
  eccentricity, implicit evaluation, and graph-expression bounds.

Needs work:

- Arbitrary mixed symbolic constraints on unknown conic parameters still use
  the Solver/CAS app; the native construction worksheets cover the common
  point, center, vertex, and focus cases.
- Graph export plots implicit conics as one or two `y(x)` branches. Vertical
  line-only degeneracies cannot be represented in that graph path.
- Directrices, axes, foci, and latus recta are reported numerically but are not
  yet drawn as labeled graph primitives. Conic intersections use the Graph
  app's numerical intersection workflow rather than an exact resultant solver.
- Detailed algebraic completion-of-square steps and general symbolic
  focus-directrix/polar conversion remain CAS operations rather than a guided
  step screen.

## Inequality

Implemented:

- Dedicated four-item Inequalities workspace for one-variable solving, systems,
  graph regions, and sign charts.
- Free-form relations imported from Calculator input, including `<`, `<=`,
  `>`, `>=`, `!=`, chained notation such as `-2<x<2`, and up to four clauses
  joined by `and` or `or`.
- Exact symbolic requests through Giac plus a numerical interval fallback for
  polynomial, rational, absolute-value, exponential, logarithmic, and
  trigonometric expressions supported by the evaluator.
- Selectable interval, set-builder, and standard inequality notation;
  real/integer-domain modes; critical-point detection; number-line rendering;
  open/closed endpoints; and sign charts.
- Six persistent system slots with per-relation enable/disable/delete,
  selectable AND/OR combination, and separate graph colors.
- Explicit, vertical, and implicit two-variable inequalities such as
  `x^2+y^2<=25`, with solid inclusive boundaries, dashed strict boundaries,
  and shaded union/intersection regions.
- Boundary export to Graph, finite interval endpoints to the active List, and
  explicit-boundary intersection coordinates to `L1`/`L2`.
- Bounded linear-objective evaluation over detected feasible vertices, using
  the current Calculator expression as the objective.
- NVS persistence for relations, enabled state, domain mode, join mode, and the
  last one-variable problem.
- Host regression coverage for relation parsing, compounds, chained notation,
  interval endpoints, rational domain breaks, and implicit x/y evaluation.

Needs work:

- Numerical interval solving samples `[-100,100]`; the exact Giac result should
  be preferred for symbolic completeness, remote roots, and parameterized
  families.
- Linear optimization currently evaluates detected boundary vertices. It
  reports when no bounded feasible vertices are found, but it does not yet
  produce simplex tableaux, certify every unbounded objective, or enumerate
  integer-programming branches.
- Implicit-curve boundaries and intersections are sampled for display, so
  tangent or extremely close features can still require a tighter graph window.
- A guided algebra step viewer and full mixed-Boolean precedence editor are not
  implemented; one stored relation may use all `and` or all `or`, while the six
  system slots share a separate AND/OR mode.

## Games

Implemented:

- Game menu with Tetris, Doom, Snake, Breakout, and Mario.
- Persistent high scores through NVS.
- Held-key support for games that need it.
- Optional game audio when enabled in config and supported by PCB profile.

Needs work:

- The five games have been exercised on the current hardware. The optional
  audio path still needs final validation on the new audio-equipped PCB.

## Reality Check

OpenCalc OS is an advanced prototype with unusually broad calculator coverage,
but it is not yet a production-finished TI-84 replacement or desktop-class CAS.
The normal calculator, launcher, graph, table, variables, list, matrix, solver,
statistics, reference, conic, inequality, finance, power, storage, and game paths
are implemented. Host regressions provide useful protection for the math engines
and Tiny Python runtime.

The remaining release risks are concentrated rather than foundational:

- sustained Giac heap behavior under repeated timeout/cancellation cycles;
- repeated on-device Python run/input/cancel/error/exit cycles after the worker
  handoff rewrite;
- worst-case 99x99 matrix operations and large-editor responsiveness;
- specialized statistics workflows and physical AP Statistics validation;
- sampled non-Cartesian, implicit-conic, and inequality intersections; and
- final physical-LCD typography, icon, and dense-screen review.

Touch is intentionally not planned. Existing power behavior and the five games
have been hardware tested; optional audio remains tied to the new PCB bring-up.
