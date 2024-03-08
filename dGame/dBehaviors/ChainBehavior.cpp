#include "ChainBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "Logger.h"

void ChainBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, const BehaviorBranchContext branch) {
	uint32_t chainIndex{};

	if (!bitStream.Read(chainIndex)) {
		Log::Warn("Unable to read chainIndex from bitStream, aborting Handle! {:d}", bitStream.GetNumberOfUnreadBits());
		return;
	}

	chainIndex--;

	if (chainIndex < this->m_behaviors.size()) {
		this->m_behaviors.at(chainIndex)->Handle(context, bitStream, branch);
	} else {
		Log::Warn("chainIndex out of bounds, aborting handle of chain {:d} bits unread {:d}", chainIndex, bitStream.GetNumberOfUnreadBits());
	}
}

void ChainBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, const BehaviorBranchContext branch) {
	bitStream.Write(1);

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
