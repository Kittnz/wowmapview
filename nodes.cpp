#include "nodes.h"
#include "wowmapview.h"
#include "world.h"
#include "model.h"
#include "Database/Database.h"
#include "Objects/WorldObject.h"
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

WorldBotNodes::WorldBotNodes() : 
    nodeModel(nullptr), linkVBO(0), linksNeedUpdate(true)
{
    InitVBO();
}

WorldBotNodes::~WorldBotNodes()
{
    if (nodeModelId) {
        gWorld->modelmanager.delbyname(modelName);
    }

    if (linkVBO) {
        glDeleteBuffersARB(1, &linkVBO);
    }

    if (pathPointVBO) {
        glDeleteBuffersARB(1, &pathPointVBO);
    }

    // Clear links for each node
    for (auto& node : nodes) {
        node.links.clear();
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

            //gLog("Loaded travel node %u: %s (%.2f, %.2f, %.2f) on map %u\n", node.id, node.name.c_str(), node.x, node.y, node.z, node.mapId);

            Position nodePos(node.x, node.y, node.z, 0.0f);
            Position viewerPos = WorldObject::ConvertGameCoordsToViewerCoords(nodePos);

            node.position = Vec3D(viewerPos.x, viewerPos.y, viewerPos.z);
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

    // Link path points to their respective links and nodes
    for (auto& link : links)
    {
        // Find corresponding nodes for this link
        auto fromNode = std::find_if(nodes.begin(), nodes.end(),
            [&](const TravelNode& n) { return n.id == link.fromNodeId; });

        auto toNode = std::find_if(nodes.begin(), nodes.end(),
            [&](const TravelNode& n) { return n.id == link.toNodeId; });

        if (fromNode != nodes.end() && toNode != nodes.end())
        {
            // Add link to the from node's links
            fromNode->links.push_back(&link);

            // Find path points for this link
            for (auto& point : pathPoints)
            {
                if (point.fromNodeId == link.fromNodeId && point.toNodeId == link.toNodeId)
                {
                    link.points.push_back(&point);
                }
            }

            // Sort path points by their nr to ensure correct order
            std::sort(link.points.begin(), link.points.end(),
                [](const TravelNodePathPoint* a, const TravelNodePathPoint* b) {
                    return a->nr < b->nr;
                });
        }
    }

    // Update VBOs
    linksNeedUpdate = true;
    UpdateLinkVBO();
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
    // Save OpenGL state
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();

    // Setup state for 3D geometry
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);

    // Draw boxes and links
    for (const auto& node : nodes)
    {
        if (node.mapId != mapId)
            continue;

        float distanceToCamera = (node.position - gWorld->camera).length();
        if (distanceToCamera > VIEW_DISTANCE)
            continue;

        // Draw node box
        Vec4D color = node.linked ?
            Vec4D(0.0f, 1.0f, 0.0f, 0.7f) : // Green for linked
            Vec4D(1.0f, 0.0f, 0.0f, 0.7f);  // Red for unlinked

        DrawBox(node.position, DEFAULT_BOX_SIZE, color);
    }

    DrawPathPoints(mapId);

    // Restore state
    glPopMatrix();
    glPopAttrib();
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

    // Start with models we know work well
    const char* modelPaths[] = {
        "CREATURE\\WISP\\WISP.M2",
        //"CREATURE\\CHICKEN\\CHICKEN.M2",
        "CREATURE\\MURLOC\\MURLOC.M2",
        /*"CREATURE\\RAT\\RAT.M2",
        "CREATURE\\RABBIT\\RABBIT.M2",
		"CREATURE\\SQUIRREL\\SQUIRREL.M2",
		"CREATURE\\CRAB\\CRAB.M2",*/
        "CREATURE\\KELTHUZAD\\KELTHUZAD.M2"
    };

    // Randomly select a model
    int modelIndex = randint(0, sizeof(modelPaths) / sizeof(modelPaths[0]) - 1);
    modelName = modelPaths[modelIndex];

    // Convert to lowercase for WoW path format
    std::string altPath = modelName;
    std::transform(altPath.begin(), altPath.end(), altPath.begin(), ::tolower);

    // Load model
    nodeModel = new Model(altPath, true);

    if (nodeModel && nodeModel->ok) {
        nodeModelId = gWorld->modelmanager.add(altPath);
        gLog("Successfully loaded model: %s\n", altPath.c_str());

        // Debug texture info
        if (nodeModel->header.nTextures > 0) {
            gLog("Model has %d textures\n", nodeModel->header.nTextures);
            // Could add more texture debugging here
        }
        else {
            gLog("Model has no textures!\n");
        }

        return true;
    }

    // Clean up failed attempt
    if (nodeModelId) {
        gWorld->modelmanager.delbyname(altPath);
        nodeModelId = 0;
    }
    delete nodeModel;
    nodeModel = nullptr;

    gLog("Failed to load model: %s\n", altPath.c_str());
    return false;
}

void WorldBotNodes::DrawModel(const Vec3D& pos, const Vec4D& color)
{
    if (!nodeModel || !nodeModel->ok)
        return;

    float distanceToCamera = (pos - gWorld->camera).length();
    if (distanceToCamera > VIEW_DISTANCE)
        return;

    // Setup rendering states
    glDisable(GL_FOG);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);

    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glAlphaFunc(GL_GREATER, 0.3f);

    // Set model color
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    // Model transformation
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(pos.x, pos.y, pos.z);

    // Rotate model to face a random direction
    static float randomRotation = randint(0, 360);
    glRotatef(randomRotation, 0, 1, 0);

    // Scale model
    const float scale = 2.0f;
    glScalef(scale, scale, scale);

    // Update animation time if model has animations
    if (nodeModel->header.nAnimations > 0) {
        nodeModel->animtime = gWorld->animtime;
        nodeModel->animate(0);
    }

    // Draw the model
    nodeModel->draw();

    // Cleanup states
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_FOG);  // Restore fog if world uses it

    // Reset color and blend mode
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void WorldBotNodes::InitVBO()
{
    // Create VBO
    glGenBuffersARB(1, &linkVBO);
    linksNeedUpdate = true;
    glGenBuffersARB(1, &pathPointVBO);
    pathPointsNeedUpdate = true;
}

void WorldBotNodes::UpdateLinkVBO()
{
    if (!linksNeedUpdate)
        return;

    linkVertices.clear();

    // Prepare vertex data
    for (const auto& link : links)
    {
        auto fromNode = std::find_if(nodes.begin(), nodes.end(),
            [&](const TravelNode& n) { return n.id == link.fromNodeId; });
        auto toNode = std::find_if(nodes.begin(), nodes.end(),
            [&](const TravelNode& n) { return n.id == link.toNodeId; });

        if (fromNode != nodes.end() && toNode != nodes.end())
        {
            LinkVertexData vertexData;
            vertexData.start = fromNode->position;
            vertexData.end = toNode->position;
            linkVertices.push_back(vertexData);
        }
    }

    // Upload to GPU
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, linkVBO);
    glBufferDataARB(GL_ARRAY_BUFFER_ARB, linkVertices.size() * sizeof(LinkVertexData),
        linkVertices.data(), GL_STATIC_DRAW_ARB);
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

    linksNeedUpdate = false;
}

void WorldBotNodes::DrawLinks(int mapId)
{
    if (linkVertices.empty())
        return;

    glLineWidth(2.0f);
    glColor4f(0.0f, 1.0f, 0.0f, 0.4f);

    glBindBufferARB(GL_ARRAY_BUFFER_ARB, linkVBO);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(Vec3D), 0);

    for (size_t i = 0; i < linkVertices.size(); i++)
    {
        const LinkVertexData& link = linkVertices[i];
        float distanceFrom = (link.start - gWorld->camera).length();
        float distanceTo = (link.end - gWorld->camera).length();

        if (distanceFrom > VIEW_DISTANCE && distanceTo > VIEW_DISTANCE)
            continue;

        glDrawArrays(GL_LINES, i * 2, 2);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    glLineWidth(1.0f);
}

void WorldBotNodes::UpdatePathPointVBO()
{
    pathPointVertices.clear();

    Vec4D startNodeColor(0.0f, 1.0f, 0.0f, 0.7f);    // Green for start nodes
    Vec4D endNodeColor(1.0f, 0.0f, 0.0f, 0.7f);      // Red for end nodes
    Vec4D pathPointColor(1.0f, 1.0f, 0.0f, 0.7f);    // Yellow for path points
    Vec4D connectionColor(0.5f, 0.5f, 1.0f, 0.7f);   // Blue for connections

    for (const auto& node : nodes)
    {
        if (node.mapId != gWorld->currentMapId)
            continue;

        for (const auto& link : node.links)
        {
            auto toNode = std::find_if(nodes.begin(), nodes.end(),
                [&](const TravelNode& n) { return n.id == link->toNodeId; });

            if (toNode->mapId != gWorld->currentMapId)
                continue;

            // If no path points, draw direct line between nodes
            if (link->points.empty())
            {
                pathPointVertices.push_back({
                    node.position.x, node.position.y, node.position.z,
                    startNodeColor.x, startNodeColor.y, startNodeColor.z, startNodeColor.w,
                    PathPointVertex::LINE
                    });
                pathPointVertices.push_back({
                    toNode->position.x, toNode->position.y, toNode->position.z,
                    endNodeColor.x, endNodeColor.y, endNodeColor.z, endNodeColor.w,
                    PathPointVertex::LINE
                    });
                continue;
            }

            // Draw line from start node to first path point
            pathPointVertices.push_back({
                node.position.x, node.position.y, node.position.z,
                startNodeColor.x, startNodeColor.y, startNodeColor.z, startNodeColor.w,
                PathPointVertex::LINE
                });
            pathPointVertices.push_back({
                link->points.front()->position.x,
                link->points.front()->position.y,
                link->points.front()->position.z,
                connectionColor.x, connectionColor.y, connectionColor.z, connectionColor.w,
                PathPointVertex::LINE
                });

            // Draw lines between path points
            for (auto it = link->points.begin(); it != link->points.end(); ++it)
            {
                auto nextIt = std::next(it);
                if (nextIt == link->points.end())
                {
                    // Draw line to end node for last point
                    pathPointVertices.push_back({
                        (*it)->position.x, (*it)->position.y, (*it)->position.z,
                        connectionColor.x, connectionColor.y, connectionColor.z, connectionColor.w,
                        PathPointVertex::LINE
                        });
                    pathPointVertices.push_back({
                        toNode->position.x, toNode->position.y, toNode->position.z,
                        endNodeColor.x, endNodeColor.y, endNodeColor.z, endNodeColor.w,
                        PathPointVertex::LINE
                        });
                }
                else
                {
                    // Draw line to next point
                    pathPointVertices.push_back({
                        (*it)->position.x, (*it)->position.y, (*it)->position.z,
                        connectionColor.x, connectionColor.y, connectionColor.z, connectionColor.w,
                        PathPointVertex::LINE
                        });
                    pathPointVertices.push_back({
                        (*nextIt)->position.x, (*nextIt)->position.y, (*nextIt)->position.z,
                        connectionColor.x, connectionColor.y, connectionColor.z, connectionColor.w,
                        PathPointVertex::LINE
                        });
                }
            }
        }
    }

    // If vertices are empty, return
    if (pathPointVertices.empty())
        return;

    // Delete existing VBO if it exists
    if (pathPointVBO)
        glDeleteBuffersARB(1, &pathPointVBO);

    // Generate new VBO
    glGenBuffersARB(1, &pathPointVBO);
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, pathPointVBO);
    glBufferDataARB(GL_ARRAY_BUFFER_ARB,
        pathPointVertices.size() * sizeof(PathPointVertex),
        pathPointVertices.data(),
        GL_STATIC_DRAW_ARB);
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

void WorldBotNodes::DrawPathPoints(int mapId)
{
    if (pathPointVertices.empty())
        UpdatePathPointVBO();

    if (pathPointVertices.empty())
        return;

    glLineWidth(2.0f);  // Thicker lines for better visibility

    // Bind VBO and setup vertex pointers
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, pathPointVBO);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(PathPointVertex), 0);

    // Enable color pointer
    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_FLOAT, sizeof(PathPointVertex), (void*)(3 * sizeof(float)));

    // Draw all lines
    glDrawArrays(GL_LINES, 0, pathPointVertices.size());

    // Cleanup
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    glLineWidth(1.0f);  // Reset line width
}

void WorldBotNodes::DrawNodeLabel(const TravelNode& node)
{
    // Skip if node is too far from camera 
    float distanceToCamera = (node.position - gWorld->camera).length();
    if (distanceToCamera > VIEW_DISTANCE)
        return;

    // Calculate screen position
    Vec2D screenPos;
    bool isVisible;
    WorldToScreen(node.position, screenPos, isVisible);

    // Create label text
    char label[256];
    snprintf(label, sizeof(label), "[%u] %s", node.id, node.name.c_str());
    float textWidth = f16->textwidth(label);

    // Save current matrices
    glPushMatrix();

    // Switch to orthographic projection
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glOrtho(0, viewport[2], viewport[3], 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Minimal state changes required for font rendering
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Center text horizontally
    float x = screenPos.x - textWidth / 2;
    float y = screenPos.y;

    // Draw text outline
    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
    f16->drawtext(x - 1, y - 1, label);
    f16->drawtext(x - 1, y + 1, label);
    f16->drawtext(x + 1, y - 1, label);
    f16->drawtext(x + 1, y + 1, label);

    // Draw text
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    f16->drawtext(x, y, label);

    // Restore matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void WorldBotNodes::DrawAllNodeLabels(int mapId) {
    for (const auto& node : nodes) {
        if (node.mapId != mapId)
            continue;

        DrawNodeLabel(node);
    }
}

bool WorldBotNodes::WorldToScreen(const Vec3D& worldPos, Vec2D& screenPos, bool& isVisible)
{
    // First do a quick check if the point is behind the camera
    Vec3D toNode = worldPos - gWorld->camera;
    Vec3D viewDir = (gWorld->lookat - gWorld->camera).normalize();
    float dotProduct = viewDir * toNode.normalize();

    // If point is behind camera (dot product < 0), reject immediately
    if (dotProduct < 0.0f) {
        isVisible = false;
        screenPos.x = -1000; // Put well off screen
        screenPos.y = -1000;
        return false;
    }

    // Distance check
    float distance = toNode.length();
    if (distance > VIEW_DISTANCE) {
        isVisible = false;
        screenPos.x = -1000;
        screenPos.y = -1000;
        return false;
    }

    // Save current matrix mode
    GLint matrixMode;
    glGetIntegerv(GL_MATRIX_MODE, &matrixMode);

    // Setup camera view matrices
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (GLfloat)video.xres / (GLfloat)video.yres, 1.0f, 1024.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(gWorld->camera.x, gWorld->camera.y, gWorld->camera.z,
        gWorld->lookat.x, gWorld->lookat.y, gWorld->lookat.z,
        0, 1, 0);

    // Get matrices and viewport
    GLint viewport[4];
    GLdouble modelMatrix[16], projMatrix[16];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);

    GLdouble winX, winY, winZ;

    // Project world coordinates
    if (gluProject(worldPos.x, worldPos.y, worldPos.z,
        modelMatrix, projMatrix, viewport,
        &winX, &winY, &winZ) == GL_TRUE)
    {
        // Check if point is within screen bounds
        const float margin = 10.0f;
        isVisible = winZ >= 0.0f && winZ <= 1.0f;

        if (isVisible) {
            screenPos.x = winX;
            screenPos.y = viewport[3] - winY;  // Flip Y coordinate

            isVisible = screenPos.x >= margin &&
                screenPos.x < (viewport[2] - margin) &&
                screenPos.y >= margin &&
                screenPos.y < (viewport[3] - margin);
        }

        if (!isVisible) {
            screenPos.x = -1000;
            screenPos.y = -1000;
        }

        // Debug output
        /*static int debugCounter = 0;
        if (debugCounter++ % 100 == 0) {
            if (isVisible) {
                gLog("Drawing label: World pos (%.1f,%.1f,%.1f) -> Screen (%.1f,%.1f,%.1f) dot=%.2f\n",
                    worldPos.x, worldPos.y, worldPos.z,
                    screenPos.x, screenPos.y, winZ, dotProduct);
            }
        }*/

        glMatrixMode(matrixMode);
        return isVisible;
    }

    glMatrixMode(matrixMode);
    isVisible = false;
    screenPos.x = -1000;
    screenPos.y = -1000;
    return false;
}
