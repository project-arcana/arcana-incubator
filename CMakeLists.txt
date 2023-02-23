cmake_minimum_required(VERSION 3.8)
project(ArcanaIncubator)

if (NOT TARGET clean-core)
    message(FATAL_ERROR "[arcana-incubator] clean-core must be available")
endif()

option(INC_ENABLE_IMGUI_FREETYPE "Use FreeType instead of stb_truetype for ImGui fonts" OFF)
option(INC_ENABLE_IMGUI_PHI_BINDLESS "Use bindless textures in the ImGui PHI Backend" OFF)

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

if (MSVC)
    # For device_tentative
    target_link_libraries(arcana-incubator PRIVATE "Shcore.lib")
endif()

if (INC_ENABLE_IMGUI_FREETYPE)
	if (NOT TARGET libfreetype)
		message(FATAL_ERROR "[Arcana Incubator] When enabling INC_ENABLE_IMGUI_FREETYPE, a target 'libfreetype' must be available")
	else()
		target_link_libraries(arcana-incubator PUBLIC libfreetype)
		target_compile_definitions(arcana-incubator PUBLIC IMGUI_ENABLE_FREETYPE)
	endif()
endif()

if (INC_ENABLE_IMGUI_PHI_BINDLESS)
	target_compile_definitions(arcana-incubator PUBLIC INC_ENABLE_IMGUI_PHI_BINDLESS=1)
endif()
