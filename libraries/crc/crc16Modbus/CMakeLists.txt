file(GLOB_RECURSE CRC_SOURCE_LIST "${CMAKE_CURRENT_LIST_DIR}/*.c" "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_library(protolib_crc16_modbus STATIC ${CRC_SOURCE_LIST})
add_library(protolib::crc16_modbus ALIAS protolib_crc16_modbus)
set_target_properties(protolib_crc16_modbus PROPERTIES EXPORT_NAME crc16_modbus)

#target_link_libraries(protolib_crc16_modbus PUBLIC protolib::headers)

target_include_directories(protolib_crc16_modbus PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<INSTALL_INTERFACE:include>)


install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libraries/crc/crc16Modbus
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h"
        PATTERN "Tests" EXCLUDE)