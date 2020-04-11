cmake_minimum_required(VERSION 3.8)
project(ArcanaIncubator)

if (NOT TARGET clean-core)
    message(FATAL_ERROR "[arcana-incubator] clean-core must be available")
endif()

# =========================================
# define library

file(GLOB_RECURSE SOURCES "src/*.cc" "src/*.c")
file(GLOB_RECURSE HEADERS "src/*.hh" "src/*.h")

arcana_source_group(SOURCES)

add_library(arcana-incubator STATIC ${SOURCES} ${HEADERS})

target_include_directories(arcana-incubator PUBLIC src/)

target_link_libraries(arcana-incubator PUBLIC
    clean-core
    typed-geometry
    reflector
    phantasm-hardware-interface
    phantasm-renderer
    rich-log
    sdl2
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
