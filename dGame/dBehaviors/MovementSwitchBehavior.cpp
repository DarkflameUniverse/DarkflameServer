#include "MovementSwitchBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "Logger.h"

void MovementSwitchBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, const BehaviorBranchContext branch) {
	uint32_t movementType{};
	if (!bitStream.Read(movementType)) {
		if (this->m_groundAction->m_templateId == BehaviorTemplate::EMPTY &&
			this->m_jumpAction->m_templateId == BehaviorTemplate::EMPTY &&
			this->m_fallingAction->m_templateId == BehaviorTemplate::EMPTY &&
			this->m_doubleJumpAction->m_templateId == BehaviorTemplate::EMPTY &&
			this->m_airAction->m_templateId == BehaviorTemplate::EMPTY &&
			this->m_jetpackAction->m_templateId == BehaviorTemplate::EMPTY &&
			this->m_movingAction->m_templateId == BehaviorTemplate::EMPTY) {
			return;
		}
		LOG("Unable to read movementType from bitStream, aborting Handle! %i", bitStream.GetNumberOfUnreadBits());
		return;
	};

	switch (movementType) {
	case 1:
		this->m_groundAction->Handle(context, bitStream, branch);
		break;
	case 2:
		this->m_jumpAction->Handle(context, bitStream, branch);
		break;
	case 3:
		this->m_airAction->Handle(context, bitStream, branch);
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
		break;
	}
}

Behavior* MovementSwitchBehavior::LoadMovementType(std::string movementType) {
	float actionValue = GetFloat(movementType, -1.0f);
	auto loadedBehavior = GetAction(actionValue != -1.0f ? actionValue : 0.0f);
	if (actionValue == -1.0f && loadedBehavior->m_templateId == BehaviorTemplate::EMPTY) {
		loadedBehavior = this->m_groundAction;
	}
	return loadedBehavior;
}

void MovementSwitchBehavior::Load() {
	float groundActionValue = GetFloat("ground_action", -1.0f);
	this->m_groundAction = GetAction(groundActionValue != -1.0f ? groundActionValue : 0.0f);

	this->m_airAction = LoadMovementType("air_action");
	this->m_doubleJumpAction = LoadMovementType("double_jump_action");
	this->m_fallingAction = LoadMovementType("falling_action");
	this->m_jetpackAction = LoadMovementType("jetpack_action");
	this->m_jumpAction = LoadMovementType("jump_action");
	this->m_movingAction = LoadMovementType("moving_action");
}
