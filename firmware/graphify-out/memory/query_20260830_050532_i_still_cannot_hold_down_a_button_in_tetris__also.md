---
type: "query"
date: "2026-08-30T05:05:32.188543+00:00"
question: "i still cannot hold down a button in tetris, also do not build until i say"
contributor: "graphify"
outcome: "useful"
source_nodes: ["opencalc_tetris.c", "board_keypad_scan_matrix", "tetris_step"]
---

# Q: i still cannot hold down a button in tetris, also do not build until i say

## Answer

Expanded from graph vocabulary via [tetris, keypad, matrix, scan, held, hold, press, tick, input]. The Tetris Down input was emitted as a brief soft-drop pulse every 140 ms, while tetris_step requires a continuous true held state and resets gravity on transitions. Updated opencalc_tetris_tick to pass the physical Down matrix state continuously and added DAS/ARR repeat for held Left and Right. Kept rotate, pause, hold, and hard drop edge-triggered. No build was run at user request.

## Outcome

- Signal: useful

## Source Nodes

- opencalc_tetris.c
- board_keypad_scan_matrix
- tetris_step