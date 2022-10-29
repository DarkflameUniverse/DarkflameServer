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
