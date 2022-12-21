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

	uint32_t movementType{};
	if (!bitStream->Read(movementType)) {
		Game::logger->Log("MovementSwitchBehavior", "Unable to read movementType from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
		return;
	};
	Game::logger->LogDebug("MovementSwitchBehavior", "Movement type %i", movementType);
	switch (movementType) {
	case 1:
	case 3:
		this->m_groundAction->Handle(context, bitStream, branch);
		break;
	case 2:
		this->m_jumpAction->Handle(context, bitStream, branch);
		break;
	case 4:
		this->m_doubleJumpAction->Handle(context, bitStream, branch);
		break;
	case 5:
		this->m_fallingAction->Handle(context, bitStream, branch);
		break;
	case 6:
		this->m_jetpackAction->Handle(context, bitStream, branch);
		break;
	default:
		this->m_groundAction->Handle(context, bitStream, branch);
	}
}

void MovementSwitchBehavior::Load() {
	this->m_groundAction = GetAction("ground_action");

	this->m_airAction = GetAction("air_action");
	if (!this->m_airAction) this->m_airAction = this->m_groundAction;

	this->m_doubleJumpAction = GetAction("double_jump_action");
	if (!this->m_doubleJumpAction) this->m_airAction = this->m_groundAction;

	this->m_fallingAction = GetAction("falling_action");
	if (!this->m_fallingAction) this->m_airAction = this->m_groundAction;

	this->m_jetpackAction = GetAction("jetpack_action");
	if (!this->m_jetpackAction) this->m_airAction = this->m_groundAction;

	this->m_jumpAction = GetAction("jump_action");
	if (!this->m_jumpAction) this->m_airAction = this->m_groundAction;

	this->m_movingAction = GetAction("moving_action");
	if (!this->m_movingAction) this->m_airAction = this->m_groundAction;
}
