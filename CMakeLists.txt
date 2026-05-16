cmake_minimum_required(VERSION 3.21)
project(wolf3d C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)
option(WOLF3D_FETCH_SDL3 "Fetch SDL3 when not found by find_package" ON)
set(SDL3_MIN_VERSION 3.2.10 CACHE STRING "Minimum SDL3 version")

find_package(SDL3 ${SDL3_MIN_VERSION} QUIET CONFIG)
if (NOT TARGET SDL3::SDL3)
    if (NOT WOLF3D_FETCH_SDL3)
        message(STATUS "SDL3 package not found; enabling SDL3 fetch fallback")
    endif()
    if (WOLF3D_FETCH_SDL3)
        message(STATUS "Fetching SDL3 from upstream (release ${SDL3_MIN_VERSION})")
        FetchContent_Declare(
            SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-${SDL3_MIN_VERSION}
            GIT_SHALLOW TRUE
        )
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(SDL3)
    else()
        message(FATAL_ERROR
            "SDL3 not found. Set -DWOLF3D_FETCH_SDL3=ON to fetch SDL3, "
            "or install SDL3 and point CMake at it."
        )
    endif()
endif()

file(GLOB WOLF3D_SOURCES src/*.c)
file(GLOB WOLF3D_HEADERS src/*.h)

add_executable(wolf3d ${WOLF3D_SOURCES})

target_include_directories(wolf3d PRIVATE src)
target_link_libraries(wolf3d PRIVATE SDL3::SDL3)
if (UNIX)
    target_link_libraries(wolf3d PRIVATE m)
endif()

if(MSVC)
    target_compile_options(wolf3d PRIVATE /W4)
    target_compile_definitions(wolf3d PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
    if(WOLF3D_ASAN)
        target_compile_options(wolf3d PRIVATE /fsanitize=address /Zi)
        target_link_options(wolf3d PRIVATE /INCREMENTAL:NO /DEBUG)
    endif()
else()
    target_compile_options(wolf3d PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Copy assets directory to build output
add_custom_command(TARGET wolf3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:wolf3d>/assets
)

# Copy the SDL3 shared library next to the executable so it runs in place
add_custom_command(TARGET wolf3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:wolf3d>
)
