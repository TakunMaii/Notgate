cmake_minimum_required(VERSION 3.15)
project(MyRaylibTemplate LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 可选：让 Raylib 使用 OpenGL ES（移动端常用）
# set(GRAPHICS_API "GRAPHICS_API_OPENGL_ES2")

option(BUILD_EXAMPLES "Build raylib examples" OFF)
option(BUILD_GAMES    "Build raylib games" OFF)

add_subdirectory(libs/raylib)

add_executable(main
    src/main.c
)

if (WIN32)
    target_sources(main PRIVATE app.rc)
    set_target_properties(main PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if (EMSCRIPTEN)
    set_target_properties(main PROPERTIES SUFFIX ".html" OUTPUT_NAME "index")
    target_link_options(main PRIVATE
        "-sALLOW_MEMORY_GROWTH=1"
        "-sASYNCIFY"
        "-sFORCE_FILESYSTEM=1"
        "--preload-file" "${CMAKE_SOURCE_DIR}/assets@/assets"
    )
endif()

target_link_libraries(main PRIVATE raylib)

