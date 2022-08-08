#include "ChainBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "dLogger.h"

void ChainBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	uint32_t chain_index;

	bitStream->Read(chain_index);

	chain_index--;

	if (chain_index < this->m_behaviors.size()) {
		this->m_behaviors.at(chain_index)->Handle(context, bitStream, branch);
	}
}

void ChainBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	bitStream->Write(1);

	this->m_behaviors.at(0)->Calculate(context, bitStream, branch);
}

void ChainBehavior::Load() {
	const auto parameters = GetParameterNames();

	for (const auto& parameter : parameters) {
		if (parameter.first.rfind("behavior", 0) == 0) {
			auto* action = GetAction(parameter.second);

			this->m_behaviors.push_back(action);
		}
	}
}
