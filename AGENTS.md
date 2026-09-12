# Repository Guidelines

## Project Structure & Module Organization

- `firmware/` contains the ESP-IDF project for the ESP32-S3. Application code is in `firmware/main/components/`; `opencalc_ui*.c` owns UI coordination, while focused modules cover math, CAS, graphing, persistence, games, hardware, and Tiny Python.
- `firmware/tests/` contains portable C regression tests. `firmware/storage_image/` is packaged into the device's FAT partition; scripts and required runtime assets belong there.
- `hardware/pcb/opencalc_pcb_V5/` is the current KiCad design and enclosure source. Older revisions are retained under `hardware/pcb/V1` through `V4`.
- `docs/` contains the blog, product website, planning material, and rendered media. Root Markdown files provide user and contributor documentation.

## Build, Test, and Development Commands

Run firmware commands from `firmware/` after activating ESP-IDF 6.x:

```sh
idf.py build
idf.py -p PORT flash
idf.py -p PORT monitor
tests/run_host_tests.sh
```

`idf.py build` creates the app and storage images. Use `flash`, not `app-flash`, when storage assets changed. The host-test script compiles regression groups with `${CC:-cc}`; CI runs it with Clang. Use `idf.py fullclean` only when configuration or Python-environment state requires it.

## Coding Style & Naming Conventions

Follow surrounding C/C++ style: four-space indentation, braces on the same line for control statements, `snake_case` functions and variables, `UPPER_SNAKE_CASE` macros, and `opencalc_` prefixes for public project APIs. Keep hardware/build toggles in `firmware/main/config.h`. Prefer bounded `snprintf`/size-aware APIs and place large persistent buffers in PSRAM when appropriate. Avoid unrelated formatting changes and preserve third-party source style and license headers.

## Testing Guidelines

Add focused tests named `*_regression.c` and register new groups in `firmware/tests/run_host_tests.sh`. Run the complete host suite before submitting firmware logic changes, then run `idf.py build` to catch ESP-IDF, partition-size, and embedded-memory issues. Hardware-facing changes should include the relevant serial logs and physical validation notes.

## Commit & Pull Request Guidelines

Recent history uses short, direct summaries such as `Fix website and blog navigation`; no Conventional Commits scheme is enforced. Keep each commit coherent and use an imperative subject. Pull requests should explain behavior, affected hardware/config profiles, tests run, and remaining limitations. Include screenshots or GIFs for UI/site changes and schematic or PCB screenshots for hardware changes. Update the relevant README and `THIRD_PARTY_NOTICES.md` when behavior, setup, assets, or dependencies change.

## Security & Repository Hygiene

Do not commit credentials, machine-specific paths, build output, serial logs containing personal data, or unlicensed ROM/WAD files. Keep generated artifacts out unless they are intentional documentation or required manufacturing deliverables.
