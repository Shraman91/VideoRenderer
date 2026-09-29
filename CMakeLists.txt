cmake_minimum_required(VERSION 3.15)
project(ascii-art VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSYS2/MinGW specific flags
if(MINGW)
    add_compile_options(-Wall -Wextra -pedantic)
    # Ensure we link against mingw libraries properly
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

# Source files
set(SOURCES
    src/main.cpp
    src/image_renderer.cpp
    src/terminal.cpp
    src/charset.cpp
    src/demos/cube.cpp
    src/demos/donut.cpp
    src/demos/sphere.cpp
    src/demos/planet.cpp
    src/demos/demo_registry.cpp
)

# Add video renderer only if FFmpeg is available
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG QUIET libavformat libavcodec libswscale libavutil)
    if(FFMPEG_FOUND)
        message(STATUS "FFmpeg found - video support enabled")
        list(APPEND SOURCES src/video_renderer.cpp)
        add_compile_definitions(HAVE_FFMPEG)
    else()
        message(WARNING "FFmpeg not found - video support disabled. Install with: pacman -S mingw-w64-x86_64-ffmpeg")
    endif()
else()
    message(WARNING "pkg-config not found - video support disabled")
endif()

# Main executable
add_executable(ascii-art ${SOURCES})

# Link FFmpeg if available
if(FFMPEG_FOUND)
    target_include_directories(ascii-art PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_directories(ascii-art PRIVATE ${FFMPEG_LIBRARY_DIRS})
    target_link_libraries(ascii-art ${FFMPEG_LIBRARIES})
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(ascii-art)
else()
    target_link_libraries(ascii-art m pthread)
endif()

# Install target
install(TARGETS ascii-art DESTINATION bin)
