include(GNUInstallDirs)

add_library(protolib_containers INTERFACE)
add_library(protolib::containers ALIAS protolib_containers)
set_target_properties(protolib_containers PROPERTIES EXPORT_NAME containers)

# Public dependencies
target_link_libraries(protolib_containers INTERFACE
        protolib::fields
        protolib::crc
        protolib::interfaces
)

# Public include path for consumers
# - build tree: current dir (so local headers are found)
# - install tree: <prefix>/<include>/protolib/containers
#   (we install headers there below)
target_include_directories(protolib_containers INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Install headers for this component (exclude tests)
install(DIRECTORY .
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/prototypes/container
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h"
        PATTERN "tests" EXCLUDE)

## Install the interface target into the project's export set
#install(TARGETS protolib_containers
#        EXPORT protolibTargets)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()

