#pragma once
#include "BasicSc2Bot.h"
#include "sc2api/sc2_api.h"

struct Strategy {
	Strategy(BasicSc2Bot* bot_); // constructor
	Strategy(const Strategy& rhs);	// copy constructor
	Strategy& operator=(const Strategy& rhs);	// copy assignment operator
	~Strategy();	// destructor
	void loadGameSettings(int *map_index, sc2::Race *bot_race, sc2::Race *opp_race, sc2::Difficulty *difficulty, bool *human_player=false, bool *fullscreen=false, bool *realtime=false);
	void loadStrategies();

	BasicSc2Bot* bot;
private:
	std::vector<const char*> maps;
	int map_index;
	int p_id;
};
