#include "AndBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "dLogger.h"

#include <sstream>

void AndBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch)
{
	for (auto* behavior : this->m_behaviors)
	{
		behavior->Handle(context, bitStream, branch);
	}
}

void AndBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	for (auto* behavior : this->m_behaviors) {
		behavior->Calculate(context, bitStream, branch);
	}
}

void AndBehavior::UnCast(BehaviorContext* context, const BehaviorBranchContext branch) {
	for (auto behavior : this->m_behaviors) {
		behavior->UnCast(context, branch);
	}
}

void AndBehavior::Load()
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
