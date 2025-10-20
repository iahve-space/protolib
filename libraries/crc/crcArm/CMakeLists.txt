file(GLOB_RECURSE CRC_SOURCE_LIST "${CMAKE_CURRENT_LIST_DIR}/*.c" "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_library(CRC_ARM STATIC ${CRC_SOURCE_LIST} )
target_include_directories(CRC_ARM PUBLIC . .)
target_link_libraries(CRC_ARM HAL_STM32H7 DRIVERS )