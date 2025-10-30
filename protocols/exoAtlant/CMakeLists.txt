add_library(exoAtlantProtocol INTERFACE)

# Публичные зависимости
target_link_libraries(exoAtlantProtocol INTERFACE
        protolib::containers
        protolib::crc16_modbus
)

target_include_directories(exoAtlantProtocol INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
