#include "WorldObject.h"

// Convert game coordinates to viewer coordinates
Position WorldObject::ConvertGameCoordsToViewerCoords(const Position& gamePosition) 
{
    constexpr float ZEROPOINT = 17066.666f;

    return Position(
        -(gamePosition.y - ZEROPOINT),  // x
        gamePosition.z,                 // y 
        -(gamePosition.x - ZEROPOINT),  // z
        gamePosition.o                  // orientation remains the same
    );
}

// Convert viewer coordinates back to game coordinates
Position WorldObject::ConvertViewerCoordsToGameCoords(const Position& viewerPosition)
{
    constexpr float ZEROPOINT = 17066.666f;

    return Position(
        -(viewerPosition.z - ZEROPOINT),  // x
        -(viewerPosition.x - ZEROPOINT),  // y
        viewerPosition.y,                 // z
        viewerPosition.o                  // orientation remains the same
    );
}

// Convert Position to Vec3D
Vec3D WorldObject::PositionToVec3D(const Position& position)
{
    return Vec3D(position.x, position.y, position.z);
}

// Convert Vec3D to Position
// Note: This method will set the orientation to 0.0f as Vec3D doesn't contain orientation
Position WorldObject::Vec3DToPosition(const Vec3D& vec)
{
    return Position(vec.x, vec.y, vec.z, 0.0f);
}

// If you want to preserve orientation when converting back to Position, 
// you might want to add an overload or modify the method:
Position WorldObject::Vec3DToPosition(const Vec3D& vec, float orientation)
{
    return Position(vec.x, vec.y, vec.z, orientation);
}
