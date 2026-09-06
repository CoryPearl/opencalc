# Giac/KhiCAS in OpenCalc

This is a vendored Giac 1.4.9/KhiCAS-derived embedded snapshot imported from
NeoCalculator commit `823ac86c4fafc32c55ce0f17584992ddf953b43f`. Its closest
documented KhiCAS baseline is `KhiCAS/ti-ce-giac` commit
`8d24f392f3edcb4fbf44b11325e92ca37edee470`.

OpenCalc uses the Giac engine only. OpenCalc OS continues to provide the
calculator UI, expression renderer, graphing interface, keypad handling, and
application lifecycle.

OpenCalc-specific integration consists of:

- ESP-IDF component build files in this directory and `../libtommath/`.
- `firmware/main/components/opencalc_giac.cpp`, the serialized C API bridge.
- A 64 KB PSRAM-backed Giac task stack.
- PSRAM-first C++ allocation while the Giac backend is enabled.
- An ESP-IDF extension to the PSRAM allocation condition in `src/kgen.cc`.

The bridge is the primary symbolic backend for Calculator and Solver requests.
Native numerical routines and the embedded Eigenmath port remain bounded
fallbacks. This is the Giac engine, not the desktop Xcas application: OpenCalc
does not include Xcas's GUI, plotting layer, or a dedicated UI for every Giac
command. Long-running command coverage, heap stability, and result rendering
remain hardware-validation items.

The imported provenance audit is preserved in `NUMOS_CHANGES.md`. Giac and
the port modifications are GPL-3.0-or-later. See `COPYING.GPL3` and the
repository's `THIRD_PARTY_NOTICES.md`.
