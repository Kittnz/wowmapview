#include "nodes.h"
#include "wowmapview.h"
#include "world.h"
#include "Database/Database.h"
#include <cmath>
#include <algorithm>
#include <SDL.h>
#include <SDL_opengl.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>

extern Database GameDb;

const float WorldBotNodes::DEFAULT_BOX_SIZE = 3.0f;  // Make box 3x3x3 units
const float WorldBotNodes::PATH_POINT_SIZE = 1.0f;  // Smaller than boxes
const float WorldBotNodes::TEXT_HEIGHT_OFFSET = 1.0f;  // Adjust this value to position text higher/lower
const float WorldBotNodes::VIEW_DISTANCE = 300.0f;  // Draw nodes within 1000 units

WorldBotNodes::WorldBotNodes() : nodeModel(nullptr)
{

}

WorldBotNodes::~WorldBotNodes()
{
    if (nodeModelId) {
        gWorld->modelmanager.delbyname(modelName);
    }
}

void WorldBotNodes::LoadFromDB()
{
    nodes.clear();

    // Load nodes
    auto result = GameDb.Query("SELECT id, name, map_id, x, y, z, linked FROM ai_playerbot_travelnode ORDER BY id");
    if (result)
    {
        do {
            DbField* fields = result->fetchCurrentRow();

            TravelNode node;
            node.id = fields[0].GetUInt32();
            node.name = fields[1].GetString();
            node.mapId = fields[2].GetUInt32();
            node.x = fields[3].GetFloat();
            node.y = fields[4].GetFloat();
            node.z = fields[5].GetFloat();
            node.linked = fields[6].GetUInt8();

            if (node.mapId != gWorld->currentMapId)
                continue;

            gLog("Loaded travel node %u: %s (%.2f, %.2f, %.2f) on map %u\n", node.id, node.name.c_str(), node.x, node.y, node.z, node.mapId);

            // Convert WoW coordinates to OpenGL coordinates
            node.position = Vec3D(-(node.y - ZEROPOINT), (node.z), -(node.x - ZEROPOINT));
            node.radius = 2.0f;

            nodes.push_back(node);

        } while (result->NextRow());
    }

    // Load links
    result = GameDb.Query("SELECT node_id, to_node_id, type, object, distance, swim_distance, extra_cost, calculated, max_creature_0, max_creature_1, max_creature_2 FROM ai_playerbot_travelnode_link");
    if (result)
    {
        do {
            DbField* fields = result->fetchCurrentRow();
            TravelNodeLink link;
            link.fromNodeId = fields[0].GetUInt32();
            link.toNodeId = fields[1].GetUInt32();
            link.type = fields[2].GetUInt8();
            link.object = fields[3].GetUInt32();
            link.distance = fields[4].GetFloat();
            link.swimDistance = fields[5].GetFloat();
            link.extraCost = fields[6].GetFloat();
            link.calculated = fields[7].GetBool();
            link.maxCreature[0] = fields[8].GetUInt8();
            link.maxCreature[1] = fields[9].GetUInt8();
            link.maxCreature[2] = fields[10].GetUInt8();
            links.push_back(link);
        } while (result->NextRow());
    }

    // Load path points
    result = GameDb.Query("SELECT node_id, to_node_id, nr, map_id, x, y, z FROM ai_playerbot_travelnode_path ORDER BY node_id, to_node_id, nr");
    if (result)
    {
        do {
            DbField* fields = result->fetchCurrentRow();
            TravelNodePathPoint point;
            point.fromNodeId = fields[0].GetUInt32();
            point.toNodeId = fields[1].GetUInt32();
            point.nr = fields[2].GetUInt32();
            point.mapId = fields[3].GetUInt32();
            point.x = fields[4].GetFloat();
            point.y = fields[5].GetFloat();
            point.z = fields[6].GetFloat();

            // Convert coordinates
            point.position = Vec3D(-(point.y - ZEROPOINT), point.z, -(point.x - ZEROPOINT));
            pathPoints.push_back(point);
        } while (result->NextRow());
    }
}

void WorldBotNodes::DrawSphere(const Vec3D& pos, float radius, const Vec4D& color)
{
    glColor4f(color.x, color.y, color.z, color.w);

    static const int segments = 12;

    // Draw a simple spherical point using circles in three planes
    for (int i = 0; i < 3; i++) {
        glBegin(GL_LINE_LOOP);
        for (int j = 0; j < segments; j++) {
            float theta = 2.0f * PI * float(j) / float(segments);
            float x = radius * cosf(theta);
            float y = radius * sinf(theta);
            switch (i) {
            case 0: glVertex3f(pos.x + x, pos.y + y, pos.z); break;
            case 1: glVertex3f(pos.x + x, pos.y, pos.z + y); break;
            case 2: glVertex3f(pos.x, pos.y + x, pos.z + y); break;
            }
        }
        glEnd();
    }
}

void WorldBotNodes::Draw(int mapId)
{
    if (!nodeModel && !LoadNodeModel()) {
        // If model loading failed, fall back to drawing boxes
        glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_CURRENT_BIT);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        DrawLinks(mapId);
        //DrawPathPoints(mapId);

        // Draw normal boxes instead
        for (const auto& node : nodes)
        {
            if (node.mapId != mapId)
                continue;

            Vec4D color = node.linked ?
                Vec4D(0.0f, 1.0f, 0.0f, 0.7f) :
                Vec4D(1.0f, 0.0f, 0.0f, 0.7f);

            DrawBox(node.position, DEFAULT_BOX_SIZE, color);
        }

        glPopAttrib();
    }
    else {
        // Draw with model
        glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_CURRENT_BIT);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        DrawLinks(mapId);
        //DrawPathPoints(mapId);

        for (const auto& node : nodes)
        {
            if (node.mapId != mapId)
                continue;

            Vec4D color = node.linked ?
                Vec4D(0.0f, 1.0f, 0.0f, 0.7f) :
                Vec4D(1.0f, 0.0f, 0.0f, 0.7f);

            DrawModel(node.position, color);
        }

        glPopAttrib();
    }

    // Draw labels last
    for (const auto& node : nodes)
    {
        if (node.mapId != mapId)
            continue;

        DrawNodeLabel(node);
    }
}

void WorldBotNodes::DrawBox(const Vec3D& pos, float size, const Vec4D& color)
{
    // Skip if too far from camera
    float distanceToCamera = (pos - gWorld->camera).length();
    if (distanceToCamera > VIEW_DISTANCE) // Same view distance as labels
        return;

    float half = size / 2.0f;

    // Set the color with transparency
    glColor4f(color.x, color.y, color.z, color.w);

    glBegin(GL_QUADS);

    // Front
    glVertex3f(pos.x - half, pos.y - half, pos.z + half);
    glVertex3f(pos.x + half, pos.y - half, pos.z + half);
    glVertex3f(pos.x + half, pos.y + half, pos.z + half);
    glVertex3f(pos.x - half, pos.y + half, pos.z + half);

    // Back
    glVertex3f(pos.x - half, pos.y - half, pos.z - half);
    glVertex3f(pos.x - half, pos.y + half, pos.z - half);
    glVertex3f(pos.x + half, pos.y + half, pos.z - half);
    glVertex3f(pos.x + half, pos.y - half, pos.z - half);

    // Top
    glVertex3f(pos.x - half, pos.y + half, pos.z - half);
    glVertex3f(pos.x - half, pos.y + half, pos.z + half);
    glVertex3f(pos.x + half, pos.y + half, pos.z + half);
    glVertex3f(pos.x + half, pos.y + half, pos.z - half);

    // Bottom
    glVertex3f(pos.x - half, pos.y - half, pos.z - half);
    glVertex3f(pos.x + half, pos.y - half, pos.z - half);
    glVertex3f(pos.x + half, pos.y - half, pos.z + half);
    glVertex3f(pos.x - half, pos.y - half, pos.z + half);

    // Right
    glVertex3f(pos.x + half, pos.y - half, pos.z - half);
    glVertex3f(pos.x + half, pos.y + half, pos.z - half);
    glVertex3f(pos.x + half, pos.y + half, pos.z + half);
    glVertex3f(pos.x + half, pos.y - half, pos.z + half);

    // Left
    glVertex3f(pos.x - half, pos.y - half, pos.z - half);
    glVertex3f(pos.x - half, pos.y - half, pos.z + half);
    glVertex3f(pos.x - half, pos.y + half, pos.z + half);
    glVertex3f(pos.x - half, pos.y + half, pos.z - half);

    glEnd();
}

bool WorldBotNodes::LoadNodeModel()
{
    if (!gWorld) {
        gLog("Error: gWorld not initialized\n");
        return false;
    }

    modelName = "Creature\\KelThuzad\\KelThuzad.m2";

    // Force animated flag for character models
    nodeModel = new Model(modelName, true);

    nodeModelId = gWorld->modelmanager.add(modelName);
    if (nodeModelId) {
        nodeModel = (Model*)gWorld->modelmanager.items[nodeModelId];
        if (nodeModel && nodeModel->ok) {
            // Debug texture info
            if (nodeModel->HasTextures()) {
                for (size_t i = 0; i < nodeModel->GetTextureCount(); i++) {
                    gLog("Model texture %d: %d\n", i, nodeModel->GetTexture(i));
                }
            }
            else {
                gLog("Model has no textures\n");
            }
            return true;
        }
    }
    gLog("Failed to load node model: %s\n", modelName.c_str());
    return false;
}

void WorldBotNodes::DrawModel(const Vec3D& pos, const Vec4D& color)
{
    if (!nodeModel || !nodeModel->ok)
        return;

    float distanceToCamera = (pos - gWorld->camera).length();
    if (distanceToCamera > VIEW_DISTANCE)
        return;

    GLfloat currentColor[4];
    glGetFloatv(GL_CURRENT_COLOR, currentColor);

    glColor4f(color.x, color.y, color.z, color.w);

    glPushMatrix();
    {
        glTranslatef(pos.x, pos.y, pos.z);

        const float scale = 1.0f;
        glScalef(scale, scale, scale);

        nodeModel->draw();
    }
    glPopMatrix();

    glColor4fv(currentColor);
}

void WorldBotNodes::DrawLinks(int mapId)
{
    glLineWidth(2.0f);  // Thicker lines for links
    glColor4f(0.0f, 1.0f, 0.0f, 0.4f);  // Semi-transparent green

    glBegin(GL_LINES);
    for (const auto& link : links)
    {
        // Find connected nodes
        auto fromNode = std::find_if(nodes.begin(), nodes.end(),
            [&](const TravelNode& n) { return n.id == link.fromNodeId; });
        auto toNode = std::find_if(nodes.begin(), nodes.end(),
            [&](const TravelNode& n) { return n.id == link.toNodeId; });

        if (fromNode != nodes.end() && toNode != nodes.end() &&
            fromNode->mapId == mapId && toNode->mapId == mapId)
        {
            // Check if either end of the link is within view distance
            float distanceFrom = (fromNode->position - gWorld->camera).length();
            float distanceTo = (toNode->position - gWorld->camera).length();

            if (distanceFrom > VIEW_DISTANCE && distanceTo > VIEW_DISTANCE)
                continue;  // Skip if both ends are too far

            glVertex3fv(fromNode->position);
            glVertex3fv(toNode->position);
        }
    }
    glEnd();
    glLineWidth(1.0f);
}

void WorldBotNodes::DrawPathPoints(int mapId)
{
    Vec4D pathColor(1.0f, 1.0f, 0.0f, 0.7f);  // Yellow for path points

    for (const auto& point : pathPoints)
    {
        if (point.mapId != mapId)
            continue;

        // Skip if too far from camera
        float distanceToCamera = (point.position - gWorld->camera).length();
        if (distanceToCamera > VIEW_DISTANCE)
            continue;

        DrawSphere(point.position, PATH_POINT_SIZE, pathColor);
    }
}

void WorldBotNodes::DrawNodeLabel(const TravelNode& node)
{
    Vec3D camera = gWorld->camera;

    // Skip if node is too far from camera
    float distanceToCamera = (node.position - camera).length();
    if (distanceToCamera > VIEW_DISTANCE)
        return;

    // Create label text
    char label[256];
    snprintf(label, sizeof(label), "[%u] %s", node.id, node.name.c_str());

    // Calculate the position above the box in world space
    Vec3D labelPos = node.position;
    labelPos.y += DEFAULT_BOX_SIZE + 2.0f;

    // Transform world position to screen coordinates
    Vec2D screenPos;
    bool isVisible;
    if (!WorldToScreen(labelPos, screenPos, isVisible) || !isVisible)
        return;

    // Save states
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Switch to 2D rendering mode
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, viewport[2], viewport[3], 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Calculate text position
    float textWidth = f16->textwidth(label);
    float screenX = screenPos.x - (textWidth / 2.0f);
    float screenY = screenPos.y;

    // Setup text rendering
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw text outline
    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            f16->print(screenX + dx, screenY + dy, "%s", label);
        }
    }

    // Draw text
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    f16->print(screenX, screenY, "%s", label);

    // Restore states
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}

bool WorldBotNodes::WorldToScreen(const Vec3D& worldPos, Vec2D& screenPos, bool& isVisible)
{
    // Get current matrices and viewport
    GLint viewport[4];
    GLdouble mvmatrix[16], projmatrix[16];

    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projmatrix);

    // Project world position to screen coordinates
    GLdouble winX, winY, winZ;
    gluProject(worldPos.x, worldPos.y, worldPos.z,
        mvmatrix, projmatrix, viewport,
        &winX, &winY, &winZ);

    // Check if point is behind camera or outside frustum
    isVisible = (winZ <= 1.0f);
    if (!isVisible)
        return false;

    // Convert OpenGL screen coordinates to our desired screen coordinates
    screenPos.x = winX;
    screenPos.y = viewport[3] - winY;  // Flip Y coordinate

    return true;
}
