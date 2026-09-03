file(GLOB TOMMATH_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.c")

idf_component_register(
    SRCS ${TOMMATH_SOURCES}
    INCLUDE_DIRS "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE -O1 -w)
