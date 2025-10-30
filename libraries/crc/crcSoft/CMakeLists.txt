file(GLOB_RECURSE CRC_SOURCE_LIST "${CMAKE_CURRENT_LIST_DIR}/*.c" "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_library(protolib_crc_soft STATIC ${CRC_SOURCE_LIST} )
add_library(protolib::crc_soft ALIAS protolib_crc_soft)
set_target_properties(protolib_crc_soft PROPERTIES EXPORT_NAME crc_soft)

target_link_libraries(protolib_crc_soft )


target_include_directories(protolib_crc_soft PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<INSTALL_INTERFACE:include>)


install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libraries/crc/crcSoft
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h"
        PATTERN "Tests" EXCLUDE)