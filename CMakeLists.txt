cmake_minimum_required(VERSION 3.16)
project(linux_native_camera_tweaks C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)

# SDL2's headers only: bg3le hands the plugin SDL's functions.
find_package(SDL2 REQUIRED)

file(GLOB_RECURSE SOURCES "src/*.c" "src/*.h")
add_library(${PROJECT_NAME} SHARED ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

target_include_directories(${PROJECT_NAME} PRIVATE src ${SDL2_INCLUDE_DIRS})
target_compile_definitions(${PROJECT_NAME} PRIVATE _GNU_SOURCE)
