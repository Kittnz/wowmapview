#ifndef WOWMAPVIEW_H
#define WOWMAPVIEW_H

#include <vector>
#include <string>
#include "appstate.h"
#include "font.h"
/// XXX this really needs to be refactored into a singleton class

#define APP_TITLE "WoW Map Viewer"

#define APP_VERSION "v0.5.1"

extern std::string gamePath;
extern int expansion;

extern std::vector<AppState*> gStates;
extern bool gPop;

extern Font *f16, *f24, *f32;

extern float gFPS;

float frand();
float randfloat(float lower, float upper);
int randint(int lower, int upper);
void fixname(std::string &name);
void fixnamen(char *name, size_t len);
void fixname2(char* name, size_t len);
const char* GetPlainName(const char* FileName);
char* GetPlainName(char* FileName);

bool StartsWith (std::string const &fullString, std::string const &starting);
bool EndsWith (std::string const &fullString, std::string const &ending);
std::string BeforeLast(const char* String, const char* Last);
std::string AfterLast(const char* String, const char* Last);
std::string GetLast(const char* String);

void Lower(std::string &text);
void getGamePath();
void check_stuff();
void gLog(const char *str, ...);
int file_exists(const char *path);

extern int fullscreen;
// Area database
class AreaDB;
extern AreaDB gAreaDB;

#endif
