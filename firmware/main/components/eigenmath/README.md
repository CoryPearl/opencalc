# Eigenmath Embedded Port

This directory vendors Eigenmath from commit
`e876054e3d767b535632220c8637440f5818d938` of
<https://github.com/georgeweigt/eigenmath>.

OpenCalc's local changes make the standalone C engine suitable for ESP32-S3:

- disable the command-line entry point;
- capture output through a callback;
- place large persistent arrays and allocation blocks in PSRAM;
- reduce embedded stack/block defaults;
- report allocation failures through Eigenmath's evaluator error path; and
- namespace symbols that conflict with Doom.

OpenCalc serializes access to this engine, captures errors without terminating
the firmware, and uses it after the native polynomial helpers but before the
larger Giac fallback when an expression fits Eigenmath's supported surface.
Host coverage lives in `tests/eigenmath_regression.c` and
`tests/cas_regression.c`.

Giac is now OpenCalc's primary general-purpose symbolic backend. Eigenmath is
kept deliberately as a smaller fallback and regression oracle; it is not
expected to provide full CAS or desktop Xcas parity by itself.

The upstream BSD-2-Clause license is preserved in `LICENSE` and at the top of
`eigenmath.c`.
