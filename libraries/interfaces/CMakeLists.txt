add_library(protolib_interfaces STATIC
        UartLinux.cpp
        Echo.cpp)
add_library(protolib::interfaces ALIAS protolib_interfaces)

find_package(Threads REQUIRED)

# Если кросс-тулчейн/Yocto чудит, добавь страховку:
target_compile_options(protolib_interfaces PUBLIC -pthread)
target_link_options(protolib_interfaces PUBLIC -pthread)

target_link_libraries(protolib_interfaces PUBLIC Threads::Threads fs_tools)
set_target_properties(protolib_interfaces PROPERTIES EXPORT_NAME interfaces)

target_include_directories(protolib_interfaces PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<INSTALL_INTERFACE:include>)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libraries/interfaces
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h"
        PATTERN "Tests" EXCLUDE)