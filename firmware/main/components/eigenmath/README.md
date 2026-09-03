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

The upstream BSD-2-Clause license is preserved in `LICENSE` and at the top of
`eigenmath.c`.
