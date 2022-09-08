#pragma once

#include <arcana-incubator/imgui/lib/imgui.h>
// order relevant
#include <arcana-incubator/imgui/imguizmo/imguizmo.hh>

#include <phantasm-hardware-interface/fwd.hh>
#include <phantasm-renderer/fwd.hh>

struct SDL_Window;

namespace inc
{
// the backends in this folder are designed for standalone use
// below is an all-in-one API, also covering what has to be done regarding imgui in a simple application

// all-in-one imgui init using the SDL2 and PHI backends in this folder
IMGUI_IMPL_API void imgui_init(SDL_Window* sdl_window,
                               phi::Backend* backend,
                               int num_frames_in_flight,
                               phi::format target_format,
                               bool enable_docking = true,
                               bool enable_multi_viewport = false);

// shuts down SDL2, PHI backends and imgui itself
IMGUI_IMPL_API void imgui_shutdown();

// begins a new frame in imgui, the SDL2 and PHI backends, and ImGuizmo
// NOTE: does not cover window events, use ImGui_ImplSDL2_ProcessEvent before calling this (ie. with a loop over inc::da::SDLWindow::pollSingleEvent)
// empty_run: if you want to disable imgui and still survive all ImGui::XY() calls, set to true and use imgui_discard_frame() at the end of the frame
IMGUI_IMPL_API void imgui_new_frame(SDL_Window* sdl_window, bool empty_run = false);

// renders the current frame in imgui, and writes resulting commands to a pr Frame
// NOTE: requires an active pr::raii::Framebuffer targetting a single rgba8un target (like the backbuffer)
IMGUI_IMPL_API void imgui_render(pr::raii::Frame& frame);

// updates and renders (non-main) multi-viewports
// pFrame: if given, renders the viewports (otherwise just updates)
IMGUI_IMPL_API void imgui_viewport_update(pr::raii::Frame* pFrame);

// call instead of imgui_render at the end of the frame to not render imgui
IMGUI_IMPL_API void imgui_discard_frame();

//
// Bonus

enum class imgui_theme
{
    classic_source_hl2,
    corporate_grey,
    photoshop_dark,
    light_green, // default theme in glow::viewer
    fontstudio,  // default theme of ImGuiFontStudio
    darcula,
};

IMGUI_IMPL_API void load_imgui_theme(imgui_theme theme);

} // namespace inc
