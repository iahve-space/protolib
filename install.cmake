include_guard(GLOBAL)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if (NOT DEFINED SOFTWARE_VERSION)
    message(FATAL_ERROR "SOFTWARE_VERSION is not defined before including install.cmake")
endif ()

set(PROTOLIB_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/protolib-${SOFTWARE_VERSION}")

set(PROTOLIB_TARGETS
        protolib_interfaces
        protolib_crc_soft
        protolib_crc16_modbus
        protolib_crc
        protolib_fields
        protolib_containers
        lacte_protocol
        fs_tools
)

install(TARGETS ${PROTOLIB_TARGETS}
        EXPORT protolibTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Если каталог называется 'Include', поправь здесь
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

install(EXPORT protolibTargets
        NAMESPACE protolib::
        DESTINATION ${PROTOLIB_CMAKE_DIR}
        FILE protolibTargets.cmake
)

set(_PROTOLIB_BUILD_EXPORT_FILE "${CMAKE_BINARY_DIR}/protolibTargets.cmake")
if (NOT PROTOLIB_EXPORT_DONE)
    export(EXPORT protolibTargets
            NAMESPACE protolib::
            FILE "${_PROTOLIB_BUILD_EXPORT_FILE}")
    set(PROTOLIB_EXPORT_DONE TRUE CACHE INTERNAL "protolib export() already called")
endif ()

write_basic_package_version_file(
        "${CMAKE_BINARY_DIR}/protolibConfigVersion.cmake"
        VERSION "${SOFTWARE_VERSION}"
        COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
        "${CMAKE_SOURCE_DIR}/cmake/protolibConfig.cmake.in"
        "${CMAKE_BINARY_DIR}/protolibConfig.cmake"
        INSTALL_DESTINATION "${PROTOLIB_CMAKE_DIR}"
)

install(FILES
        "${CMAKE_BINARY_DIR}/protolibConfig.cmake"
        "${CMAKE_BINARY_DIR}/protolibConfigVersion.cmake"
        DESTINATION "${PROTOLIB_CMAKE_DIR}"
)