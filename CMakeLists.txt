cmake_minimum_required(VERSION 3.24)
project(SpongebobGame LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
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

find_package(assimp 6 QUIET CONFIG)

if(NOT TARGET assimp::assimp)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)

    FetchContent_Declare(assimp
        URL https://codeload.github.com/assimp/assimp/tar.gz/refs/tags/v6.0.5
        URL_HASH SHA256=edf3749559c2b7d1f758ffb66fc5bec62186221e623b7f2e8969f17ee46ecb6f
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )
    FetchContent_MakeAvailable(assimp)
endif()

find_package(ALSA)
find_package(PkgConfig)
if(NOT ALSA_FOUND OR NOT PKG_CONFIG_FOUND)
    message(FATAL_ERROR "ALSA-Entwicklungsdateien oder pkg-config fehlen (JUCE braucht beides). Bitte installieren: sudo apt install libasound2-dev pkg-config")
endif()
set(JUCE_MODULES_ONLY ON CACHE BOOL "" FORCE)
FetchContent_Declare(JUCE
    URL https://codeload.github.com/juce-framework/JUCE/tar.gz/refs/tags/9.0.2
    URL_HASH SHA256=16d01c27e8327f3644306cc5c223aa6e21af1cc1ec62faa709fc0dced57d208a
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(imgui
    URL https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.92.9b
    URL_HASH SHA256=21d8a0a565e85dce943e375db00812c2f3f0ab21f3f0f7964e364a63422d7f99
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(imgui)
add_library(game_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(game_imgui PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
target_link_libraries(game_imgui PUBLIC SDL2::SDL2 PRIVATE ${CMAKE_DL_LIBS})

add_library(game_music STATIC music.cpp)
target_include_directories(game_music PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(game_music PRIVATE juce::juce_audio_devices juce::juce_audio_formats)
target_compile_definitions(game_music PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_JACK=0
    JUCE_ALSA=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_MODAL_LOOPS_PERMITTED=1
)

add_executable(SpongebobGame main.cpp stb_image.cpp navigationeditor.cpp)
target_link_libraries(SpongebobGame PRIVATE SDL2::SDL2main SDL2::SDL2 OpenGL::GL GLEW::GLEW glm::glm assimp::assimp game_imgui game_music)
target_compile_definitions(SpongebobGame PRIVATE
    SPONGEBOB_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/"
)
set_target_properties(SpongebobGame PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

target_include_directories(SpongebobGame PRIVATE external)
