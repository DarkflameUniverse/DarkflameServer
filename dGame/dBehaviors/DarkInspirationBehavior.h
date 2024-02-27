#pragma once
#include "Behavior.h"

class DarkInspirationBehavior final : public Behavior
{
public:
	/*
	 * Inherited
	 */

	explicit DarkInspirationBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;

	void Load() override;
private:
	Behavior* m_ActionIfFactionMatches;
	uint32_t m_FactionList;
};
