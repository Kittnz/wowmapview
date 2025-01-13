#ifndef WORLDOBJECT_H
#define WORLDOBJECT_H

#include "vec3d.h"

struct Position
{
    Position() = default;
    Position(float position_x, float position_y, float position_z, float orientation) : x(position_x), y(position_y), z(position_z), o(orientation) {}
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
};

class WorldObject {
public:
    virtual ~WorldObject() {}

    // Convert WoW coordinates to viewer coordinates
    static Position ConvertGameCoordsToViewerCoords(const Position& gamePosition);

    // Convert viewer coordinates back to game coordinates
    static Position ConvertViewerCoordsToGameCoords(const Position& viewerPosition);

    // Convert Position to Vec3D
    static Vec3D PositionToVec3D(const Position& position);

    // Convert Vec3D to Position (with default 0 orientation)
    static Position Vec3DToPosition(const Vec3D& vec);

    // Convert Vec3D to Position with specified orientation
    static Position Vec3DToPosition(const Vec3D& vec, float orientation);
};

#endif // WORLDOBJECT_H