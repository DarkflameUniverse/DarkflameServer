#include "AndBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "dLogger.h"

void AndBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	for (auto* behavior : this->m_behaviors) {
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

void AndBehavior::Load() {
	const auto parameters = GetParameterNames();

	for (const auto& parameter : parameters) {
		if (parameter.first.rfind("behavior", 0) == 0) {
			auto* action = GetAction(parameter.second);

			this->m_behaviors.push_back(action);
		}
	}
}
