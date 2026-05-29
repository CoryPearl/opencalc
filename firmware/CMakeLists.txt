# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(OPENCALC_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/main/config.h")

file(READ "${OPENCALC_CONFIG_FILE}" OPENCALC_CONFIG_H)

function(opencalc_read_define OUT_VAR DEFINE_NAME DEFAULT_VALUE)
    string(REGEX MATCH "#define[ \t]+${DEFINE_NAME}[ \t]+([^ \t\r\n/]+)" _match "${OPENCALC_CONFIG_H}")
    if(_match)
        set(${OUT_VAR} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    else()
        set(${OUT_VAR} "${DEFAULT_VALUE}" PARENT_SCOPE)
    endif()
endfunction()

opencalc_read_define(OPENCALC_FACTORY_APP_PARTITION_SIZE OPENCALC_FACTORY_APP_PARTITION_SIZE 0x400000)
opencalc_read_define(OPENCALC_STORAGE_PARTITION_SIZE OPENCALC_STORAGE_PARTITION_SIZE 0x800000)

set(OPENCALC_FACTORY_APP_PARTITION_OFFSET 0x10000)
math(EXPR OPENCALC_STORAGE_PARTITION_OFFSET
    "${OPENCALC_FACTORY_APP_PARTITION_OFFSET} + ${OPENCALC_FACTORY_APP_PARTITION_SIZE}"
    OUTPUT_FORMAT HEXADECIMAL
)

file(WRITE "${CMAKE_CURRENT_LIST_DIR}/partitions.csv"
"# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000
phy_init, data, phy,     0xe000,  0x1000
factory,  app,  factory, ${OPENCALC_FACTORY_APP_PARTITION_OFFSET}, ${OPENCALC_FACTORY_APP_PARTITION_SIZE}
storage,  data, fat,     ${OPENCALC_STORAGE_PARTITION_OFFSET}, ${OPENCALC_STORAGE_PARTITION_SIZE}
")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(opencalc)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${OPENCALC_CONFIG_FILE}"
)

set(OPENCALC_STORAGE_IMAGE_DIR "${CMAKE_CURRENT_LIST_DIR}/storage_image")

fatfs_create_spiflash_image(storage "${OPENCALC_STORAGE_IMAGE_DIR}" FLASH_IN_PROJECT)
