file(GLOB GIAC_CANDIDATES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.c"
)

set(GIAC_SOURCES)
foreach(source IN LISTS GIAC_CANDIDATES)
    get_filename_component(base "${source}" NAME)
    if(base MATCHES "^(kdisplay|k_|fx|ti|Xcas|Fl_|Graph|hist)" OR
       base STREQUAL "test.cpp")
        continue()
    endif()
    list(APPEND GIAC_SOURCES "${source}")
endforeach()

idf_component_register(
    SRCS ${GIAC_SOURCES}
    INCLUDE_DIRS "." "src"
    REQUIRES libtommath esp_psram heap
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    HAVE_CONFIG_H
    GIAC_KHICAS
    NO_GUI
    GIAC_GENERIC
    EMBEDDED
    UMAP
    DOUBLEVAL
    NO_BSD
    BOARD_HAS_PSRAM
    USE_GMP_REPLACEMENTS
    NO_CLOCK
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O1
    -w
    -fexceptions
    -ffunction-sections
    -fdata-sections
)
