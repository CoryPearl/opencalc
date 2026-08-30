---
type: "query"
date: "2026-08-30T05:10:40.657946+00:00"
question: "games are running at 10fps, we need them at 45 just like the apps"
contributor: "graphify"
outcome: "useful"
source_nodes: ["opencalc_ui_task", "board_draw_rgb888_frame_320x240", "opencalc_mario_tick", "opencalc_power_set_power_save"]
---

# Q: games are running at 10fps, we need them at 45 just like the apps

## Answer

Expanded from graph vocabulary via [game, tick, frame, fps, display, draw, lcd, spi, dma]. The game loop added a full frame delay after rendering, so render time plus 22 ms produced about 10 FPS. Changed main scheduling to subtract render time from the 45 FPS budget, set normal CPU max to 240 MHz, made PCB LCD SPI configurable at 80 MHz, switched application compilation from -Og to -O2, added alternating RGB565 DMA frame buffers, removed the extra Doom delay, and changed Mario from a hard-coded 30 FPS cap to OPENCALC_TARGET_FPS. Doom retains its native 35 Hz simulation. No build was run by user request.

## Outcome

- Signal: useful

## Source Nodes

- opencalc_ui_task
- board_draw_rgb888_frame_320x240
- opencalc_mario_tick
- opencalc_power_set_power_save