cmake_minimum_required(VERSION 3.24)
project(SpongebobGame LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(SDL2 QUIET CONFIG)


if(NOT SDL2_FOUND)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL2
        URL https://codeload.github.com/libsdl-org/SDL/tar.gz/refs/tags/release-2.32.10
        URL_HASH SHA256=03f9d7c191a837525c9cda6406af2f2e48be02b5e7eb03d949cc9f1e9ca41c8b
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )
    FetchContent_MakeAvailable(SDL2)
endif()

add_executable(SpongebobGame main.cpp)
if(TARGET SDL2::SDL2main)
    target_link_libraries(SpongebobGame PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(SpongebobGame PRIVATE SDL2::SDL2 OpenGL::GL)
set_target_properties(SpongebobGame PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>"
)

target_include_directories(SpongebobGame PRIVATE external)

add_custom_command(TARGET SpongebobGame POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:SpongebobGame>/assets"
    COMMENT "Kopiere Hintergrundbilder"
)

add_custom_target(run
    COMMAND $<TARGET_FILE:SpongebobGame>
    DEPENDS SpongebobGame
    USES_TERMINAL
)

