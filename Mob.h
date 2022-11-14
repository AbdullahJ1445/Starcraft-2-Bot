#pragma once
#include "sc2api/sc2_api.h"
#include "Directive.h"
#include "Agents.h"

class Directive;
class BotAgent;

enum class MOB {
	MOB_STRUCTURE,
	MOB_WORKER,
	MOB_ARMY,
	MOB_PROXY,
	MOB_TOWNHALL
};

enum class FLAGS {
	IS_STRUCTURE,
	IS_TOWNHALL,
	IS_SUPPLY,
	IS_WORKER,
	IS_MINERAL_GATHERER,
	IS_GAS_GATHERER,
	IS_DEFENSE,
	IS_PROXY,
	IS_SIEGE,
	IS_ALERT,
	IS_ATTACKER,
	IS_ATTACKING,
	IS_FLYING,
	IS_INVISIBLE,
	PERFORMING_ORDER
};

class Mob {
public:
	Mob(const sc2::Unit& unit_, MOB mobs_type);
	void initVars();

	bool is_idle();
	bool has_flag(FLAGS flag);
	void assignDirective(Directive directive_);
	bool hasDefaultDirective();
	bool hasQueuedOrder();
	bool executeDefaultDirective(BotAgent* agent, const sc2::ObservationInterface* obs);
	bool executeQueuedOrder(BotAgent* agent);

	const sc2::Unit& unit;
	std::unordered_set<FLAGS> flags;
	sc2::Point2D birth_location;
	sc2::Point2D home_location;
	sc2::Point2D assigned_location;

private:
	bool has_default_directive;
	Directive* default_directive;
	std::vector<Directive> queued_orders;
};