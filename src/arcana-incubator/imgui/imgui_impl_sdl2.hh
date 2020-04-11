#pragma once

struct SDL_Window;
typedef union SDL_Event SDL_Event;

#include "lib/imgui.hh"

IMGUI_IMPL_API bool ImGui_ImplSDL2_Init(SDL_Window* window);
IMGUI_IMPL_API void ImGui_ImplSDL2_Shutdown();
IMGUI_IMPL_API void ImGui_ImplSDL2_NewFrame(SDL_Window* window);
IMGUI_IMPL_API bool ImGui_ImplSDL2_ProcessEvent(const SDL_Event* event);
