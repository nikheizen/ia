#include "DungeonClimb.h"

#include "Engine.h"
#include "Map.h"
#include "MapBuildBSP.h"
#include "MapBuild.h"
#include "PopulateMonsters.h"
#include "PopulateItems.h"
#include "ActorPlayer.h"
#include "Log.h"
#include "Item.h"

#include "PlayerAllocBonus.h"

void DungeonClimb::makeLevel() {
	tracer << "DungeonClimb::makeLevel()..." << endl;
	const int DLVL = eng->map->getDungeonLevel();
	bool levelBuilt = false;
	if(eng->config->BOT_PLAYING == false) {
		//------------------------------------- TRAPEZOHEDRON LEVEL
		if(levelBuilt == false) {
			if(DLVL > LAST_CAVERN_LEVEL) {
				tracer << "DungeonClimb: Calling MapBuild::buildTrapezohedronLevel()" << endl;
				eng->mapBuild->buildTrapezohedronLevel();
				levelBuilt = true;
			}
		}

		//------------------------------------- KINGS TOMB
		if(levelBuilt == false) {
			if(DLVL == FIRST_CAVERN_LEVEL - 1) {
				tracer << "DungeonClimb: Calling MapBuild::buildKingsTomb()" << endl;
				eng->mapBuild->buildKingsTomb();
				levelBuilt = true;
			}
		}
	}
	//------------------------------------- DUNGEON LEVELS
	if(levelBuilt == false) {
		if(DLVL < FIRST_CAVERN_LEVEL || eng->config->BOT_PLAYING) {
			//eng->mapBuild->buildDungeonLevel();
			tracer << "DungeonClimb: Calling MapBuildBSP::run()" << endl;
			eng->mapBuildBSP->run();
//			tracer << "DungeonClimb: Calling Populate::populate()" << endl;
//			eng->populateMonsters->populate();
			levelBuilt = true;
		}
	}
	//------------------------------------- CAVERN LEVELS
	if(levelBuilt == false) {
		if(DLVL >= FIRST_CAVERN_LEVEL) {
			tracer << "DungeonClimb: Calling MapBuild::buildCavern()" << endl;
			eng->mapBuild->buildCavern();
//			tracer << "DungeonClimb: Calling Populate::populate()" << endl;
//			eng->populate->populate();
		}
	}
	if(DLVL > 0 && DLVL <= LAST_CAVERN_LEVEL) {
		tracer << "DungeonClimb: Calling PopulateItems::spawnItems()" << endl;
		eng->populateItems->spawnItems();
	}
	tracer << "DungeonClimb::makeLevel() [DONE]" << endl;
}

void DungeonClimb::travelDown(const int levels) {
	tracer << "DungeonClimb::travelDown()..." << endl;

	eng->player->restoreShock();

	eng->map->incrDungeonLevel(levels);
	makeLevel();

	eng->player->target = NULL;
	eng->player->FOVupdate();
	eng->player->updateColor();
	eng->renderer->drawMapAndInterface();
	tracer << "DungeonClimb::travelDown() [DONE]" << endl;
}

void DungeonClimb::attemptUseDownStairs() {
	tracer << "DungeonClimb::attemptUseDownStairs()..." << endl;
	const int DLVL = eng->map->getDungeonLevel();

	const coord& playerPos = eng->player->pos;
	if(eng->map->featuresStatic[playerPos.x][playerPos.y]->getId() == feature_stairsDown) {
		tracer << "DungeonClimb: Player is on stairs" << endl;
		if(DLVL >= FIRST_CAVERN_LEVEL && DLVL <= LAST_CAVERN_LEVEL) {
			eng->log->addMessage("I climb downwards.");
		} else {
			eng->log->addMessage("I descend the stairs.");
		}
		eng->renderer->updateWindow();
		travelDown();
	} else {
		if(DLVL >= FIRST_CAVERN_LEVEL && DLVL <= LAST_CAVERN_LEVEL) {
			eng->log->addMessage("I see no path leading downwards here.");
		} else {
			eng->log->addMessage("I see no stairs leading downwards here.");
		}
		eng->renderer->updateWindow();
	}
	tracer << "DungeonClimb::attemptUseDownStairs() [DONE]" << endl;
}

