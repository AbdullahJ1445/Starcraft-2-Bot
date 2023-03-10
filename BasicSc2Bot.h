#pragma once

#include "Directive.h"
#include "Triggers.h"
#include "Mob.h"
#include "LocationHandler.h"
#include "Base.h"
#include "Strategy.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2api/sc2_client.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"
#include "MobHandler.h"

class Precept;
class Mob;
class Base;
class Strategy;
class MobHandler; 

#define STEP_SIZE 1 // should be 1 when submitted


class Human : public sc2::Agent {
public:
	virtual void OnGameStart() final {
		Debug()->DebugTextOut("Testing with Human");
		Debug()->SendDebug();
	}
};

class BasicSc2Bot : public sc2::Agent {
public:

	// public functions

	BasicSc2Bot();
	BasicSc2Bot(const BasicSc2Bot& rhs);
	BasicSc2Bot& operator=(const BasicSc2Bot& rhs);
	~BasicSc2Bot();
	void setLoadingProgress(int loaded_);
	int getLoadingProgress();
	void setCurrentStrategy(Strategy* strategy_);
	void BasicSc2Bot::addStrat(Precept precept_);
	void storeDirective(Directive directive_);
	void storeStrategy(Strategy strategy_);
	void storeUnitType(std::string identifier_, sc2::UNIT_TYPEID unit_type_);
	void storeLocation(std::string identifier_, sc2::Point2D location_);
	int getMapIndex();
	void setTimer1(int steps_);
	void setTimer2(int steps_);
	void setTimer3(int steps_);
	int getStepsPastTimer1();
	int getStepsPastTimer2();
	int getStepsPastTimer3();
	int getTimer1Value();
	int getTimer2Value();
	int getTimer3Value();
	void resetTimer1();
	void resetTimer2();
	void resetTimer3();
	void setInitialized();
	std::string gameTime(int steps_);

	// various bool functions
	bool haveUpgrade(const sc2::UpgradeID upgrade_);
	bool canUnitUseAbility(const sc2::Unit& unit, const sc2::ABILITY_ID ability_);
	bool isStructure(const sc2::Unit* unit);
	bool isStructure(sc2::UNIT_TYPEID unit_type);
	void storeInt(std::string identifier_, int value_);
	bool isMineralPatch(const sc2::Unit* unit_);
	bool isGeyser(const sc2::Unit* unit_);

	// various getters
	std::vector<sc2::Attribute> getAttributes(const sc2::Unit* unit);
	sc2::UnitTypeData getUnitTypeData(const sc2::Unit* unit);
	int getMineralCost(const sc2::Unit* unit);
	int getGasCost(const sc2::Unit* unit);
	int getFoodCost(const sc2::Unit* unit);
	float getValue(const sc2::Unit* unit);
	sc2::UNIT_TYPEID getUnitType(std::string identifier_);
	sc2::Point2D getStoredLocation(std::string identifier_);
	int getStoredInt(std::string identifier_);
	Directive* getLastStoredDirective();
	void checkBuildingQueues();
	void listUnitSummary();
	sc2::Race getEnemyRace();
	std::unordered_set<const sc2::Unit*> getEnemyUnits();

	// public variables 
	MobHandler* mobH;
	LocationHandler* locH;
	Mob* proxy_worker;
	std::unordered_map<size_t, Directive*> directive_by_id;
	Strategy* current_strategy;
	int time_of_first_attack; // recorded for data gathering purposes
	int time_first_attacked;   // recorded for data gathering purposes
	int reset_shield_overcharge;  // used to prevent every nexus from using the ability at the same time

private:

	// private functions
	void loadStep_01(); // breaking loading into sequential segments, so that nothing is referenced before it is initialized
	void loadStep_02();
	void loadStep_03();
	void loadStep_04();
	void loadStep_05();
	void onStep_100(const sc2::ObservationInterface* obs);
	void onStep_1000(const sc2::ObservationInterface* obs);
	bool addEnemyUnit(const sc2::Unit* unit);
	bool flushOrders();
	void checkBuildingsStatus();
	void checkGasStructures();

	// virtual functions 
	// These functions must use PascalCase instead of camelCase since they must match the sc2 api's function names
	virtual void OnGameStart();
	virtual void OnStep();
	void checkSiegeTanks();
	virtual void OnGameEnd();
	virtual void OnBuildingConstructionComplete(const sc2::Unit* unit);
	virtual void OnUnitCreated(const sc2::Unit* unit);
	virtual void OnUnitIdle(const sc2::Unit* unit);
	virtual void OnUnitDamaged(const sc2::Unit* unit, float health, float shields);
	virtual void OnUnitDestroyed(const sc2::Unit* unit);
	virtual void OnUnitEnterVision(const sc2::Unit* unit);


	// data containers
	std::vector<Precept> precepts_onstep;
	std::vector<std::unique_ptr<Directive>> directive_storage;
	std::unordered_set<Directive*> stored_directives;
	std::unordered_map<std::string, sc2::UNIT_TYPEID> special_units;
	std::unordered_map<std::string, sc2::Point2D> special_locations;
	std::unordered_map<std::string, int> special_ints;
	std::vector<std::unique_ptr<Strategy>> strategy_storage;
	std::vector<sc2::UNIT_TYPEID> data_buildings;
	std::unordered_set<const sc2::Unit*> enemy_units;
	std::unordered_map<sc2::Tag, const sc2::Unit*> enemy_unit_by_tag;
	std::unordered_map<int, int> mineral_cost;
	std::unordered_map<int, int> gas_cost;
	std::unordered_map<int, int> food_cost;
	std::vector<std::pair<int, sc2::UNIT_TYPEID>> units_created;     // record the timestep that each unit was created

	// private variables
	int player_start_id;
	int enemy_start_id;
	std::string map_name;
	sc2::Race enemy_race;
	Mob* special; // for testing purposes
	int timer_1;
	int timer_2;
	int timer_3;
	bool first_friendly_death;
	int loading_progress;
	int townhalls_built;
	bool initialized;
	int gateways_busy;
	int gateways_idle;
	int robotics_busy;
	int robotics_idle;
	int max_minerals;
	int max_gas;	
	int map_index; // 1 = CactusValleyLE,  2 = BelShirVestigeLE,  3 = ProximaStationLE
};