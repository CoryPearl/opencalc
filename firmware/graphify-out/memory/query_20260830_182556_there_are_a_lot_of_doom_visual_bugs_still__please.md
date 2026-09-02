---
type: "query"
date: "2026-08-30T18:25:56.021165+00:00"
question: "there are a lot of doom visual bugs still, please fix them"
contributor: "graphify"
outcome: "useful"
source_nodes: ["i_video.c", "I_FinishUpdate()", "I_SetPalette()", "I_GetPaletteIndex()", "R_StoreWallRange()"]
---

# Q: there are a lot of doom visual bugs still, please fix them

## Answer

Expanded from original query via graph vocab: doom, video, framebuffer, palette, color, scale, conversion, render, display, lcd, dma, buffer. The current source showed two concrete defects: R_StoreWallRange used a non-cyclic unsigned angle difference, corrupting wall distance across the angle wrap, and i_video retained a generic offset/scaling conversion plus an uninitialized RGB565 palette used by I_GetPaletteIndex. Fixed the angle with modular shortest-distance math and replaced frame conversion with one fixed 320x200 indexed-to-0x00RRGGBB palette path with compile-time geometry/pixel checks and correct nearest-palette lookup.

## Outcome

- Signal: useful

## Source Nodes

- i_video.c
- I_FinishUpdate()
- I_SetPalette()
- I_GetPaletteIndex()
- R_StoreWallRange()