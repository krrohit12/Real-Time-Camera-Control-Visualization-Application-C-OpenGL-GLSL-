cmake_minimum_required(VERSION 3.20)
project(CameraApp LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Output directories ────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ── Platform detection ────────────────────────────────────────────────────────
if(APPLE)
    set(MACOS TRUE)
endif()

# ── Find system dependencies ──────────────────────────────────────────────────
find_package(OpenCV REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)

# Prefer pkg-config for GLFW on Linux if find_package fails
if(NOT glfw3_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
endif()

# ── GLAD (header-only + single .c file, vendored) ────────────────────────────
# We use FetchContent to pull a minimal GLAD loader rather than requiring
# a system-installed copy.
include(FetchContent)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glad)

# ── Dear ImGui (vendored source) ──────────────────────────────────────────────
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

# ── Application sources ───────────────────────────────────────────────────────
set(APP_SOURCES
    src/main.cpp
    src/Application.cpp
    src/CameraManager.cpp
    src/Renderer.cpp
    src/ShaderManager.cpp
    src/ControlPanel.cpp
    src/SnapshotManager.cpp
    src/RecordingManager.cpp
)


if(MACOS)
    set(MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist.in")
    add_executable(CameraApp MACOSX_BUNDLE ${APP_SOURCES})
    set_target_properties(CameraApp PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist.in"
    )
else()
    add_executable(CameraApp ${APP_SOURCES})
endif()

target_include_directories(CameraApp PRIVATE
    src/
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(CameraApp PRIVATE
    glad
    imgui
    glfw
    OpenGL::GL
    ${OpenCV_LIBS}
)

# macOS: link required frameworks
if(MACOS)
    target_link_libraries(CameraApp PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework AVFoundation"
    )
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(CameraApp PRIVATE /W4)
else()
    target_compile_options(CameraApp PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Copy shaders next to the executable ──────────────────────────────────────
if(MACOS)
    # For .app bundle: shaders go inside Contents/MacOS/shaders
    add_custom_command(TARGET CameraApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/shaders"
            "$<TARGET_FILE_DIR:CameraApp>/shaders"
        COMMENT "Copying shaders into app bundle"
    )
else()
    add_custom_command(TARGET CameraApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/shaders"
            "$<TARGET_FILE_DIR:CameraApp>/shaders"
        COMMENT "Copying shaders to output directory"
    )
endif()

message(STATUS "OpenCV version : ${OpenCV_VERSION}")
message(STATUS "Build type     : ${CMAKE_BUILD_TYPE}")
message(STATUS "Output dir     : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
