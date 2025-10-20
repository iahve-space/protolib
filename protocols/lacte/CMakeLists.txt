include(GNUInstallDirs)

add_library(LacteProtocol STATIC
        ymodem.cpp
)

set_target_properties(LacteProtocol PROPERTIES EXPORT_NAME lacte)

# Публичные зависимости
target_link_libraries(LacteProtocol PUBLIC
        protolib::fields
        protolib::containers
        protolib::crc16_modbus
        protolib::interfaces
)

target_include_directories(LacteProtocol PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
