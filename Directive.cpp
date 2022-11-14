#include <cassert>
#include "sc2api/sc2_api.h"
#include "Directive.h"
#include "Agents.h"


Directive::Directive(ASSIGNEE assignee_, ACTION_TYPE action_type_, sc2::UNIT_TYPEID unit_type_, sc2::ABILITY_ID ability_, sc2::Point2D location_, float proximity_) {
	// This constructor is only valid when a unit type is targeting a Point2D location
	assert(assignee_ == ASSIGNEE::UNIT_TYPE);
	assert(action_type_ == ACTION_TYPE::EXACT_LOCATION ||
		action_type_ == ACTION_TYPE::NEAR_LOCATION ||
		action_type_ == ACTION_TYPE::GET_MINERALS_NEAR_LOCATION ||
		action_type_ == ACTION_TYPE::GET_GAS_NEAR_LOCATION);
	action_type = action_type_;
	assignee = assignee_;
	unit_type = unit_type_;
	ability = ability_;
	target_location = location_;
	proximity = proximity_;
}

Directive::Directive(ASSIGNEE assignee_, ACTION_TYPE action_type_, sc2::UNIT_TYPEID unit_type_, sc2::ABILITY_ID ability_, sc2::Unit target_) {
	// This constructor is only valid when a unit type is targeting a unit
	assert(assignee_ == ASSIGNEE::UNIT_TYPE);
	assert(action_type_ == ACTION_TYPE::TARGET_UNIT);
	action_type = action_type_;
	assignee = assignee_;
	unit_type = unit_type_;
	ability = ability_;
	target_unit = target_;
}

Directive::Directive(ASSIGNEE assignee_, ACTION_TYPE action_type_, sc2::Point2D location_, float proximity_) {
	// This constructor is only valid for gathering minerals/gas as a default directive
	assert(assignee_ == ASSIGNEE::DEFAULT_DIRECTIVE);
	assert(action_type_ == ACTION_TYPE::GET_MINERALS_NEAR_LOCATION ||
		action_type_ == ACTION_TYPE::GET_GAS_NEAR_LOCATION);
	action_type = action_type_;
	assignee = assignee_;
	target_location = location_;
	proximity = proximity_;
}

Directive::Directive(ASSIGNEE assignee_, ACTION_TYPE action_type_, std::unordered_set<FLAGS> flags_, sc2::ABILITY_ID ability_, sc2::Point2D location_, float proximity_) {
	// This constructor is only valid for issuing an order to all units matching a flag, towards a location
	assert(assignee_ == ASSIGNEE::MATCH_FLAGS);
	assignee = assignee_;
	action_type = action_type_;
	flags = flags_;
	ability = ability_;
	target_location = location_;
	proximity = proximity_;

}

Directive::Directive(ASSIGNEE assignee_, ACTION_TYPE action_type_, std::unordered_set<FLAGS> flags_, sc2::ABILITY_ID ability_,
	sc2::Point2D assignee_location_, sc2::Point2D target_location_, float assignee_proximity_, float target_proximity_) {
	// This constructor is only valid for issuing an order to all units matching a flag near a location, towards a location
	assert(assignee_ == ASSIGNEE::MATCH_FLAGS_NEAR_LOCATION);
	assignee = assignee_;
	action_type = action_type_;
	flags = flags_;
	ability = ability_;
	assignee_location = assignee_location_;
	assignee_proximity = assignee_proximity_;
	target_location = target_location_;
	proximity = target_proximity_;
}

Directive::Directive(ASSIGNEE assignee_, ACTION_TYPE action_type_, sc2::UNIT_TYPEID unit_type_, sc2::ABILITY_ID ability_) {
	// This constructor is only valid for simple actions for a unit_type
	assert(action_type_ == ACTION_TYPE::SIMPLE_ACTION);
	assert(assignee_ == ASSIGNEE::UNIT_TYPE);
	assignee = assignee_;
	unit_type = unit_type_;
	action_type = action_type_;
	ability = ability_;
}

void Directive::enqueueDirective(Directive directive_) {
	order_queue.push_back(directive_);
}

bool Directive::hasQueuedDirective() {
	// check if there is another Directive attached to this one to be performed upon completion
	return (order_queue.size() > 0);
}

bool Directive::execute(BotAgent* agent, const sc2::ObservationInterface* obs) {
	bool found_valid_unit = false; // ensure unit has been assigned before issuing order
	const sc2::AbilityData ability_data = obs->GetAbilityData()[(int)ability]; // various info about the ability
	sc2::QueryInterface* query_interface = agent->Query(); // used to query data
	std::vector<Mob*> mobss = agent->get_mobs(); // vector of all friendly units
	Mob* mob; // used to store temporary mob

	if (assignee == ASSIGNEE::UNIT_TYPE || assignee == ASSIGNEE::UNIT_TYPE_NEAR_LOCATION) {
		if (action_type == ACTION_TYPE::EXACT_LOCATION || action_type == ACTION_TYPE::NEAR_LOCATION) {
			
			if (ability == sc2::ABILITY_ID::BUILD_ASSIMILATOR ||
				ability == sc2::ABILITY_ID::BUILD_EXTRACTOR ||
				ability == sc2::ABILITY_ID::BUILD_REFINERY) {
				// find a geyser near target location to build the structure on
				return execute_build_gas_structure(agent);
			}

			sc2::Point2D location = target_location;

			if (ability == sc2::ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST) {
				return execute_protoss_nexus_chronoboost(agent);
			}

			return execute_order_for_unit_type_with_location(agent);
		}
		if (action_type == SIMPLE_ACTION) {
			execute_simple_action_for_unit_type(agent);
		}
	}

	if (assignee == MATCH_FLAGS || assignee == MATCH_FLAGS_NEAR_LOCATION) {
		return execute_match_flags(agent);
	}
	
	return false;
}

bool Directive::executeForUnit(BotAgent* agent, const sc2::ObservationInterface* obs, const sc2::Unit& unit) {
	// used to assign an order to a specific unit
	Mob* mob = agent->getMob(unit);
	if (!mob) {
		std::cerr << "executeForUnit called for unit without an associated Mob object" << std::endl;
		return false;
	}

	if (action_type == GET_MINERALS_NEAR_LOCATION) {
		const sc2::Unit* mineral_target = agent->FindNearestMineralPatch(target_location);
		if (!mineral_target) {
			return false;
		}
		agent->Actions()->UnitCommand(&unit, sc2::ABILITY_ID::SMART, mineral_target);
		return true;
	}
	if (action_type == GET_GAS_NEAR_LOCATION) {
		const sc2::Unit* gas_target = agent->FindNearestGasStructure(target_location);
		if (!gas_target) {
			return false;
		}
		agent->Actions()->UnitCommand(&unit, sc2::ABILITY_ID::HARVEST_GATHER, gas_target);
		return true;
	}
	if (action_type == SIMPLE_ACTION) {
		mob->flags.insert(FLAGS::PERFORMING_ORDER);
		agent->Actions()->UnitCommand(&unit, ability);
		return true;
	}
	if (action_type == EXACT_LOCATION || action_type == NEAR_LOCATION) {
		sc2::Point2D location = target_location;

		if (action_type == NEAR_LOCATION)
			location = uniform_random_point_in_circle(target_location, proximity);

		agent->Actions()->UnitCommand(&unit, ability, location);
		return true;
	}
	return false;
}

void Directive::setDefault() {
	// a default directive is something that a unit performs when it has no actions
	// usually used for workers to return to gathering after building/defending
	assignee = DEFAULT_DIRECTIVE;
}

sc2::Point2D Directive::uniform_random_point_in_circle(sc2::Point2D center, float radius) {
	// given the center point and radius, return a uniform random point within the circle

	float r = radius * sqrt(sc2::GetRandomScalar());
	float theta = sc2::GetRandomScalar() * 2 * M_PI;
	float x = center.x + r * cos(theta);
	float y = center.y + r * sin(theta);
	return sc2::Point2D(x, y);
}

bool Directive::execute_simple_action_for_unit_type(BotAgent* agent) {
	// perform an action that does not require a target unit or point

	std::vector<Mob*> mobss = agent->get_mobs(); // vector of all friendly units
	Mob* mob;

	// filter idle units which match unit_type
	mobss = filter_by_unit_type(mobss, unit_type);
	mobss = filter_idle(mobss);

	if (assignee == UNIT_TYPE_NEAR_LOCATION) {
		mobss = filter_near_location(mobss, assignee_location, assignee_proximity);
	}

	if (mobss.size() == 0)
		return false;

	mob = sc2::GetRandomEntry(mobss);
	mob->flags.insert(FLAGS::PERFORMING_ORDER);
	agent->Actions()->UnitCommand(&mob->unit, ability);
	return true;
}

bool Directive::execute_build_gas_structure(BotAgent* agent) {
	// perform the necessary actions to have a gas structure built closest to the specified target_location

	std::vector<Mob*> mobss = agent->get_mobs(); // vector of all friendly units
	Mob* mob; // used to store temporary mob
	bool found_valid_unit = false;

	// find a vespene geyser near the location and build on it
	const sc2::Unit* geyser_target = agent->FindNearestGeyser(target_location);
	sc2::Point2D location = geyser_target->pos;

	if (assignee == UNIT_TYPE_NEAR_LOCATION) {
		mobss = filter_near_location(mobss, assignee_location, assignee_proximity);
	}

	mobss = filter_by_unit_type(mobss, unit_type);

	// there are no valid units to perform this action
	if (mobss.size() == 0)
		return false;

	// check if any of this unit type are already building a gas structure
	if (is_any_executing_order(mobss, ability))
		return false;
	
	// pick valid mob to execute order, closest to geyser
	mob = get_closest_to_location(mobss, target_location);

	if (!mob)
		return false;

	mob->flags.insert(FLAGS::PERFORMING_ORDER);
	agent->Actions()->UnitCommand(&(mob->unit), ability, geyser_target);
	return true;
}

bool Directive::execute_protoss_nexus_chronoboost(BotAgent* agent) {
	// this is more complex than it originally seemed
	// must find the clostest nexus to the given location
	// that has the chronoboost ability ready
	// then find a structure that would benefit from it

	std::vector<Mob*> mobss = agent->get_mobs(); // vector of all friendly units
	Mob* mob; // used to store temporary mob
	Mob* chrono_target;
	std::vector<Mob*> mobss_filter1;
	static const sc2::Unit* unit_to_target;

	if (assignee == UNIT_TYPE_NEAR_LOCATION) {
		mobss = filter_near_location(mobss, assignee_location, assignee_proximity);
	}

	// first get list of Mobs matching type
	mobss = filter_by_unit_type(mobss, unit_type);

	// then filter by those with ability available
	std::vector<Mob*> mobss_filter;
	std::copy_if(mobss.begin(), mobss.end(), std::back_inserter(mobss_filter),
		[agent, this](Mob* s) { return agent->AbilityAvailable(s->unit, ability); });
	mobss = mobss_filter;
	
	// return false if no structures exist with chronoboost ready to cast
	int num_units = mobss.size();
	if (num_units == 0)
		return false;

	// then pick the one closest to location
	mob = get_closest_to_location(mobss, target_location);

	// return false if mob has not been assigned above
	if (!mob)
		return false;

	// get all structures
	std::vector<Mob*> structures = agent->filter_by_flag(agent->get_mobs(), FLAGS::IS_STRUCTURE);
	std::vector<Mob*> structures_with_orders; 

	// look for buildings that are doing something
	std::copy_if(structures.begin(), structures.end(), std::back_inserter(structures_with_orders),
		[this](Mob* s) { return (s->unit.orders).size() > 0; });

	if (structures_with_orders.size() == 0)
		return false;

	// then pick one of THESE closest to location
	chrono_target = get_closest_to_location(structures_with_orders, target_location);

	// return false if there is nothing worth casting chronoboost on
	if (!chrono_target) {
		return false;
	}

	mob->flags.insert(FLAGS::PERFORMING_ORDER);
	agent->Actions()->UnitCommand(&mob->unit, ability, &chrono_target->unit);
	return true;
}

bool Directive::execute_match_flags(BotAgent* agent) {
	// issue an order to units matching the provided flags

	std::vector<Mob*> mobss = agent->get_mobs(); // vector of all friendly units
	std::vector<Mob*> matching_mobss = agent->filter_by_flags(mobss, flags);

	// get only units near the assignee_location parameter
	if (assignee == MATCH_FLAGS_NEAR_LOCATION) {
		matching_mobss = filter_near_location(matching_mobss, assignee_location, assignee_proximity);
	}

	// no units match the condition(s)
	if (matching_mobss.size() == 0) {
		return false;
	}

	bool found_valid_unit = false;
	sc2::Point2D location = target_location;
	sc2::Units units;
	std::vector<Mob*> filtered_mobss;
	if (action_type == ACTION_TYPE::EXACT_LOCATION || action_type == ACTION_TYPE::NEAR_LOCATION) {
		for (Mob* s : matching_mobss) {
			if (action_type == ACTION_TYPE::NEAR_LOCATION) {
				location = uniform_random_point_in_circle(target_location, proximity);
			}
			// Unit has no orders
			if ((s->unit.orders).size() == 0) {
				found_valid_unit = true;
				filtered_mobss.push_back(s);
				units.push_back(&s->unit);
			}
		}
	}
	if (!found_valid_unit)
		return false;

	for (auto s : filtered_mobss) {
		s->flags.insert(FLAGS::PERFORMING_ORDER);
	}
	agent->Actions()->UnitCommand(units, ability, location);
	return true;
}

bool Directive::execute_order_for_unit_type_with_location(BotAgent* agent) {
	const sc2::AbilityData ability_data = agent->Observation()->GetAbilityData()[(int)ability]; // various info about the ability
	sc2::QueryInterface* query_interface = agent->Query(); // used to query data
	sc2::Point2D location = target_location;
	std::vector<Mob*> mobss = agent->get_mobs();
	Mob* mob = nullptr;

	if (action_type == ACTION_TYPE::NEAR_LOCATION) {
		location = uniform_random_point_in_circle(target_location, proximity);
	}

	mobss = filter_by_unit_type(mobss, unit_type);

	// unit of type does not exist
	if (mobss.size() == 0)
		return false;

	// check if a unit of this type is already executing this order
	if (is_any_executing_order(mobss, ability))
		return false;
	
	// get closest matching unit to target location
	mob = get_closest_to_location(mobss, target_location);

	if (!mob)
		return false;
	
	// if order is a build structure order, ensure a valid placement location
	if (ability_data.is_building) {
		int i = 0;
		while (!query_interface->Placement(ability, location)) {
			location = uniform_random_point_in_circle(target_location, proximity);
			i++;
			if (i > 20) {
				// can't find a suitable spot to build
				return false;
			}
		}
	}

	// if the ability does not require a target location
	if (ability_data.target == sc2::AbilityData::Target::None) {
		mob->flags.insert(FLAGS::PERFORMING_ORDER);
		agent->Actions()->UnitCommand(&mob->unit, ability);
		return true;
	}

	// if the ability requires a target unit
	if (ability_data.target == sc2::AbilityData::Target::Unit) {
		std::cerr << ability_data.friendly_name << " requires a target unit." << std::endl;
		return false;
	}

	// else, the ability requires a target location
	mob->flags.insert(FLAGS::PERFORMING_ORDER);
	agent->Actions()->UnitCommand(&mob->unit, ability, location);
	return true;
}

bool Directive::is_any_executing_order(std::vector<Mob*> mobs_vector, sc2::ABILITY_ID ability_) {
	// check if any Mob in the vector is already executing the specified order
	for (Mob* s : mobs_vector) {
		for (const auto& order : s->unit.orders) {
			if (order.ability_id == ability_)
				return false;
		}
	}
	return true;
}

Mob* Directive::get_closest_to_location(std::vector<Mob*> mobs_vector, sc2::Point2D pos_) {
	// return a pointer to the Mob object closest to a location

	float lowest_distance = std::numeric_limits<float>::max();
	Mob* closest_sm = nullptr;
	for (Mob* s : mobs_vector) {
		float dist = sc2::DistanceSquared2D(s->unit.pos, pos_);
		if (dist < lowest_distance) {
			closest_sm = s;
			lowest_distance = dist;
		}
	}
	return closest_sm;
}

std::vector<Mob*> Directive::filter_near_location(std::vector<Mob*> mobs_vector, sc2::Point2D pos_, float radius_) {
	// filters a vector of Mob* by only those within the specified distance to location
	float sq_dist = pow(radius_, 2);

	std::vector<Mob*> filtered_mobss;
	std::copy_if(mobs_vector.begin(), mobs_vector.end(), std::back_inserter(filtered_mobss),
		[sq_dist, pos_](Mob* s) { return (
			sc2::DistanceSquared2D(s->unit.pos, pos_) <= sq_dist);
		});
	return filtered_mobss;
}

std::vector<Mob*> Directive::filter_by_unit_type(std::vector<Mob*> mobs_vector, sc2::UNIT_TYPEID unit_type_) {
	std::vector<Mob*> filtered;
	std::copy_if(mobs_vector.begin(), mobs_vector.end(), std::back_inserter(filtered),
		[this](Mob* s) { return s->unit.unit_type == unit_type; });
	return filtered;
}

std::vector<Mob*> Directive::filter_idle(std::vector<Mob*> mobs_vector) {
	std::vector<Mob*> filtered;
	std::copy_if(mobs_vector.begin(), mobs_vector.end(), std::back_inserter(filtered),
		[this](Mob* s) { return (s->unit.orders).size() == 0; });
	return filtered;
}