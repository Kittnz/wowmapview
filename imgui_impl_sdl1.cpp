// imgui_impl_sdl1.cpp
#include "imgui_impl_sdl1.h"
#include <SDL.h>

struct ImGui_ImplSDL1_Data
{
    Uint32          Time;
    bool            MousePressed[3];
    SDL_Cursor*     MouseCursors[ImGuiMouseCursor_COUNT];
    int             PendingMouseLeaveFrame;
    char*          ClipboardTextData;
    bool            MouseCanUseGlobalState;
};

// Backend data stored in io.BackendPlatformUserData
static ImGui_ImplSDL1_Data* ImGui_ImplSDL1_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplSDL1_Data*)ImGui::GetIO().BackendPlatformUserData : nullptr;
}

static ImGuiKey ImGui_ImplSDL1_KeycodeToImGuiKey(int keycode)
{
    switch (keycode)
    {
    case SDLK_TAB: return ImGuiKey_Tab;
    case SDLK_LEFT: return ImGuiKey_LeftArrow;
    case SDLK_RIGHT: return ImGuiKey_RightArrow;
    case SDLK_UP: return ImGuiKey_UpArrow;
    case SDLK_DOWN: return ImGuiKey_DownArrow;
    case SDLK_PAGEUP: return ImGuiKey_PageUp;
    case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
    case SDLK_HOME: return ImGuiKey_Home;
    case SDLK_END: return ImGuiKey_End;
    case SDLK_INSERT: return ImGuiKey_Insert;
    case SDLK_DELETE: return ImGuiKey_Delete;
    case SDLK_BACKSPACE: return ImGuiKey_Backspace;
    case SDLK_SPACE: return ImGuiKey_Space;
    case SDLK_RETURN: return ImGuiKey_Enter;
    case SDLK_ESCAPE: return ImGuiKey_Escape;
    default: return ImGuiKey_None;
    }
}

bool ImGui_ImplSDL1_Init()
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized");

    ImGui_ImplSDL1_Data* bd = IM_NEW(ImGui_ImplSDL1_Data)();
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = "imgui_impl_sdl1";

    bd->ClipboardTextData = nullptr;
    bd->MouseCanUseGlobalState = true;

    return true;
}

void ImGui_ImplSDL1_Shutdown()
{
    ImGui_ImplSDL1_Data* bd = ImGui_ImplSDL1_GetBackendData();
    IM_ASSERT(bd != nullptr && "No platform backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    if (bd->ClipboardTextData)
        SDL_free(bd->ClipboardTextData);

    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    IM_DELETE(bd);
}

void ImGui_ImplSDL1_NewFrame()
{
    ImGui_ImplSDL1_Data* bd = ImGui_ImplSDL1_GetBackendData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplSDL1_Init()?");
    ImGuiIO& io = ImGui::GetIO();

    // Setup display size
    SDL_Surface* surface = SDL_GetVideoSurface();
    io.DisplaySize = ImVec2((float)surface->w, (float)surface->h);

    // Setup time step (we don't use SDL_GetTicks() directly as it may be unsynchronized with our game loop)
    static Uint32 frequency = SDL_GetTicks();
    Uint32 current_time = SDL_GetTicks();

    if (bd->Time > 0)
        io.DeltaTime = (float)(current_time - bd->Time) / 1000.0f;
    else
        io.DeltaTime = 1.0f / 60.0f;

    if (io.DeltaTime <= 0.0f)
        io.DeltaTime = 1.0f / 60.0f;

    bd->Time = current_time;

    // Update mouse position and buttons
    int mx, my;
    Uint8 mouseState = SDL_GetMouseState(&mx, &my);
    
    io.MousePos = ImVec2((float)mx, (float)my);
    io.MouseDown[0] = bd->MousePressed[0] || (mouseState & SDL_BUTTON(1));
    io.MouseDown[1] = bd->MousePressed[1] || (mouseState & SDL_BUTTON(3));
    io.MouseDown[2] = bd->MousePressed[2] || (mouseState & SDL_BUTTON(2));
}

bool ImGui_ImplSDL1_ProcessEvent(SDL_Event* event)
{
    ImGui_ImplSDL1_Data* bd = ImGui_ImplSDL1_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();

    switch (event->type)
    {
        case SDL_MOUSEBUTTONDOWN:
        {
            if (event->button.button == SDL_BUTTON_LEFT) bd->MousePressed[0] = true;
            if (event->button.button == SDL_BUTTON_RIGHT) bd->MousePressed[1] = true;
            if (event->button.button == SDL_BUTTON_MIDDLE) bd->MousePressed[2] = true;
            return true;
        }
        case SDL_MOUSEBUTTONUP:
        {
            if (event->button.button == SDL_BUTTON_LEFT) bd->MousePressed[0] = false;
            if (event->button.button == SDL_BUTTON_RIGHT) bd->MousePressed[1] = false;
            if (event->button.button == SDL_BUTTON_MIDDLE) bd->MousePressed[2] = false;
            return true;
        }
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            ImGuiKey key = ImGui_ImplSDL1_KeycodeToImGuiKey(event->key.keysym.sym);
            if (key != ImGuiKey_None)
                io.AddKeyEvent(key, (event->type == SDL_KEYDOWN));
            io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
            io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
            io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
            return true;
        }
    }
	return false;
}