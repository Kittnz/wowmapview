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

        // Increase selection radius for path points
        float selectionSize = (obj.type == SelectableType::PathPoint) ? 2.0f : 3.0f;

        if (dist < selectionSize && dist < closestDist) {
            closestDist = dist;
            closestObj = &obj;
        }
    }

    return closestObj;
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

    // Get key state for Y-axis movement
    Uint8* keystate = SDL_GetKeyState(NULL);
    bool allowYMovement = keystate[SDLK_LCTRL];

    // Get current view and projection matrices
    GLint viewport[4];
    GLdouble modelMatrix[16], projMatrix[16];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);

    // Calculate ray direction from camera through mouse point
    GLdouble nearX, nearY, nearZ;
    GLdouble farX, farY, farZ;
    gluUnProject(mouseX, viewport[3] - mouseY, 0.0,
        modelMatrix, projMatrix, viewport,
        &nearX, &nearY, &nearZ);
    gluUnProject(mouseX, viewport[3] - mouseY, 1.0,
        modelMatrix, projMatrix, viewport,
        &farX, &farY, &farZ);

    Vec3D rayOrigin(nearX, nearY, nearZ);
    Vec3D rayDir = Vec3D(farX - nearX, farY - nearY, farZ - nearZ).normalize();

    // Use different intersection planes based on CTRL state
    Vec3D planeNormal;
    float planeDistance;

    if (allowYMovement) {
        // Use camera-facing plane when CTRL is held
        planeNormal = (rayOrigin - dragStartPos).normalize();
        planeDistance = -(planeNormal * dragStartPos);
    }
    else {
        // Use ground plane when CTRL is not held
        planeNormal = Vec3D(0, 1, 0);
        planeDistance = -dragStartPos.y;
    }

    // Calculate intersection
    float denom = planeNormal * rayDir;
    if (std::abs(denom) > 0.0001f) {
        float t = -(planeNormal * rayOrigin + planeDistance) / denom;
        if (t >= 0) {
            Vec3D intersectionPoint = rayOrigin + rayDir * t;
            Vec3D newPos = intersectionPoint;

            if (!allowYMovement) {
                // Keep original Y height when not holding CTRL
                newPos.y = dragStartPos.y;
            }

            selectedObject->position = newPos;
            if (selectedObject->moveFunc) {
                selectedObject->moveFunc(selectedObject->position);
            }
        }
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
            "VALUES ('New Node', %u, %f, %f, %f, 0)",  world->currentMapId, gamePos.x, gamePos.y, gamePos.z);

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

void WorldObjectManipulator::Draw()
{
    if (!selectedObject)
        return;

    // Save OpenGL state
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    // Setup state for 3D geometry
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    // Draw highlight sphere for selected object
    if (selectedObject->type == SelectableType::PathPoint) {
        // Draw a slightly larger yellow sphere for selected path points
        Vec4D highlightColor(1.0f, 1.0f, 0.0f, 0.7f); // Yellow with some transparency
        float sphereRadius = 1.5f; // Make it noticeable but not too large

        glColor4fv(highlightColor);

        static const int segments = 12;
        Vec3D pos = selectedObject->position;

        // Draw three circles to form a sphere-like shape
        for (int i = 0; i < 3; i++) {
            glBegin(GL_LINE_LOOP);
            for (int j = 0; j < segments; j++) {
                float theta = 2.0f * PI * float(j) / float(segments);
                float x = sphereRadius * cosf(theta);
                float y = sphereRadius * sinf(theta);
                switch (i) {
                case 0: glVertex3f(pos.x + x, pos.y + y, pos.z); break;
                case 1: glVertex3f(pos.x + x, pos.y, pos.z + y); break;
                case 2: glVertex3f(pos.x, pos.y + x, pos.z + y); break;
                }
            }
            glEnd();
        }
    }

    // Restore state
    glPopAttrib();
}
