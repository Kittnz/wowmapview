#include "gui_manager.h"
#include "wowmapview.h"
#include "Objects/WorldObject.h"
#include "Objects/WorldObjectManipulator.h"
#include "test.h"
#include "areadb.h"

GuiManager::GuiManager(Test* testInstance) : test(testInstance) {}

GuiManager::~GuiManager() {
    Shutdown();
}

bool GuiManager::Init(SDL_Surface* screen) {
    if (initialized) {
        gLog("GUI Manager already initialized\n");
        return true;
    }

    if (!screen) {
        gLog("Invalid screen surface passed to GUI manager\n");
        return false;
    }

    if (ImGui::GetCurrentContext()) {
        gLog("ImGui context already exists\n");
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    if (!ImGui::GetCurrentContext()) {
        gLog("Failed to create ImGui context\n");
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.DisplaySize = ImVec2((float)video.xres, (float)video.yres);
    io.DeltaTime = 1.0f / 60.0f;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 0.8f;

    if (!ImGui_ImplSDL1_Init()) {
        gLog("Failed to initialize ImGui SDL1 backend\n");
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL2_Init()) {
        gLog("Failed to initialize ImGui OpenGL2 backend\n");
        ImGui_ImplSDL1_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    initialized = true;
    gLog("GUI Manager initialized successfully\n");
    return true;
}

void GuiManager::Shutdown() {
    if (!initialized)
        return;

    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL2_Shutdown();
        ImGui_ImplSDL1_Shutdown();
        ImGui::DestroyContext();
    }

    initialized = false;
}

bool GuiManager::HandleEvent(SDL_Event* event)
{
    if (!initialized)
        return false;

    return ImGui_ImplSDL1_ProcessEvent(event);
}

void GuiManager::Render(World* world, Test* test) {
    if (!initialized)
        return;

    // Start ImGui frame
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplSDL1_NewFrame();

    try {
        ImGui::NewFrame();
    }
    catch (...) {
        gLog("Error in ImGui::NewFrame()\n");
        return;
    }

    // Render GUI elements
    if (showMainWindow)
        RenderMainControls(test);
    if (showCameraInfo)
        RenderCameraMapInfo(world);
    if (showPerformance)
        RenderPerformance();
    if (showNodeControls)
        RenderNodeControls(world);
	if (showObjectManipulator)
		RenderObjectManipulatorUI(test->manipulator.get());

    ImGui::Render();

    // Take control of rendering state
    glPushAttrib(GL_ALL_ATTRIB_BITS); // Save ALL states

    // Set up our own state explicitly
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);

    // Ensure we're in projection mode
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, video.xres, video.yres, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Render ImGui
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

    // Restore matrices
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    // Restore all states
    glPopAttrib();
}

void GuiManager::RenderMainControls(Test* test) {
    ImGui::Begin("Controls", &showMainWindow);

    if (ImGui::Button("Toggle Fog"))
        test->world->drawfog = !test->world->drawfog;

    if (ImGui::Button("Toggle Models"))
        test->world->drawmodels = !test->world->drawmodels;

    if (ImGui::Button("Toggle Terrain"))
        test->world->drawterrain = !test->world->drawterrain;

    if (ImGui::Button("Toggle Doodads"))
        test->world->drawdoodads = !test->world->drawdoodads;

    if (ImGui::Button("Toggle WMO"))
        test->world->drawwmo = !test->world->drawwmo;

    if (ImGui::Button("Toggle Nodes"))
        test->world->drawnodes = !test->world->drawnodes;

    if (ImGui::Button("Toggle Node Labels"))
        test->world->drawnodelabels = !test->world->drawnodelabels;

    if (ImGui::Button("Toggle Node Path Points"))
        test->world->drawpathpoints = !test->world->drawpathpoints;

    ImGui::SliderFloat("Movement Speed", &test->movespd, 10.0f, 500.0f, "%.1f");

    ImGui::SliderFloat("Highres Distance", &test->world->highresdistance, 384.0f, 1000.0f, "%.1f");
    ImGui::SliderFloat("Map Distance", &test->world->mapdrawdistance, 998.0f, 2000.0f, "%.1f");
    ImGui::SliderFloat("Model Distance", &test->world->modeldrawdistance, 384.0f, 1000.0f, "%.1f");
    ImGui::SliderFloat("Doodad Distance", &test->world->doodaddrawdistance, 64.0f, 1000.0f, "%.1f");

    ImGui::SliderFloat("Fog Distance", &test->world->fogdistance, 357.0f, 777.0f, "%.1f");

    ImGui::End();
}

void GuiManager::RenderCameraMapInfo(World* world)
{
    ImGui::Begin("Camera / Map", &showCameraInfo);

    ImGui::Text("Camera Viewer Position: %.1f, %.1f, %.1f", world->camera.x, world->camera.y, world->camera.z);

    Position gamePos = WorldObject::ConvertViewerCoordsToGameCoords(Position(world->camera.x, world->camera.y, world->camera.z, 0.0f));
    ImGui::Text("Game XYZ Coords: %.1f, %.1f, %.1f", gamePos.x, gamePos.y, gamePos.z);

    unsigned int areaID = world->getAreaID();
    unsigned int regionID = 0;
    try {
        AreaDB::Record rec = gAreaDB.getByAreaID(areaID);
        std::string areaName = rec.getString(AreaDB::Name);
        regionID = rec.getUInt(AreaDB::Region);

        ImGui::Text("Area: %s", areaName.c_str());
    }
    catch (AreaDB::NotFound) {}

    if (regionID != 0) {
        try {
            AreaDB::Record rec = gAreaDB.getByAreaID(regionID);
            std::string regionName = rec.getString(AreaDB::Name);

            ImGui::Text("Region: %s", regionName.c_str());
        }
        catch (AreaDB::NotFound) {}
    }

    ImGui::End();
}

void GuiManager::RenderPerformance() {
    ImGui::Begin("Performance", &showPerformance);
    ImGui::Text("FPS: %.1f", gFPS);
    ImGui::End();
}

void GuiManager::RenderNodeControls(World* world) {
    ImGui::Begin("Node Controls", &showNodeControls);

    if (ImGui::Button("Go to Nearest Node")) {
        // We need to modify Test class to expose this functionality
        test->moveToNearestNode();
    }

    // Show node list
    if (ImGui::BeginChild("Nodes", ImVec2(0, 300), true)) {
        for (const auto& node : world->botNodes.nodes) {
            if (ImGui::Selectable(node.name.c_str())) {
                test->moveToNode(node);
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

void GuiManager::RenderObjectManipulatorUI(WorldObjectManipulator* manipulator)
{
    if (!manipulator)
        return;

    ImGui::Begin("Object Manipulator");

    ImGui::Separator();
    if (ImGui::RadioButton("Select", manipulator->GetMode() == ManipulatorMode::Select))
        manipulator->SetMode(ManipulatorMode::Select);
    ImGui::SameLine();
    if (ImGui::RadioButton("Move", manipulator->GetMode() == ManipulatorMode::Move))
        manipulator->SetMode(ManipulatorMode::Move);
    ImGui::SameLine();
    if (ImGui::RadioButton("Add", manipulator->GetMode() == ManipulatorMode::Add))
        manipulator->SetMode(ManipulatorMode::Add);
    ImGui::SameLine();
    if (ImGui::RadioButton("Delete", manipulator->GetMode() == ManipulatorMode::Delete))
        manipulator->SetMode(ManipulatorMode::Delete);

    if (const SelectableObject* selectedObject = manipulator->GetSelectedObject()) {
        ImGui::Separator();
        ImGui::Text("Selected: %s", selectedObject->name.c_str());

        ImGui::Text("Viewer Coords: %.1f, %.1f, %.1f", selectedObject->position.x, selectedObject->position.y, selectedObject->position.z);

        Position selectedPos = WorldObject::ConvertViewerCoordsToGameCoords(Position(selectedObject->position.x, selectedObject->position.y, selectedObject->position.z, 0.0f));
        ImGui::Text("Game XYZ Coords: %.1f, %.1f, %.1f", selectedPos.x, selectedPos.y, selectedPos.z);
        static char nameBuf[256];
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf),
            ImGuiInputTextFlags_EnterReturnsTrue))
        {
            if (selectedObject->renameFunc) {
                selectedObject->renameFunc(nameBuf);
            }
        }

        if (ImGui::Button("Delete") && selectedObject->deleteFunc) {
            selectedObject->deleteFunc();
        }
    }

    ImGui::End();
}
