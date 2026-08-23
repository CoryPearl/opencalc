# Graph Report - main  (2026-08-20)

## Corpus Check
- 263 files · ~272,333 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 2796 nodes · 8062 edges · 106 communities (103 shown, 3 thin omitted)
- Extraction: 80% EXTRACTED · 20% INFERRED · 0% AMBIGUOUS · INFERRED: 1591 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- Script Runtime UI
- Doom Save System
- Doom Cheats Text
- Doom Enemy Logic
- Calculator Evaluation
- Doom Demo Loop
- Doom Automap
- Doom Collision
- App Home Graphing
- Key Dispatch Tools
- NES Mapper Cache
- Doom Network Loop
- Doom Sound Stubs
- NES Cartridge Bus
- Math Parser
- Doom Menu System
- Doom Platform Backend
- NES 6502 CPU
- NES Mapper One
- Doom Startup IWAD
- Doom Intermission
- NES CPU State
- Power Persistence
- Doom HUD Text
- Doom Weapons
- Board Hardware IO
- NES Mapper SixtyNine
- Breakout Game
- Doom Screen Scaling
- Doom Combat Logic
- Doom R Bsp
- Doom I Sdlsound
- Doom V Video
- NES Ppu2C02
- Doom D Iwad
- Doom R Draw
- Doom M Misc
- Doom I System
- NES Mapper004
- OpenCalc UI
- Doom I Sdlmusic
- Doom P Plats
- Doom R Main
- Doom I Video
- Tetris Game
- Doom P Setup
- Doom M Menu
- NES Opencalc Mario
- Doom F Wipe
- Doom R Data
- Doom Sha1
- Doom M Config
- Doom P Spec
- Doom P Ceilng
- NES Controller
- Doom R Things
- Snake Game
- Flash Mmap
- Doom P Lights
- NES Bus
- OpenCalc UI
- NES Mapper
- NES Ppu2C02
- Opencalc Doom
- OpenCalc UI
- Doom I Allegromusic
- Doom Memio
- Doom Mus2Mid
- Tetris Game
- OpenCalc UI
- Doom W Wad
- NES Cpu6502
- Doom P Switch
- Doom I Joystick
- Doom F Finale
- Doom M Controls
- Doom Doomgeneric Win
- Doom I Allegrosound
- NES Bus
- NES Apu2A03
- Doom Doomgeneric Soso
- Doom M Menu
- Doom D Mode
- Doom Doomgeneric Linuxvt
- Doom Doomgeneric Sosox
- Doom Gusconf
- Doom P Lights
- Doom I Sdlmusic
- Snake Game
- Doom P Doors
- Doom P User
- Sd
- Doom M Bbox
- Sd
- Doom P Enemy
- Tft Espi
- NES Cpu6502
- NES Ppu2C02
- Cmakelists
- Doom Wi Stuff
- Python Runtime
- NES Bus

## God Nodes (most connected - your core abstractions)
1. `Cpu6502` - 108 edges
2. `I_Error()` - 107 edges
3. `S_StartSound()` - 82 edges
4. `Z_Malloc()` - 62 edges
5. `Mapper` - 61 edges
6. `ui_draw_current()` - 61 edges
7. `D_DoomMain()` - 54 edges
8. `Ppu2C02` - 52 edges
9. `P_Random()` - 47 edges
10. `run_home_app_tool()` - 43 edges

## Surprising Connections (you probably didn't know these)
- `app_main()` --calls--> `board_init()`  [INFERRED]
  main.c → components/board_init.c
- `opencalc_ui_task()` --calls--> `board_set_event_task()`  [INFERRED]
  main.c → components/board_init.c
- `app_main()` --calls--> `opencalc_persist_init()`  [INFERRED]
  main.c → components/opencalc_persist.c
- `opencalc_ui_task()` --calls--> `opencalc_ui_handle_serial_buttons()`  [INFERRED]
  main.c → components/opencalc_ui.c
- `opencalc_ui_task()` --calls--> `opencalc_ui_handle_keypad_interrupt()`  [INFERRED]
  main.c → components/opencalc_ui.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Embedded Storage Script Execution** — main_components_tiny_python_readme_tiny_python, main_components_tiny_python_readme_py_run_file, main_components_tiny_python_readme_opencalc_os_integration [INFERRED 0.75]

## Communities (106 total, 3 thin omitted)

### Community 0 - "Script Runtime UI"
Cohesion: 0.09
Nodes (110): run_selected_script(), append_body_token(), append_program_text(), append_source_line(), assignment_to_binary(), call_function(), call_user_function(), continues_if_chain() (+102 more)

### Community 1 - "Doom Save System"
Cohesion: 0.07
Nodes (74): actionf_t, G_BeginRecording(), G_DoLoadGame(), G_DoSaveGame(), G_VanillaVersionCode(), I_InitTimidityConfig(), M_StringJoin(), M_TempFile() (+66 more)

### Community 2 - "Doom Cheats Text"
Cohesion: 0.06
Nodes (60): ammotype_t, card_t, cheatseq_t, I_SetPalette(), cht_CheckCheat(), cht_GetParam(), boolean, player_t (+52 more)

### Community 3 - "Doom Enemy Logic"
Cohesion: 0.09
Nodes (68): G_ExitLevel(), A_BabyMetal(), A_BossDeath(), A_BrainAwake(), A_BrainDie(), A_BrainPain(), A_BrainSpit(), A_BruisAttack() (+60 more)

### Community 4 - "Calculator Evaluation"
Cohesion: 0.07
Nodes (61): board_touch_take_interrupt(), opencalc_math_eval_expression(), app_output(), calc_eval(), calc_expand_ans(), calc_expand_ans_value(), calc_format_fraction_value(), calc_history_push() (+53 more)

### Community 5 - "Doom Demo Loop"
Cohesion: 0.07
Nodes (55): D_AdvanceDemo(), D_DoAdvanceDemo(), D_PageTicker(), boolean, player_t, ticcmd_t, PlayerQuitGame(), RunTic() (+47 more)

### Community 6 - "Doom Automap"
Cohesion: 0.08
Nodes (49): AM_activateNewScale(), AM_addMark(), AM_changeWindowLoc(), AM_changeWindowScale(), AM_clearFB(), AM_clearMarks(), AM_clipMline(), AM_doFollowPlayer() (+41 more)

### Community 7 - "Doom Collision"
Cohesion: 0.11
Nodes (51): FixedMul(), boolean, fixed_t, intercept_t, line_t, mobj_t, player_t, sector_t (+43 more)

### Community 8 - "App Home Graphing"
Cohesion: 0.11
Nodes (52): app_id_t, app_info_t, board_battery_is_charging(), graph_view_t, graph_screen_x(), graph_screen_y(), graph_world_x(), app_id_for_info() (+44 more)

### Community 9 - "Key Dispatch Tools"
Cohesion: 0.09
Nodes (51): app_tool_t, opencalc_math_set_degrees(), opencalc_persist_set_u32(), active_expression_buffer(), adjust_brightness(), adjust_graph_window_value(), adjust_mode_value(), app_tools_for() (+43 more)

### Community 10 - "NES Mapper Cache"
Cohesion: 0.08
Nodes (42): IRAM_ATTR, getBank(), getBankIndex(), invalidateCache(), Mapper, state, mapper001_dumpState(), mapper001_loadState() (+34 more)

### Community 11 - "Doom Network Loop"
Cohesion: 0.07
Nodes (27): BlockUntilStart(), BuildNewTic(), boolean, net_gamesettings_t, ticcmd_t, D_Disconnected(), D_ReceiveTic(), D_RegisterLoopCallbacks() (+19 more)

### Community 12 - "Doom Sound Stubs"
Cohesion: 0.08
Nodes (46): boolean, sfxinfo_t, CheckVolumeSeparation(), I_GetSfxLumpNum(), I_InitMusic(), I_InitSound(), I_MusicIsPlaying(), I_PauseSong() (+38 more)

### Community 13 - "NES Cartridge Bus"
Cohesion: 0.07
Nodes (42): Bus, Cartridge, bus, chr_base, cpuCycle, cpuRead, cpuWrite, CRC32 (+34 more)

### Community 14 - "Math Parser"
Cohesion: 0.16
Nodes (42): calc_parser_t, angle_to_radians(), calc_apply_func(), calc_eval_expression_with_x(), calc_eval_fn_int(), calc_eval_n_deriv(), calc_factorial(), calc_gcd() (+34 more)

### Community 15 - "Doom Menu System"
Cohesion: 0.10
Nodes (40): D_StartTitle(), skill_t, G_DeferedInitNew(), G_SaveGame(), G_ScreenShot(), I_Quit(), I_WaitVBL(), boolean (+32 more)

### Community 16 - "Doom Platform Backend"
Cohesion: 0.07
Nodes (22): doomgeneric_Tick(), main(), doomgeneric_Create(), addKeyToQueue(), convertToDoomKey(), DG_DrawFrame(), handleKeyInput(), main() (+14 more)

### Community 17 - "NES 6502 CPU"
Cohesion: 0.05
Nodes (41): ABX, IDY, IMM, IMP, IND, Instr_BCC, Instr_BCS, Instr_BEQ (+33 more)

### Community 18 - "NES Mapper One"
Cohesion: 0.06
Nodes (41): Cartridge, MappedROM, MIRROR, ROMBackend, createMapper001(), getCHRBank4K(), getCHRBank8K(), getPRGBank() (+33 more)

### Community 19 - "Doom Startup IWAD"
Cohesion: 0.08
Nodes (30): net_connect_data_t, D_InitNetGame(), boolean, D_AddFile(), D_DoomMain(), D_Endoom(), D_GrabMouseCallback(), D_IdentifyVersion() (+22 more)

### Community 20 - "Doom Intermission"
Cohesion: 0.13
Nodes (40): M_ClearRandom(), M_Random(), V_DrawPatch(), patch_t, wbstartstruct_t, WI_checkForAccelerate(), WI_drawAnimatedBack(), WI_drawDeathmatchStats() (+32 more)

### Community 21 - "NES CPU State"
Cohesion: 0.05
Nodes (41): Cpu6502, A, ABS, ABY, addr_abs, addr_rel, addrmode_implied, apu (+33 more)

### Community 22 - "Power Persistence"
Cohesion: 0.07
Nodes (27): board_get_backlight_brightness(), board_set_backlight_brightness(), opencalc_breakout_init(), opencalc_mario_init(), opencalc_persist_factory_reset(), opencalc_persist_get_u32(), opencalc_persist_init(), opencalc_power_get_power_save() (+19 more)

### Community 23 - "Doom HUD Text"
Cohesion: 0.12
Nodes (34): boolean, patch_t, HUlib_addCharToTextLine(), HUlib_addLineToSText(), HUlib_addMessageToSText(), HUlib_addPrefixToIText(), HUlib_clearTextLine(), HUlib_delCharFromIText() (+26 more)

### Community 24 - "Doom Weapons"
Cohesion: 0.17
Nodes (39): angle_t, P_AimLineAttack(), P_LineAttack(), A_BFGsound(), A_CheckReload(), A_FireBFG(), A_FireCGun(), A_FireMissile() (+31 more)

### Community 25 - "Board Hardware IO"
Cohesion: 0.09
Nodes (35): backlight_off(), backlight_on(), battery_monitor_init(), board_battery_get_percent(), board_battery_get_voltage_mv(), board_draw_rgb565_frame_320x200(), board_draw_rgb888_frame_320x200(), board_enter_deep_sleep() (+27 more)

### Community 26 - "NES Mapper SixtyNine"
Cohesion: 0.06
Nodes (37): Cartridge, MappedROM, MIRROR, ROMBackend, createMapper069(), getCHRBank(), getPRGBank(), mapper069_cpuRead() (+29 more)

### Community 27 - "Breakout Game"
Cohesion: 0.16
Nodes (34): breakout_t, breakout_active_ball_count(), breakout_all_cleared(), breakout_apply_powerup(), breakout_clear_powerups(), breakout_fill_bricks(), breakout_init(), breakout_launch() (+26 more)

### Community 28 - "Doom Screen Scaling"
Cohesion: 0.13
Nodes (36): boolean, byte, FindNearestColor(), GenerateStretchTable(), I_InitScale(), I_InitSquashTable(), I_InitStretchTables(), I_ResetScaleTables() (+28 more)

### Community 29 - "Doom Combat Logic"
Cohesion: 0.15
Nodes (36): P_Random(), A_BrainExplode(), A_BrainScream(), A_CPosRefire(), A_SpawnFly(), A_SpidRefire(), A_Tracer(), mobj_t (+28 more)

### Community 30 - "Doom R Bsp"
Cohesion: 0.09
Nodes (26): boolean, fixed_t, seg_t, R_AddLine(), R_CheckBBox(), R_ClearClipSegs(), R_ClearDrawSegs(), R_ClipPassWallSegment() (+18 more)

### Community 31 - "Doom I Sdlsound"
Cohesion: 0.15
Nodes (31): allocated_sound_t, AllocatedSoundLink(), AllocatedSoundUnlink(), AllocateSound(), boolean, byte, sfxinfo_t, CacheSFX() (+23 more)

### Community 32 - "Doom V Video"
Cohesion: 0.11
Nodes (30): D_Display(), D_PageDrawer(), wipe_EndScreen(), wipe_ScreenWipe(), R_FillBackScreen(), byte, patch_t, error_fn() (+22 more)

### Community 33 - "NES Ppu2C02"
Cohesion: 0.07
Nodes (29): Bus, Cartridge, Ppu2C02, bus, cart, control, mask, nametable (+21 more)

### Community 34 - "Doom D Iwad"
Cohesion: 0.15
Nodes (29): AddDoomWadPath(), AddIWADDir(), BuildIWADDirList(), boolean, GameMission_t, GameMode_t, CheckCollectorsEdition(), CheckDirectoryHasIWAD() (+21 more)

### Community 35 - "Doom R Draw"
Cohesion: 0.10
Nodes (22): I_Error(), LoadResponseFile(), M_FindResponseFile(), R_DrawColumn(), R_DrawColumnLow(), R_DrawFuzzColumn(), R_DrawFuzzColumnLow(), R_DrawSpan() (+14 more)

### Community 36 - "Doom M Misc"
Cohesion: 0.09
Nodes (22): byte, FILE, M_ExtractFileBase(), M_FileLength(), M_ReadFile(), M_StringDuplicate(), M_StringStartsWith(), M_vsnprintf() (+14 more)

### Community 37 - "Doom I System"
Cohesion: 0.09
Nodes (19): atexit_func_t, DG_DrawFrame(), DG_Init(), AutoAllocMemory(), boolean, byte, EscapeShellString(), I_AtExit() (+11 more)

### Community 38 - "NES Mapper004"
Cohesion: 0.07
Nodes (28): Cartridge, MappedROM, MIRROR, ROMBackend, createMapper004(), Mapper004_state, backend, bank_register (+20 more)

### Community 39 - "OpenCalc UI"
Cohesion: 0.16
Nodes (28): calc_take_wrapped_expression(), graph_add_poi(), graph_collect_pois(), graph_eval_fn_at(), graph_jump_to_nearest_intersection(), graph_poi_label(), graph_refine_intersection(), graph_refine_zero() (+20 more)

### Community 40 - "Doom I Sdlmusic"
Cohesion: 0.13
Nodes (22): AddSubstituteMusic(), boolean, GetFullPath(), GetMusicPosition(), I_SDL_InitMusic(), I_SDL_MusicIsPlaying(), I_SDL_PauseSong(), I_SDL_PlaySong() (+14 more)

### Community 41 - "Doom P Plats"
Cohesion: 0.10
Nodes (24): vldoor_t, T_SlidingDoor(), T_VerticalDoor(), boolean, fixed_t, floormove_t, sector_t, T_MoveFloor() (+16 more)

### Community 42 - "Doom R Main"
Cohesion: 0.12
Nodes (20): R_InitBuffer(), R_InitTranslationTables(), angle_t, fixed_t, seg_t, R_AddPointToBox(), R_ExecuteSetViewSize(), R_Init() (+12 more)

### Community 43 - "Doom I Video"
Cohesion: 0.10
Nodes (21): D_DoomLoop(), I_InitInput(), swapLE32(), boolean, cmap_to_fb(), I_BeginRead(), I_BindVideoVariables(), I_CheckIsScreensaver() (+13 more)

### Community 44 - "Tetris Game"
Cohesion: 0.26
Nodes (23): opencalc_tetris_press_button_number(), tetris_piece_t, tetris_cell_set(), tetris_finish_clear(), tetris_fits(), tetris_ghost_y(), tetris_gravity_interval_ms(), tetris_hard_drop() (+15 more)

### Community 45 - "Doom P Setup"
Cohesion: 0.26
Nodes (22): DumpSubstituteConfig(), IsMusicLump(), byte, sector_t, skill_t, GetSectorAtNullAddress(), P_LoadBlockMap(), P_LoadLineDefs() (+14 more)

### Community 46 - "Doom M Menu"
Cohesion: 0.17
Nodes (23): M_DrawEmptyCell(), M_DrawEpisode(), M_Drawer(), M_DrawLoad(), M_DrawMainMenu(), M_DrawNewGame(), M_DrawOPLDev(), M_DrawOptions() (+15 more)

### Community 47 - "NES Opencalc Mario"
Cohesion: 0.16
Nodes (20): board_display_lock(), board_display_unlock(), board_draw_rgb888_frame_320x240(), board_draw_text_screen(), glyph_for(), DG_DrawFrame(), opencalc_doom_start(), opencalc_doom_wad_available() (+12 more)

### Community 48 - "Doom F Wipe"
Cohesion: 0.12
Nodes (8): wipe_exitMelt(), wipe_StartScreen(), GetTypedChar(), I_GetEvent(), TranslateKey(), UpdateShiftStatus(), byte, I_ReadScreen()

### Community 49 - "Doom R Data"
Cohesion: 0.20
Nodes (21): wipe_initMelt(), wipe_shittyColMajorXform(), byte, column_t, GenerateTextureHashTable(), R_DrawColumnInCache(), R_GenerateComposite(), R_GenerateLookup() (+13 more)

### Community 50 - "Doom Sha1"
Cohesion: 0.17
Nodes (18): GetSubstituteMusicFile(), byte, sha1_context_t, sha1_digest_t, SHA1_Final(), SHA1_Init(), SHA1_Update(), SHA1_UpdateInt32() (+10 more)

### Community 51 - "Doom M Config"
Cohesion: 0.17
Nodes (21): boolean, GetDefaultConfigDir(), GetDefaultForName(), LoadDefaultCollection(), M_GetFloatVariable(), M_GetIntVariable(), M_GetSaveGameDir(), M_GetStrVariable() (+13 more)

### Community 52 - "Doom P Spec"
Cohesion: 0.25
Nodes (21): EV_DoFloor(), EV_DoPlat(), fixed_t, line_t, sector_t, DonutOverrun(), EV_DoDonut(), getNextSector() (+13 more)

### Community 53 - "Doom P Ceilng"
Cohesion: 0.16
Nodes (12): ceiling_e, ceiling_t, line_t, EV_CeilingCrushStop(), EV_DoCeiling(), P_ActivateInStasisCeiling(), P_AddActiveCeiling(), P_RemoveActiveCeiling() (+4 more)

### Community 54 - "NES Controller"
Cohesion: 0.18
Nodes (16): delayMicroseconds(), digitalRead(), digitalWrite(), controllerRead(), gpioRead(), initController(), isDownPressed(), NESControllerRead() (+8 more)

### Community 55 - "Doom R Things"
Cohesion: 0.16
Nodes (20): R_RenderMaskedSegRange(), boolean, column_t, mobj_t, pspdef_t, sector_t, R_AddSprites(), R_DrawMasked() (+12 more)

### Community 56 - "Snake Game"
Cohesion: 0.20
Nodes (19): opencalc_breakout_active(), opencalc_doom_tick(), opencalc_mario_active(), border(), draw_frame(), draw_panel_box(), font_for(), opencalc_snake_active() (+11 more)

### Community 57 - "Flash Mmap"
Cohesion: 0.12
Nodes (11): Cartridge, MappedROM, mappedROM_init(), Cartridge::Cartridge(), createMapper, ROMBackend, mapperNoCycle(), mapperNoScanline() (+3 more)

### Community 58 - "Doom P Lights"
Cohesion: 0.18
Nodes (19): sector_t, P_SpawnDoorCloseIn30(), P_SpawnDoorRaiseIn5Mins(), glow_t, lightflash_t, sector_t, strobe_t, P_SpawnFireFlicker() (+11 more)

### Community 59 - "NES Bus"
Cohesion: 0.10
Nodes (20): Bus, cart, clock, controller, controller_state, controller_strobe, cpu, cpuClock (+12 more)

### Community 60 - "OpenCalc UI"
Cohesion: 0.11
Nodes (19): board_key_t, board_keypad_key_at(), active_game_press_button_number(), digit_for_key(), opencalc_ui_handle_serial_buttons(), opencalc_ui_press_button_number(), script_editor_clear(), script_editor_cursor_line_col() (+11 more)

### Community 61 - "NES Mapper"
Cohesion: 0.11
Nodes (19): Bank, bank_id, bank_ptr, last_used, size, BankCache, banks, cart (+11 more)

### Community 62 - "NES Ppu2C02"
Cohesion: 0.16
Nodes (17): IRAM_ATTR, clearVBlank, connectFramebuffer, cpuRead, cpuWrite, fakeSpriteHit, finishScanline, incrementY (+9 more)

### Community 63 - "Opencalc Doom"
Cohesion: 0.18
Nodes (13): DG_GetKey(), doom_clear_input_state(), doom_key_for_matrix_position(), doom_key_push(), doom_key_queue_empty(), doom_key_queue_full(), doom_next_weapon_key(), doom_poll_keypad() (+5 more)

### Community 64 - "OpenCalc UI"
Cohesion: 0.15
Nodes (18): delete_selected_script(), open_scripts_browser(), open_scripts_browser_for(), perform_selected_script_action(), script_editor_open_selected(), script_editor_save(), script_path_for_name(), scripts_scan() (+10 more)

### Community 65 - "Doom I Allegromusic"
Cohesion: 0.14
Nodes (6): boolean, byte, I_Allegro_InitMusic(), I_Allegro_MusicIsPlaying(), I_Allegro_PlaySong(), IsMid()

### Community 66 - "Doom Memio"
Cohesion: 0.24
Nodes (17): ConvertMus(), I_Allegro_RegisterSong(), byte, ConvertMus(), I_SDL_RegisterSong(), IsMid(), M_WriteFile(), MEMFILE (+9 more)

### Community 67 - "Doom Mus2Mid"
Cohesion: 0.43
Nodes (17): mem_fwrite(), AllocateMIDIChannel(), boolean, byte, MEMFILE, GetMIDIChannel(), mus2mid(), ReadMusHeader() (+9 more)

### Community 68 - "Tetris Game"
Cohesion: 0.25
Nodes (16): border(), tetris_piece_t, draw_block(), draw_ghost(), draw_mini_piece(), draw_panel_box(), draw_tetris_frame(), font_for() (+8 more)

### Community 69 - "OpenCalc UI"
Cohesion: 0.20
Nodes (18): display_simple_fraction(), expression_find_frac_parts(), expression_find_matching_paren(), expression_find_nroot_parts(), expression_fraction_move_left(), expression_fraction_move_right(), font_for(), ui_calc_char_width() (+10 more)

### Community 70 - "Doom W Wad"
Cohesion: 0.17
Nodes (12): P_InitSlidingDoorFrames(), P_Init(), P_InitPicAnims(), P_InitSwitchList(), R_CheckTextureNumForName(), R_FlatNumForName(), R_TextureNumForName(), R_InitSprites() (+4 more)

### Community 71 - "NES Cpu6502"
Cohesion: 0.14
Nodes (17): fetch, Instr_ASL, Instr_BRK, Instr_DEC, Instr_INC, Instr_JSR, Instr_LSR, Instr_PHA (+9 more)

### Community 72 - "Doom P Switch"
Cohesion: 0.17
Nodes (7): bwhere_e, boolean, line_t, mobj_t, P_ChangeSwitchTexture(), P_StartButton(), P_UseSpecialLine()

### Community 73 - "Doom I Joystick"
Cohesion: 0.18
Nodes (12): event_t, D_PopEvent(), D_PostEvent(), D_ProcessEvents(), boolean, GetAxisState(), GetButtonsState(), I_InitJoystick() (+4 more)

### Community 74 - "Doom F Finale"
Cohesion: 0.22
Nodes (15): boolean, event_t, patch_t, F_ArtScreenDrawer(), F_BunnyScroll(), F_CastDrawer(), F_CastPrint(), F_CastResponder() (+7 more)

### Community 75 - "Doom M Controls"
Cohesion: 0.30
Nodes (14): D_BindVariables(), I_BindJoystickVariables(), I_BindSoundVariables(), M_BindVariable(), M_ApplyPlatformDefaults(), M_BindBaseControls(), M_BindChatControls(), M_BindHereticControls() (+6 more)

### Community 76 - "Doom Doomgeneric Win"
Cohesion: 0.15
Nodes (8): addKeyToQueue(), convertToDoomKey(), wndProc(), HWND, LPARAM, LRESULT, UINT, WPARAM

### Community 77 - "Doom I Allegrosound"
Cohesion: 0.23
Nodes (9): boolean, sfxinfo_t, CacheSFX(), GetSfxLumpName(), I_Allegro_GetSfxLumpNum(), I_Allegro_InitSound(), I_Allegro_PrecacheSounds(), I_Allegro_SoundIsPlaying() (+1 more)

### Community 78 - "NES Bus"
Cohesion: 0.21
Nodes (13): connectFramebuffer, getPPUMirrorMode, reset, IRAM_ATTR, MIRROR, uint8_t Bus::cpuRead(), void Bus::clock(), void Bus::cpuWrite() (+5 more)

### Community 79 - "NES Apu2A03"
Cohesion: 0.18
Nodes (4): Apu2A03, Bus, Cpu6502, Bus

### Community 81 - "Doom Doomgeneric Soso"
Cohesion: 0.23
Nodes (6): addKeyToQueue(), convertToDoomKey(), DG_DrawFrame(), DG_Init(), enableRawMode(), handleKeyInput()

### Community 82 - "Doom M Menu"
Cohesion: 0.20
Nodes (12): G_LoadGame(), M_LoadSelect(), M_QuickLoadResponse(), M_SaveSelect(), boolean, M_StringConcat(), M_StringCopy(), M_StringEndsWith() (+4 more)

### Community 83 - "Doom D Mode"
Cohesion: 0.38
Nodes (10): boolean, GameMission_t, GameMode_t, D_GameMissionString(), D_GetNumEpisodes(), D_IsEpisodeMap(), D_ValidEpisodeMap(), D_ValidGameMode() (+2 more)

### Community 84 - "Doom Doomgeneric Linuxvt"
Cohesion: 0.27
Nodes (7): addKeyToQueue(), checkInputDevs(), checkKeys(), convertToDoomKey(), DG_DrawFrame(), DG_GetKey(), DG_Init()

### Community 85 - "Doom Doomgeneric Sosox"
Cohesion: 0.24
Nodes (5): add_key_to_queue(), convert_to_doom_keyx(), DG_DrawFrame(), DG_Init(), enable_raw_mode()

### Community 86 - "Doom Gusconf"
Cohesion: 0.38
Nodes (10): boolean, FreeDMXConfig(), GUS_WriteConfig(), MappingIndex(), ParseDMXConfig(), ParseLine(), ReadDMXConfig(), SplitLine() (+2 more)

### Community 88 - "Doom P Lights"
Cohesion: 0.24
Nodes (10): G_SecretExitLevel(), line_t, EV_BuildStairs(), line_t, EV_LightTurnOn(), EV_StartLightStrobing(), EV_TurnTagLightsOff(), mobj_t (+2 more)

### Community 89 - "Doom I Sdlmusic"
Cohesion: 0.44
Nodes (10): FILE, ParseFlacFile(), ParseFlacStreaminfo(), ParseOggFile(), ParseOggIdHeader(), ParseVorbisComment(), ParseVorbisComments(), ParseVorbisTime() (+2 more)

### Community 90 - "Snake Game"
Cohesion: 0.44
Nodes (9): snake_cell_has_body(), snake_init(), snake_interval_ms(), snake_place_food(), snake_rand(), snake_set_direction(), snake_step(), snake_toggle_pause() (+1 more)

### Community 91 - "Doom P Doors"
Cohesion: 0.47
Nodes (8): line_t, mobj_t, EV_DoDoor(), EV_DoLockedDoor(), EV_SlidingDoor(), EV_VerticalDoor(), P_FindSlidingDoorType(), vldoor_e

### Community 92 - "Doom P User"
Cohesion: 0.47
Nodes (8): angle_t, fixed_t, player_t, P_CalcHeight(), P_DeathThink(), P_MovePlayer(), P_PlayerThink(), P_Thrust()

### Community 93 - "Sd"
Cohesion: 0.29
Nodes (3): isKeyboard(), loadState, saveState

### Community 94 - "Doom M Bbox"
Cohesion: 0.47
Nodes (4): fixed_t, M_AddToBox(), M_ClearBox(), P_GroupLines()

### Community 96 - "Doom P Enemy"
Cohesion: 0.60
Nodes (5): A_CloseShotgun2(), A_LoadShotgun2(), A_OpenShotgun2(), player_t, pspdef_t

### Community 98 - "NES Cpu6502"
Cohesion: 0.40
Nodes (5): IRAM_ATTR, apuWrite, clock, OAM_DMA, OAM_Write

### Community 99 - "NES Ppu2C02"
Cohesion: 0.40
Nodes (5): Cartridge, MIRROR, connectCartridge, getMirror, setMirror

### Community 100 - "Cmakelists"
Cohesion: 0.40
Nodes (5): Doomgeneric Sources, idf_component_register, Mario Emulator Sources, esp_lcd_ili9341 Dependency, esp_tinyusb Dependency

### Community 101 - "Doom Wi Stuff"
Cohesion: 0.67
Nodes (3): boolean, event_t, WI_Responder()

### Community 102 - "Python Runtime"
Cohesion: 0.67
Nodes (3): OpenCalc OS Integration, py_run_file, tiny-python

## Knowledge Gaps
- **156 isolated node(s):** `fp_`, `cpu`, `ppu`, `cart`, `RAM` (+151 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **3 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `I_Error()` connect `Doom R Draw` to `Doom Save System`, `Doom Cheats Text`, `Doom Enemy Logic`, `Doom Demo Loop`, `Doom Automap`, `Doom Collision`, `Doom Network Loop`, `Doom Sound Stubs`, `Doom Startup IWAD`, `Doom Intermission`, `Doom Combat Logic`, `Doom R Bsp`, `Doom I Sdlsound`, `Doom V Video`, `Doom D Iwad`, `Doom M Misc`, `Doom I System`, `Doom I Sdlmusic`, `Doom P Plats`, `Doom I Video`, `Doom P Setup`, `Doom M Menu`, `Doom R Data`, `Doom M Config`, `Doom P Spec`, `Doom R Things`, `Doom P Lights`, `Doom W Wad`, `Doom P Switch`, `Doom Doomgeneric Linuxvt`, `Doom P Doors`?**
  _High betweenness centrality (0.110) - this node is a cross-community bridge._
- **Why does `isKeyboard()` connect `Sd` to `Doom Doomgeneric Linuxvt`?**
  _High betweenness centrality (0.062) - this node is a cross-community bridge._
- **Why does `File` connect `NES Mapper Cache` to `NES Cartridge Bus`, `NES 6502 CPU`, `NES CPU State`, `Sd`, `Sd`?**
  _High betweenness centrality (0.054) - this node is a cross-community bridge._
- **Are the 103 inferred relationships involving `I_Error()` (e.g. with `D_FindIWAD()` and `BlockUntilStart()`) actually correct?**
  _`I_Error()` has 103 INFERRED edges - model-reasoned connections that need verification._
- **Are the 78 inferred relationships involving `S_StartSound()` (e.g. with `F_BunnyScroll()` and `F_CastResponder()`) actually correct?**
  _`S_StartSound()` has 78 INFERRED edges - model-reasoned connections that need verification._
- **Are the 60 inferred relationships involving `Z_Malloc()` (e.g. with `GetGameName()` and `wipe_EndScreen()`) actually correct?**
  _`Z_Malloc()` has 60 INFERRED edges - model-reasoned connections that need verification._
- **What connects `fp_`, `cpu`, `ppu` to the rest of the system?**
  _156 weakly-connected nodes found - possible documentation gaps or missing edges._