# CMakeLists.txt этого модуля (Field)
include(GNUInstallDirs)

add_library(protolib_fields INTERFACE
        FieldFlags.hpp)
add_library(protolib::fields ALIAS protolib_fields)
set_target_properties(protolib_fields PROPERTIES EXPORT_NAME fields)


target_include_directories(protolib_fields INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<INSTALL_INTERFACE:include>  # совпадает с FILE_SET install
)

# Install headers for this component (exclude tests)
install(DIRECTORY .
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/prototypes/field
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h"
        PATTERN "Tests" EXCLUDE)

if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()