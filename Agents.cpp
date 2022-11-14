#include "Agents.h"
#include "Directive.h"
#include "Squad.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2api/sc2_client.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

class TriggerCondition;
class SquadMember;

void BotAgent::initVariables() {
	const sc2::ObservationInterface* observation = Observation();
	map_name = observation->GetGameInfo().map_name;

	if (map_name == "Proxima Station LE")
		map_index = 3; else
		if (map_name == "Bel'Shir Vestige LE (Void)" || map_name == "Bel'Shir Vestige LE")
			map_index = 2; else
			if (map_name == "Cactus Valley LE")
				map_index = 1; else
				map_index = 0;

	player_start_id = getPlayerIDForMap(map_index, observation->GetStartLocation());
	initLocations(map_index, player_start_id);

	std::cout << "Map Name: " << map_name << std::endl;
	std::cout << "Player Start ID: " << player_start_id << std::endl;
}

SquadMember* BotAgent::getSquadMember(const sc2::Unit& unit) {
	assert(unit.alliance == sc2::Unit::Alliance::Self); // only call this on allied units!

	// get the SquadMember object for a given sc2::Unit object
	for (SquadMember* s : squad_members) {
		if (&(s->unit) == &unit) {
			return s;
		}
	}
	return nullptr; // handle erroneous case where unit has no squad
}

std::vector<SquadMember*> BotAgent::getIdleWorkers() {
	// get all worker units with no active orders
	std::vector<SquadMember*> idle_workers;
	
	std::vector<SquadMember*> workers = filter_by_flag(squad_members, FLAGS::IS_WORKER);
	for (SquadMember* s : workers) {
		if (s->is_idle()) {
			idle_workers.push_back(s);
		}
	}
	
	return idle_workers;
}

void BotAgent::initStartingUnits() {
	// add all starting units to their respective squads
	const sc2::ObservationInterface* observation = Observation();
	sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self);
	for (const sc2::Unit* u : units) {
		sc2::UNIT_TYPEID u_type = u->unit_type;
		if (u_type == sc2::UNIT_TYPEID::TERRAN_SCV ||
			u_type == sc2::UNIT_TYPEID::ZERG_DRONE ||
			u_type == sc2::UNIT_TYPEID::PROTOSS_PROBE) 
		{
			SquadMember* worker = new SquadMember(*u, SQUAD::SQUAD_WORKER);
			Directive directive_get_minerals_near_Base(Directive::DEFAULT_DIRECTIVE, Directive::GET_MINERALS_NEAR_LOCATION, start_location);
			worker->assignDirective(directive_get_minerals_near_Base);
			squad_members.push_back(worker);
		}
		if (u_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS ||
			u_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER ||
			u_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING ||
			u_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND ||
			u_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING ||
			u_type == sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS ||
			u_type == sc2::UNIT_TYPEID::ZERG_HATCHERY ||
			u_type == sc2::UNIT_TYPEID::ZERG_HIVE ||
			u_type == sc2::UNIT_TYPEID::ZERG_LAIR) {
			SquadMember* townhall = new SquadMember(*u, SQUAD::SQUAD_TOWNHALL);
			squad_members.push_back(townhall);
		}
	}
}

void BotAgent::OnGameStart() {
	start_location = Observation()->GetStartLocation();
	BotAgent::initVariables();
	BotAgent::initStartingUnits();
	std::cout << "Start Location: " << start_location.x << "," << start_location.y << std::endl;
	std::cout << "Build Area 0: " << bases[0].get_build_area(0).x << "," << bases[0].get_build_area(0).y << std::endl;

	current_strategy->loadStrategies();
}

void::BotAgent::OnStep_100() {
	// occurs every 100 steps
	const sc2::ObservationInterface* observation = Observation();
	std::vector<SquadMember*> idle_workers = getIdleWorkers();
	for (SquadMember* s: idle_workers) {
		s->executeDefaultDirective(this, observation);
	}
}

void::BotAgent::OnStep_1000() {
	// occurs every 1000 steps
	std::cout << ".";
}

void BotAgent::OnStep() {
	const sc2::ObservationInterface* observation = Observation();
	int minerals = observation->GetMinerals();
	int gameloop = observation->GetGameLoop();
	if (gameloop % 100 == 0) {
		OnStep_100();
	}
	if (gameloop % 1000 == 0) {
		OnStep_1000();
	}

	for (StrategyOrder s : strategies) {
		if (s.checkTriggerConditions(observation)) {
			s.execute(observation);
		}
	}
}

bool BotAgent::have_upgrade(const sc2::UpgradeID upgrade_) {
	const sc2::ObservationInterface* observation = Observation();
	const std::vector<sc2::UpgradeID> upgrades = observation->GetUpgrades();
	return (std::find(upgrades.begin(), upgrades.end(), upgrade_) != upgrades.end());
}

void BotAgent::OnBuildingConstructionComplete(const sc2::Unit* unit) {
	if (getSquadMember(*unit)) { // unit already belongs to a SquadMember
		return;
	}

	SquadMember* structure = new SquadMember(*unit, SQUAD::SQUAD_STRUCTURE);
	sc2::UNIT_TYPEID unit_type = unit->unit_type;
	if (unit_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS ||
		unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER ||
		unit_type == sc2::UNIT_TYPEID::ZERG_HATCHERY) {
		// a townhall structure was created
		structure->flags.insert(FLAGS::IS_TOWNHALL);
		structure->flags.insert(FLAGS::IS_SUPPLY);
		int base_index = get_index_of_closest_base(unit->pos);
		std::cout << "expansion " << base_index << " has been activated." << std::endl;
		bases[base_index].set_active();
	}
	if (unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON ||
		unit_type == sc2::UNIT_TYPEID::TERRAN_BUNKER ||
		unit_type == sc2::UNIT_TYPEID::TERRAN_MISSILETURRET ||
		unit_type == sc2::UNIT_TYPEID::ZERG_SPINECRAWLER ||
		unit_type == sc2::UNIT_TYPEID::ZERG_SPORECRAWLER) {
		structure->flags.insert(FLAGS::IS_DEFENSE);
	}
	if (unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR ||
		unit_type == sc2::UNIT_TYPEID::TERRAN_REFINERY ||
		unit_type == sc2::UNIT_TYPEID::ZERG_EXTRACTOR ||
		unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATORRICH ||
		unit_type == sc2::UNIT_TYPEID::TERRAN_REFINERYRICH ||
		unit_type == sc2::UNIT_TYPEID::ZERG_EXTRACTORRICH) {
		// assign 3 workers to mine this
		AssignNearbyWorkerToGasStructure(*unit);
		AssignNearbyWorkerToGasStructure(*unit);
		AssignNearbyWorkerToGasStructure(*unit);
	}
	squad_members.push_back(structure);
}

void BotAgent::OnUnitCreated(const sc2::Unit* unit) {
	const sc2::ObservationInterface* observation = Observation();
	if (getSquadMember(*unit)) { // unit already belongs to a SquadMember
		return;
	}

	/*   Testing various stuff
	sc2::QueryInterface* query_interface = Query();
	std::vector<sc2::AvailableAbility> available_abilities = query_interface->GetAbilitiesForUnit(unit).abilities;
	const sc2::UnitTypes units_data = observation->GetUnitTypeData();
	const sc2::Abilities abilities_data = observation->GetAbilityData();
	const sc2::UnitTypeData unit_type_data = units_data[(int)unit->unit_type];
	std::cout << "Abilities for " << unit_type_data.name << ":" << std::endl;
	for (auto a : available_abilities) {
		std::cout << abilities_data[a.ability_id].friendly_name << "(" << a.ability_id << "), ";
	}
	std::cout << std::endl;
	bool test = AbilityAvailable(*unit, sc2::ABILITY_ID::GENERAL_MOVE);
	*/

	SquadMember* new_squad;
	bool squad_created = false;
	sc2::UNIT_TYPEID unit_type = unit->unit_type;
	if (unit_type == sc2::UNIT_TYPEID::TERRAN_SCV |
		unit_type == sc2::UNIT_TYPEID::ZERG_DRONE |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_PROBE) {
		new_squad = new SquadMember(*unit, SQUAD::SQUAD_WORKER);
		int base_index = get_index_of_closest_base(unit->pos);
		Directive directive_get_minerals_near_birth(Directive::DEFAULT_DIRECTIVE, Directive::GET_MINERALS_NEAR_LOCATION, bases[base_index].get_townhall());
		new_squad->assignDirective(directive_get_minerals_near_birth);
		squad_created = true;
	}
	if (unit_type == sc2::UNIT_TYPEID::PROTOSS_STALKER |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_ZEALOT |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_IMMORTAL |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_DARKTEMPLAR |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_ARCHON |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_COLOSSUS |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_HIGHTEMPLAR |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_ADEPT |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_CARRIER |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_COLOSSUS |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_VOIDRAY |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOENIX |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_INTERCEPTOR |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_MOTHERSHIP |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_ORACLE |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_SENTRY |
		unit_type == sc2::UNIT_TYPEID::PROTOSS_TEMPEST)
	{
		new_squad = new SquadMember(*unit, SQUAD::SQUAD_ARMY);
		squad_created = true;
	}
	if (squad_created)
		squad_members.push_back(new_squad);
}

void BotAgent::OnUnitDamaged(const sc2::Unit* unit, float health, float shields) {
	const sc2::ObservationInterface* observation = Observation();
	// make Stalkers Blink away if low health
	if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_STALKER) {
		if (have_upgrade(sc2::UPGRADE_ID::BLINKTECH)) {
			if (unit->health / unit->health_max < .5f) {
				// check if Blink is on cooldown
				if (AbilityAvailable(*unit, sc2::ABILITY_ID::EFFECT_BLINK)) {
					Actions()->UnitCommand(unit, sc2::ABILITY_ID::EFFECT_BLINK, bases[0].get_townhall());
				}
			}
		}
	}
}

bool BotAgent::AbilityAvailable(const sc2::Unit& unit, const sc2::ABILITY_ID ability_) {
	// check if a unit is able to use an ability
	sc2::QueryInterface* query_interface = Query();
	std::vector<sc2::AvailableAbility> abilities = (query_interface->GetAbilitiesForUnit(&unit)).abilities;
	for (auto a : abilities) {
		if (a.ability_id == ability_) {
			return true;
		}
	}
	return false;
}

bool BotAgent::AssignNearbyWorkerToGasStructure(const sc2::Unit& gas_structure) {
	std::unordered_set<FLAGS> flags;
	bool found_viable_unit = false;
	flags.insert(FLAGS::IS_WORKER);
	flags.insert(FLAGS::IS_MINERAL_GATHERER);
	std::vector<SquadMember*> worker_miners = filter_by_flags(squad_members, flags);
	//std::vector<SquadMember*> worker_miners = filter_by_flag(squad_members, FLAGS::IS_MINERAL_GATHERER);
	float distance = std::numeric_limits<float>::max();
	SquadMember* target = nullptr;
	for (SquadMember* s : worker_miners) {
		float d = sc2::DistanceSquared2D(s->unit.pos, gas_structure.pos);
		if (d < distance) {
			distance = d;
			target = s;
			found_viable_unit = true;
		}
	}
	if (found_viable_unit) {
		target->flags.erase(FLAGS::IS_MINERAL_GATHERER);
		target->flags.insert(FLAGS::IS_GAS_GATHERER);
		// make the unit continue to mine gas after being idle
		Directive directive_get_gas(Directive::DEFAULT_DIRECTIVE, Directive::GET_GAS_NEAR_LOCATION, gas_structure.pos);
		target->assignDirective(directive_get_gas);
		Actions()->UnitCommand(&(target->unit), sc2::ABILITY_ID::HARVEST_GATHER, &gas_structure);

		
		return true;
	}
	return false;
}

//void BotAgent::OnUnitIdle(const sc2::Unit* unit) {
	// do stuff
//}

int BotAgent::getPlayerIDForMap(int map_index, sc2::Point2D location) {
	location = getNearestStartLocation(location);
	int p_id = 0;
	switch (map_index) {
	case 1:
		// Cactus Valley LE
		if (location == sc2::Point2D(33.5, 158.5)) {
			// top left
			p_id = 1;
		}
		if (location == sc2::Point2D(158.5, 158.5)) {
			// top right
			p_id = 2;
		}
		if (location == sc2::Point2D(158.5, 158.5)) {
			// bottom right
			p_id = 3;
		}
		if (location == sc2::Point2D(33.5, 33.5)) {
			// bottom left
			p_id = 4;
		}
		break;
	case 2:
		// Bel'Shir Vestige LE
		if (location == sc2::Point2D(114.5, 25.5)) {
			// bottom right
			p_id = 1;
		}
		if (location == sc2::Point2D(29.5, 134.5)) {
			// top left
			p_id = 2;
		}
		break;
	case 3:
		// Proxima Station LE
		if (location == sc2::Point2D(137.5, 139.5)) {
			// top right
			p_id = 1;
		}
		if (location == sc2::Point2D(62.5, 28.5)) {
			// bottom left
			p_id = 2;
		}
		break;
	}
	return p_id;
}

sc2::Point2D BotAgent::getNearestStartLocation(sc2::Point2D spot) {
	// Get the nearest Start Location from a given point
	float nearest_distance = 10000.0f;
	sc2::Point2D nearest_point;

	for (auto& iter : sc2::Client::Observation()->GetGameInfo().start_locations) {
		float dist = sc2::Distance2D(spot, iter);
		if (dist <= nearest_distance) {
			nearest_distance = dist;
			nearest_point = iter;
		}
	}
	return nearest_point;
}

bool isMineralPatch(const sc2::Unit* unit_) {
	// check whether a given unit is a mineral patch
	sc2::UNIT_TYPEID type_ = unit_->unit_type;
	return (type_ == sc2::UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD750 ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_LABMINERALFIELD750 ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_LABMINERALFIELD ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD750 ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD750 ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD750 ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD750 ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD);
}

bool isGeyser(const sc2::Unit* unit_) {
	// check whether a given unit is a geyser
	sc2::UNIT_TYPEID type_ = unit_->unit_type;
	return (type_ == sc2::UNIT_TYPEID::NEUTRAL_VESPENEGEYSER ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_PROTOSSVESPENEGEYSER ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_SPACEPLATFORMGEYSER ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_PURIFIERVESPENEGEYSER ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_SHAKURASVESPENEGEYSER ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_RICHVESPENEGEYSER);
}

const sc2::Unit* BotAgent::FindNearestMineralPatch(sc2::Point2D location) {
	const sc2::ObservationInterface* obs = Observation();
	sc2::Units units = sc2::Client::Observation()->GetUnits(sc2::Unit::Alliance::Neutral);
	float distance = std::numeric_limits<float>::max();
	const sc2::Unit * target = nullptr;
	for (const auto& u : units) {
		if (isMineralPatch(u)) {
			float d = sc2::DistanceSquared2D(u->pos, location);		
			if (d < distance) {
				distance = d;
				target = u;
			}
		}
	}
	return target;
}

const sc2::Unit* BotAgent::FindNearestGeyser(sc2::Point2D location) {
	const sc2::ObservationInterface* obs = Observation();
	sc2::Units units = sc2::Client::Observation()->GetUnits(sc2::Unit::Alliance::Neutral);
	float distance = std::numeric_limits<float>::max();
	const sc2::Unit* target = nullptr;
	for (const auto& u : units) {
		if (isGeyser(u)) {
			float d = sc2::DistanceSquared2D(u->pos, location);
			if (d < distance) {
				distance = d;
				target = u;
			}
		}
	}
	return target;
}

const sc2::Unit* BotAgent::FindNearestGasStructure(sc2::Point2D location) {
	const sc2::ObservationInterface* obs = Observation();
	sc2::Units units = obs->GetUnits(sc2::Unit::Alliance::Self);
	float distance = std::numeric_limits<float>::max();
	const sc2::Unit* target = nullptr;
	for (const auto& u : units) {
		sc2::UNIT_TYPEID unit_type = u->unit_type;
		if (unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR || 
			unit_type == sc2::UNIT_TYPEID::TERRAN_REFINERY ||
			unit_type == sc2::UNIT_TYPEID::ZERG_EXTRACTOR ||
			unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATORRICH ||
			unit_type == sc2::UNIT_TYPEID::TERRAN_REFINERYRICH ||
			unit_type == sc2::UNIT_TYPEID::ZERG_EXTRACTORRICH) {
			float d = sc2::DistanceSquared2D(u->pos, location);
			if (d < distance) {
				distance = d;
				target = u;
			}
		}
	}
	return target;
}

void BotAgent::setCurrentStrategy(Strategy* strategy_) {
	// set the current strategy
	current_strategy = strategy_;
}

std::vector<SquadMember*> BotAgent::filter_by_flag(std::vector<SquadMember*> squad_vector, FLAGS flag) {
	// filter a vector of SquadMember* by the given flag
	std::vector<SquadMember*> filtered_squad;
	
	std::copy_if(squad_vector.begin(), squad_vector.end(), std::back_inserter(filtered_squad),
		[flag](SquadMember* s) { return s->has_flag(flag); });
	
	return filtered_squad;
}

std::vector<SquadMember*> BotAgent::filter_by_flags(std::vector<SquadMember*> squad_vector, std::unordered_set<FLAGS> flag_list) {
	// filter a vector of SquadMember* by several flags
	std::vector<SquadMember*> filtered_squad = squad_vector;
	for (FLAGS f : flag_list) {
		filtered_squad = filter_by_flag(filtered_squad, f);
	}
	return filtered_squad;
}

std::vector<SquadMember*> BotAgent::get_squad_members() {
	return squad_members;
}

int BotAgent::get_index_of_closest_base(sc2::Point2D location_) {
	// get the index of the closest base to a location
	float distance = std::numeric_limits<float>::max();
	int lowest_index = 0;
	for (int i = 0; i < bases.size(); i++) {
		float b_dist = sc2::DistanceSquared2D(bases[i].get_townhall(), location_);
		if (b_dist < distance) {
			lowest_index = i;
			distance = b_dist;
		}
	}
	return lowest_index;
}

void BotAgent::addStrat(StrategyOrder strategy) {
	strategies.push_back(strategy);
}

void BotAgent::initLocations(int map_index, int p_id) {
	const sc2::ObservationInterface* observation = Observation();
	switch (map_index) {
	case 1:
		switch (p_id) {
		case 1:
		{
			Base main_base(observation->GetStartLocation());
			Base exp_1(66.5, 161.5);
			bases.push_back(exp_1);

			Base exp_2(54.5, 132.5);
			bases.push_back(exp_2);

			Base exp_3(45.5, 20.5);
			bases.push_back(exp_3);

			Base exp_4(93.5, 156.5);
			bases.push_back(exp_4);

			Base exp_5(35.5, 93.5);
			bases.push_back(exp_5);

			Base exp_6(30.5, 66.5);
			bases.push_back(exp_6);

			Base exp_7(132.5, 137.5);
			bases.push_back(exp_7);

			Base exp_8(59.5, 54.5);
			bases.push_back(exp_8);

			Base exp_9(161.5, 125.5);
			bases.push_back(exp_9);

			Base exp_10(156.5, 98.5);
			bases.push_back(exp_10);

			Base exp_11(98.5, 35.5);
			bases.push_back(exp_11);

			Base exp_12(125.5, 30.5);
			bases.push_back(exp_12);

			Base exp_13(137.5, 59.5);
			bases.push_back(exp_13);
			break;
		}
		break;
		}
	case 2:
		switch (p_id) {
		case 1:
		{
			enemy_location = sc2::Point2D(114.5, 25.5);
			Base main_base(observation->GetStartLocation());
			bases.push_back(main_base);

			Base exp_1(82.5, 23.5);
			bases.push_back(exp_1);

			Base exp_2(115.5, 63.5);
			bases.push_back(exp_2);

			Base exp_3(45.5, 20.5);
			bases.push_back(exp_3);

			Base exp_4(120.5, 104.5);
			bases.push_back(exp_4);

			Base exp_5(98.5, 138.5);
			bases.push_back(exp_5);

			Base exp_6(23.5, 55.5);
			bases.push_back(exp_6);

			Base exp_7(28.5, 96.5);
			bases.push_back(exp_7);

			Base exp_8(61.5, 136.5);
			bases.push_back(exp_8);
			break;
		}
		case 2:
		{
			enemy_location = sc2::Point2D(29.5, 134.5);
			Base main_base(observation->GetStartLocation());
			bases.push_back(main_base);

			Base exp_1(82.5, 23.5);
			bases.push_back(exp_1);

			Base exp_1(61.5, 136.5);
			bases.push_back(exp_1);

			Base exp_2(28.5, 96.5);
			bases.push_back(exp_2);

			Base exp_3(23.5, 55.5);
			bases.push_back(exp_3);

			Base exp_4(98.5, 138.5);
			bases.push_back(exp_4);

			Base exp_5(120.5, 104.5);
			bases.push_back(exp_5);

			Base exp_6(45.5, 20.5);
			bases.push_back(exp_6);

			Base exp_7(115.5, 63.5);
			bases.push_back(exp_7);

			Base exp_8(82.5, 23.5);
			bases.push_back(exp_8);
			break;
		}
		break;
		}
	case 3:
		switch (p_id) {
		case 1:
		{
			proxy_location = sc2::Point2D(28.0, 56.0);
			enemy_location = sc2::Point2D(62.5, 28.5);

			Base main_base(observation->GetStartLocation());
			main_base.add_build_area(146.0, 128.0);
			main_base.add_build_area(132.0, 132.0);
			main_base.add_defend_point(149.0, 120.0);
			main_base.set_active();
			bases.push_back(main_base);

			Base exp_1(164.5, 140.5);
			exp_1.add_build_area(165.0, 135.0);
			exp_1.add_defend_point(168.0, 132.0);
			bases.push_back(exp_1);

			Base exp_2(149.5, 102.5);
			exp_2.add_build_area(144.0, 106.0);
			exp_2.add_defend_point(141.0, 95.0);
			bases.push_back(exp_2);

			Base exp_3(166.5, 69.5);
			exp_3.add_build_area(161.0, 79.0);
			exp_3.add_defend_point(155.0, 69.0);
			bases.push_back(exp_3);

			Base exp_4(119.5, 111.5);
			exp_4.add_defend_point(115.0, 105.0);
			exp_4.add_defend_point(110.0, 115.0);
			bases.push_back(exp_4);

			Base exp_5(127.5, 57.5);
			exp_5.add_defend_point(122.0, 69.0);
			exp_5.add_build_area(137.0, 54.0);
			bases.push_back(exp_5);

			Base exp_6(165.5, 23.5);
			exp_6.add_defend_point(157.0, 23.0);
			bases.push_back(exp_6);

			Base exp_7(93.5, 147.5);
			exp_7.add_defend_point(94.0, 141.0);
			bases.push_back(exp_7);

			Base exp_8(106.5, 20.5);
			exp_8.add_defend_point(106.0, 27.0);
			bases.push_back(exp_8);

			Base exp_9(34.5, 144.5);
			exp_9.add_defend_point(43.0, 145.0);
			bases.push_back(exp_9);

			Base exp_10(72.5, 110.5);
			exp_10.add_defend_point(78.0, 99.0);
			exp_10.add_defend_point(63.0, 114.0);
			bases.push_back(exp_10);

			Base exp_11(80.5, 56.5);
			exp_11.add_defend_point(85.0, 63.0);
			exp_11.add_defend_point(90.0, 53.0);
			bases.push_back(exp_11);

			Base exp_12(33.5, 98.5);
			exp_12.add_build_area(39.0, 89.0);
			exp_12.add_defend_point(45.0, 99.0);
			bases.push_back(exp_12);

			Base exp_13(50.5, 65.5);
			exp_13.add_build_area(56.0, 62.0);
			exp_13.add_defend_point(59.0, 73.0);
			bases.push_back(exp_13);

			Base exp_14(35.5, 27.5);
			exp_14.add_build_area(35.0, 33.0);
			exp_14.add_defend_point(32.0, 36.0);
			bases.push_back(exp_14);

			break;
		}
		case 2:
		{
			proxy_location = sc2::Point2D(172.0, 112.0);
			enemy_location = sc2::Point2D(137.5, 139.5);
			Base main_base(observation->GetStartLocation());
			main_base.add_build_area(54.0, 40.0);
			main_base.add_build_area(68.0, 36.0);
			main_base.add_defend_point(51.0, 48.0);
			main_base.set_active();
			bases.push_back(main_base);

			Base exp_1(35.5, 27.5);
			exp_1.add_build_area(35.0, 33.0);
			exp_1.add_defend_point(32.0, 36.0);
			bases.push_back(exp_1);

			Base exp_2(50.5, 65.5);
			exp_2.add_build_area(56.0, 62.0);
			exp_2.add_defend_point(59.0, 73.0);
			bases.push_back(exp_2);

			Base exp_3(33.5, 98.5);
			exp_3.add_build_area(39.0, 89.0);
			exp_3.add_defend_point(45.0, 99.0);
			bases.push_back(exp_3);

			Base exp_4(80.5, 56.5);
			exp_4.add_defend_point(85.0, 63.0);
			exp_4.add_defend_point(90.0, 53.0);
			bases.push_back(exp_4);

			Base exp_5(72.5, 110.5);
			exp_5.add_defend_point(78.0, 99.0);
			exp_5.add_defend_point(63.0, 114.0);
			bases.push_back(exp_5);

			Base exp_6(34.5, 144.5);
			exp_6.add_defend_point(43.0, 145.0);
			bases.push_back(exp_6);

			Base exp_7(106.5, 20.5);
			exp_7.add_defend_point(106.0, 27.0);
			bases.push_back(exp_7);

			Base exp_8(93.5, 147.5);
			exp_8.add_defend_point(94.0, 141.0);
			bases.push_back(exp_8);

			Base exp_9(165.5, 23.5);
			exp_9.add_defend_point(157.0, 23.0);
			bases.push_back(exp_9);

			Base exp_10(127.5, 57.5);
			exp_10.add_defend_point(122.0, 69.0);
			exp_10.add_build_area(137.0, 54.0);
			bases.push_back(exp_10);

			Base exp_11(119.5, 111.5);
			exp_11.add_defend_point(115.0, 105.0);
			exp_11.add_defend_point(110.0, 115.0);
			bases.push_back(exp_11);

			Base exp_12(166.5, 69.5);
			exp_12.add_build_area(161.0, 79.0);
			exp_12.add_defend_point(155.0, 69.0);
			bases.push_back(exp_12);

			Base exp_13(149.5, 102.5);
			exp_13.add_build_area(144.0, 106.0);
			exp_13.add_defend_point(141.0, 95.0);
			bases.push_back(exp_13);

			Base exp_14(164.5, 140.5);
			exp_14.add_build_area(165.0, 135.0);
			exp_14.add_defend_point(168.0, 132.0);
			bases.push_back(exp_14);

			break;
		}
		break;
		}
	}
}