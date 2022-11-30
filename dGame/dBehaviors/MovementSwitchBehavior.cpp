#include "MovementSwitchBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "dLogger.h"

void MovementSwitchBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	if (this->m_groundAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY &&
		this->m_jumpAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY &&
		this->m_fallingAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY &&
		this->m_doubleJumpAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY &&
		this->m_airAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY &&
		this->m_jetpackAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY) {
		return;
	}

	uint32_t movementType;

	bitStream->Read(movementType);

	switch (movementType) {
	case 1:
		this->m_groundAction->Handle(context, bitStream, branch);
		break;
	case 2:
		this->m_jumpAction->Handle(context, bitStream, branch);
		break;
	case 3:
		this->m_fallingAction->Handle(context, bitStream, branch);
		break;
	case 4:
		this->m_doubleJumpAction->Handle(context, bitStream, branch);
		break;
	case 5:
		this->m_airAction->Handle(context, bitStream, branch);
		break;
	case 6:
		this->m_jetpackAction->Handle(context, bitStream, branch);
		break;
	default:
		Game::logger->Log("MovementSwitchBehavior", "Invalid movement behavior type (%i)!", movementType);
		break;
	}
}

void MovementSwitchBehavior::Load() {
	this->m_airAction = GetAction("air_action");

	this->m_doubleJumpAction = GetAction("double_jump_action");

	this->m_fallingAction = GetAction("falling_action");

	this->m_groundAction = GetAction("ground_action");

	this->m_jetpackAction = GetAction("jetpack_action");

	this->m_jumpAction = GetAction("jump_action");
}

