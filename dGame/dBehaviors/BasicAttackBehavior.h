#pragma once
#include "Behavior.h"

class BasicAttackBehavior final : public Behavior
{
public:
	explicit BasicAttackBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	/**
	 * @brief Reads a 16bit short from the bitStream and when the actual behavior handling finishes with all of its branches, the bitStream
	 * is then offset to after the allocated bits for this stream.
	 * 
	 */
	void DoHandleBehavior(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch);

	/**
	 * @brief Handles a client initialized Basic Attack Behavior cast to be deserialized and verified on the server.
	 * 
	 * @param context The Skill's Behavior context.  All behaviors in the same tree share the same context 
	 * @param bitStream The bitStream to deserialize.  BitStreams will always check their bounds before reading in a behavior
	 * and will fail gracefully if an overread is detected.
	 * @param branch The context of this specific branch of the Skill Behavior.  Changes based on which branch you are going down.
	 */
	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	/**
	 * @brief Writes a 16bit short to the bitStream and when the actual behavior calculation finishes with all of its branches, the number
	 * of bits used is then written to where the 16bit short initially was.
	 * 
	 */
	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	/**
	 * @brief Calculates a server initialized Basic Attack Behavior cast to be serialized to the client
	 * 
	 * @param context The Skill's Behavior context.  All behaviors in the same tree share the same context
	 * @param bitStream The bitStream to serialize to.
	 * @param branch The context of this specific branch of the Skill Behavior.  Changes based on which branch you are going down.
	 */
	void DoBehaviorCalculation(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch);

	/**
	 * @brief Loads this Behaviors parameters from the database.  For this behavior specifically:
	 * max and min damage will always be the same.  If min is less than max, they are both set to max.
	 * If an action is not in the database, then no action is taken for that result.
	 * 
	 */
	void Load() override;
private:
	uint32_t m_MinDamage;

	uint32_t m_MaxDamage;

	Behavior* m_OnSuccess;

	Behavior* m_OnFailArmor;

	Behavior* m_OnFailImmune;

	Behavior* m_OnFailBlocked;
};
