#include "WorldObjectManipulator.h"
#include "imgui.h"
#include "Database/Database.h"
#include "Objects/WorldObject.h"

extern Database GameDb;

WorldObjectManipulator::~WorldObjectManipulator()
{
    // Nothing specific needed for cleanup
}

WorldObjectManipulator::WorldObjectManipulator(World* world)
    : world(world)
    , currentMode(ManipulatorMode::Select)
    , selectedObject(nullptr)
    , isDragging(false)
    , dragStartX(0)
    , dragStartY(0)
{
    UpdateSelectableObjects();
}

void WorldObjectManipulator::UpdateSelectableObjects() {
    selectableObjects.clear();
    RegisterTravelNodes();
    RegisterPathPoints();
}

void WorldObjectManipulator::RegisterTravelNodes() {
    for (auto& node : world->botNodes.nodes) {
        SelectableObject obj;
        obj.type = SelectableType::TravelNode;
        obj.object = &node;
        obj.id = node.id;
        obj.position = node.position;
        obj.name = node.name;
        obj.isSelected = false;

        // Setup move callback
        obj.moveFunc = [this, &node](const Vec3D& newPos) {
            Position viewerPos(newPos.x, newPos.y, newPos.z, 0.0f);
            Position gamePos = WorldObject::ConvertViewerCoordsToGameCoords(viewerPos);
            GameDb.ExecuteQueryInstant(
                "UPDATE ai_playerbot_travelnode SET x = %f, y = %f, z = %f WHERE id = %u",
                gamePos.x, gamePos.y, gamePos.z, node.id);
            node.position = newPos;
            };

        // Setup delete callback
        obj.deleteFunc = [this, &node]() {
            GameDb.ExecuteQueryInstant("DELETE FROM ai_playerbot_travelnode WHERE id = %u", node.id);
            world->botNodes.LoadFromDB(); // Reload nodes
            };

        // Setup rename callback
        obj.renameFunc = [this, &node](const std::string& newName) {
            GameDb.ExecuteQueryInstant(
                "UPDATE ai_playerbot_travelnode SET name = '%s' WHERE id = %u",
                newName.c_str(), node.id);
            node.name = newName;
            };

        selectableObjects.push_back(obj);
    }
}

void WorldObjectManipulator::RegisterPathPoints() {
    for (auto& link : world->botNodes.links) {
        for (auto& point : link.points) {
            SelectableObject obj;
            obj.type = SelectableType::PathPoint;
            obj.object = point;
            obj.id = point->nr;
            obj.position = point->position;
            obj.name = "Path Point " + std::to_string(point->nr);
            obj.isSelected = false;

            // Setup callbacks similar to nodes
            obj.moveFunc = [this, point](const Vec3D& newPos) {
                Position gamePos = WorldObject::ConvertViewerCoordsToGameCoords(
                    Position(newPos.x, newPos.y, newPos.z, 0.0f));
                GameDb.ExecuteQueryInstant(
                    "UPDATE ai_playerbot_travelnode_path SET x = %f, y = %f, z = %f "
                    "WHERE node_id = %u AND to_node_id = %u AND nr = %u",
                    gamePos.x, gamePos.y, gamePos.z,
                    point->fromNodeId, point->toNodeId, point->nr);
                point->position = newPos;
                };

            obj.deleteFunc = [this, point]() {
                GameDb.ExecuteQueryInstant(
                    "DELETE FROM ai_playerbot_travelnode_path "
                    "WHERE node_id = %u AND to_node_id = %u AND nr = %u",
                    point->fromNodeId, point->toNodeId, point->nr);
                world->botNodes.LoadFromDB();
                };

            selectableObjects.push_back(obj);
        }
    }
}

void WorldObjectManipulator::UpdateSelection(const Vec3D& rayOrigin, const Vec3D& rayDir) {
    if (currentMode != ManipulatorMode::Select)
        return;

    selectedObject = GetObjectAtPosition(rayOrigin, rayDir);

    // Clear previous selections
    for (auto& obj : selectableObjects) {
        obj.isSelected = (selectedObject && obj.id == selectedObject->id);
    }
}

SelectableObject* WorldObjectManipulator::GetObjectAtPosition(const Vec3D& rayOrigin, const Vec3D& rayDir)
{
    float closestDist = FLT_MAX;
    SelectableObject* closestObj = nullptr;

    for (auto& obj : selectableObjects) {
        Vec3D toObj = obj.position - rayOrigin;
        float t = toObj * rayDir;

        if (t < 0) continue;

        Vec3D closest = rayOrigin + rayDir * t;
        float dist = (closest - obj.position).length();
        float selectionSize = (obj.type == SelectableType::TravelNode) ? 3.0f : 1.0f;

        if (dist < selectionSize && dist < closestDist) {
            closestDist = dist;
            closestObj = &obj;  // Now the types match
        }
    }

    return closestObj;  // Return closest object found
}

void WorldObjectManipulator::StartDragging(float mouseX, float mouseY)
{
    if (!selectedObject)
        return;

    isDragging = true;
    dragStartX = mouseX;
    dragStartY = mouseY;
    dragStartPos = selectedObject->position;
}

void WorldObjectManipulator::UpdateDragging(float mouseX, float mouseY)
{
    if (!isDragging || !selectedObject)
        return;

    // Get key states and calculate final speed multiplier
    Uint8* keystate = SDL_GetKeyState(NULL);
    float speedMultiplier = dragSpeedMultiplier;
    if (keystate[SDLK_LSHIFT] || keystate[SDLK_RSHIFT])
        speedMultiplier *= 5.0f;
    if (keystate[SDLK_LCTRL] || keystate[SDLK_RCTRL])
        speedMultiplier *= 0.2f;

    // Get the view and projection matrices
    GLdouble modelMatrix[16], projMatrix[16];
    GLint viewport[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Calculate world space movement based on screen space delta
    GLdouble startX, startY, startZ;
    GLdouble endX, endY, endZ;

    gluUnProject(dragStartX, viewport[3] - dragStartY, 0.5,
        modelMatrix, projMatrix, viewport,
        &startX, &startY, &startZ);

    gluUnProject(mouseX, viewport[3] - mouseY, 0.5,
        modelMatrix, projMatrix, viewport,
        &endX, &endY, &endZ);

    Vec3D delta((endX - startX) * speedMultiplier, 0, (endZ - startZ) * speedMultiplier);

    // Handle vertical movement when Z key is pressed
    if (keystate[SDLK_z]) {
        float verticalDelta = (mouseY - dragStartY) * 0.1f * speedMultiplier;
        delta.y = verticalDelta;
    }

    selectedObject->position = dragStartPos + delta;

    if (selectedObject->moveFunc) {
        selectedObject->moveFunc(selectedObject->position);
    }
}

void WorldObjectManipulator::StopDragging()
{
    if (isDragging && selectedObject) {
        // Final update of position if needed
        if (selectedObject->moveFunc) {
            selectedObject->moveFunc(selectedObject->position);
        }
    }
    isDragging = false;
}

void WorldObjectManipulator::DeleteSelected()
{
    if (selectedObject && selectedObject->deleteFunc) {
        selectedObject->deleteFunc();
        selectedObject = nullptr;
    }
}

void WorldObjectManipulator::AddNewObject(const Vec3D& position, SelectableType type)
{
    // Conversion to game coordinates
    Position viewerPos(position.x, position.y, position.z, 0.0f);
    Position gamePos = WorldObject::ConvertViewerCoordsToGameCoords(viewerPos);

    switch (type) {
    case SelectableType::TravelNode:
        // Add new node to database
        GameDb.ExecuteQueryInstant(
            "INSERT INTO ai_playerbot_travelnode (name, map_id, x, y, z, linked) "
            "VALUES ('New Node', %u, %f, %f, %f, 0)",
            world->currentMapId, gamePos.x, gamePos.y, gamePos.z);

        // Refresh nodes from database
        world->botNodes.LoadFromDB();
        break;

    case SelectableType::PathPoint:
        // Add path point logic here if needed
        break;

    default:
        break;
    }

    // Refresh selectable objects
    UpdateSelectableObjects();
}
