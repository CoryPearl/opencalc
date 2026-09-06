# OpenCalc OS Complete Guide

This is the living user and developer guide for OpenCalc OS. It documents the
behavior implemented in the current firmware, explains how the major workflows
fit together, and records known limits that still need validation. Update this
file whenever a key binding, screen, app, hardware profile, or build workflow
changes.

**Current guide revision:** September 6, 2026

**Project stage:** advanced working prototype

**Firmware target:** ESP32-S3, 16 MB flash, 8 MB PSRAM, 320x240 ILI9341 LCD

> **Important:** OpenCalc OS is still under development. A feature marked
> **Hardware validation pending** is implemented in source and may pass host
> tests, but has not completed a long physical-device test. Back up important
> scripts before flashing a new storage image.

<a id="contents"></a>
## Contents

- [1. Reading This Guide](#reading-this-guide)
- [2. Hardware Overview](#hardware-overview)
- [3. Keypad and Common Controls](#keypad-and-common-controls)
- [4. Starting, Sleeping, and Powering Off](#starting-sleeping-and-powering-off)
- [5. Home Screen and Navigation](#home-screen-and-navigation)
- [6. Mode Menu](#mode-menu)
- [7. Calculator](#calculator)
- [8. CAS and Symbolic Mathematics](#cas-and-symbolic-mathematics)
- [9. Numerical Calculus](#numerical-calculus)
- [10. Complex Numbers](#complex-numbers)
- [11. Unit-Aware Mathematics](#unit-aware-mathematics)
- [12. Variables, STO, VARS, and GET](#variables-sto-vars-and-get)
- [13. Graph](#graph)
- [14. Linked Symbolic Graphing](#linked-symbolic-graphing)
- [15. Table and Table Setup](#table-and-table-setup)
- [16. Statistics](#statistics)
- [17. Lists](#lists)
- [18. Matrices](#matrices)
- [19. Solver](#solver)
- [20. Finance](#finance)
- [21. Conics](#conics)
- [22. Inequalities](#inequalities)
- [23. Reference Center](#reference-center)
- [24. Python Programs](#python-programs)
- [25. Python Debugger and Sandbox](#python-debugger-and-sandbox)
- [26. Python Modules](#python-modules)
- [26A. Scientific Data Logging](#scientific-data-logging)
- [26B. Rear 30-Pin Header](#rear-30-pin-header)
- [27. Games](#games)
- [28. Settings](#settings)
- [29. Persistent Data](#persistent-data)
- [30. USB Storage and Serial](#usb-storage-and-serial)
- [31. Battery and Audio](#battery-and-audio)
- [32. Build, Flash, and Monitor](#build-flash-and-monitor)
- [33. Firmware Configuration](#firmware-configuration)
- [34. Testing and Validation](#testing-and-validation)
- [35. Troubleshooting](#troubleshooting)
- [36. Current Limitations](#current-limitations)
- [37. Developer Architecture](#developer-architecture)
- [38. Maintaining This Guide](#maintaining-this-guide)
- [Glossary](#glossary)

<a id="reading-this-guide"></a>
## 1. Reading This Guide

Key names are written as `Enter`, `Alpha`, `2nd`, and `Graph`. A combination
such as `Alpha` + `Zoom` means press `Alpha`, release it, and then press `Zoom`.
"Hold" means keep a key physically down while pressing another key.

The guide uses these status terms:

- **Implemented:** a complete firmware path exists.
- **Host tested:** an automated desktop regression covers the underlying logic.
- **Hardware tested:** the workflow has been exercised on an OpenCalc board.
- **Hardware validation pending:** code exists, but repeated on-device testing is
  still required.
- **Not implemented:** do not rely on this behavior yet.

Use `On (Home)` whenever you need to return directly to the launcher. Use
`DEL/Back` when you want to unwind only one screen and return to the app or
workflow you were using previously.

[Back to contents](#contents)

<a id="hardware-overview"></a>
## 2. Hardware Overview

The current OpenCalc OS target has:

- ESP32-S3-WROOM-1-N16R8 module.
- 16 MB flash with a 6 MB factory application partition.
- 8 MB PSRAM for framebuffers, game state, CAS stacks, matrices, and scripts.
- 8 MB FAT storage partition for scripts and game data.
- 320x240 ILI9341 color LCD over SPI.
- 10-row by 5-column, 50-key diode-isolated keypad.
- USB-C for power, flashing, CDC serial, and optional mass storage.
- 3.7 V single-cell Li-ion battery input and ADC battery monitoring.
- PWM-controlled display backlight.
- Optional PAM8302A game-audio path on the newer PCB profile.

The keypad is interrupt-assisted, but it is still a matrix. The firmware drives
one column low and reads rows with pull-ups. A pressed switch and its series
diode connect the selected low column to a row. The diodes prevent current from
creating false paths through other held keys, which permits simultaneous game
controls such as movement plus fire or sprint.

Touch input is intentionally out of scope. The LCD touch pins may remain
unwired, and no user workflow in this guide depends on touch.

[Back to contents](#contents)

<a id="keypad-and-common-controls"></a>
## 3. Keypad and Common Controls

### Global keys

| Key | Normal behavior |
| --- | --- |
| Arrow keys | Move a cursor, selection, trace point, graph window, or game object |
| `Enter` | Confirm, execute, open, save a field, or evaluate Calculator input |
| `DEL/Back` | Return to the previous screen or previous app |
| `CLEAR` | Delete the current character or token |
| `2nd` + `CLEAR` | Clear the complete active input field |
| `On (Home)` | Return directly to the launcher; exit an active game |
| `2nd` + `On` | Enter software-off sleep |
| `2nd` | Enable the blue secondary function for the next key |
| `Alpha` | Enable alphabetic entry; pressing it again turns Alpha lock off |
| `Mode` | Open number, print, angle, graph, and complex mode settings |
| `Math` | Open the six-tab Math/CAS catalog |
| `PRGM` | Open the Python program menu |

### Common second functions

| Combination | Action |
| --- | --- |
| `2nd` + `Y=` | Open statistical plots |
| `2nd` + `Window` | Open Table Setup (`TblSet`) |
| `2nd` + `Zoom` | Open Graph Format |
| `2nd` + `Graph` | Open the current graph-mode table |
| `2nd` + `PRGM` | Jump directly to the scripts browser |
| `2nd` + `STO` | Open `GET`, which inserts a variable's current value |
| `2nd` + `Up` | Increase display brightness |
| `2nd` + `Down` | Decrease display brightness |
| `2nd` + `sqrt` | Insert an editable nth-root structure |
| `2nd` + `^2` | Insert an editable exponent structure |
| `2nd` + fraction key | Insert inline division |

### Alpha shortcuts

- `Alpha` + `Zoom` opens the Reference Center.
- `Alpha` + `Graph` opens linked symbolic analysis for the selected graph.
- `Alpha` + `2nd` opens the game launcher.
- In the script editor, `Alpha` + `Y=` selects uppercase entry and `Alpha` +
  `Window` selects lowercase entry.
- `Alpha` + `sin`, `cos`, or `tan` inserts `csc(`, `sec(`, or `cot(`.
- Letter labels printed on the keypad enter `A` through `Z` while Alpha is
  active. The letter mode remains active when locked and turns off when Alpha
  is pressed again.

### Menu conventions

- `Up` and `Down` move through actions.
- `Left` and `Right` usually change a value, tab, matrix, list, or graph series.
- `Enter` opens or confirms the selected action.
- Number keys choose the matching numbered item when that screen supports
  direct selection.
- A scrollbar at the right edge means more items are available off-screen.

[Back to contents](#contents)

<a id="starting-sleeping-and-powering-off"></a>
## 4. Starting, Sleeping, and Powering Off

### Start and reset

Apply USB or battery power and press Reset if the board does not start. During a
normal boot the firmware initializes PSRAM, USB, LCD, keypad, battery ADC,
settings, and then the UI task. The display should show more than a white or
backlit-only panel after initialization.

### Software off

Press `2nd` + `On`. The current default uses light sleep: the LCD panel and
backlight turn off while RAM and tasks remain available. Press the physical
`On (Home)` key to wake quickly. A compile-time option can replace this with
deep sleep, which reduces power further but performs a full reboot on wake.

Software off does not disconnect the battery rail. Use the hardware power
switch on boards that include one when a true electrical cutoff is required.

### Auto sleep and power save

Auto sleep is controlled in Settings. Power Save lowers the allowed CPU
frequency and caps brightness; it does not dynamically disable one CPU core.
Settings persist in NVS.

[Back to contents](#contents)

<a id="home-screen-and-navigation"></a>
## 5. Home Screen and Navigation

The launcher contains 12 apps in a grid:

1. Calculator
2. Graph
3. Table
4. Python
5. Statistics
6. Lists
7. Matrices
8. Solver
9. Settings
10. Finance
11. Conics
12. Inequality

Move with the arrow keys and press `Enter`. Each app uses its own icon in the
launcher and title bar. `On (Home)` always returns here. `DEL/Back` instead
returns through the current workflow and eventually to the previously active
app, which is useful when moving a result between Calculator, Graph, Solver,
Lists, or Matrices.

The battery symbol in the title bar is based on the measured battery voltage.
An invalid or missing reading is shown as a crossed-out empty battery rather
than a fabricated percentage.

[Back to contents](#contents)

<a id="mode-menu"></a>
## 6. Mode Menu

Press `Mode`, use `Up`/`Down` to choose a row, and `Left`/`Right` or `Enter` to
change it.

| Row | Choices | Effect |
| --- | --- | --- |
| Display | Normal, Sci, Eng | Select normal, scientific, or engineering number formatting |
| Print | MathPrint, Classic | Select textbook-style or compact expression presentation |
| Angle | Degree, Radian | Set trigonometric input/output convention |
| Graph | Func, Par, Pol, Seq | Choose Cartesian, parametric, polar, or sequence graphing |
| Complex | REAL, `a+bi`, `re^ti` | Select real-only, rectangular complex, or polar complex results |

Real mode rejects a genuinely complex result. It does not silently discard the
imaginary component. Some mode values are persisted and displayed even where a
specialized operation still has a fixed internal format.

[Back to contents](#contents)

<a id="calculator"></a>
## 7. Calculator

### Entering and evaluating expressions

Type an expression and press `Enter`. The input and result are added to the
history, and the result becomes `Ans`. Multiplication may be entered explicitly
with `*`; expressions such as `2x` are also recognized in supported math paths.
Use parentheses when the intended grouping could be ambiguous.

Evaluation runs in the background, so drawing and keypad handling remain
responsive during CAS work. While `calculating...` is shown, press `Back` to
cancel the UI request. CAS work has a configured time limit and reports
`CAS timed out` rather than waiting forever.

Examples:

```text
2+3*4
sqrt(81)
(3+5)^2
sin(30)
5!
nCr(10,3)
factor(x^2-1)
solve(x^2+2*x+4=0,x)
```

Degree mode is the default, so `sin(30)` is `0.5` unless Angle has been changed
to Radian.

### Editing

- `Left` and `Right` move through the expression.
- `CLEAR` deletes at the cursor.
- `2nd` + `CLEAR` clears the input.
- `DEL/Back` leaves the calculator workflow rather than deleting text.
- The fraction key inserts a vertical fraction with independently editable
  numerator and denominator.
- `sqrt` inserts a square root; `2nd` + `sqrt` inserts an nth root.
- `^2` inserts a square; `2nd` + `^2` inserts a general exponent.

MathPrint renders powers, roots, and fractions in a textbook-style layout.
Classic mode favors compact one-line text.

### History

Press `Up` from the input line to select recent history. The selection covers
only the expression or result text. Use `Left`/`Right` to choose the original
expression or its answer, then press `Enter`. The selected text is appended to
the current input instead of replacing it.

### Math catalog

Press `Math`. Use `Left` and `Right` to change tabs:

- **MATH:** fraction/decimal conversion, solve, expand, factor, roots,
  derivatives, and integrals.
- **NUM:** absolute value, rounding, integer/fractional part, remainder, min,
  max, GCD, and LCM.
- **CPX:** conjugate, real part, imaginary part, magnitude, and angle.
- **PRB:** random values, random integers/normals, factorial, permutations, and
  combinations.
- **CAS:** simplify, roots, symbolic calculus, sums, products, Taylor series,
  numerator, and denominator.
- **ADV:** limits, rationalization, complex conversion, gamma/error functions,
  eigenvectors, rank, and transpose.

Select a command and press `Enter` to insert it into the expression editor.

### Error handling

An invalid expression reports an error instead of changing `Ans`. Check missing
parentheses, an unavailable real result, division by zero, incompatible units,
and misspelled function names. Expressions support 768 bytes and results support
1024 bytes. A result too wide for a history row opens in the Full result screen;
use `Up`/`Down` to scroll, `Enter` to copy it into Calculator input, or `Back`
to close it.

[Back to contents](#contents)

<a id="cas-and-symbolic-mathematics"></a>
## 8. CAS and Symbolic Mathematics

OpenCalc uses an embedded Giac/KhiCAS-derived engine as its primary CAS.
Eigenmath and native polynomial code provide fallbacks for selected operations.
The CAS runs serially on a dedicated 64 KB PSRAM-backed task, so two screens do
not evaluate Giac concurrently.

### Main symbolic commands

```text
simplify(expression)
expand(expression)
factor(expression)
solve(equation,variable)
roots(polynomial)
deriv(expression,variable)
int(expression,variable)
defint(expression,variable,lower,upper)
limit(expression,variable,value)
sum(expression,variable)
product(expression,variable)
taylor(expression,...)
numerator(expression)
denominator(expression)
rationalize(expression)
```

Examples:

```text
factor(x^4-1)
solve(x^2+2*x+4=0,x)
deriv(sin(x)*x^2,x)
int(3*x^2+2*x,x)
limit(sin(x)/x,x,0)
```

The quadratic example has roots `-1+sqrt(3)i` and `-1-sqrt(3)i`, which may be
displayed exactly or approximately depending on the command and display mode.

### CAS variables

Symbolic assignments can persist through the calculator variable workflow.
Variable substitution is case-insensitive for OpenCalc user variables. Avoid
using a variable name that conflicts with a built-in function.

### Scope of the embedded CAS

The firmware includes a real symbolic engine, but it is not the complete Xcas
desktop application. The desktop GUI, desktop plotting, some platform-specific
facilities, and a dedicated UI for every Giac command are not included. CAS
heap stability and unusually large outputs still require hardware soak tests.

[Back to contents](#contents)

<a id="numerical-calculus"></a>
## 9. Numerical Calculus

### Numerical derivative

Supported forms are:

```text
nDeriv(expression,value)
nDeriv(expression,variable,value)
```

Examples:

```text
nDeriv(x^2,3)
nDeriv(sin(t),t,0)
```

The first form assumes `x`. The second permits names such as `x`, `t`, or `n`.
The evaluator uses a symbolic derivative when a supported symbolic path exists,
then falls back to numerical approximation.

### Definite integral

Supported forms are:

```text
fnInt(expression,lower,upper)
fnInt(expression,variable,lower,upper)
```

Examples:

```text
fnInt(x^2,0,3)
fnInt(cos(t),t,0,pi)
```

Numerical results are approximate. Discontinuities, very narrow peaks, and
poorly scaled bounds may need a symbolic integral or a smaller interval.

### Symbolic helpers

`deriv(...)` and `int(...)` route general work to the CAS when enabled. The
native fallback recognizes common constants, powers, linear terms, trig,
logarithmic, root, and exponential patterns.

[Back to contents](#contents)

<a id="complex-numbers"></a>
## 10. Complex Numbers

Choose `a+bi` or `re^ti` under `Mode` > Complex. Enter the imaginary unit as
`i`. Supported operations include normal arithmetic and CPX functions:

```text
(2+3i)*(4-i)
conj(2+3i)
real(2+3i)
imag(2+3i)
abs(2+3i)
angle(2+3i)
```

Complex variables can be stored and reused. Complex-mode numerical solving is
available through a complex Newton path. Real mode reports an error for a
complex variable or result rather than converting it incorrectly.

[Back to contents](#contents)

<a id="unit-aware-mathematics"></a>
## 11. Unit-Aware Mathematics

OpenCalc can evaluate scalar quantities with common SI and US customary units.
Whitespace is optional for simple number-unit multiplication:

```text
5 m / 2 s
5m/2s
12ft+3in
9.8m/s^2*2s
```

Compatible dimensions can be added or subtracted, compound units can be
multiplied or divided, and integer powers are supported. Internally, values are
normalized to SI base units. Incompatible operations such as adding time to
distance return a dimensional error.

Current limits: affine temperature conversion, user-defined units, persistent
unit-bearing variables, and symbolic units through every CAS transformation are
not implemented.

[Back to contents](#contents)

<a id="variables-sto-vars-and-get"></a>
## 12. Variables, STO, VARS, and GET

### Store a value

Evaluate or enter a value, press `STO`, and choose `A` through `Z` or enter a
custom identifier such as `radius`. Stored variables persist in NVS.

Conceptually:

```text
5 -> A
12.5 -> radius
```

### Insert a reference with VARS

Press `VARS`, browse a category, and select an item. The variable reference is
inserted, so later changes to the stored variable affect the expression.

### Insert a current value with GET

Press `2nd` + `STO`, browse the same registry, and select an item. GET inserts
the current value itself rather than its name.

### Browser categories

- User Variables
- Lists
- Matrices
- Functions
- Strings
- Statistics
- Graph Variables
- System Variables

The browser shows name, type, and a value preview. User variables can be
renamed or deleted. Custom names may contain letters, digits, and underscores
and are treated case-insensitively. Lists and matrices remain managed by their
own apps even though they appear in the browser.

[Back to contents](#contents)

<a id="graph"></a>
## 13. Graph

### Choose a graphing mode

Press `Mode`, select Graph, and choose:

- **Func:** 10 Cartesian slots `Y1` through `Y10`.
- **Par:** six parametric pairs `X1T/Y1T` through `X6T/Y6T`.
- **Pol:** six polar definitions `r1` through `r6`.
- **Seq:** three sequence definitions `u1` through `u3`.

### Enter equations

Press `Y=` and select a slot. Enter its expression, then enable or disable that
slot with `Enter`. The small label such as `[line]` identifies the selected
rendering style; it is not part of the equation.

### Draw and navigate

Press `Graph`. Without Trace active, arrows pan the window. Zoom mode changes
the arrow behavior to zoom in or out. Use `Window` to edit x/y limits and tick
spacing. Parametric and polar modes also use a t range; sequence mode uses an n
range.

### Trace and series selection

Press `Trace` to activate trace and cycle enabled series. `Left` and `Right`
move along the active curve. The trace readout identifies the selected series
without drawing a full-width selection bar. Sequence trace snaps to integer n.

### Graph Calc

Open Graph Calc from the graph and choose:

1. Value
2. Zero
3. Minimum
4. Maximum
5. Intersect
6. Y-intercept
7. `dy/dx`
8. Integral from zero to the current point

Cartesian roots and intersections are refined numerically. Parametric and polar
derivatives report `dy/dx`. Parametric integration computes `integral y dx`,
polar integration computes enclosed area, and sequence calculus uses forward
differences and discrete sums.

### Graph Format

Press `2nd` + `Zoom`. Available controls include:

- Full graph or split graph/table view.
- Grid visibility.
- Active series selection.
- Line, thick, dotted, or point rendering style.
- Optional background image.

The optional background file is `/data/graph.bmp`. It must be an uncompressed
320x240, 24-bit or 32-bit BMP.

### Graph limitations

Non-Cartesian points of interest and intersections use plotted-sample matching.
Very close intersections and tangencies can be missed. Tighten the graph window
or use CAS/Solver when an exact answer matters.

[Back to contents](#contents)

<a id="linked-symbolic-graphing"></a>
## 14. Linked Symbolic Graphing

Linked analysis keeps Graph, Table, and the CAS tied to the same selected
series.

1. Enable one or more graph definitions.
2. Open Graph.
3. Press `Trace` until the desired series is active.
4. Press `Alpha` + `Graph`.

The symbolic screen requests the series derivative, integral, roots, and
asymptotic or end behavior from the CAS. Controls in this screen are:

- `Left`/`Right`: change active series.
- `Up`/`Down`: change the analysis point.
- `Y=`: toggle the tangent line for a Cartesian function.
- `Window`: toggle definite-integral shading from zero to the selected x-value.
- `Enter`: return to the graph.

The tangent and shading overlays continue using the original graph definition,
so changing the equation updates Graph, Table, and future symbolic analysis
together.

[Back to contents](#contents)

<a id="table-and-table-setup"></a>
## 15. Table and Table Setup

Press `2nd` + `Graph` to open the table for the current graphing mode. Press
`2nd` + `Window` to edit:

- Table start.
- Table step.
- Number of visible rows.
- Decimal precision.

Use `Up`/`Down` in the table to change the starting input. Use `Left`/`Right`
to page across function columns when more enabled series exist than fit on the
screen. Parametric tables show t with x/y pairs, polar tables show theta and r,
and sequence tables show n and sequence values. Split view embeds a compact
live table below the graph.

Rows are evaluated on demand. Expensive expressions may make scrolling uneven;
this is not necessarily a keypad problem.

[Back to contents](#contents)

<a id="statistics"></a>
## 16. Statistics

The Statistics home page contains:

1. Summary Statistics
2. Regression
3. Statistical Plots
4. Distributions
5. Confidence Intervals
6. Hypothesis Tests
7. One-Variable Stats

Most workflows read data from `L1` through `L6`. Prepare data in Lists first,
then select a list in the guided setup screen.

### Summary statistics

- One-variable summary includes count, sum, mean, median, sample standard
  deviation, and quartiles.
- Two-variable summary uses paired `L1`/`L2` values.
- Lists can be sorted ascending or descending or cleared from the catalog.
- The guided One-Variable Stats worksheet selects a data list, optional
  frequency list, quartile convention, and number format before Calculate.

### Regression

Available models include linear, quadratic, exponential, logarithmic, power,
cubic, quartic, and approximate median-median regression. Regression reads
paired x/y lists and shows coefficients in a dedicated result screen.

### Statistical plots

Press `2nd` + `Y=` or use Statistical Plots. Available plot types are scatter,
XY line, histogram, five-number box, and normal-probability plot. Use
`Left`/`Right` in the plot screen to change plot type.

### Distributions

- Normal PDF, CDF, and inverse normal.
- Student's t PDF and CDF.
- Chi-square PDF and CDF.
- F PDF and CDF.
- Binomial PDF and CDF.
- Poisson PDF and CDF.

Guided screens validate domains, degrees of freedom, probabilities, and tail
inputs. The CDF implementations use stable beta/gamma methods, and discrete
distributions use log-space calculations where needed.

### Confidence intervals

- One-sample Z and T intervals.
- One- and two-proportion Z intervals.
- Two-sample Z interval with known sigmas.
- Welch two-sample T interval.

### Hypothesis tests

- One-sample Z and T tests.
- Chi-square goodness-of-fit.
- One- and two-proportion Z tests.
- Welch two-sample T test.
- One-way ANOVA from three selectable lists.

Setup screens support alternative hypotheses (`<`, `>`, or not equal), sample
sizes, confidence levels, and list selection. The result screen warns when
expected Chi-square counts are below 5 or proportion success/failure counts are
below 10.

### Current statistics limits

Dedicated contingency-table editing, paired-data inference, regression
inference/diagnostic plots, and a full physical AP Statistics question pass are
still pending.

[Back to contents](#contents)

<a id="lists"></a>
## 17. Lists

Open Lists to work with `L1` through `L6`; each list stores up to 999 numeric
values.

### Workspace

Use `Left`/`Right` on the Lists home page to change the active list. The screen
shows a list-slot strip, preview, and actions:

1. Edit list
2. New/select list
3. Sum list
4. Min/max
5. Clear list

### Spreadsheet editor

- Arrows move between cells and list columns.
- Typing begins or replaces the current cell entry.
- `Enter` saves and advances.
- `DEL` removes a value.
- `2nd` + `DEL` clears the selected list.
- `2nd` + `Mode` quits the editor.
- The right-side scrollbar reports position in long lists.

Lists feed Statistics, Finance cash flows, matrix conversion, Solver exports,
and inequality endpoint/intersection exports. User-named lists are not yet
implemented.

[Back to contents](#contents)

<a id="matrices"></a>
## 18. Matrices

Open Matrices to work with matrices `A` through `J`, each with dimensions from
1x1 through 99x99. Large storage is PSRAM-aware.

Use `Left`/`Right` on the matrix home page to switch matrix slots and
`Up`/`Down` to choose an operation.

### Editing and browsing

- **Edit cells:** cell-by-cell grid with arrow navigation, signed decimal and
  scientific entry, row-major `Enter` advancement, and row/column scrolling.
- **Browse:** read-only scrolling view of the full matrix or last result.
- **Dimensions:** change rows and columns while preserving overlapping cells.
  Newly exposed and removed storage is cleared appropriately.
- **Import Calc:** parse Calculator text such as `1,2;3,4`.

### Arithmetic

- Add selected matrix to the next slot.
- Subtract the next slot from the selected matrix.
- Multiply by the next slot, with dimension checking.
- Scalar multiply using Calculator input or `Ans`.
- Raise to an integer power. Power zero creates identity; negative powers
  require a nonsingular square matrix.

### Transformations and analysis

- Transpose.
- Determinant.
- Inverse.
- Row echelon form (`ref`).
- Reduced row echelon form (`rref`).
- Identity matrix creation.
- Zero matrix creation while retaining dimensions.
- Augment with the next matrix when row counts match.

### Extraction and row operations

Enter an extraction command in Calculator, then choose Extract:

```text
row,2
col,3
sub,1,2,3,4
```

The submatrix arguments are inclusive row/column bounds. For elementary row
operations, enter one of:

```text
swap,1,2
scale,1,2.5
add,1,2,-3
```

`add,1,2,-3` adds -3 times source row 1 to destination row 2.

### List conversion

Convert the selected list to a one-column matrix, or export matrix column 1 to
the selected list.

### Matrix limits

The heaviest 99x99 multiplication, inverse, REF, and RREF operations still need
prolonged hardware stress testing. A destructive boot stress test exists behind
`OPENCALC_MATRIX_BOOT_STRESS_TEST`; do not enable it on data you need to retain.

[Back to contents](#contents)

<a id="solver"></a>
## 19. Solver

The Solver dashboard contains:

1. Equation Solver
2. System Solver
3. Polynomial Solver
4. Numeric Solver
5. Saved Problems

### Equation Solver

Set the left and right sides from Calculator input, then choose exact CAS solve,
approximate near the current guess, graph both sides, or save the problem. The
Advanced CAS action runs a complete `solve(...)` command from Calculator input
for parameterized, nonlinear, complex, or interval-restricted forms supported
by Giac.

### System Solver

For a linear system, edit the selected matrix as an augmented matrix with one
final constant column. Solve Matrix System uses RREF and reports:

- A unique solution.
- An inconsistent system.
- A dependent system with free-variable formulas.

The result browser supports up to 99 variables. A unique result can be exported
to the active list. For nonlinear or symbolic systems, enter Giac-compatible
system syntax in Calculator and choose Symbolic System.

### Polynomial Solver

Set a polynomial from Calculator input. Exact Roots and Factor use the CAS.
Numeric Roots accepts degree 1 through 10 coefficients ordered from highest
power to constant. The root-detail screen shows one root per row, separates
real and imaginary parts, and reports the polynomial residual.

### Numeric Solver

Set left side, right side, initial guess, lower bound, upper bound, and precision
(4 through 12 digits). Choose Find Nearby Root for bracket/Newton solving or
Find All in Bounds for a sampled multiple-real-root scan. Plot Both Sides sends
the equations to Graph for visual inspection.

### Saved Problems and handoff

Five problem slots persist in NVS. Solutions can be copied to Calculator,
equation sides can be sent to Graph, and unique linear-system solutions can be
sent to Lists.

### Solver limits

Exact results come from Giac, but the UI does not generate pedagogical algebra
steps or independently prove completeness and extraneous-root removal for every
equation class. Nonlinear symbolic systems use CAS command syntax rather than a
dedicated equation-grid editor. Numerical scans can miss tightly clustered
roots or difficult discontinuities.

[Back to contents](#contents)

<a id="finance"></a>
## 20. Finance

The Finance dashboard provides time-value-of-money and cash-flow analysis.

### TVM worksheet

Edit `N`, `I%`, `PV`, `PMT`, `FV`, payments per year (`P/Y`), and compounds per
year (`C/Y`). Signed decimal and scientific values are accepted. Select the
unknown field and solve it. `I%` uses a bracketed numerical search and chooses
the root nearest the current rate estimate. Nominal-rate conversion accounts
for differing payment and compounding frequencies.

Choose begin or end payment timing as appropriate:

- **End:** payments occur at the end of each period.
- **Begin:** payments occur at the beginning of each period.

### Cash flows, NPV, and IRR

The cash-flow ledger uses one of `L1` through `L6`. It supports insertion,
deletion, clearing, list switching, and a live present-value column. NPV uses the
selected discount rate. IRR uses a bracketed numerical solve and reports one
valid root.

Multiple-sign-change cash-flow sets can have multiple IRRs; the current screen
does not enumerate every possible root. Long ledgers and extreme rates still
need broader device validation.

[Back to contents](#contents)

<a id="conics"></a>
## 21. Conics

The Conics home page contains:

1. Circle
2. Parabola
3. Ellipse
4. Hyperbola
5. General Conic
6. Conic Graphs

### Worksheets and analysis

The dedicated worksheets accept common axis-aligned parameters and the general
second-degree form:

```text
A*x^2+B*x*y+C*y^2+D*x+E*y+F=0
```

Analysis includes classification using `B^2-4AC`, rotation angle, center,
principal coefficients, real/degenerate state, intercepts, and standard/general
form summaries. Specialized results include:

- Circle radius, diameter, area, circumference, parametric and polar forms.
- Parabola vertex, focus, directrix, axis, latus rectum, and parametric form.
- Ellipse/hyperbola vertices, foci, axes, eccentricity, latus rectum, area or
  asymptotes, and rotated parametric form.
- Point-membership residual, tangent, and normal calculations.

### Construction

Construct a circle through three points, a parabola from vertex/focus, an
ellipse or hyperbola from center/vertex/focus, or a general conic from five
independent points. Supply point data through Calculator when prompted.

### Graphs and handoff

Four overlay slots support multiple conics and automatic windows. Export to
Graph for pan, zoom, trace, tables, and numerical intersections. Equations and
selected result rows can be copied to Calculator.

### Conic limits

Graph export represents implicit conics as one or two `y(x)` branches, so a
vertical-line-only degeneracy cannot use that path. Reported directrices, axes,
foci, and latus recta are not yet drawn as labeled graph primitives. Exact
general-conic intersections and guided completion-of-square steps remain CAS
workflows.

[Back to contents](#contents)

<a id="inequalities"></a>
## 22. Inequalities

The Inequalities workspace contains:

1. One-variable
2. Systems
3. Graph regions
4. Sign chart

### One-variable solving

Enter a relation in Calculator and import it. Supported relation forms include
`<`, `<=`, `>`, `>=`, `!=`, chains such as `-2<x<2`, and up to four clauses
joined by `and` or `or`. The app requests an exact CAS solution and has a
numerical interval fallback for supported polynomial, rational, absolute-value,
exponential, logarithmic, and trigonometric expressions.

Choose interval, set-builder, or standard inequality notation and real or
integer domain. Number-line and sign-chart screens show critical points,
undefined points, and open/closed endpoints.

### Systems and regions

Store up to six persistent relations, each with its own color and enabled
state. Change the overall AND/OR combination and edit, disable, or delete each
relation. Graph Regions supports explicit, vertical, and implicit relations
such as `x^2+y^2<=25`. Inclusive boundaries are solid, strict boundaries are
dashed, and the satisfying region is shaded.

### Export and optimization

- Export boundary equations to Graph.
- Export finite interval endpoints to the active list.
- Export explicit-boundary intersection x/y coordinates to `L1`/`L2`.
- Put a linear objective in Calculator and run bounded optimization over
  detected feasible vertices.

Relations and mode choices persist in NVS.

### Inequality limits

The numerical fallback samples `[-100,100]`. The linear optimizer evaluates
detected vertices but does not provide simplex tableaux, complete unboundedness
certificates, or integer branch-and-bound. Implicit curves and intersections
are sampled, and mixed Boolean precedence is not a full free-form expression
tree editor.

[Back to contents](#contents)

<a id="reference-center"></a>
## 23. Reference Center

Press `Alpha` + `Zoom`. The hub contains Periodic Table, Math, Physics, and
Engineering.

### Periodic table

All 118 elements appear once in a spatial 9x18 layout. Move with the arrow keys
and press `Enter` for a larger element card with atomic number, atomic mass,
group, period, and classification. Use `Left`/`Right` in detail view to move to
the adjacent atomic number.

### Formula databases

Math, Physics, and Engineering are scrollable catalogs. Choose a formula to see
its expression, units, and explanation. Use `Left`/`Right` to change category
where available. The formula database is a reference aid; verify conventions
and units before applying a formula to safety-critical work.

[Back to contents](#contents)

<a id="python-programs"></a>
## 24. Python Programs

Scripts live in `/data/scripts/`. The flashed source directory is
`firmware/storage_image/scripts/`; the default image includes `fib.py`.

Press `PRGM` for:

1. Run
2. Debug
3. Edit
4. New
5. Delete

Run, Debug, Edit, and Delete open the script browser. New creates a blank
`programNN.py`. Press `2nd` + `PRGM` to jump directly to the browser.

### Editor

Use arrows to move the visible blue cursor. Alpha entry supports uppercase and
lowercase selection. `Trace` toggles a breakpoint on the current source line.
Save or leave using the actions shown on screen. Script names use the `.py`
extension.

### Running and input

The LCD becomes a script console. `print(...)` appears on screen. `input()`
opens a keypad input line:

- Type digits, operators, or Alpha letters.
- `CLEAR` deletes.
- `2nd` + `CLEAR` clears the input.
- `Enter` submits.
- `DEL/Back` cancels.

`input()` always returns a string, so convert numeric input:

```python
n = int(input("Enter a number: "))
print(n * n)
```

When the script finishes, press `Enter` or Back to return to the script list.

### Supported language surface

Tiny Python supports integers, floats, booleans, strings, lists, tuples,
dictionaries, `None`, basic f-strings, assignment, augmented assignment,
functions, recursion, `if`/`elif`/`else`, `while`, iterable `for`, `range`,
`break`, `continue`, and `pass`.

Common builtins include `len`, `int`, `float`, `str`, `bool`, `abs`, `min`,
`max`, `pow`, `sum`, `round`, `range`, `enumerate`, `sorted`, `list`, `tuple`,
`any`, `all`, `ord`, `chr`, `type`, and `input`.

Lists support indexing, assignment, stepped slicing, concatenation,
repetition, `append`, `extend`, `insert`, `pop`, `remove`, `clear`, `reverse`,
`copy`, `count`, and `index`. Dictionaries support key lookup/assignment,
iteration, `get`, `keys`, `values`, and `items`.

Tiny Python is not CPython. It does not currently include imports, exceptions,
classes, comprehensions, generators, decorators, lambdas, `with`, async syntax,
the CPython standard library, arbitrary-size integers, Unicode, bytecode, or a
garbage collector.

[Back to contents](#contents)

<a id="python-debugger-and-sandbox"></a>
## 25. Python Debugger and Sandbox

Choose Debug instead of Run. A debug session pauses before the first statement
and at lines where `Trace` set a breakpoint.

| Key | Debug behavior |
| --- | --- |
| `Enter` | Execute the next statement |
| `Trace` | Continue to the next breakpoint or completion |
| `Graph` | Cycle Console, Variables, Traceback, and Profile views |
| `Back` or `Clear` | Request a controlled stop |

The Variables view shows current interpreter values. Traceback shows the compact
call path retained after an error. Profile reports executed statements, user
function calls, maximum call depth, and active runtime.

Scripts run on a dedicated 32 KB internal-RAM worker reserved early in boot so FATFS access remains
safe during flash-cache operations. Parser/program buffers and dynamic
containers remain in PSRAM. The UI task remains the only owner of LCD drawing
and keypad dispatch. Output uses a protected buffer;
input uses task notifications. Configurable statement, recursion-depth, and
active-time limits stop ordinary infinite loops, runaway recursion, and long
runs. Debugger and input pauses do not consume the active-time budget.

This is cooperative containment, not process isolation. A defect in a native C
service can still affect the OS. The asynchronous path is host tested but needs
repeated hardware run/input/error/cancel/exit soak testing.

[Back to contents](#contents)

<a id="python-modules"></a>
## 26. Python Modules

Six modules are preloaded; do not write `import`.

### Graphics

```python
graphics.clear(0x101820)
graphics.pixel(20, 20, 0xffffff)
graphics.line(10, 60, 200, 60, 0x4aa3ff)
graphics.rect(20, 80, 40, 25, 0x33d17a)
graphics.text(20, 115, "hello", 0xffffff)
```

Commands enter a bounded queue and are replayed by the UI task. Scripts do not
take direct ownership of the LCD driver.

### Keys

```python
if keys.down(50):
    print("Enter is held")
```

Button numbers are 1 through 50 in row-major matrix order.

### Storage

```python
storage.write("result.txt", "42")
print(storage.exists("result.txt"))
print(storage.read("result.txt"))
storage.remove("result.txt")
```

Storage is restricted to simple file names in `/data/user/`. Absolute paths and
parent traversal are rejected.

### Audio

```python
print(audio.available())
print(audio.volume())
audio.tone(880, 80, 25)
```

Audio is a safe no-op or reports unavailable when the configured board has no
audio backend.

### Math

```python
print(math.eval("sqrt(2)"))
print(math.cas("factor(x^2-1)"))
```

Scalar helpers include `sin`, `cos`, `tan`, `sqrt`, `log`, `log10`, `exp`,
`floor`, and `ceil`.

[Back to contents](#contents)

<a id="scientific-data-logging"></a>
## 26A. Scientific Data Logging

The optional scientific-I/O PCB profile adds `D0-D11` through an MCP23017 and
`A0-A3` through an ADS1115. It also exposes the shared 3.3 V I2C bus for common
temperature, acceleration, light, pressure, distance, and environmental sensor
modules. It is enabled at build time with `OPENCALC_ENABLE_SCIENTIFIC_IO`.

Enable the corresponding new-PCB profile before building:

```c
#define OPENCALC_USE_REAL_PCB 1
#define OPENCALC_USE_NEW_AUDIO_PCB 1
#define OPENCALC_ENABLE_SCIENTIFIC_IO 1
```

The expansion connector provides `D0-D11`, `A0-A3`, SDA, SCL, two 3.3 V pins,
distributed grounds, and three hardware event signals. See the
[complete rear 30-pin header guide](#rear-30-pin-header) before connecting a
sensor. Scripts use channel numbers rather than ESP GPIO numbers. The `sensors`
module and `INPUT`, `OUTPUT`, and `INPUT_PULLUP` constants are preloaded; no
import statement is needed.

Digital channels use these modes:

```python
sensors.mode(0, 1)          # D0 output
sensors.digital_write(0, 1)
sensors.mode(1, 2)          # D1 input with weak pull-up
print(sensors.digital_read(1))
```

The standard `pinMode`, `digitalWrite`, and `digitalRead` builtins address the
same channel numbers. All twelve exposed channels are bidirectional. The
MCP23017 GPA7 and GPB7 are intentionally left unused by this board revision.
The exposed channels are 3.3 V logic pins and are not 5 V tolerant in this
board design.

Read one ADS1115 channel in volts or raw converter counts:

```python
sensors.rate(128)
voltage = sensors.analog_read(0)
counts = sensors.analog_raw(0)
print(voltage, counts)
```

The available converter rates are 8, 16, 32, 64, 128, 250, 475, and 860 SPS.
Requests between those values select the nearest supported rate at or below
the request. I2C and task overhead make the actual wall-clock capture rate
slightly lower. Inputs must remain between GND and 3V3 even when the PGA
full-scale range is configured above 3.3 V.

Supported differential pairs are A0-A1, A0-A3, A1-A3, and A2-A3:

```python
voltage_difference = sensors.analog_diff(0, 1)
print(voltage_difference)
```

Use a bounded trigger before recording:

```python
# Wait up to 10 seconds for A0 to reach 1.5 V.
if sensors.wait_analog(0, 1.5, 1, 10000):
    samples = sensors.capture(0, 200, 128, 1)
    print("saved", samples, "samples to L1")
```

`capture(channel, count, rate, list)` accepts up to 999 samples and list numbers
1 through 6. It replaces the selected list. `list_clear`, `list_append`, and
`list_count` support custom loops. The Lists and Statistics apps see the values
immediately, and the normal worksheet snapshot persists them after the script.

Use a digital trigger in the same way. This waits for D1 to become high:

```python
sensors.mode(1, INPUT_PULLUP)
if sensors.wait_digital(1, 1, 10000):
    sensors.capture(0, 200, 128, 1)
```

For a live graph, clear the script canvas once, append each sample to a list,
and draw consecutive line segments with `graphics.line`. The included
`storage_image/scripts/logger.py` demonstrates a 60-sample A0 plot and L1 log.

Register-based external I2C sensors can use:

```python
if sensors.i2c_present(0x76):
    chip_id = sensors.i2c_read8(0x76, 0xD0)
    sensors.i2c_write8(0x76, 0xF4, 0x27)
```

`i2c_read8(address, register)` reads one byte. `i2c_read16` reads two bytes and
treats the first as the most-significant byte. `i2c_write8` writes one register
byte. Consult the sensor's datasheet for register addresses, initialization,
conversion formulas, byte order, supply voltage, and required pull-ups.

Typical experiment workflow:

1. Connect sensor power to 3V3 and GND, then connect its signal to a digital,
   analog, or shared I2C pin as appropriate.
2. Check `sensors.available()` and read one value before starting a long run.
3. Select an ADS1115 rate or add `sensors.delay()` to pace the loop.
4. Optionally wait for an analog or digital trigger.
5. Save raw or converted values into `L1-L6` and draw a live graph if useful.
6. Exit the script and open Lists or Statistics to inspect and analyze the data.

Use `storage.write()` when a project needs a custom text log in addition to the
calculator lists. Keep control loops bounded so Back/Stop and the script watchdog
can terminate the program cleanly.

The generic I2C methods deliberately reject writes to `0x20` and `0x48`, which
belong to the OpenCalc MCP23017 and ADS1115. There is no dedicated UART on this
profile because the N16R8 module uses GPIO35-GPIO37 for octal memory and GPIO46
is an input-only strapping pin. Use an I2C-to-UART bridge when UART is required.

[Back to contents](#contents)

<a id="rear-30-pin-header"></a>
## 26B. Rear 30-Pin Header

The V5 rear connector is a 2x15, 2.54 mm female header for 3.3 V sensors and
small external circuits. Find the square pad, triangle, or printed `1` on the
PCB before counting pins. Viewed from the mating side, one physical column
contains odd pins `1, 3, ... 29` and the other contains even pins
`2, 4, ... 30`. Connector orientation can change when viewed through the PCB,
so use the pin-1 mark rather than assuming that pin 1 is always on the left.

| Pin | Label | Connected to | Direction and intended use |
| ---: | ----- | ------------ | -------------------------- |
| 1 | `3V3` | Board 3.3 V rail | Power output for low-power 3.3 V sensors |
| 2 | `GND` | Board ground | Power and signal return |
| 3 | `D0` | MCP23017 `GPA2` | 3.3 V digital input/output; Tiny Python channel `0` |
| 4 | `D1` | MCP23017 `GPA3` | 3.3 V digital input/output; Tiny Python channel `1` |
| 5 | `D2` | MCP23017 `GPA4` | 3.3 V digital input/output; Tiny Python channel `2` |
| 6 | `D3` | MCP23017 `GPA5` | 3.3 V digital input/output; Tiny Python channel `3` |
| 7 | `D4` | MCP23017 `GPA6` | 3.3 V digital input/output; Tiny Python channel `4` |
| 8 | `D5` | MCP23017 `GPB0` | 3.3 V digital input/output; Tiny Python channel `5` |
| 9 | `D6` | MCP23017 `GPB1` | 3.3 V digital input/output; Tiny Python channel `6` |
| 10 | `D7` | MCP23017 `GPB2` | 3.3 V digital input/output; Tiny Python channel `7` |
| 11 | `D8` | MCP23017 `GPB3` | 3.3 V digital input/output; Tiny Python channel `8` |
| 12 | `D9` | MCP23017 `GPB4` | 3.3 V digital input/output; Tiny Python channel `9` |
| 13 | `D10` | MCP23017 `GPB5` | 3.3 V digital input/output; Tiny Python channel `10` |
| 14 | `D11` | MCP23017 `GPB6` | 3.3 V digital input/output; Tiny Python channel `11` |
| 15 | `3V3` | Board 3.3 V rail | Duplicate sensor-power connection |
| 16 | `GND` | Board ground | Power and signal return |
| 17 | `A0` | ADS1115 `AIN0` | 16-bit analog input; Tiny Python channel `0` |
| 18 | `GND` | Board ground | Adjacent return for `A0` |
| 19 | `A1` | ADS1115 `AIN1` | 16-bit analog input; Tiny Python channel `1` |
| 20 | `GND` | Board ground | Adjacent return for `A1` |
| 21 | `A2` | ADS1115 `AIN2` | 16-bit analog input; Tiny Python channel `2` |
| 22 | `GND` | Board ground | Adjacent return for `A2` |
| 23 | `A3` | ADS1115 `AIN3` | 16-bit analog input; Tiny Python channel `3` |
| 24 | `GND` | Board ground | Adjacent return for `A3` |
| 25 | `SDA` | ESP GPIO13 / I2C data | Shared 3.3 V I2C bus for external sensors |
| 26 | `SCL` | ESP GPIO21 / I2C clock | Shared 400 kHz 3.3 V I2C bus |
| 27 | `ADS_RDY` | ADS1115 `ALERT/RDY` | Open-drain event output from the ADC; do not drive it |
| 28 | `GND` | Board ground | Return for event or I2C wiring |
| 29 | `MCP_INTA` | MCP23017 `INTA` | Expander open-drain/polarity-configurable event output; do not drive it |
| 30 | `MCP_INTB` | MCP23017 `INTB` | Expander open-drain/polarity-configurable event output; do not drive it |

`D0-D11` are logical expansion channels, not native ESP32 GPIO numbers. They
power up as inputs. A script must call `sensors.mode(channel, OUTPUT)` before
driving one. `INPUT_PULLUP` enables the MCP23017's weak internal pull-up; use an
external resistor when a circuit needs a precise resistance or faster edge.

`A0-A3` accept only voltages from GND through 3.3 V. They can be read
single-ended, or in supported differential pairs, but neither input may leave
that physical range. The header has no 5 V, VBAT, negative supply, analog
output, native UART, or raw ESP32 GPIO. Use a 3.3 V I2C bridge for interfaces
such as UART, SPI, or additional ADC channels.

The `SDA` and `SCL` pins are the same bus used by the onboard MCP23017 at
address `0x20` and ADS1115 at `0x48`. Do not add another strong set of pull-ups
when a sensor module already contains them, and never use those two reserved
addresses for an external device. Keep wiring short at 400 kHz; reduce the
firmware bus speed if a long cable or high bus capacitance makes communication
unreliable.

The three event pins are provided for probing and future expansion. Current
OpenCalc firmware does not route them back to ESP32 interrupt-capable pins, so
normal scripts should use bounded polling helpers such as `wait_analog()` and
`wait_digital()` instead. Do not connect an external push-pull output to an
event pin.

Common connections:

| Device | Header wiring |
| ------ | ------------- |
| Pushbutton | One side to `D0`, other side to `GND`; configure D0 as `INPUT_PULLUP` |
| 3.3 V digital module | `3V3`, `GND`, and its signal to any `D0-D11` |
| Analog voltage sensor | `3V3`, adjacent `GND`, and analog output to `A0-A3` |
| I2C sensor | `3V3`, `GND`, `SDA`, and `SCL`; confirm its address is not `0x20` or `0x48` |

Before attaching hardware, measure between `3V3` and `GND`, confirm about
3.3 V, and verify that the external circuit cannot back-power the calculator
when OpenCalc is off. Disconnect power before moving header wires. The rail is
for small sensors, not motors, heaters, relays, speakers, or other high-current
loads; power those separately and join grounds when required.

[Back to contents](#contents)

<a id="games"></a>
## 27. Games

Press `Alpha` + `2nd`. Select Tetris, Doom, Snake, Breakout, or Mario. High
scores are stored in NVS and survive software off and normal reflashing unless
NVS is erased or Factory Reset is used. Held and simultaneous keys are supported
for games that need them.

### Tetris

| Key | Action |
| --- | --- |
| Left/Right | Move piece; hold for repeated movement |
| Down | Soft drop |
| `Y=` | Hard drop |
| `Window` | Hold piece |
| Up | Rotate clockwise |
| `Back` | Pause/resume |
| `On (Home)` | Save high score and return to game menu |

### Doom

Doom requires `/data/doom1.wad`. The repository storage image contains the
shareware IWAD; commercial data must be supplied and licensed separately.

| Key | Action |
| --- | --- |
| Up/Down/Left/Right | Move |
| `Y=` | Fire |
| `Window` | Cycle weapons |
| `Zoom` | Use/open doors |
| `Trace` | Open/close automap |
| `1`-`7` | Select weapon slot |
| Hold `Mode` | Run/sprint |
| Hold `Stat` + Left/Right | Strafe |
| `Enter` | Menu/select |
| `Back` | Escape/back |
| `On (Home)` | Quit Doom and return to game menu |

### Snake

| Key | Action |
| --- | --- |
| Arrow keys | Change direction |
| `Back` | Pause/resume |
| `Enter` | Restart after game over |
| `On (Home)` | Save high score and return to game menu |

### Breakout

| Key | Action |
| --- | --- |
| Left/Right | Move paddle; held movement is supported |
| `Enter` | Launch ball or restart after game over |
| `Back` | Pause/resume |
| `On (Home)` | Save high score and return to game menu |

### Mario / NES

Mario requires `/data/mario.nes`. The emulator path is iNES loader -> mapper 0
cartridge -> 6502 CPU/NES bus/controller -> PPU 2C02 -> 256x240 framebuffer ->
ILI9341 display. Use a legal mapper-0 ROM compatible with this implementation.

| Key | Action |
| --- | --- |
| Arrow keys | D-pad movement |
| `Y=` | B/action/run |
| `Zoom` | A/jump |
| `Enter` | Start |
| `Back` | Select/back |
| `On (Home)` | Save score, stop emulator, and return to game menu |

### Frame rate and audio

The global pacing target defaults to 45 FPS. NES timing and Doom's internal
logic may report or feel different from the UI redraw rate. On the new audio
PCB, Tetris, Snake, and Breakout synthesize effects, Doom uses game sound events,
and Mario uses NES APU channels. The amplifier shuts down outside games.

[Back to contents](#contents)

<a id="settings"></a>
## 28. Settings

The Settings app contains six rows:

1. **Brightness:** change with `Left`/`Right` or `Enter`; normal step is 10%.
2. **Auto sleep:** toggle automatic sleep.
3. **Power save:** toggle reduced maximum CPU frequency and brightness cap.
4. **Theme:** toggle dark or blue-and-white light mode.
5. **Audio:** change volume with `Left`/`Right` or `Enter` in 5% steps.
6. **Reset to factory:** clear persistent OpenCalc settings and app data.

The footer shows brightness or audio directions only when the corresponding row
is selected. Audio volume can be configured even on an old/no-audio board; it
becomes active only when the compiled hardware/audio profile supports output.

Factory Reset is destructive. It resets settings and persistent app state,
including variables and scores managed through OpenCalc NVS.

[Back to contents](#contents)

<a id="persistent-data"></a>
## 29. Persistent Data

On-chip NVS stores settings and small structured state, including:

- Brightness, theme, auto sleep, power save, and audio volume.
- User variables.
- Solver saved-problem slots.
- Inequality relations and modes.
- Game high scores.
- Other app selections or values explicitly saved by their workflow.

The FAT storage partition stores larger files:

- `/data/.opencalc/worksheet.bin` for calculator history, graph equations and
  windows, lists, populated matrix cells, finance values, and conic worksheets.
- `/data/scripts/*.py`
- `/data/user/*` script-created files
- `/data/doom1.wad`
- `/data/mario.nes`
- `/data/graph.bmp`

Worksheet changes are saved after a short idle delay and immediately before
software-off sleep or USB storage export. The snapshot has a CRC and an atomic
backup so an interrupted write cannot silently load partial data. Factory Reset
clears both NVS and this worksheet snapshot. Flashing with `idf.py flash` may
rewrite the storage image depending on `OPENCALC_FLASH_STORAGE_IMAGE`; back up
scripts and worksheets first.

[Back to contents](#contents)

<a id="usb-storage-and-serial"></a>
## 30. USB Storage and Serial

One USB-C connector can expose CDC serial and mass storage. The firmware must
choose whether the host or app owns the FAT volume; both must never write it at
the same time.

With `OPENCALC_EXPORT_USB_STORAGE_TO_HOST=1`, the laptop can mount the drive.
Eject it before an app requests ownership. With the option set to `0`, `/data`
stays mounted for reliable scripts and games and the laptop does not receive the
mass-storage volume.

On macOS, find the CDC port with:

```zsh
ls /dev/cu.*
```

It commonly appears as `/dev/cu.usbmodemopencalc1`. On Linux, look for
`/dev/ttyACM*`. In Windows PowerShell, use:

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
```

In Windows Command Prompt, use:

```bat
mode
```

A CDC disconnect during reset, flash, or ownership transitions is normal. The
monitor should wait for reconnection. A disconnect at the exact moment of a
panic usually means the device reset before the final panic text was delivered.

[Back to contents](#contents)

<a id="battery-and-audio"></a>
## 31. Battery and Audio

### Battery

GPIO7 reads the battery through the configured 1 Mohm / 1 Mohm divider and a
100 nF ADC-node capacitor. Firmware calibration scales the divider voltage back
to cell voltage and maps it through a Li-ion discharge curve. Default valid
bounds are 2.5 V through 4.6 V, with 3.3 V empty and 4.2 V full references.

The percentage is an estimate, not a fuel-gauge measurement. Load, temperature,
cell age, USB charging, and divider tolerance affect it.

### Audio hardware profiles

- **Old PCB:** LCD MISO uses GPIO13 and charger status uses GPIO41; game audio
  must remain disabled.
- **New audio PCB without scientific I/O:** LCD is write-only, GPIO13 drives
  PAM8302A shutdown, and GPIO41 emits filtered PDM audio.
- **Scientific-I/O PCB:** GPIO13/GPIO21 are SDA/SCL, MCP23017 GPA0 controls
  PAM8302A shutdown, MCP23017 GPA1 controls the power LED, and GPIO41 remains
  the filtered PDM audio output.

The ESP32-S3 has no analog DAC on GPIO41. Connecting raw PDM directly to the amp
input is not equivalent to the intended filtered analog signal. In the
scientific-I/O profile, the MCP23017-controlled amp `SD` net needs a 100 kOhm
pull-down so the amp remains off during reset and sleep.

[Back to contents](#contents)

<a id="build-flash-and-monitor"></a>
## 32. Build, Flash, and Monitor

Install and source ESP-IDF for ESP32-S3, then run from `firmware/`:

```sh
idf.py set-target esp32s3
idf.py build
idf.py flash
idf.py monitor
```

Or specify the port:

```sh
idf.py -p /dev/cu.usbmodemopencalc1 flash monitor
```

Use `idf.py flash`, not `idf.py app-flash`, when the generated storage image
must also be written. If the board remains in download mode, press Reset after
flashing. Exit the monitor with `Ctrl+]`.

If ESP-IDF reports that the active Python environment differs from the one used
to configure the build, source the intended IDF environment and run:

```sh
idf.py fullclean
idf.py build
```

This discards build output, not source files. The current project uses a 6 MB
application partition and an 8 MB storage partition on 16 MB flash.

[Back to contents](#contents)

<a id="firmware-configuration"></a>
## 33. Firmware Configuration

Feature toggles are in `firmware/main/config.h`. Rebuild after changing them.

### Main feature toggles

| Setting | Purpose |
| --- | --- |
| `OPENCALC_ENABLE_GIAC_CAS` | Build the Giac/KhiCAS symbolic backend |
| `OPENCALC_ENABLE_AUTOMATED_TESTS` | Master gate for all boot-time automated tests; default off |
| `OPENCALC_GIAC_BOOT_SELF_TEST` | Select the serial-logged CAS smoke test when the master gate is on |
| `OPENCALC_MATRIX_BOOT_STRESS_TEST` | Select the destructive 99x99 matrix stress test when the master gate is on |
| `OPENCALC_DEVICE_SELF_TEST` | Select the CAS, script, persistence, sleep, keypad, game, and PCB-profile test suite when the master gate is on |
| `OPENCALC_ENABLE_DOOM` | Include the Doom launcher/runtime |
| `OPENCALC_ENABLE_WIFI` | Enable Wi-Fi support; default off |
| `OPENCALC_ENABLE_BLUETOOTH` | Enable Bluetooth support; default off |
| `OPENCALC_ENABLE_GAME_AUDIO` | Master game-sound switch |
| `OPENCALC_USE_NEW_AUDIO_PCB` | Select write-only LCD plus PAM8302A pin profile |
| `OPENCALC_USE_AO3401A_BACKLIGHT` | Select new active-low or old active-high backlight circuit |
| `OPENCALC_USE_REAL_PCB` | Enable real keypad, battery, backlight, and wake hardware |
| `OPENCALC_FLASH_STORAGE_IMAGE` | Include `storage_image/` in normal flash workflow |

### USB and debugging

| Setting | Purpose |
| --- | --- |
| `OPENCALC_ENABLE_USB_CDC_CONSOLE` | Enable serial on the shared USB-C port |
| `OPENCALC_EXPORT_USB_STORAGE_TO_HOST` | Export FAT storage to the laptop instead of keeping app ownership |
| `OPENCALC_ENABLE_SERIAL_BUTTON_INPUT` | Accept button numbers 1-50 over serial |
| `OPENCALC_DEBUG_LOG_KEYPAD_PRESSES` | Print detected physical key presses |
| `OPENCALC_DEBUG_LOG_RAW_KEYPAD_LEVELS` | Print raw row/column levels for bring-up |
| `OPENCALC_DEBUG_LOG_FPS` | Print UI/game tick FPS once per second |
| `OPENCALC_DEBUG_TETRIS_HEALTH` | Print periodic Tetris heap/stack diagnostics |

### Performance and script limits

- `OPENCALC_TARGET_FPS`: global continuous redraw target, default 45.
- `OPENCALC_CPU_MAX_MHZ`: normal maximum CPU frequency, default 240 MHz.
- `OPENCALC_LCD_SPI_CLOCK_MHZ`: LCD SPI clock, default 60 MHz.
- `OPENCALC_POWER_SAVE_CPU_MAX_MHZ`: power-save CPU cap, default 160 MHz.
- `OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT`: default 35%.
- `OPENCALC_SCRIPT_STATEMENT_LIMIT`: maximum executed statements.
- `OPENCALC_SCRIPT_CALL_DEPTH_LIMIT`: maximum script recursion depth.
- `OPENCALC_SCRIPT_TIMEOUT_MS`: active execution time limit.

Do not reduce the Giac, math-worker, or script task stack below the compile-time
minimum checks. Those stacks are intentionally PSRAM-backed to preserve internal
DRAM.

[Back to contents](#contents)

<a id="testing-and-validation"></a>
## 34. Testing and Validation

### User acceptance checklist

After a major flash, test in this order:

1. Boot to launcher; verify LCD colors, title bar, and battery icon.
2. Press every keypad key once; verify Alpha, `2nd`, Enter, Back, and Home.
3. Evaluate arithmetic, fraction, exponent, trig, complex, unit, and CAS inputs.
4. Store and retrieve a variable, reboot, and confirm persistence.
5. Graph two functions, cycle Trace, open Graph Calc, and open linked analysis.
6. Configure and scroll a table in each graph mode.
7. Edit lists; run summary statistics, one regression, one distribution, one
   interval, and one hypothesis test.
8. Edit two matrices and test add, multiply, determinant, inverse, and RREF.
9. Test exact, polynomial, numeric, and linear-system solver workflows.
10. Run Finance, Conics, and Inequality handoffs to Calculator/Graph/Lists.
11. Run `fib.py` repeatedly with valid input, canceled input, and a syntax error.
12. Set a breakpoint and verify step, continue, variables, traceback, and profile.
13. Run every game, hold simultaneous controls, exit, and verify high scores.
14. Enter software off, wake with On, and confirm the previous state returns.
15. Reboot and confirm settings, variables, relations, saved problems, and scores.

### Host regressions

Tests in `firmware/tests/` cover:

- `tiny_python_regression.c`
- `cas_regression.c`
- `eigenmath_regression.c`
- `graph_modes_regression.c`
- `stats_regression.c`
- `conics_regression.c`
- `inequality_regression.c`
- `reference_regression.c`
- `variables_regression.c`
- `units_regression.c`

See `firmware/FIRMWARE_README.md` for exact compiler commands. Host tests do not
replace physical LCD, keypad, USB ownership, PSRAM, sleep/wake, or audio tests.

[Back to contents](#contents)

<a id="troubleshooting"></a>
## 35. Troubleshooting

### White or backlit-only LCD

Confirm serial boot reaches `Display ready`. Measure 3.3 V at LCD VCC, check
ground continuity, and verify configured SCLK, MOSI, CS, DC, Reset, and backlight
pins match the PCB profile. A high backlight does not prove SPI initialization
succeeded. Confirm the active-high TPS22918 versus active-low AO3401A config.

### No key presses

Temporarily enable keypad press and raw-level logs. Check that idle rows are
pulled high, the scanned column goes low, and a pressed key pulls its row low
through the correctly oriented diode. Do not infer GPIO numbers from package
pin numbers.

### USB monitor reconnects with no output

Confirm the new `/dev/cu.usbmodem...`, `/dev/ttyACM...`, or COM port after reset.
Only one program may own the serial port. Reset once after opening monitor. If
mass storage owns the only USB function in a mismatched build, enable CDC and
reflash.

### Storage busy or script browser unavailable

Eject the OpenCalc volume from the host so firmware can mount `/data`. For a
runtime-focused build, set `OPENCALC_EXPORT_USB_STORAGE_TO_HOST` to `0`.

### CAS resets the board

Enable `OPENCALC_GIAC_BOOT_SELF_TEST`, monitor internal/PSRAM heap logs, and test
a small expression first. Confirm PSRAM passes boot testing and that Giac uses
its 64 KB PSRAM stack. Capture the complete panic/backtrace before changing heap
settings.

### Script crashes or hangs

Try the smallest script, then input, recursion, and module calls separately.
Use Debug and inspect Traceback/Profile. Confirm the script worker starts with a
32 KB internal stack and storage is app-mounted. A forced statement/depth/time limit
should return an interpreter error, not reset the board.

### Build exceeds DRAM or app partition

Read the final linker region, not the generic size hint. `.dram0.bss` overflow
means move eligible large static buffers to PSRAM or reduce internal allocations.
Application partition overflow means reduce code/data or deliberately revise the
partition map. Do not confuse these two failures.

### Device repeatedly wakes after software off

Check On-key idle polarity, diode orientation, wake source configuration, and
whether the key is still held while entering sleep. Light sleep must clear the
wake condition before suspending.

[Back to contents](#contents)

<a id="current-limitations"></a>
## 36. Current Limitations

OpenCalc OS is broad but not production complete. The main remaining risks are:

- Sustained Giac heap behavior and broad command validation under repeated
  timeout/cancellation cycles.
- Repeated on-device Python run/input/cancel/error/exit soak testing after the
  asynchronous worker rewrite.
- Worst-case 99x99 matrix operation speed and memory behavior.
- Specialized statistics workflows and physical AP Statistics validation.
- Sampled non-Cartesian, implicit conic, and inequality intersections.
- Final physical-LCD typography, icon, and dense-screen review.
- New-PCB PAM8302A audio validation.

OpenCalc does not currently claim complete TI-84 parity, desktop Xcas parity,
full CPython compatibility, or formal proof that every numerical solver found
all roots. Touch support is not planned.

[Back to contents](#contents)

<a id="developer-architecture"></a>
## 37. Developer Architecture

### Core layout

- Core 1 runs UI state, LCD drawing, keypad dispatch, and game drawing.
- Core 0 runs setup/USB work, serial simulation, and asynchronous graph/math
  jobs.
- Calculator, Numeric Solver, symbolic Solver, and Graph Calc use a 24 KB
  PSRAM-backed math-worker stack. Calculator jobs carry cancellation IDs.
- Giac uses a serialized 64 KB PSRAM-backed task.
- Tiny Python uses a separate synchronized 32 KB internal-stack task; its large
  parser, program, and container allocations remain in PSRAM.

`opencalc_calc.c` owns calculator evaluation and history,
`opencalc_ui_work.c` owns PSRAM-backed job/result lifetimes, and
`opencalc_ui_canvas.c` owns the shared framebuffer and raster primitives.
Domain behavior remains separated into statistics, conics, inequalities,
reference, math, persistence, and game modules; `opencalc_ui.c` coordinates
page layouts and keypad routing.

Only the UI task should present normal OpenCalc frames. Script graphics are
queued; worker results are copied back into UI-owned state. Storage ownership
must be transferred explicitly between USB MSC and the app.

### Important paths

```text
firmware/main/config.h
firmware/main/main.c
firmware/main/components/board_init.c
firmware/main/components/opencalc_ui.c
firmware/main/components/opencalc_calc.c
firmware/main/components/opencalc_ui_work.c
firmware/main/components/opencalc_ui_canvas.c
firmware/main/components/opencalc_giac.cpp
firmware/main/components/tiny-python.c
firmware/storage_image/
firmware/tests/
hardware/pcb/PCB_README.md
```

### Licensing

OpenCalc-authored firmware is GPL-3.0-or-later. Bundled Giac/KhiCAS, Doom,
Anemoia/NES, Eigenmath, ESP-IDF, TinyUSB, and display code retain their own
copyright and license terms. See `THIRD_PARTY_NOTICES.md`. Doom WADs and NES
ROMs are separate game data and are not relicensed by OpenCalc.

[Back to contents](#contents)

<a id="maintaining-this-guide"></a>
## 38. Maintaining This Guide

When behavior changes:

1. Update the relevant workflow and controls in this file.
2. Update the status or limitation beside it; do not call code hardware-tested
   until it has actually run on a board.
3. Update `firmware/APP_STATUS.md` for implementation boundaries.
4. Update `firmware/FIRMWARE_README.md` for build, pin, storage, or config changes.
5. Update the root `README.md` only for significant public-facing features.
6. Update `hardware/pcb/PCB_README.md` for electrical or PCB changes.
7. Keep explicit `<a id="...">` anchors stable so external links do not break.
8. Update the revision date near the top.

New sections should be added to the Contents list. Prefer tested examples and
exact key names over aspirational feature lists.

[Back to contents](#contents)

<a id="glossary"></a>
## Glossary

- **Ans:** most recent successful Calculator result.
- **CAS:** computer algebra system for exact and symbolic mathematics.
- **CDC:** USB Communications Device Class, used for serial monitoring.
- **CDF/PDF:** cumulative distribution function / probability density or mass
  function.
- **DRAM:** internal data RAM; scarce compared with PSRAM.
- **Giac/KhiCAS:** primary embedded symbolic math backend.
- **MathPrint:** textbook-style fraction, root, and exponent layout.
- **MSC:** USB Mass Storage Class.
- **NVS:** ESP-IDF nonvolatile key/value storage.
- **PSRAM:** external pseudo-static RAM used for large buffers and task stacks.
- **RREF/REF:** reduced row echelon form / row echelon form.
- **Software off:** LCD-off sleep state; not a battery-rail disconnect.
- **TblSet:** table start, step, row-count, and precision settings.
- **Trace:** graph cursor mode and enabled-series cycling key.

[Back to contents](#contents)
