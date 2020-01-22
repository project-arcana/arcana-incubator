cmake_minimum_required(VERSION 3.8)
project(ArcanaIncubator)

# =========================================
# find SDL2

find_package(SDL2 REQUIRED)

# =========================================
# define library

file(GLOB_RECURSE SOURCES "src/*.cc" "src/*.hh")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${SOURCES})

add_library(arcana-incubator STATIC ${SOURCES})

target_include_directories(arcana-incubator PUBLIC src/ ${SDL2_INCLUDE_DIRS})

target_link_libraries(arcana-incubator PUBLIC
    clean-core
    typed-geometry
    reflector
    phantasm-hardware-interface
    rich-log
    ${SDL2_LIBRARIES}
)

# =========================================
# set up compile flags

if (MSVC)
    target_compile_options(arcana-incubator PUBLIC /MP)
else()
    target_compile_options(arcana-incubator PRIVATE -Wall -fPIC)
    target_link_libraries(arcana-incubator PUBLIC -fuse-ld=gold -lX11)
endif()

if (MSVC)
    # For device_tentative
    target_link_libraries(arcana-incubator PRIVATE "Shcore.lib")
endif()
