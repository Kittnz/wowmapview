#ifndef GUI_MANAGER_H
#define GUI_MANAGER_H

#include "imgui.h"
#include "imgui_impl_sdl1.h"
#include "imgui_impl_opengl2.h"
#include "SDL.h"
#include "world.h"

class Test;
class WorldObjectManipulator;

class GuiManager {
public:
    //GuiManager();
    GuiManager(Test* testInstance);
    ~GuiManager();

    bool Init(SDL_Surface* screen);
    void Shutdown();
    void Render(World* world, Test* test);
    bool HandleEvent(SDL_Event* event);

    // GUI state
    bool showMainWindow = true;
    bool showCameraInfo = true;
    bool showPerformance = true;
    bool showNodeControls = true;
	bool showObjectManipulator = true;

    bool IsInitialized() const { return initialized; }

    void RenderObjectManipulatorUI(WorldObjectManipulator* manipulator);

private:

    Test* test;

    void RenderMainControls(Test* test);
    void RenderCameraMapInfo(World* world);
    void RenderPerformance();
    void RenderNodeControls(World* world);

    bool initialized = false;
};

#endif
