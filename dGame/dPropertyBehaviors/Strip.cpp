#include "Strip.h"

#include <algorithm>
#include "CombatMessages.h"

#include "Amf3.h"
#include "ControlBehaviorMsgs.h"
#include "tinyxml2.h"
#include "dEntity/EntityInfo.h"
#include "ModelComponent.h"
#include "ChatPackets.h"
#include "PropertyManagementComponent.h"
#include "PlayerManager.h"
#include "SimplePhysicsComponent.h"
#include "DestroyableComponent.h"

#include "dChatFilter.h"

#include "DluAssert.h"
#include "Loot.h"
#include "PropertyBehaviorActions.h"
#include "ObjectMessages.h"
#include "InventoryMessages.h"
#include "PropertyMessages.h"

template <>
void Strip::HandleMsg(AddStripMessage& msg) {
	m_Actions = msg.GetActionsToAdd();
	m_Position = msg.GetPosition();
};

template <>
void Strip::HandleMsg(AddActionMessage& msg) {
	if (msg.GetActionIndex() == -1) return;
	m_Actions.insert(m_Actions.begin() + msg.GetActionIndex(), msg.GetAction());
};

template <>
void Strip::HandleMsg(UpdateStripUiMessage& msg) {
	m_Position = msg.GetPosition();
};

template <>
void Strip::HandleMsg(RemoveStripMessage& msg) {
	m_Actions.clear();
};

template <>
void Strip::HandleMsg(RemoveActionsMessage& msg) {
	if (msg.GetActionIndex() >= m_Actions.size()) return;
	m_Actions.erase(m_Actions.begin() + msg.GetActionIndex(), m_Actions.end());
};

template <>
void Strip::HandleMsg(UpdateActionMessage& msg) {
	if (msg.GetActionIndex() >= m_Actions.size()) return;
	m_Actions.at(msg.GetActionIndex()) = msg.GetAction();
};

template <>
void Strip::HandleMsg(RearrangeStripMessage& msg) {
	if (msg.GetDstActionIndex() >= m_Actions.size() || msg.GetSrcActionIndex() >= m_Actions.size() || msg.GetSrcActionIndex() <= msg.GetDstActionIndex()) return;
	std::rotate(m_Actions.begin() + msg.GetDstActionIndex(), m_Actions.begin() + msg.GetSrcActionIndex(), m_Actions.end());
};

template <>
void Strip::HandleMsg(SplitStripMessage& msg) {
	if (msg.GetTransferredActions().empty() && !m_Actions.empty()) {
		auto startToMove = m_Actions.begin() + msg.GetSrcActionIndex();
		msg.SetTransferredActions(startToMove, m_Actions.end());
		m_Actions.erase(startToMove, m_Actions.end());
	} else {
		m_Actions = msg.GetTransferredActions();
		m_Position = msg.GetPosition();
	}
};

template <>
void Strip::HandleMsg(MergeStripsMessage& msg) {
	if (msg.GetMigratedActions().empty() && !m_Actions.empty()) {
		msg.SetMigratedActions(m_Actions.begin(), m_Actions.end());
		m_Actions.erase(m_Actions.begin(), m_Actions.end());
	} else {
		m_Actions.insert(m_Actions.begin() + msg.GetDstActionIndex(), msg.GetMigratedActions().begin(), msg.GetMigratedActions().end());
	}
};

template <>
void Strip::HandleMsg(MigrateActionsMessage& msg) {
	if (msg.GetMigratedActions().empty() && !m_Actions.empty()) {
		auto startToMove = m_Actions.begin() + msg.GetSrcActionIndex();
		msg.SetMigratedActions(startToMove, m_Actions.end());
		m_Actions.erase(startToMove, m_Actions.end());
	} else {
		m_Actions.insert(m_Actions.begin() + msg.GetDstActionIndex(), msg.GetMigratedActions().begin(), msg.GetMigratedActions().end());
	}
}

template<>
void Strip::HandleMsg(GameMessages::RequestUse& msg) {
	if (m_PausedTime > 0.0f || !HasMinimumActions()) return;

	auto& nextAction = GetNextAction();

	if (nextAction.GetType() == PropertyBehaviorActions::ON_INTERACT) {
		IncrementAction();
		m_WaitingForAction = false;
		m_StripInitiatorID = msg.target;
	}
}

template<>
void Strip::HandleMsg(GameMessages::ResetModelToDefaults& msg) {
	m_WaitingForAction = false;
	m_PausedTime = 0.0f;
	m_NextActionIndex = 0;
	m_InActionTranslation = NiPoint3Constant::ZERO;
	m_PreviousFramePosition = NiPoint3Constant::ZERO;
	m_InActionRotation = NiPoint3Constant::ZERO;
	m_RotationProgress = 0.0f;
	m_Speed = DEFAULT_SPEED;
	m_PausedFromOnTimer = false;
	m_MovingToStart = false;
}

void Strip::OnChatMessageReceived(const std::string& sMessage, const LWOOBJID sender) {
	if (m_PausedTime > 0.0f || !HasMinimumActions()) return;

	const auto& nextAction = GetNextAction();
	if (nextAction.GetType() == PropertyBehaviorActions::ON_CHAT && nextAction.GetValueParameterString() == sMessage) {
		IncrementAction();
		m_WaitingForAction = false;
		m_StripInitiatorID = sender;
	}
}

void Strip::OnHit(const LWOOBJID attacker) {
	if (m_PausedTime > 0.0f || !HasMinimumActions()) return;

	const auto& nextAction = GetNextAction();
	if (nextAction.GetType() == PropertyBehaviorActions::ON_ATTACK) {
		IncrementAction();
		m_WaitingForAction = false;
		m_StripInitiatorID = attacker;
	}
}

void Strip::IncrementAction() {
	if (m_Actions.empty()) return;
	m_NextActionIndex++;
	m_NextActionIndex %= m_Actions.size();
}

void Strip::Spawn(LOT lot, Entity& entity) {
	EntityInfo info{};
	info.lot = lot;
	info.pos = entity.GetPosition();
	info.rot = QuatUtils::IDENTITY;
	info.spawnerID = entity.GetObjectID();
	auto* const spawnedEntity = Game::entityManager->CreateEntity(info, nullptr, &entity);
	spawnedEntity->AddToGroup("SpawnedPropertyEnemies");
	Game::entityManager->ConstructEntity(spawnedEntity);
}

// Spawns a specific drop for all
void Strip::SpawnDrop(LOT dropLOT, Entity& entity) {
	for (auto* const player : PlayerManager::GetAllPlayers()) {
		GameMessages::DropClientLoot lootMsg{};
		lootMsg.target = player->GetObjectID();
		lootMsg.ownerID = player->GetObjectID();
		lootMsg.sourceID = entity.GetObjectID();
		lootMsg.item = dropLOT;
		lootMsg.count = 1;
		lootMsg.spawnPos = entity.GetPosition();
		Loot::DropItem(*player, lootMsg);
	}
}

void Strip::ProcNormalAction(float deltaTime, ModelComponent& modelComponent, UpdateResult& updateResult) {
	auto& entity = *modelComponent.GetParent();
	auto& nextAction = GetNextAction();
	auto number = nextAction.GetValueParameterDouble();
	auto valueStr = nextAction.GetValueParameterString();
	auto numberAsInt = static_cast<int32_t>(number);
	auto nextActionType = GetNextAction().GetType();
	LOG_DEBUG("Processing Strip Action: %s with number %.2f and string %s", nextActionType.data(), number, valueStr.data());

	// The blocks and what they do are listed in PropertyBehaviorActions (shared with the dashboard's behavior player)
	namespace Actions = PropertyBehaviorActions;
	/* BEGIN Move */
	if (const auto* move = Actions::Find(Actions::MOVES, nextActionType)) {
		// Local right (x), up (y) and forward (z) axes
		const int axis = move->axis == 'x' ? 0 : move->axis == 'y' ? 1 : 2;
		if (modelComponent.TryStartMove(axis, move->sign, m_Speed)) {
			m_PreviousFramePosition = entity.GetPosition();
			m_MoveInterruptCount = modelComponent.GetMoveInterruptCount();
			auto& distance = move->axis == 'x' ? m_InActionTranslation.x : move->axis == 'y' ? m_InActionTranslation.y : m_InActionTranslation.z;
			distance = move->sign < 0 ? -number : number;
		}
	}
	/* END Move */

	/* BEGIN Rotate */
	else if (const auto* rotation = Actions::Find(Actions::ROTATIONS, nextActionType)) {
		// Local tilt (x), spin (y) and roll (z) axes
		const int axis = rotation->axis == 'x' ? 0 : rotation->axis == 'y' ? 1 : 2;
		if (number != 0.0 && modelComponent.TryStartRotation(axis, rotation->sign, m_Speed)) {
			m_MoveInterruptCount = modelComponent.GetMoveInterruptCount();
			auto& degrees = rotation->axis == 'x' ? m_InActionRotation.x : rotation->axis == 'y' ? m_InActionRotation.y : m_InActionRotation.z;
			degrees = rotation->sign * number;
			m_RotationProgress = 0.0f;
		}
	}
	/* END Rotate */

	/* BEGIN Navigation */
	else if (nextActionType == Actions::SET_SPEED) {
		// Floored so a move or rotation can never stall forever
		m_Speed = std::max(static_cast<float>(number), MIN_SPEED);
	} else if (nextActionType == Actions::MOVE_BACK_TO_START) {
		modelComponent.StartMoveTo(modelComponent.GetOriginalPosition(), m_Speed);
		m_MovingToStart = true;
	}
	/* END Navigation */

	/* BEGIN Action */
	else if (nextActionType == Actions::SMASH) {
		if (!modelComponent.IsUnSmashing()) {
			GameMessages::Smash smash{};
			smash.target = entity.GetObjectID();
			smash.killerID = entity.GetObjectID();
			smash.Send(UNASSIGNED_SYSTEM_ADDRESS);
		}
	} else if (nextActionType == Actions::UNSMASH) {
		GameMessages::UnSmash unsmash{};
		unsmash.target = entity.GetObjectID();
		unsmash.duration = number;
		unsmash.builderID = LWOOBJID_EMPTY;
		unsmash.Send(UNASSIGNED_SYSTEM_ADDRESS);
		modelComponent.AddUnSmash();

		// since it may take time for the message to relay to clients
		m_PausedTime = number + 0.5f;
	} else if (nextActionType == Actions::WAIT) {
		m_PausedTime = number;
	} else if (nextActionType == Actions::CHAT) {
		bool isOk = Game::chatFilter->IsSentenceOkay(valueStr.data(), eGameMasterLevel::CIVILIAN).empty();
		// In case a word is removed from the whitelist after it was approved
		const auto modelName = "%[Objects_" + std::to_string(entity.GetLOT()) + "_name]";
		if (isOk) {
			ChatPackets::Client::GeneralChatMessage chatMessage;
			chatMessage.chatChannel = 12;
			chatMessage.senderName = LUWString(modelName);
			chatMessage.senderID = entity.GetObjectID();
			chatMessage.message = GeneralUtils::ASCIIToUTF16(valueStr);
			chatMessage.Broadcast();
		}
		PropertyManagementComponent::Instance()->OnChatMessageReceived(valueStr.data(), m_StripInitiatorID);
	} else if (nextActionType == Actions::PRIVATE_MESSAGE) {
		PropertyManagementComponent::Instance()->OnChatMessageReceived(valueStr.data(), m_StripInitiatorID);
	} else if (nextActionType == Actions::PLAY_SOUND) {
		GameMessages::PlayBehaviorSound sound;
		sound.target = modelComponent.GetParent()->GetObjectID();
		sound.soundID = numberAsInt;
		sound.Send(UNASSIGNED_SYSTEM_ADDRESS);
	} else if (nextActionType == Actions::RESTART) {
		modelComponent.RestartAtEndOfFrame();
	}
	/* END Action */
	/* BEGIN Gameplay */
	else if (const auto* spawn = Actions::Find(Actions::SPAWNS, nextActionType)) {
		Spawn(spawn->lot, entity);
	} else if (nextActionType == Actions::DO_DAMAGE) {
		modelComponent.DoDamage(m_StripInitiatorID);
	} else if (const auto* drop = Actions::Find(Actions::DROPS, nextActionType)) {
		for (; numberAsInt > 0; numberAsInt--) SpawnDrop(drop->lot, entity);
	}
	/* END Gameplay */
	/* BEGIN StateMachine */
	else if (const auto* stateChange = Actions::Find(Actions::STATE_CHANGES, nextActionType)) {
		updateResult.newState = stateChange->state;
	}
	/* END StateMachine*/
	else {
		static std::set<std::string> g_WarnedActions;
		if (!g_WarnedActions.contains(nextActionType.data())) {
			LOG("Tried to play action (%s) which is not supported.", nextActionType.data());
			g_WarnedActions.insert(nextActionType.data());
		}
	}

	IncrementAction();
}

// Decrement references to the previous state if we have progressed to the next one.
void Strip::RemoveStates(ModelComponent& modelComponent) const {
	const auto& prevAction = GetPreviousAction();
	const auto prevActionType = prevAction.GetType();

	if (prevActionType == PropertyBehaviorActions::ON_INTERACT) {
		modelComponent.RemoveInteract();
		Game::entityManager->SerializeEntity(modelComponent.GetParent());
	} else if (prevActionType == PropertyBehaviorActions::ON_ATTACK) {
		modelComponent.RemoveAttack();
	} else if (prevActionType == PropertyBehaviorActions::UNSMASH) {
		modelComponent.RemoveUnSmash();
	}
}

bool Strip::CheckMovement(float deltaTime, ModelComponent& modelComponent) {
	if (m_MovingToStart) {
		if (modelComponent.IsMovingToTarget()) return false;
		m_MovingToStart = false;
	}

	// A MoveBackToStart cancelled our move, so skip to the next action
	if (m_MoveInterruptCount != modelComponent.GetMoveInterruptCount()) m_InActionTranslation = NiPoint3Constant::ZERO;

	auto& entity = *modelComponent.GetParent();
	const auto& currentPos = entity.GetPosition();
	const auto diff = currentPos - m_PreviousFramePosition;
	m_PreviousFramePosition = currentPos;

	for (int axis = 0; axis < 3; axis++) {
		const float target = m_InActionTranslation[axis];
		if (target == 0.0f) continue;

		// The local axes are orthonormal so this isolates our axis from any other active moves
		const auto& axisVector = modelComponent.GetMoveAxis(axis);
		m_InActionTranslation[axis] -= diff.DotProduct(axisVector);

		// If the sign bit is different between the two numbers, then we have finished our move.
		if (std::signbit(m_InActionTranslation[axis]) == std::signbit(target)) return false;

		// Do the final adjustment so we will have moved exactly the requested units
		entity.SetPosition(entity.GetPosition() + axisVector * m_InActionTranslation[axis]);
		modelComponent.StopMove(axis);
		m_InActionTranslation = NiPoint3Constant::ZERO;
	}

	return true;
}

bool Strip::CheckRotation(float deltaTime, ModelComponent& modelComponent) {
	if (m_MoveInterruptCount != modelComponent.GetMoveInterruptCount()) {
		for (int axis = 0; axis < 3; axis++) {
			if (m_InActionRotation[axis] != 0.0f) modelComponent.StopRotation(axis);
		}
		m_InActionRotation = NiPoint3Constant::ZERO;
		m_RotationProgress = 0.0f;
	}

	for (int axis = 0; axis < 3; axis++) {
		const float target = m_InActionRotation[axis];
		if (target == 0.0f) continue;

		// Snapping to the target keeps the final angle exact regardless of speed or frame time
		const float step = modelComponent.GetAngularSpeed(axis) * deltaTime;
		if (std::abs(target - m_RotationProgress) <= step) m_RotationProgress = target;
		else m_RotationProgress += std::copysign(step, target);

		modelComponent.SetRotationProgress(axis, m_RotationProgress);
		if (m_RotationProgress != target) return false;

		modelComponent.StopRotation(axis);
		m_InActionRotation = NiPoint3Constant::ZERO;
		m_RotationProgress = 0.0f;
	}

	return true;
}

void Strip::Update(float deltaTime, ModelComponent& modelComponent, UpdateResult& updateResult) {
	// No point in running a strip with only one action.
	// Strips are also designed to have 2 actions or more to run.
	if (!HasMinimumActions()) return;

	// Return if this strip has an active movement or rotation action
	if (!CheckMovement(deltaTime, modelComponent)) return;
	if (!CheckRotation(deltaTime, modelComponent)) return;

	// Don't run this strip if we're paused.
	m_PausedTime -= deltaTime;
	if (m_PausedTime > 0.0f) return;

	m_PausedTime = 0.0f;

	// Return here if we're waiting for external interactions to continue.
	if (m_WaitingForAction) return;

	auto& entity = *modelComponent.GetParent();
	auto& nextAction = GetNextAction();

	RemoveStates(modelComponent);

	// Check for trigger blocks and if not a trigger block proc this blocks action
	if (m_NextActionIndex == 0) {
		m_StripInitiatorID = LWOOBJID_EMPTY;
		LOG("Behavior strip started %s", nextAction.GetType().data());
		m_Speed = DEFAULT_SPEED;
		if (nextAction.GetType() == PropertyBehaviorActions::ON_INTERACT) {
			modelComponent.AddInteract();
			m_WaitingForAction = true;
		} else if (nextAction.GetType() == PropertyBehaviorActions::ON_CHAT) {
			m_WaitingForAction = true;
		} else if (nextAction.GetType() == PropertyBehaviorActions::ON_ATTACK) {
			modelComponent.AddAttack();
			m_WaitingForAction = true;
		} else if (nextAction.GetType() == PropertyBehaviorActions::ON_STARTUP) {
			IncrementAction();
		} else if (nextAction.GetType() == PropertyBehaviorActions::ON_TIMER) {
			if (!m_PausedFromOnTimer) {
				m_PausedTime = nextAction.GetValueParameterDouble();
				m_PausedFromOnTimer = true;
			} else {
				IncrementAction();
				m_PausedFromOnTimer = false;
			}
		} else {
			// in case we run into an unimplemented action or one that isnt a start node
			// mark as waiting for action so we dont waste time re-starting the same logic and serializing
			// every frame
			m_WaitingForAction = true;
		}

		Game::entityManager->SerializeEntity(entity);
	} else { // should be a normal block
		ProcNormalAction(deltaTime, modelComponent, updateResult);
	}
}

void Strip::SendBehaviorBlocksToClient(AMFArrayValue& args) const {
	m_Position.SendBehaviorBlocksToClient(args);

	auto* const actions = args.InsertArray("actions");
	for (const auto& action : m_Actions) {
		action.SendBehaviorBlocksToClient(*actions);
	}
}

void Strip::Serialize(tinyxml2::XMLElement& strip) const {
	auto* const positionElement = strip.InsertNewChildElement("Position");
	m_Position.Serialize(*positionElement);
	for (const auto& action : m_Actions) {
		auto* const actionElement = strip.InsertNewChildElement("Action");
		action.Serialize(*actionElement);
	}
}

void Strip::Deserialize(const tinyxml2::XMLElement& strip) {
	const auto* positionElement = strip.FirstChildElement("Position");
	if (positionElement) {
		m_Position.Deserialize(*positionElement);
	}

	for (const auto* actionElement = strip.FirstChildElement("Action"); actionElement; actionElement = actionElement->NextSiblingElement("Action")) {
		auto& action = m_Actions.emplace_back();
		action.Deserialize(*actionElement);
	}
}

const Action& Strip::GetNextAction() const {
	DluAssert(m_NextActionIndex < m_Actions.size()); return m_Actions[m_NextActionIndex];
}

const Action& Strip::GetPreviousAction() const {
	DluAssert(m_NextActionIndex < m_Actions.size());
	size_t index = m_NextActionIndex == 0 ? m_Actions.size() - 1 : m_NextActionIndex - 1;
	return m_Actions[index];
}
