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

struct TravelNodePathPoint {
    uint32 fromNodeId;
    uint32 toNodeId;
    uint32 nr;
    uint32 mapId;
    float x, y, z;
    Vec3D position; // Converted position
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
    std::vector<TravelNodePathPoint*> points;
};

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

    std::vector<TravelNodeLink*> links;
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

    void DrawLabels(int mapId);
    void DrawNodeLabel(const TravelNode& node);
    void DrawAllNodeLabels(int mapId);

    void DumpGLState(const char* label);

private:

    struct LinkVertexData {
        Vec3D start;
        Vec3D end;
    };

    struct PathPointVertex {
        float x, y, z;          // Position
        float r, g, b, a;       // Color
        enum VertexType {
            LINE,               // Line segment vertex
            POINT               // Path point vertex
        } type;
    };

    GLuint sphereDisplayList;
    GLuint linkVBO;
    GLuint pathPointVBO;

    std::vector<LinkVertexData> linkVertices;
    std::vector<PathPointVertex> pathPointVertices;

    bool linksNeedUpdate;
    bool pathPointsNeedUpdate = true;

    void DrawBox(const Vec3D& pos, float size, const Vec4D& color);

    void InitVBO();
    void UpdateLinkVBO();
    void DrawLinks(int mapId);

    void UpdatePathPointVBO();
    void DrawPathPoints(int mapId);

    void DrawSphere(const Vec3D& pos, float radius, const Vec4D& color);

    bool WorldToScreen(const Vec3D& worldPos, Vec2D& screenPos, bool& isVisible);

    Model* nodeModel;
    int nodeModelId = 0;
    std::string modelName;
    void DrawModel(const Vec3D& pos, const Vec4D& color);
    int randomAnim = 0;

};

#endif
