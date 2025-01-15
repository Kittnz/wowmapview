// Objects/WorldObjectManipulator.h 
#ifndef WORLD_OBJECT_MANIPULATOR_H
#define WORLD_OBJECT_MANIPULATOR_H

#include <memory>
#include <functional>
#include <vector>
#include <string>
#include "vec3d.h"
#include "world.h"

enum class ManipulatorMode {
    None,
    Select,
    Move,
    Add,
    Delete
};

enum class SelectableType {
    None,
    TravelNode,
    PathPoint,
    Link
};

struct SelectableObject {
    SelectableType type;
    void* object;
    uint32 id;
    Vec3D position;
    std::string name;
    bool isSelected;

    std::function<void(const Vec3D&)> moveFunc;
    std::function<void()> deleteFunc;
    std::function<void(const std::string&)> renameFunc;
};

class WorldObjectManipulator {
public:
    WorldObjectManipulator(World* world);
    ~WorldObjectManipulator();

    void SetMode(ManipulatorMode mode) { currentMode = mode; }
    ManipulatorMode GetMode() const { return currentMode; }

    void UpdateSelection(const Vec3D& rayOrigin, const Vec3D& rayDir);
    bool HasSelection() const { return selectedObject != nullptr; }
    const SelectableObject* GetSelectedObject() const { return selectedObject; }

    void StartDragging(float mouseX, float mouseY);
    void UpdateDragging(float mouseX, float mouseY);
    void StopDragging();
    void DeleteSelected();
    void AddNewObject(const Vec3D& position, SelectableType type);

    float GetDragSpeed() const { return dragSpeedMultiplier; }
    void SetDragSpeed(float speed) { dragSpeedMultiplier = speed; }

private:
    World* world;
    ManipulatorMode currentMode;
    SelectableObject* selectedObject;
    std::vector<SelectableObject> selectableObjects;

    bool isDragging;
    float dragStartX, dragStartY;
    Vec3D dragStartPos;

    void UpdateSelectableObjects();
    void RegisterTravelNodes();
    void RegisterPathPoints();
    SelectableObject* GetObjectAtPosition(const Vec3D& rayOrigin, const Vec3D& rayDir);
    float dragSpeedMultiplier = 5.0f;
};

#endif
