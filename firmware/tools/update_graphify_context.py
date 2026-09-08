#!/usr/bin/env python3
"""Add deterministic OpenCalc project context to Graphify's structural graph.

Graphify's AST pass is intentionally strong on symbols and weak on project-level
intent. This script adds a small, hand-maintained knowledge layer without using
an LLM. It is idempotent and can be rerun after ``graphify update .``.
"""

from __future__ import annotations

import argparse
import json
import os
import tempfile
from pathlib import Path


PREFIX = "opencalc_context_"


CONTEXT = [
    {
        "key": "project",
        "label": "OpenCalc OS: open-source ESP32-S3 graphing calculator firmware, hardware, documentation, scripting, symbolic math, science I/O, and games",
        "source_file": "../README.md",
        "source_location": "L1",
        "targets": [("main/main.c", "app_main()")],
    },
    {
        "key": "architecture",
        "label": "Dual-core architecture: UI and rendering run on core 1 while queued math and system work run on core 0 with PSRAM-backed worker stacks",
        "source_file": "../guied.MD",
        "source_location": "L1820",
        "targets": [
            ("main/main.c", "app_main()"),
            ("main/components/opencalc_ui_work.c", "opencalc_ui_work_start()"),
            ("main/components/opencalc_ui.c", "opencalc_ui_tick()"),
        ],
    },
    {
        "key": "configuration",
        "label": "Firmware configuration: compile-time feature gates select PCB, display, audio, scientific I/O, USB, tests, target FPS, CPU limits, stacks, and memory reserves",
        "source_file": "main/config.h",
        "source_location": "L8",
        "targets": [("main/config.h", "main/config.h")],
    },
    {
        "key": "ui_apps",
        "label": "OpenCalc application suite: Calculator, Graph, Table, Statistics, Lists, Matrices, Solver, Finance, Conics, Inequalities, Programs, Settings, and Reference Center",
        "source_file": "APP_STATUS.md",
        "source_location": "L28",
        "targets": [
            ("main/components/opencalc_ui.c", "ui_draw_home()"),
            ("main/components/opencalc_ui_navigation.c", "opencalc_navigation_process_ui()"),
        ],
    },
    {
        "key": "calculator_cas",
        "label": "Calculator and CAS: textbook input, exact and numerical math, complex numbers, calculus, units, variables, Giac primary CAS, Eigenmath and native fallbacks",
        "source_file": "APP_STATUS.md",
        "source_location": "L63",
        "targets": [
            ("main/components/opencalc_calc.c", "opencalc_calc_evaluate()"),
            ("main/components/opencalc_giac.cpp", "opencalc_giac_eval()"),
            ("main/components/opencalc_cas.c", "opencalc_cas_eval()"),
            ("main/components/opencalc_units.c", "opencalc_units_eval()"),
        ],
    },
    {
        "key": "cas_reliability",
        "label": "CAS reliability: Giac is serialized in an isolated 64 KB PSRAM task with request IDs, cancellation, timeout, quarantine, and context recreation",
        "source_file": "components/giac/OPENCALC_INTEGRATION.md",
        "source_location": "L1",
        "targets": [
            ("main/components/opencalc_giac.cpp", "giac_task()"),
            ("main/components/opencalc_giac.cpp", "opencalc_giac_eval()"),
            ("main/components/opencalc_giac.cpp", "opencalc_giac_reset()"),
        ],
    },
    {
        "key": "graphing",
        "label": "Graphing: Cartesian, parametric, polar, and sequence modes share equations with tables, trace, styles, analysis, symbolic derivatives, integrals, roots, and asymptotes",
        "source_file": "../guied.MD",
        "source_location": "L527",
        "targets": [
            ("main/components/opencalc_graph_model.c", "opencalc_graph_model()"),
            ("main/components/opencalc_ui.c", "graph_calc_run_selected()"),
            ("main/components/opencalc_ui.c", "graph_trace_cycle_line()"),
        ],
    },
    {
        "key": "statistics",
        "label": "Statistics: summary statistics, regressions, plots, distributions, confidence intervals, and hypothesis tests use Lists as data sources with dedicated setup and result screens",
        "source_file": "APP_STATUS.md",
        "source_location": "L242",
        "targets": [
            ("main/components/opencalc_stats.c", "opencalc_stats_normal_cdf()"),
            ("main/components/opencalc_ui.c", "stats_setup_execute()"),
            ("main/components/opencalc_ui.c", "ui_draw_stats_plot()"),
        ],
    },
    {
        "key": "lists_matrices",
        "label": "Lists and Matrices: spreadsheet-style list editing and ten PSRAM-backed matrices up to 99x99 support arithmetic, powers, transpose, inverse, determinant, REF, RREF, augment, and extraction",
        "source_file": "APP_STATUS.md",
        "source_location": "L226",
        "targets": [
            ("main/components/opencalc_ui.c", "ui_draw_list_editor()"),
            ("main/components/opencalc_ui.c", "ui_draw_matrix_editor()"),
            ("main/components/opencalc_ui.c", "matrix_rref_a()"),
        ],
    },
    {
        "key": "solver",
        "label": "Solver: equation, system, polynomial, and bounded numeric workflows use CAS exact solving plus numerical fallback, root browsing, saved problems, graph handoff, and result export",
        "source_file": "APP_STATUS.md",
        "source_location": "L317",
        "targets": [
            ("main/components/opencalc_ui.c", "solver_run_workflow_action()"),
            ("main/components/opencalc_ui.c", "ui_draw_solver_roots()"),
            ("main/components/opencalc_ui.c", "solver_plot_sides()"),
        ],
    },
    {
        "key": "finance",
        "label": "Finance: dedicated TVM and cash-flow worksheets solve PV, FV, payment, periods, and interest rate and calculate NPV and IRR from editable cash flows",
        "source_file": "APP_STATUS.md",
        "source_location": "L371",
        "targets": [
            ("main/components/opencalc_ui.c", "finance_solve_selected()"),
            ("main/components/opencalc_ui.c", "finance_npv_from_list()"),
            ("main/components/opencalc_ui.c", "finance_irr_from_list()"),
        ],
    },
    {
        "key": "conics_inequalities",
        "label": "Conics and Inequalities: dedicated worksheets analyze and graph conics, solve one- and two-variable inequalities, shade regions, make sign charts, optimize, and export results",
        "source_file": "APP_STATUS.md",
        "source_location": "L396",
        "targets": [
            ("main/components/opencalc_conics.c", "opencalc_conic_analyze()"),
            ("main/components/opencalc_inequality.c", "opencalc_inequality_solve_1d()"),
            ("main/components/opencalc_ui.c", "draw_inequality_layer()"),
        ],
    },
    {
        "key": "variables",
        "label": "Variables: STO stores A-Z or custom names; VARS inserts references; GET inserts values; browsers cover user values, lists, matrices, functions, strings, statistics, graphs, and system state",
        "source_file": "../guied.MD",
        "source_location": "L484",
        "targets": [
            ("main/components/opencalc_ui.c", "variable_store_into()"),
            ("main/components/opencalc_ui.c", "variables_open()"),
            ("main/components/opencalc_ui.c", "variables_save_all()"),
        ],
    },
    {
        "key": "reference_center",
        "label": "Reference Center: Alpha plus Zoom opens an interactive 118-element periodic table and searchable math, physics, and engineering formula catalogs",
        "source_file": "../guied.MD",
        "source_location": "L1024",
        "targets": [
            ("main/components/opencalc_reference.c", "opencalc_element_get()"),
            ("main/components/opencalc_ui.c", "open_reference_center()"),
            ("main/components/opencalc_ui.c", "ui_draw_periodic_table()"),
        ],
    },
    {
        "key": "worksheets_persistence",
        "label": "Persistence: NVS stores compact settings and variables; a versioned CRC-checked FAT snapshot with atomic backup stores history, graphs, lists, matrices, finance, and conics",
        "source_file": "FIRMWARE_README.md",
        "source_location": "L3",
        "targets": [
            ("main/components/opencalc_persist.c", "opencalc_persist_init()"),
            ("main/components/opencalc_workspace_io.c", "opencalc_workspace_write_payload()"),
            ("main/components/opencalc_worksheet_model.c", "opencalc_worksheet_model_init()"),
        ],
    },
    {
        "key": "scripting",
        "label": "Tiny Python: on-device editor and sandboxed Python-like interpreter with input, recursion limits, debugger, graphics, keys, storage, audio, math, sensors, and list APIs",
        "source_file": "main/components/tiny-python-readme.md",
        "source_location": "L1",
        "targets": [
            ("main/components/tiny-python.c", "py_run_file()"),
            ("main/components/opencalc_script_model.c", "opencalc_script_model()"),
            ("main/components/opencalc_ui.c", "script_worker_task()"),
        ],
    },
    {
        "key": "science_io",
        "label": "Scientific I/O: optional shared I2C sensor bus drives MCP23017 digital channels and ADS1115 four-channel 16-bit analog input for Tiny Python experiments and Lists analysis",
        "source_file": "../guied.MD",
        "source_location": "L1206",
        "targets": [
            ("main/components/opencalc_sensor_hub.c", "opencalc_sensor_hub_init()"),
            ("main/components/opencalc_sensor_hub.c", "opencalc_sensor_digital_read()"),
            ("main/components/opencalc_sensor_hub.c", "opencalc_sensor_analog_read_volts()"),
        ],
    },
    {
        "key": "hardware_v5",
        "label": "V5 hardware: ESP32-S3, 320x240 ST7789 HS280S030RX display, diode-isolated 10x5 keypad, USB-C, Li-ion power, optional audio, and 30-pin sensor header",
        "source_file": "../hardware/pcb/PCB_README.md",
        "source_location": "L1",
        "targets": [
            ("main/components/board_init.c", "board_init()"),
            ("main/components/board_init.c", "keypad_init()"),
            ("main/components/board_init.c", "lcd_draw_full_frame_async()"),
        ],
    },
    {
        "key": "display_input",
        "label": "Display and input: SPI LCD uses pipelined full-frame DMA; row interrupts wake diode-isolated matrix scans and support simultaneous held keys for games",
        "source_file": "FIRMWARE_README.md",
        "source_location": "L162",
        "targets": [
            ("main/components/board_init.c", "lcd_draw_full_frame_async()"),
            ("main/components/board_init.c", "board_keypad_scan()"),
            ("main/components/board_init.c", "board_keypad_scan_matrix()"),
        ],
    },
    {
        "key": "usb_storage",
        "label": "One-cable USB: USB-C supports flashing, CDC serial, charging, and FAT mass storage with explicit ownership transitions between host and firmware",
        "source_file": "FIRMWARE_README.md",
        "source_location": "L75",
        "targets": [
            ("main/components/usb_msc.c", "init_usb_msc()"),
            ("main/components/usb_msc.c", "usb_msc_mount_app()"),
            ("main/components/usb_msc.c", "usb_msc_mount_usb()"),
        ],
    },
    {
        "key": "power",
        "label": "Power management: configurable brightness and CPU caps, light-sleep software off, keypad wake handling, battery voltage estimation, and old/new backlight circuit profiles",
        "source_file": "../guied.MD",
        "source_location": "L174",
        "targets": [
            ("main/components/board_init.c", "board_enter_deep_sleep()"),
            ("main/components/board_init.c", "board_battery_get_percent()"),
            ("main/components/board_init.c", "board_set_backlight_brightness()"),
            ("main/components/opencalc_power.c", "opencalc_power_set_power_save()"),
        ],
    },
    {
        "key": "games_audio",
        "label": "Games and audio: launcher includes Tetris, Doom, Snake, Breakout, and NES Mario with held-key controls, persistent scores, shared display paths, and optional PAM8302A sound",
        "source_file": "../guied.MD",
        "source_location": "L1410",
        "targets": [
            ("main/components/opencalc_tetris.c", "opencalc_tetris_tick()"),
            ("main/components/opencalc_doom.c", "opencalc_doom_tick()"),
            ("main/components/opencalc_snake.c", "opencalc_snake_tick()"),
            ("main/components/opencalc_breakout.c", "opencalc_breakout_tick()"),
            ("main/components/opencalc_mario.cpp", "opencalc_mario_tick()"),
            ("main/components/opencalc_audio.c", "opencalc_audio_init()"),
        ],
    },
    {
        "key": "testing_status",
        "label": "Validation status: 12 host regression groups cover calculator, CAS fallbacks, graph modes, stats, units, variables, conics, inequalities, reference data, and Tiny Python",
        "source_file": "../README.md",
        "source_location": "L121",
        "targets": [
            ("tests/run_host_tests.sh", "run_host_tests.sh"),
            ("main/components/opencalc_self_test.c", "opencalc_self_test_run()"),
        ],
    },
    {
        "key": "current_risks",
        "label": "Current release risks: prolonged Giac and script hardware reliability, interrupted persistence recovery, large-matrix stress, V5 audio and scientific-I/O validation, and final LCD polish",
        "source_file": "APP_STATUS.md",
        "source_location": "L9",
        "targets": [
            ("main/components/opencalc_giac.cpp", "opencalc_giac_eval()"),
            ("main/components/opencalc_workspace_io.c", "opencalc_workspace_write_payload()"),
            ("main/components/opencalc_sensor_hub.c", "opencalc_sensor_hub_init()"),
        ],
    },
    {
        "key": "license",
        "label": "Licensing: OpenCalc-authored firmware is GPL-3.0-or-later; Giac, Doom, NES emulator, ESP-IDF, TinyUSB, and panel drivers retain their documented third-party licenses",
        "source_file": "../THIRD_PARTY_NOTICES.md",
        "source_location": "L1",
        "targets": [],
    },
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--graph",
        type=Path,
        default=Path("graphify-out/graph.json"),
        help="Graphify node-link JSON to update",
    )
    return parser.parse_args()


def atomic_write(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(payload, stream, ensure_ascii=False, separators=(",", ":"))
            stream.write("\n")
        os.replace(temp_name, path)
    except BaseException:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass
        raise


def main() -> int:
    args = parse_args()
    graph = json.loads(args.graph.read_text(encoding="utf-8"))
    nodes = [n for n in graph.get("nodes", []) if not str(n.get("id", "")).startswith(PREFIX)]
    links = [
        edge
        for edge in graph.get("links", [])
        if not str(edge.get("source", "")).startswith(PREFIX)
        and not str(edge.get("target", "")).startswith(PREFIX)
    ]

    by_symbol = {
        (node.get("source_file"), node.get("label")): node["id"]
        for node in nodes
        if node.get("source_file") and node.get("label")
    }
    project_id = f"{PREFIX}project"
    missing_targets: list[tuple[str, str]] = []

    for item in CONTEXT:
        node_id = f"{PREFIX}{item['key']}"
        nodes.append(
            {
                "id": node_id,
                "label": item["label"],
                "norm_label": item["label"].lower(),
                "_origin": "semantic",
                "file_type": "project_context",
                "source_file": item["source_file"],
                "source_location": item["source_location"],
                "community": -1,
                "community_name": "OpenCalc project context",
                "confidence": "EXTRACTED",
                "confidence_score": 1.0,
            }
        )
        if node_id != project_id:
            links.append(
                {
                    "source": project_id,
                    "target": node_id,
                    "relation": "documents",
                    "_origin": "semantic",
                    "confidence": "EXTRACTED",
                    "confidence_score": 1.0,
                    "context": "maintained project knowledge",
                    "source_file": item["source_file"],
                    "source_location": item["source_location"],
                    "weight": 1.0,
                }
            )
        for target in item["targets"]:
            target_id = by_symbol.get(target)
            if target_id is None:
                missing_targets.append(target)
                continue
            links.append(
                {
                    "source": node_id,
                    "target": target_id,
                    "relation": "implemented_by",
                    "_origin": "semantic",
                    "confidence": "EXTRACTED",
                    "confidence_score": 1.0,
                    "context": "authoritative source mapping",
                    "source_file": target[0],
                    "source_location": "",
                    "weight": 2.0,
                }
            )

    graph["nodes"] = nodes
    graph["links"] = links
    graph.setdefault("graph", {})["manual_context"] = {
        "generator": "tools/update_graphify_context.py",
        "node_count": len(CONTEXT),
        "uses_local_model": False,
    }
    atomic_write(args.graph, graph)

    audit_path = args.graph.parent / ".graphify_manual_context.json"
    atomic_write(
        audit_path,
        {
            "generator": "tools/update_graphify_context.py",
            "nodes": CONTEXT,
            "missing_targets": [list(target) for target in sorted(set(missing_targets))],
        },
    )
    print(f"Added {len(CONTEXT)} project-context nodes to {args.graph}")
    print(f"Missing implementation targets: {len(set(missing_targets))}")
    for source_file, label in sorted(set(missing_targets)):
        print(f"  {source_file}: {label}")
    return 1 if missing_targets else 0


if __name__ == "__main__":
    raise SystemExit(main())
