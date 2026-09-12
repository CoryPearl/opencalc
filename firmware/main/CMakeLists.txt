file(READ "${CMAKE_CURRENT_LIST_DIR}/config.h" OPENCALC_CONFIG_H)
string(REGEX MATCH "#define[ \t]+OPENCALC_ENABLE_GIAC_CAS[ \t]+([^ \t\r\n/]+)" _giac_match "${OPENCALC_CONFIG_H}")
set(OPENCALC_GIAC_ENABLED "${CMAKE_MATCH_1}")

set(OPENCALC_SOURCES
        "main.c"
        "components/board_init.c"
        "components/SD.cpp"
        "components/opencalc_audio.c"
        "components/opencalc_breakout.c"
        "components/opencalc_calc.c"
        "components/opencalc_cas.c"
        "components/opencalc_cas_experience.c"
        "components/opencalc_conics.c"
        "components/opencalc_doom.c"
        "components/opencalc_eigenmath.c"
        "components/opencalc_graph_analysis.c"
        "components/opencalc_graph_controller.c"
        "components/opencalc_graph_symbolic.c"
        "components/opencalc_graph_background.c"
        "components/opencalc_graph_model.c"
        "components/opencalc_graph_series.c"
        "components/opencalc_inequality.c"
        "components/opencalc_mario.cpp"
        "components/opencalc_math.c"
        "components/opencalc_math_layout.c"
        "components/opencalc_persist.c"
        "components/opencalc_power.c"
        "components/opencalc_reference.c"
        "components/opencalc_sensor_hub.c"
        "components/opencalc_self_test.c"
        "components/opencalc_script_model.c"
        "components/opencalc_snake.c"
        "components/opencalc_stats.c"
        "components/opencalc_symbols.c"
        "components/opencalc_tetris.c"
        "components/opencalc_units.c"
        "components/opencalc_ui.c"
        "components/opencalc_ui_canvas.c"
        "components/opencalc_ui_navigation.c"
        "components/opencalc_ui_work.c"
        "components/opencalc_worksheet_model.c"
        "components/opencalc_workspace_io.c"
        "components/storage.c"
        "components/usb_msc.c"
        "components/tiny-python.c"
        "components/eigenmath/eigenmath.c"
        "components/mario/bus.cpp"
        "components/mario/apu2A03.cpp"
        "components/mario/cartridge.cpp"
        "components/mario/cpu6502.cpp"
        "components/mario/mapper.cpp"
        "components/mario/ppu2C02.cpp"
        "components/mario/mappers/mapper000.cpp"
        "components/doomgeneric/am_map.c"
        "components/doomgeneric/doomdef.c"
        "components/doomgeneric/doomgeneric.c"
        "components/doomgeneric/doomstat.c"
        "components/doomgeneric/dstrings.c"
        "components/doomgeneric/d_event.c"
        "components/doomgeneric/d_items.c"
        "components/doomgeneric/d_iwad.c"
        "components/doomgeneric/d_loop.c"
        "components/doomgeneric/d_main.c"
        "components/doomgeneric/d_mode.c"
        "components/doomgeneric/d_net.c"
        "components/doomgeneric/dummy.c"
        "components/doomgeneric/f_finale.c"
        "components/doomgeneric/f_wipe.c"
        "components/doomgeneric/g_game.c"
        "components/doomgeneric/hu_lib.c"
        "components/doomgeneric/hu_stuff.c"
        "components/doomgeneric/i_cdmus.c"
        "components/doomgeneric/i_endoom.c"
        "components/doomgeneric/i_input.c"
        "components/doomgeneric/i_joystick.c"
        "components/doomgeneric/i_scale.c"
        "components/doomgeneric/i_opencalc_sound.c"
        "components/doomgeneric/i_sound.c"
        "components/doomgeneric/i_system.c"
        "components/doomgeneric/i_timer.c"
        "components/doomgeneric/i_video.c"
        "components/doomgeneric/info.c"
        "components/doomgeneric/m_argv.c"
        "components/doomgeneric/m_bbox.c"
        "components/doomgeneric/m_cheat.c"
        "components/doomgeneric/m_config.c"
        "components/doomgeneric/m_controls.c"
        "components/doomgeneric/m_fixed.c"
        "components/doomgeneric/m_menu.c"
        "components/doomgeneric/m_misc.c"
        "components/doomgeneric/m_random.c"
        "components/doomgeneric/memio.c"
        "components/doomgeneric/p_ceilng.c"
        "components/doomgeneric/p_doors.c"
        "components/doomgeneric/p_enemy.c"
        "components/doomgeneric/p_floor.c"
        "components/doomgeneric/p_inter.c"
        "components/doomgeneric/p_lights.c"
        "components/doomgeneric/p_map.c"
        "components/doomgeneric/p_maputl.c"
        "components/doomgeneric/p_mobj.c"
        "components/doomgeneric/p_plats.c"
        "components/doomgeneric/p_pspr.c"
        "components/doomgeneric/p_saveg.c"
        "components/doomgeneric/p_setup.c"
        "components/doomgeneric/p_sight.c"
        "components/doomgeneric/p_spec.c"
        "components/doomgeneric/p_switch.c"
        "components/doomgeneric/p_telept.c"
        "components/doomgeneric/p_tick.c"
        "components/doomgeneric/p_user.c"
        "components/doomgeneric/r_bsp.c"
        "components/doomgeneric/r_data.c"
        "components/doomgeneric/r_draw.c"
        "components/doomgeneric/r_main.c"
        "components/doomgeneric/r_plane.c"
        "components/doomgeneric/r_segs.c"
        "components/doomgeneric/r_sky.c"
        "components/doomgeneric/r_things.c"
        "components/doomgeneric/s_sound.c"
        "components/doomgeneric/sha1.c"
        "components/doomgeneric/sounds.c"
        "components/doomgeneric/statdump.c"
        "components/doomgeneric/st_lib.c"
        "components/doomgeneric/st_stuff.c"
        "components/doomgeneric/tables.c"
        "components/doomgeneric/v_video.c"
        "components/doomgeneric/w_checksum.c"
        "components/doomgeneric/w_file.c"
        "components/doomgeneric/w_file_stdc.c"
        "components/doomgeneric/w_main.c"
        "components/doomgeneric/w_wad.c"
        "components/doomgeneric/wi_stuff.c"
        "components/doomgeneric/z_zone.c"
)

set(OPENCALC_PRIVATE_REQUIRES
        driver
        esp_driver_ledc
        esp_driver_i2s
        esp_adc
        esp_timer
        esp_lcd
        esp_pm
        nvs_flash
        fatfs
        wear_levelling
        esp_tinyusb
)

if(OPENCALC_GIAC_ENABLED)
    list(APPEND OPENCALC_SOURCES
        "components/opencalc_giac.cpp"
        "components/opencalc_giac_alloc.cpp"
    )
    list(APPEND OPENCALC_PRIVATE_REQUIRES giac)
else()
    list(APPEND OPENCALC_SOURCES "components/opencalc_giac.cpp")
endif()

idf_component_register(
    SRCS ${OPENCALC_SOURCES}
    INCLUDE_DIRS
        "./components"
        "./components/doomgeneric"
    REQUIRES ${OPENCALC_PRIVATE_REQUIRES}
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    PY_MAX_PROGRAM=65536
    PY_MAX_TOKENS=8192
    PY_MAX_LINE=512
    PY_MAX_FUNCS=48
    PY_MAX_VARS=96
    PY_MAX_LOCALS=48
    PY_MAX_FUNC_BODY=2048
)
set_source_files_properties("components/eigenmath/eigenmath.c" PROPERTIES
    COMPILE_DEFINITIONS "OPENCALC_EIGENMATH_EMBEDDED=1"
)
if(OPENCALC_GIAC_ENABLED)
    # The vendored Giac headers predate GCC 15/C++20. Keep compatibility
    # diagnostics local to the adapter that includes them.
    set_source_files_properties("components/opencalc_giac.cpp" PROPERTIES
        COMPILE_DEFINITIONS "HAVE_CONFIG_H;GIAC_KHICAS;NO_GUI;GIAC_GENERIC;EMBEDDED;UMAP;DOUBLEVAL;NO_BSD;BOARD_HAS_PSRAM;NO_CLOCK"
        COMPILE_OPTIONS "-Wno-unknown-pragmas;-Wno-deprecated-enum-enum-conversion;-Wno-template-id-cdtor;-Wno-deprecated-declarations;-Wno-register;-Wno-reorder"
    )
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE DOOMGENERIC_RESX=320 DOOMGENERIC_RESY=200)
target_compile_definitions(${COMPONENT_LIB} PRIVATE COMPOSITE_VIDEO=1 OPENCALC_NONBLOCKING_DOOM_TICS=1)
set_source_files_properties("components/doomgeneric/am_map.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-missing-field-initializers"
)
set_source_files_properties("components/doomgeneric/d_main.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-unterminated-string-initialization"
)
set_source_files_properties(
    "components/doomgeneric/g_game.c"
    "components/doomgeneric/p_ceilng.c"
    "components/doomgeneric/p_floor.c"
    PROPERTIES
    COMPILE_OPTIONS "-Wno-implicit-fallthrough"
)
set_source_files_properties("components/doomgeneric/i_input.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-type-limits;-Wno-unused-const-variable"
)
set_source_files_properties("components/doomgeneric/i_sound.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-variable"
)
set_source_files_properties("components/doomgeneric/i_video.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-format;-Wno-unused-but-set-variable;-Wno-unused-const-variable"
)
set_source_files_properties("components/doomgeneric/m_menu.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-format-truncation;-Wno-unused-but-set-parameter"
)
set_source_files_properties(
    "components/doomgeneric/p_setup.c"
    "components/doomgeneric/wi_stuff.c"
    PROPERTIES
    COMPILE_OPTIONS "-Wno-format-truncation"
)
set_source_files_properties("components/doomgeneric/r_segs.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-absolute-value"
)
set_source_files_properties("components/doomgeneric/r_things.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-dangling-pointer"
)
set_source_files_properties("components/doomgeneric/statdump.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-const-variable"
)
set(OPENCALC_MARIO_VENDOR_SOURCES
    "components/mario/bus.cpp"
    "components/mario/apu2A03.cpp"
    "components/mario/cartridge.cpp"
    "components/mario/cpu6502.cpp"
    "components/mario/mapper.cpp"
    "components/mario/ppu2C02.cpp"
    "components/mario/mappers/mapper000.cpp"
)
set_source_files_properties(${OPENCALC_MARIO_VENDOR_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-parameter;-Wno-sign-compare"
)
set_source_files_properties("components/eigenmath/eigenmath.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-parameter;-Wno-sign-compare"
)
