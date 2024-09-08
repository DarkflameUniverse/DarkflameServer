#include "SwitchMultipleBehavior.h"

#include <sstream>

#include "BehaviorBranchContext.h"
#include "CDActivitiesTable.h"
#include "Game.h"
#include "Logger.h"
#include "EntityManager.h"


void SwitchMultipleBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	float value{};

	if (!bitStream.Read(value)) {
		LOG("Unable to read value from bitStream, aborting Handle! %i", bitStream.GetNumberOfUnreadBits());
		return;
	};

	uint32_t trigger = 0;

	for (unsigned int i = 0; i < this->m_behaviors.size(); i++) {

		const double data = this->m_behaviors.at(i).first;
		trigger = i;

		if (value <= data) break;
	}

	auto* behavior = this->m_behaviors.at(trigger).second;

	behavior->Handle(context, bitStream, branch);
}

void SwitchMultipleBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	// TODO
}

void SwitchMultipleBehavior::Load() {
	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT replace(bP1.parameterID, 'behavior ', '') as key, bP1.value as behavior, "
		"(select bP2.value FROM BehaviorParameter bP2 WHERE bP2.behaviorID = ?1 AND bP2.parameterID LIKE 'value %' "
		"AND replace(bP1.parameterID, 'behavior ', '') = replace(bP2.parameterID, 'value ', '')) as value "
		"FROM BehaviorParameter bP1 WHERE bP1.behaviorID = ?1 AND bP1.parameterID LIKE 'behavior %';");
	query.bind(1, static_cast<int>(this->m_behaviorId));

	auto result = query.execQuery();

	while (!result.eof()) {
		const auto behavior_id = static_cast<uint32_t>(result.getFloatField("behavior"));

		auto* behavior = CreateBehavior(behavior_id);

		auto value = result.getFloatField("value");

		this->m_behaviors.emplace_back(value, behavior);

		result.nextRow();
	}
}
