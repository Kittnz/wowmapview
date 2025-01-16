#ifndef TEST_H
#define TEST_H

#include "appstate.h"
#include "video.h"
#include "world.h"
#include "gui_manager.h"
#include "Objects/WorldObjectManipulator.h"

class GuiManager;

class Test :public AppState
{
public:

	float ah, av, moving, strafing, updown, mousedir, movespd;
	bool look;
	bool mapmode;
	bool hud;

	Test(World *w, float ah0 = -90.0f, float av0 = -30.0f);
	int getMapId() const { return world->currentMapId; }
	~Test();

	void tick(float t, float dt);
	void display(float t, float dt);

	void keypressed(SDL_KeyboardEvent *e);
	void mousemove(SDL_MouseMotionEvent *e);
	void mouseclick(SDL_MouseButtonEvent *e);

	void moveToNearestNode();
	void moveToNode(const TravelNode& node);

	GuiManager guiManager;
	GLuint tex;

	World* world;

	std::unique_ptr<WorldObjectManipulator> manipulator;

	void detectRoadPoints();

private:
	Vec3D CalculateRayDir(int mouseX, int mouseY);
	bool IntersectRayPlane(const Vec3D& origin, const Vec3D& dir, const Vec3D& normal, float d, float& t);

};


#endif
