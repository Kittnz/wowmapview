#ifndef _NODES_H
#define _NODES_H

#include <vector>
#include <string>
#include "vec3d.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include "video.h"
#include "quaternion.h"
#include "defines/Common.h"
#include "model.h"

struct TravelNode {
    uint32 id;
    std::string name;
    uint32 mapId;
    float x;
    float y;
    float z;
    uint8 linked;

    Vec3D position;
    float radius;
};

struct TravelNodeLink {
    uint32 fromNodeId;
    uint32 toNodeId;
    uint8 type;
    uint32 object;
    float distance;
    float swimDistance;
    float extraCost;
    bool calculated;
    uint8 maxCreature[3];
};

struct TravelNodePathPoint {
    uint32 fromNodeId;
    uint32 toNodeId;
    uint32 nr;
    uint32 mapId;
    float x, y, z;
    Vec3D position; // Converted position
};



class WorldBotNodes {
public:

    WorldBotNodes(); 
    ~WorldBotNodes();

    static const float DEFAULT_BOX_SIZE;
    static const float PATH_POINT_SIZE;
    static const float TEXT_HEIGHT_OFFSET;
    static const float VIEW_DISTANCE;

    void LoadFromDB();
    bool LoadNodeModel();
    void Draw(int mapId);

    std::vector<TravelNode> nodes;
    std::vector<TravelNodeLink> links;
    std::vector<TravelNodePathPoint> pathPoints;

private:
    GLuint sphereDisplayList;

    void DrawBox(const Vec3D& pos, float size, const Vec4D& color);
    void DrawLinks(int mapId);
    void DrawPathPoints(int mapId);
    void DrawSphere(const Vec3D& pos, float radius, const Vec4D& color);
    void DrawNodeLabel(const TravelNode& node);
    bool WorldToScreen(const Vec3D& worldPos, Vec2D& screenPos, bool& isVisible);

    Model* nodeModel;
    int nodeModelId = 0;
    std::string modelName;
    void DrawModel(const Vec3D& pos, const Vec4D& color);
};

#endif
