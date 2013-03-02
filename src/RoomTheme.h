#ifndef ROOM_THEME_H
#define ROOM_THEME_H

#include <vector>

#include "ConstTypes.h"
#include "ConstDungeonSettings.h"

using namespace std;

class Engine;
class FeatureDef;

enum RoomTheme_t {
  roomTheme_plain,
  roomTheme_human,
  roomTheme_ritual,
  roomTheme_spider,
  roomTheme_jail,
  roomTheme_tomb,
  roomTheme_monster,

  endOfRoomThemes
};

struct Room;

class RoomThemeMaker {
public:
  RoomThemeMaker(Engine* engine) : eng(engine) {
  }

  void run(const vector<Room*>& rooms);

private:
  //This array supports placing items, monsters and traps
  RoomTheme_t themeMap[MAP_X_CELLS][MAP_Y_CELLS];

  void applyThemeToRoom(Room& room);

  void placeThemeFeatures(Room& room);

  void makeRoomDarkWithChance(const Room& room);

  int attemptSetFeatureToPlace(const FeatureDef** def, coord& pos, vector<coord>& nextToWalls,
                               vector<coord>& awayFromWalls, vector<const FeatureDef*> featureDefsBelongingToTheme);

  void eraseAdjacentCellsFromVectors(const coord& pos,  vector<coord>& nextToWalls, vector<coord>& awayFromWalls);

  void assignRoomThemes(const vector<Room*>& rooms);

  Engine* eng;
};


#endif
