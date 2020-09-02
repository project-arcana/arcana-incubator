#pragma once

#include "lib/imgui.h"

#include <clean-core/span.hh>

#include <phantasm-hardware-interface/fwd.hh>

IMGUI_IMPL_API bool ImGui_ImplPHI_Init(phi::Backend* backend, int num_frames_in_flight, phi::format target_format);
IMGUI_IMPL_API void ImGui_ImplPHI_Shutdown();
IMGUI_IMPL_API void ImGui_ImplPHI_NewFrame();

/// writes draw commands to out_cmdbuffer
IMGUI_IMPL_API void ImGui_ImplPHI_RenderDrawData(ImDrawData const* draw_data, cc::span<std::byte> out_cmdbuffer);

/// returns the minimum size of out_cmdbuffer when calling _RenderDrawData
IMGUI_IMPL_API unsigned ImGui_ImplPHI_GetDrawDataCommandSize(ImDrawData const* draw_data);

/// Initializes with provided shader binaries, without compiling embedded ones
IMGUI_IMPL_API bool ImGui_ImplPHI_InitWithShaders(
    phi::Backend* backend, int num_frames_in_flight, phi::format target_format, cc::span<std::byte const> ps_data, cc::span<std::byte const> vs_data);
