add_subdirectory(crcSoft)
add_subdirectory(crc16Modbus)

add_library( protolib_crc INTERFACE)
add_library( protolib::crc ALIAS protolib_crc)
set_target_properties(protolib_crc PROPERTIES EXPORT_NAME crc)
target_link_libraries( protolib_crc INTERFACE protolib::crc16_modbus protolib::crc_soft)