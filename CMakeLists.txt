cmake_minimum_required(VERSION 3.8)
project(ArcanaIncubator)

if (NOT TARGET clean-core)
    message(FATAL_ERROR "[arcana-incubator] clean-core must be available")
endif()

# =========================================
# define library

# the nonstandard file extensions are for external libraries
file(GLOB_RECURSE SOURCES "src/*.cc" "src/*.c" "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.hh" "src/*.h")

arcana_add_library_no_unity(ARC_INC arcana-incubator SOURCES HEADERS)

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
    # For device_tentative
    target_link_libraries(arcana-incubator PRIVATE "Shcore.lib")
endif()
