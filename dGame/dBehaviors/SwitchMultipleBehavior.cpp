#include "SwitchMultipleBehavior.h"

#include <sstream>

#include "BehaviorBranchContext.h"
#include "CDActivitiesTable.h"
#include "Game.h"
#include "dLogger.h"
#include "EntityManager.h"


void SwitchMultipleBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch)
{
	float value;

	bit_stream->Read(value);
	
	uint32_t trigger = 0;

	for (unsigned int i = 0; i < this->m_behaviors.size(); i++) {
		
		const double data = this->m_behaviors.at(i).first;
		
		if (value <= data) {
			
			trigger = i;

			break;
		}
	}

	auto* behavior = this->m_behaviors.at(trigger).second;

	behavior->Handle(context, bit_stream, branch);
}

void SwitchMultipleBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch)
{
	// TODO
}

void SwitchMultipleBehavior::Load()
{
	const auto b = std::to_string(this->m_behaviorId);
	std::stringstream query;
	query << "SELECT replace(bP1.parameterID, 'behavior ', '') as key, bP1.value as behavior, "
		<< "(select bP2.value FROM BehaviorParameter bP2 WHERE bP2.behaviorID = " << b << " AND bP2.parameterID LIKE 'value %' "
		<< "AND replace(bP1.parameterID, 'behavior ', '') = replace(bP2.parameterID, 'value ', '')) as value "
		<< "FROM BehaviorParameter bP1 WHERE bP1.behaviorID = " << b << " AND bP1.parameterID LIKE 'behavior %'";
	auto result = CDClientDatabase::ExecuteQuery(query.str());

	while (!result.eof()) {
		const auto behavior_id = static_cast<uint32_t>(result.getFloatField(1));

		auto* behavior = CreateBehavior(behavior_id);

		auto value = result.getFloatField(2);

		this->m_behaviors.emplace_back(value, behavior);
		
		result.nextRow();
	}
}
