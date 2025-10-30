cmake_minimum_required(VERSION 3.28)


# Если из CI не прилетело — по умолчанию 0.0.0
if (NOT DEFINED PROJECT_SEMVER)
    set(PROJECT_SEMVER "0.0.0")
endif ()

# "Красивая" строка версии для встраивания в бинарь/ресурсы
if (NOT DEFINED SOFTWARE_VERSION)
    set(SOFTWARE_VERSION "${PROJECT_SEMVER}")
endif ()

add_compile_definitions(SOFTWARE_VERSION="${SOFTWARE_VERSION}")


message(STATUS "PROJECT_SEMVER='${PROJECT_SEMVER}'")
message(STATUS "SOFTWARE_VERSION='${SOFTWARE_VERSION}'")


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(protolib VERSION ${PROJECT_SEMVER} LANGUAGES C CXX)

include(CTest)  # создаёт опцию BUILD_TESTING и если ON — вызовет enable_testing()

include(FetchContent)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0 # Or a specific commit hash
)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
        fs_tools
        QUIET
        GIT_REPOSITORY https://github.com/iahve-space/fs-tools.git
        GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(fs_tools)

include_directories(./include)
include_directories(./)

# Turn on strict warnings globally for GCC/Clang (C and C++)
option(STRICT_WARNINGS "Enable strict warnings and treat them as errors" ON)

add_compile_options(-Werror -Wextra)
add_subdirectory(include/prototypes)
add_subdirectory(libraries)
add_subdirectory(protocols)

add_library(protolib INTERFACE)
target_link_libraries(protolib INTERFACE
        protolib_interfaces
        protolib_crc_soft
        protolib_crc16_modbus
        protolib_crc
        protolib_fields
        protolib_containers)

target_include_directories(protolib INTERFACE ./include)

include(./install.cmake)
