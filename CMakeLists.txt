cmake_minimum_required(VERSION 3.20)
project(SIMIA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

find_package(raylib QUIET)

if(NOT raylib_FOUND)
    message(STATUS "raylib not found on system, fetching from GitHub...")

    FetchContent_Declare(
            raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 5.0
    )
    # This will add a 'raylib' target we can link against
    FetchContent_MakeAvailable(raylib)
endif()

add_executable(SIMIA
        main.cpp
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/rlImGui.cpp
)

target_include_directories(SIMIA
        PRIVATE
        dependencies
        dependencies/include
        dependencies/include/extras
)

target_link_libraries(SIMIA
        PRIVATE
        raylib
)
