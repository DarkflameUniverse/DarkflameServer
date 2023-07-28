#include "ChainBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "dLogger.h"

void ChainBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	uint32_t chainIndex{};

	if (!bitStream->Read(chainIndex)) {
		Game::logger->Log("ChainBehavior", "Unable to read chainIndex from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
		return;
	}

	chainIndex--;

	if (chainIndex < this->m_behaviors.size()) {
		this->m_behaviors.at(chainIndex)->Handle(context, bitStream, branch);
	} else {
		Game::logger->Log("ChainBehavior", "chainIndex out of bounds, aborting handle of chain %i bits unread %i", chainIndex, bitStream->GetNumberOfUnreadBits());
	}
}

void ChainBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	bitStream->Write(1);

	this->m_behaviors.at(0)->Calculate(context, bitStream, branch);
}

void ChainBehavior::Load()
{
	std::string ss = "behavior ";

	int i = 1;

	while (true) {
		std::string s = ss + std::to_string(i);

		if (GetInt(s, 0) == 0) {
			break;
		}

		m_behaviors.push_back(GetAction(s));

		++i;
	}
}
