// imgui_impl_sdl1.h
#pragma once
#include "imgui.h"
#include <SDL.h>

IMGUI_IMPL_API bool     ImGui_ImplSDL1_Init();
IMGUI_IMPL_API void     ImGui_ImplSDL1_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplSDL1_NewFrame();
IMGUI_IMPL_API bool     ImGui_ImplSDL1_ProcessEvent(SDL_Event* event);
