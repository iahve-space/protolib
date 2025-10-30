add_library(lacte_protocol STATIC Ymodem.cpp)
add_library(protolib::lacte_protocol ALIAS lacte_protocol)

target_link_libraries(lacte_protocol PUBLIC
        protolib::containers
        protolib::crc16_modbus
        fs_tools::fs_tools
)

target_include_directories(lacte_protocol PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/protocols/lacte
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h"
        PATTERN "tests" EXCLUDE)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
