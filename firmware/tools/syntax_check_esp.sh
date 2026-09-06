#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
COMMANDS="$ROOT/build/compile_commands.json"
if [ ! -f "$COMMANDS" ]; then
    echo "Missing build/compile_commands.json; configure the ESP-IDF project first." >&2
    exit 2
fi

check_as() {
    source_file=$1
    template_file=${2:-$1}
    command=$(jq -r --arg suffix "$template_file" \
        '.[] | select(.file | endswith($suffix)) | .command' "$COMMANDS" | head -n 1)
    if [ -z "$command" ]; then
        echo "No compile command template for $template_file" >&2
        exit 2
    fi
    command=$(printf '%s\n' "$command" | sed -E 's@ -o [^ ]+ -c [^ ]+$@@')
    echo "syntax: $source_file"
    eval "$command -fsyntax-only '$ROOT/$source_file'"
}

check_as main/components/opencalc_calc.c
check_as main/components/opencalc_math.c
check_as main/components/opencalc_giac.cpp
check_as main/components/opencalc_graph_model.c main/components/opencalc_ui.c
check_as main/components/opencalc_script_model.c main/components/opencalc_ui.c
check_as main/components/opencalc_worksheet_model.c main/components/opencalc_ui.c
check_as main/components/opencalc_workspace_io.c main/components/opencalc_ui.c
check_as main/components/opencalc_tetris.c
check_as main/components/opencalc_snake.c
check_as main/components/opencalc_breakout.c
check_as main/components/opencalc_mario.cpp
check_as main/components/opencalc_doom.c
check_as main/components/opencalc_ui.c
check_as main/components/usb_msc.c
check_as main/main.c
check_as main/components/opencalc_ui_navigation.c main/components/opencalc_ui.c
check_as main/components/opencalc_self_test.c main/main.c

echo "ESP source syntax checks passed."
